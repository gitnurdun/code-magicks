/* MAGIC: vmsplice maps two page-aligned user buffers directly into a
   pipe, then the reader gets their concatenation. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

int main(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) {
        puts("skipped: cannot determine page size");
        return 0;
    }
    size_t page = (size_t)ps;
    char *pages = mmap(NULL, page * 2, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pages == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    memcpy(pages, "hello ", 6);
    memcpy(pages + page, "vmsplice", 8);

    struct iovec iov[2] = {
        { .iov_base = pages, .iov_len = 6 },
        { .iov_base = pages + page, .iov_len = 8 },
    };
    size_t total = 14;
    int pipefd[2] = { -1, -1 };
    int rc = 1;
    if (pipe2(pipefd, O_CLOEXEC) != 0) {
        perror("pipe2");
        goto out;
    }
    ssize_t spliced = vmsplice(pipefd[1], iov, 2, 0);
    if (spliced < 0) {
        if (errno == ENOSYS || errno == EINVAL || errno == EPERM) {
            printf("skipped: vmsplice unavailable (%s)\n", strerror(errno));
            rc = 0;
            goto out;
        }
        perror("vmsplice");
        goto out;
    }
    if (spliced != (ssize_t)total) {
        fprintf(stderr, "vmsplice: copied %zd of %zu bytes\n", spliced, total);
        goto out;
    }
    if (close(pipefd[1]) != 0) {
        perror("close pipe write");
        pipefd[1] = -1;
        goto out;
    }
    pipefd[1] = -1;

    char buf[64];
    size_t got = 0;
    while (got < total) {
        ssize_t n = read(pipefd[0], buf + got, total - got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("read pipe");
            goto out;
        }
        if (n == 0)
            break;
        got += (size_t)n;
    }
    if (got != total) {
        fprintf(stderr, "read pipe: got %zu of %zu bytes\n", got, total);
        goto out;
    }
    buf[got] = '\0';
    printf("read_back=%s\n", buf);
    rc = 0;

out:
    if (pipefd[0] >= 0 && close(pipefd[0]) != 0)
        rc = 1;
    if (pipefd[1] >= 0 && close(pipefd[1]) != 0)
        rc = 1;
    if (munmap(pages, page * 2) != 0)
        rc = 1;
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Maps two page-aligned user pages, puts "hello " and "vmsplice" at
     their starts, and passes both buffers to vmsplice(). The pipe then
     holds references to those user pages. Closing the write end and
     reading the pipe returns "hello vmsplice".

   Step by step
     1. One anonymous two-page mapping supplies two page-aligned
        buffers: pages[0..page) and pages[page..2*page).
     2. The string literals are copied into the starts of those pages.
        The rest of each page is unused for this transfer.
     3. struct iovec[2] describes lengths 6 and 8. vmsplice(pipe write
        end, iov, 2, 0) maps/attaches those user ranges to the pipe.
     4. The write end is closed. read() on the read end collects the 14
        bytes and the program prints them.
     5. The pipe and mapping are released on every path. SPLICE_F_GIFT
        is deliberately NOT used, and the pages are not modified after
        vmsplice.

   Output
     read_back=hello vmsplice

   Why it matters
     vmsplice is the "user pages to pipe" half of splice. A pipe can
     carry references to the caller's pages instead of first copying
     them into pipe buffers, which is useful for zero-copy network or
     storage paths. A downstream splice can consume those references
     directly; the page cache is never used as an intermediate staging
     area for this direction.

   Caveats
     - Linux 2.6.17+. At most IOV_MAX (usually 1024) iovecs per call.
     - The pipe has finite capacity. A large vmsplice blocks until a
       reader drains it, and may return a short count.
     - The pages should stay mapped, and their contents stable, until
       the pipe data is consumed. Without SPLICE_F_GIFT the kernel does
       not take ownership, but it still references the pages. With GIFT
       the application must never touch them again.
     - Only the user-to-pipe direction is true splicing; vmsplice from
       a pipe into user memory just copies (as the man page notes).
     - A plain read() from the pipe into buf still copies into the
       reader's buffer. The saving is at the producer/pipe boundary, not
       an end-to-end guarantee.
     - Page alignment is only mandatory for SPLICE_F_GIFT, but keeping
       each iovec page-aligned avoids sub-page reference complications.
     - If the kernel or seccomp policy rejects vmsplice, the program
       prints one skipped: line and exits 0.
   ============================================== */
