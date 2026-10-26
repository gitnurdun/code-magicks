/* MAGIC: catch a missing-page fault on a userfaultfd, then satisfy it
   from a handler thread with UFFDIO_COPY. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef UFFD_USER_MODE_ONLY
#define UFFD_USER_MODE_ONLY 0x01
#endif

struct fault_ctx {
    int uffd;
    size_t page;
};

static int open_uffd(void)
{
#ifdef SYS_userfaultfd
    return (int)syscall(SYS_userfaultfd,
                        O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
#else
    errno = ENOSYS;
    return -1;
#endif
}

static void *fault_handler(void *arg)
{
    struct fault_ctx *ctx = arg;
    for (;;) {
        struct pollfd pfd = { .fd = ctx->uffd, .events = POLLIN };
        if (poll(&pfd, 1, -1) < 0) {
            if (errno == EINTR)
                continue;
            return NULL;
        }
        if (pfd.revents & POLLERR)
            return NULL;
        struct uffd_msg msg;
        ssize_t n = read(ctx->uffd, &msg, sizeof msg);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            return NULL;
        }
        if (n != (ssize_t)sizeof msg)
            return NULL;
        if (msg.event != UFFD_EVENT_PAGEFAULT)
            continue;

        void *fill = mmap(NULL, ctx->page, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (fill == MAP_FAILED)
            return NULL;
        memset(fill, 0xAB, ctx->page);
        struct uffdio_copy copy = {
            .dst = msg.arg.pagefault.address & ~(uintptr_t)(ctx->page - 1),
            .src = (uintptr_t)fill,
            .len = ctx->page,
            .mode = 0,
        };
        int ok = ioctl(ctx->uffd, UFFDIO_COPY, &copy) == 0 &&
                 copy.copy == (long long)ctx->page;
        if (munmap(fill, ctx->page) != 0)
            ok = 0;
        return (void *)(uintptr_t)ok;
    }
}

int main(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) {
        puts("skipped: cannot determine page size");
        return 0;
    }
    size_t page = (size_t)ps;
    int uffd = open_uffd();
    if (uffd < 0) {
        printf("skipped: userfaultfd unavailable (%s)\n", strerror(errno));
        return 0;
    }

    int rc = 1;
    int registered = 0;
    unsigned char *area = MAP_FAILED;
    pthread_t thread;
    struct uffdio_api api = { .api = UFFD_API, .features = 0 };
    if (ioctl(uffd, UFFDIO_API, &api) != 0) {
        printf("skipped: userfaultfd unavailable (%s)\n", strerror(errno));
        rc = 0;
        goto out;
    }
    area = mmap(NULL, page * 2, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (area == MAP_FAILED) {
        perror("mmap");
        goto out;
    }
    struct uffdio_register reg = {
        .range = { .start = (uintptr_t)area, .len = page * 2 },
        .mode = UFFDIO_REGISTER_MODE_MISSING,
    };
    if (ioctl(uffd, UFFDIO_REGISTER, &reg) != 0) {
        printf("skipped: userfaultfd unavailable (%s)\n", strerror(errno));
        rc = 0;
        goto out;
    }
    registered = 1;

    struct fault_ctx ctx = { .uffd = uffd, .page = page };
    int err = pthread_create(&thread, NULL, fault_handler, &ctx);
    if (err != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(err));
        goto out;
    }
    usleep(10000);

    unsigned char first = *(volatile unsigned char *)area;
    void *handler_ok = NULL;
    err = pthread_join(thread, &handler_ok);
    if (err != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(err));
        goto out;
    }
    if (first == 0xAB && handler_ok == (void *)(uintptr_t)1) {
        puts("first_byte=0xab");
        rc = 0;
    } else {
        printf("first_byte=0x%02x\n", first);
    }

out:
    if (registered) {
        struct uffdio_range range = {
            .start = (uintptr_t)area, .len = page * 2
        };
        if (ioctl(uffd, UFFDIO_UNREGISTER, &range) != 0)
            rc = 1;
    }
    if (area != MAP_FAILED && munmap(area, page * 2) != 0)
        rc = 1;
    if (close(uffd) != 0)
        rc = 1;
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Creates an unprivileged userfaultfd, registers two anonymous pages
     for missing faults, and reads the first byte. A handler thread
     receives the fault, copies a page filled with 0xAB into the mapping,
     and wakes the reader.

   Step by step
     1. syscall(SYS_userfaultfd, O_CLOEXEC|O_NONBLOCK|UFFD_USER_MODE_ONLY)
        creates the fd. UFFD_USER_MODE_ONLY means only user-mode faults are
        handled; glibc has no wrapper for this syscall.
     2. UFFDIO_API performs the mandatory handshake. No extra features are
        requested.
     3. mmap() creates two demand-zero pages. UFFDIO_REGISTER with
        MODE_MISSING makes the kernel stop a faulting thread and queue an
        event instead of allocating the page.
     4. The handler polls the fd and reads one struct uffd_msg. It
        page-aligns msg.arg.pagefault.address, fills a separate page with
        0xAB, and calls UFFDIO_COPY.
     5. Main sleeps 10 ms so the handler is running, then reads the first
        byte. The read blocks on the missing page; UFFDIO_COPY installs
        the page and wakes it, so the load returns 0xAB.
     6. Main joins the handler, unregisters, unmaps, and closes. EPERM,
        ENOSYS, and EINVAL at setup print one skipped: line and exit 0.

   Output
     first_byte=0xab

   Why it matters
     userfaultfd lets user space decide what a missing page contains while
     the faulting thread waits. QEMU post-copy migration, CRIU-style
     restore, user-space paging, and some sandboxes use it to make page
     faults a controlled rendezvous rather than a SIGSEGV guess.

   Caveats
     - Linux 4.3+ with CONFIG_USERFAULTFD; UFFD_USER_MODE_ONLY is 5.11+.
       Older kernels reject the flag with EINVAL, so the demo skips.
     - /proc/sys/vm/unprivileged_userfaultfd controls privileged use.
       UFFD_USER_MODE_ONLY permits user-mode-only handling without
       CAP_SYS_PTRACE on modern kernels; EPERM is still possible.
     - The handler must never fault on the registered range. It copies
       from an unrelated page; otherwise it would wait on itself.
     - Only the first fault is serviced. A real manager loops over events,
       handles UFFDIO_COPY failures such as EEXIST, and resolves every
       fault or the faulting threads hang.
     - poll() should be checked for POLLERR. Closing the uffd unregisters
       ranges and flushes events, but does not make outstanding faults
       complete.
   ============================================== */
