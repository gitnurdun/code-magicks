/* MAGIC: timerfd turns a timer into a file descriptor, so one epoll loop can wait on time itself. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

int main(void)
{
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) {
        if (errno == ENOSYS || errno == EINVAL) {
            printf("skipped: timerfd\n");
            return 0;
        }
        perror("timerfd_create");
        return 1;
    }

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        if (errno == ENOSYS) {
            printf("skipped: epoll\n");
            close(tfd);
            return 0;
        }
        perror("epoll_create1");
        close(tfd);
        return 1;
    }

    struct itimerspec its;
    memset(&its, 0, sizeof its);
    its.it_value.tv_nsec = 20 * 1000 * 1000;    /* first tick: 20 ms */
    its.it_interval.tv_nsec = 20 * 1000 * 1000; /* then every 20 ms */

    if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
        perror("timerfd_settime");
        close(epfd);
        close(tfd);
        return 1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &ev) < 0) {
        perror("epoll_ctl");
        close(epfd);
        close(tfd);
        return 1;
    }

    int ticks = 0;
    uint64_t expirations = 0;

    while (ticks < 3) {
        int n = epoll_wait(epfd, &ev, 1, 500);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            close(epfd);
            close(tfd);
            return 1;
        }
        if (n == 0)
            break; /* safety net: 500 ms passed with no timer event */

        uint64_t count = 0;
        ssize_t r = read(tfd, &count, sizeof count);
        if (r != (ssize_t)sizeof count) {
            if (r < 0 && errno == EINTR)
                continue;
            if (r < 0)
                perror("read(timerfd)");
            else
                fprintf(stderr, "read(timerfd): short read\n");
            close(epfd);
            close(tfd);
            return 1;
        }

        expirations += count;
        ticks++;
    }

    printf("ticks=%d expirations>=3=%s\n", ticks,
           expirations >= 3 ? "yes" : "no");

    close(epfd);
    close(tfd);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Creates a 20 ms timerfd and registers it with epoll. It waits for
     three expirations, reading the uint64_t expiration counter from the
     timer descriptor each time, and reports that at least three ticks
     were observed.

   Step by step
     1. timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC)
        returns a file descriptor backed by a kernel hrtimer, not by a
        user thread. Reads return an 8-byte uint64_t.
     2. timerfd_settime arms it with a 20 ms initial expiration and a
        20 ms interval. flags == 0 means the itimerspec fields are
        relative to now, not CLOCK_REALTIME absolute times.
     3. epoll_ctl adds the timerfd for EPOLLIN. Readable means at least
        one expiration is pending.
     4. Each epoll_wait(..., 500) blocks until the timer fires. read()
        then drains the current expiration count. The kernel coalesces
        missed ticks: if the process slept through three intervals, one
        read can return 3, and the counter resets to zero.
     5. The loop stops after three successful waits. The final line is a
        boolean because the split of expirations across reads depends on
        scheduling; the sum is what is guaranteed.

   Output
     ticks=3 expirations>=3=yes

   Why it matters
     Time becomes pollable state. One event loop can wait on sockets,
     signals, inotify, and timers without a helper thread or a timeout
     calculation layered over every other descriptor. This replaces the
     classic SIGALRM/self-pipe pattern with an ordinary fd.

   Caveats
     - Linux-only. timerfd_create is always present on normal kernels;
       a seccomp filter or very old libc headers can still block it.
     - Expirations coalesce rather than queue. A slow reader sees a
       larger count, not one wakeup per missed period.
     - CLOCK_MONOTONIC does not advance during suspend. Use
       CLOCK_BOOTTIME if time asleep must count.
     - The 500 ms epoll timeout is only a safety net; if it expires the
       program prints ticks=<3 instead of claiming success.
     - An interval timer is not a cron schedule: the next expiration is
       relative, so long overruns can shift phase.
   ============================================== */
