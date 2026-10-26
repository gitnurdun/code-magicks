/* MAGIC: an eventfd in EFD_SEMAPHORE mode is a 64-bit counter that hands out one unit per read. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <unistd.h>

static int read_u64(int fd, uint64_t *value)
{
    ssize_t n;
    do {
        n = read(fd, value, sizeof *value);
    } while (n < 0 && errno == EINTR);
    if (n == (ssize_t)sizeof *value)
        return 0;
    if (n >= 0)
        errno = EIO;
    return -1;
}

int main(void)
{
    int fd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
    if (fd < 0) {
        if (errno == ENOSYS || errno == EINVAL) {
            printf("skipped: eventfd semaphore\n");
            return 0;
        }
        perror("eventfd");
        return 1;
    }

    uint64_t three = 3;
    ssize_t w;
    do {
        w = write(fd, &three, sizeof three);
    } while (w < 0 && errno == EINTR);
    if (w != (ssize_t)sizeof three) {
        perror("write(eventfd)");
        close(fd);
        return 1;
    }

    uint64_t a = 0, b = 0, c = 0;
    if (read_u64(fd, &a) != 0 || read_u64(fd, &b) != 0 ||
        read_u64(fd, &c) != 0) {
        perror("read(eventfd)");
        close(fd);
        return 1;
    }

    uint64_t d = 0;
    errno = 0;
    ssize_t r = read(fd, &d, sizeof d);
    int eagain = (r < 0 && errno == EAGAIN);

    printf("reads=%llu,%llu,%llu eagain=%s\n",
           (unsigned long long)a, (unsigned long long)b,
           (unsigned long long)c, eagain ? "yes" : "no");

    close(fd);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Creates an eventfd in semaphore mode, adds the counter value 3,
     performs three reads that each return 1, then proves the counter is
     empty by taking EAGAIN on a fourth nonblocking read.

   Step by step
     1. eventfd(0, EFD_SEMAPHORE|EFD_NONBLOCK) returns an fd for a
        kernel object containing a 64-bit counter and a wait queue.
     2. write(fd, &three, 8) adds 3 to the counter. Reads and writes must
        be exactly 8 bytes; 0xffffffffffffffff is EINVAL, so the largest
        usable count is 0xfffffffffffffffe.
     3. In semaphore mode each successful read subtracts one and returns
        the value 1. Without EFD_SEMAPHORE, a read returns the entire
        counter and resets it to zero.
     4. After three reads the counter is zero. EFD_NONBLOCK makes the
        fourth read fail with EAGAIN instead of sleeping in the kernel.
     5. The returned uint64_t values are printed. The counter is stored
        and returned in host byte order; no htonll/ntohll conversion is
        involved. An eventfd is process-local state, so there is no
        cross-endian wire format to agree on.

   Output
     reads=1,1,1 eagain=yes

   Why it matters
     eventfd is the cheapest way to wake a poll/epoll loop from another
     thread or a signal handler without a pipe or a condition variable.
     Semaphore mode additionally gives counting semantics: N posts allow
     N reads, unlike the default "read the whole counter" mode which is
     closer to a latch or a coalescing notification.

   Caveats
     - EFD_SEMAPHORE needs Linux 2.6.30; plain eventfd is 2.6.22.
     - The 8-byte size is part of the ABI. A 4-byte read or write is
       EINVAL, not a partial update.
     - It is not seekable and cannot be mmap'd; it is not a data channel
       for large messages.
     - With EFD_NONBLOCK, EAGAIN means zero, not an error. Without it, a
       read blocks until a writer posts.
     - fork() shares the counter object; exec() inherits the fd unless
       EFD_CLOEXEC was set. The counter is shared by all copies of the
       open file description.
   ============================================== */
