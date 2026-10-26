/* MAGIC: EPOLLEXCLUSIVE puts a socket on several epoll instances but
   wakes only one of them per readiness event. */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef EPOLLEXCLUSIVE
#define EPOLLEXCLUSIVE (1u << 28)
#endif

static int drain(int lfd)
{
    int count = 0;

    for (;;) {
        int c = accept4(lfd, NULL, NULL, SOCK_CLOEXEC);

        if (c >= 0) {
            close(c);
            count++;
            continue;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        perror("accept4");
        break;
    }
    return count;
}

int main(void)
{
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    struct epoll_event ev;
    socklen_t alen = sizeof addr;
    int lfd = -1, ep1 = -1, ep2 = -1, c1 = -1, c2 = -1;
    int accepted = 0, flags, i, rc = 1;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        perror("socket");
        return 1;
    }
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        perror("bind");
        goto out;
    }
    if (listen(lfd, 16) != 0) {
        perror("listen");
        goto out;
    }
    if (getsockname(lfd, (struct sockaddr *)&addr, &alen) != 0) {
        perror("getsockname");
        goto out;
    }
    flags = fcntl(lfd, F_GETFL);
    if (flags < 0 || fcntl(lfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        goto out;
    }

    ep1 = epoll_create1(EPOLL_CLOEXEC);
    ep2 = epoll_create1(EPOLL_CLOEXEC);
    if (ep1 < 0 || ep2 < 0) {
        perror("epoll_create1");
        goto out;
    }
    ev.events = EPOLLIN | EPOLLEXCLUSIVE;
    ev.data.fd = lfd;
    if (epoll_ctl(ep1, EPOLL_CTL_ADD, lfd, &ev) != 0) {
        if (errno == EINVAL) {
            printf("skipped: EPOLLEXCLUSIVE needs Linux 4.5\n");
            rc = 0;
        } else {
            perror("epoll_ctl");
        }
        goto out;
    }
    if (epoll_ctl(ep2, EPOLL_CTL_ADD, lfd, &ev) != 0) {
        if (errno == EINVAL) {
            printf("skipped: EPOLLEXCLUSIVE needs Linux 4.5\n");
            rc = 0;
        } else {
            perror("epoll_ctl");
        }
        goto out;
    }

    c1 = socket(AF_INET, SOCK_STREAM, 0);
    c2 = socket(AF_INET, SOCK_STREAM, 0);
    if (c1 < 0 || c2 < 0) {
        perror("socket");
        goto out;
    }
    if (connect(c1, (struct sockaddr *)&addr, sizeof addr) != 0 ||
        connect(c2, (struct sockaddr *)&addr, sizeof addr) != 0) {
        perror("connect");
        goto out;
    }

    for (i = 0; i < 20 && accepted < 2; i++) {
        struct epoll_event evs[4];
        int n = epoll_wait(ep1, evs, 4, 100);

        if (n < 0 && errno != EINTR) {
            perror("epoll_wait");
            goto out;
        }
        if (n > 0)
            accepted += drain(lfd);
        if (accepted >= 2)
            break;

        n = epoll_wait(ep2, evs, 4, 100);
        if (n < 0 && errno != EINTR) {
            perror("epoll_wait");
            goto out;
        }
        if (n > 0)
            accepted += drain(lfd);
    }
    printf("accepted=%d\n", accepted);
    rc = 0;

out:
    if (c1 >= 0)
        close(c1);
    if (c2 >= 0)
        close(c2);
    if (ep2 >= 0)
        close(ep2);
    if (ep1 >= 0)
        close(ep1);
    if (lfd >= 0)
        close(lfd);
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Creates a nonblocking TCP listener on a loopback port, adds it to
     two separate epoll instances with EPOLLIN|EPOLLEXCLUSIVE, opens two
     client connections, and accepts both from whichever epoll instance
     is woken.  It prints only the total: accepted=2.  Which epoll
     instance wins each event is scheduler-dependent.

   Step by step
     1. socket/bind/listen(backlog 16) create the accept queue; port 0
        lets the kernel choose, and getsockname() recovers it.
     2. The listener is set O_NONBLOCK so accept4() can drain the queue
        until EAGAIN instead of blocking on the last call.
     3. Two independent epoll instances are created.  The same listener
        fd is added to both with EPOLLIN|EPOLLEXCLUSIVE.  Without the
        flag both epoll instances would be woken for every connection,
        even though only one can win the accept.
     4. Two clients connect.  Their SYNs complete in the kernel because
        the listen backlog has room; connect() returning does not mean
        userspace accepted them.
     5. Each epoll_wait waits up to 100 ms.  On readiness, drain() calls
        accept4() repeatedly and closes each accepted fd until EAGAIN.
        Either epoll instance may report the event, and one instance may
        drain both connections.
     6. The loop stops once two connections have been accepted and
        prints the count.  The distribution is deliberately not printed
        because it is not deterministic.

   Output
     accepted=2

   Why it matters
     The "thundering herd" is many workers sleeping on one listening
     socket: every new connection wakes all of them, they race for the
     accept queue, all but one go back to sleep, and the CPU cost scales
     with the number of idle workers.  EPOLLEXCLUSIVE marks a waiter as
     exclusive so an event wakes one epoll instance instead of all;
     each instance can still have several threads blocked in
     epoll_wait(), so the flag reduces cross-instance wakeups, not
     intra-instance ones.  It is the basis of many multi-process
     accept-loop designs before SO_REUSEPORT became popular.

   Caveats
     - Linux 4.5+.  epoll_ctl returns EINVAL on older kernels, and the
       program reports a skip.  It is only valid with EPOLL_CTL_ADD and
       a level-triggered event (no EPOLLET), and EPOLLEXCLUSIVE cannot
       be combined with EPOLLONESHOT.
     - It does not make accept() atomic: the woken process can still
       lose the connection to another thread, and closing the accepted
       fd immediately (as here) is fine only because the demo does not
       talk over it.
     - When no epoll_wait is currently blocked, the event stays on the
       ready list; exclusivity affects which waiters are woken, not
       whether the connection is queued.
     - It reduces wakeups; it does not reserve the connection.  If the
       woken instance never accepts, the connection stays in the accept
       queue and level-triggered readiness keeps reporting it there.
       The kernel does not hand the connection to a different epoll
       instance just because this one ignored it.
     - SO_REUSEPORT is a different solution: multiple sockets, one per
       worker, with the kernel hashing connections across them.
   ============================================== */
