/* MAGIC: close_range closes an entire descriptor range in one syscall,
   optionally after unsharing the file-descriptor table. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_close_range
#define __NR_close_range 436
#endif
#ifndef CLOSE_RANGE_UNSHARE
#define CLOSE_RANGE_UNSHARE (1U << 1)
#endif

int main(void)
{
    int fds[10];
    int i, low_gone, high_open;

    /* The parent leaves only 0, 1, 2 open, so these land on 3..12. */
    for (i = 0; i < 10; i++) {
        fds[i] = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (fds[i] < 0) {
            perror("open /dev/null");
            while (i-- > 0)
                close(fds[i]);
            return 1;
        }
    }

    if (syscall(__NR_close_range, 3, 9, 0) != 0) {
        if (errno == ENOSYS) {
            printf("skipped: close_range needs Linux 5.9\n");
            for (i = 0; i < 10; i++)
                close(fds[i]);
            return 0;
        }
        perror("close_range");
        for (i = 0; i < 10; i++)
            close(fds[i]);
        return 1;
    }

    errno = 0;
    low_gone = (fcntl(5, F_GETFD) == -1 && errno == EBADF);
    high_open = (fcntl(10, F_GETFD) != -1);

    if (syscall(__NR_close_range, 3, ~0U, CLOSE_RANGE_UNSHARE) != 0) {
        if (errno == EINVAL) {
            printf("skipped: CLOSE_RANGE_UNSHARE needs Linux 5.11\n");
            for (i = 0; i < 10; i++)
                if (fds[i] >= 0 && fcntl(fds[i], F_GETFD) != -1)
                    close(fds[i]);
            return 0;
        }
        perror("close_range");
        for (i = 0; i < 10; i++)
            if (fds[i] >= 0 && fcntl(fds[i], F_GETFD) != -1)
                close(fds[i]);
        return 1;
    }

    printf("closed_low=%s high_still_open=%s\n",
           low_gone ? "yes" : "no", high_open ? "yes" : "no");

    /* Everything at or above 3 is gone; this loop is defensive only. */
    for (i = 0; i < 10; i++)
        if (fds[i] >= 0 && fcntl(fds[i], F_GETFD) != -1)
            close(fds[i]);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Opens ten descriptors (3 through 12) on /dev/null, closes the range
     3..9 with close_range(2), proves that fd 5 is EBADF while fd 10
     survived, then closes everything from 3 upward with
     CLOSE_RANGE_UNSHARE.  It prints closed_low=yes high_still_open=yes.

   Step by step
     1. Ten open() calls populate the lowest free descriptors.  With a
        normal shell parent these are exactly 3..12.
     2. syscall(__NR_close_range, 3, 9, 0) walks the descriptor table
        and releases every entry in [3, 9] in one kernel call - no
        per-fd close(), no userspace loop, no window for another thread
        to reuse an fd between closes.
     3. fcntl(5, F_GETFD) returns -1 with errno EBADF, confirming the
        low range is gone.  fcntl(10, F_GETFD) still returns flags,
        confirming that close_range stopped at 9.
     4. close_range(3, ~0U, CLOSE_RANGE_UNSHARE) covers every fd from 3
        to UINT_MAX.  The UNSHARE flag first gives this thread a private
        copy of the fd table, then closes the range in that copy; other
        threads that shared the old table keep their descriptors.
        Without UNSHARE the close would happen in the shared table and
        every thread would observe it.
     5. The final loop is only defensive: every fds[i] is already gone,
        so fcntl() says EBADF and nothing is closed twice.

   Output
     closed_low=yes high_still_open=yes

   Why it matters
     Servers that fork/exec helpers want a deterministic descriptor
     hygiene step.  close_range replaces the O(n) loop (and its races
     with concurrent open in another thread) with one syscall, and
     CLOSE_RANGE_CLOEXEC marks a whole range close-on-exec without
     closing it in the parent.  UNSHARE matters for multi-threaded
     programs where closing fds another thread is using could break it:
     the kernel copies the table first (the same machinery as
     unshare(CLONE_FILES)), so the close is local.

   Caveats
     - close_range() is Linux 5.9; CLOSE_RANGE_UNSHARE and
       CLOSE_RANGE_CLOEXEC are Linux 5.11.  glibc gained a wrapper in
       2.34, but calling syscall() keeps the flags explicit.
     - UNSHARE can fail with ENOMEM while copying a large fd table; it
       is not free.
     - Closing a range can still race with code that expects a specific
       fd to stay open (for example, a library caching fd numbers), even
       though it removes the close/open reuse race within the call.
     - close_range ignores descriptors that are not open; it does not
       return EBADF for holes.  EINVAL means first > last.
     - ~0U is 0xffffffff; do not use ~0 (a signed -1) in a context where
       it would be sign-extended.
     - O_CLOEXEC on the individual opens is still the right tool for a
       single fd; close_range is for bulk cleanup.
   ============================================== */
