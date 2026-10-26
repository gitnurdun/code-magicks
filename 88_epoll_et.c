/* MAGIC: edge-triggered epoll reports a readiness transition once; the
   handler must drain the fd until EAGAIN or it will never be told again. */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#include <stdio.h>
#if defined(__linux__)
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
int main(void)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { perror("socketpair"); return 1; }
    for (int i = 0; i < 2; i++) {
        int fl = fcntl(sv[i], F_GETFL);
        fcntl(sv[i], F_SETFL, fl | O_NONBLOCK);
    }
    int ep = epoll_create1(0);
    if (ep < 0) { perror("epoll_create1"); return 1; }
    struct epoll_event ev;
    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sv[0];
    if (epoll_ctl(ep, EPOLL_CTL_ADD, sv[0], &ev) != 0) { perror("epoll_ctl"); return 1; }
    const char *msg = "abcdef";
    write(sv[1], msg, strlen(msg));
    struct epoll_event out;
    int ready = epoll_wait(ep, &out, 1, 1000);
    int total = 0;
    char buf[64];
    for (;;) {
        ssize_t n = read(sv[0], buf, sizeof buf);
        if (n > 0) total += (int)n;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        else break;
    }
    printf("events=%d bytes=%d\n", ready, total);
    close(ep);
    close(sv[0]);
    close(sv[1]);
    return 0;
}
#else
int main(void)
{
    puts("epoll is Linux-only; this is a compile-time stub");
    return 0;
}
#endif

/* ================= EXPLANATION =================
   What it does
     Registers one end of a nonblocking socketpair with epoll in
     edge-triggered mode, writes six bytes to the other end, waits for one
     event, and drains the readable end until EAGAIN.

   Step by step
     1. socketpair creates two connected Unix-domain sockets. Both are set
        O_NONBLOCK, which is mandatory for edge-triggered draining: a
        blocking read would hang after the last byte.
     2. epoll_create1 creates an epoll instance. EPOLL_CTL_ADD registers
        sv[0] with EPOLLIN | EPOLLET. Level-triggered mode would keep
        reporting while data remains; edge-triggered mode reports the
        transition to readable.
     3. Writing "abcdef" makes sv[0] readable and wakes epoll_wait once.
     4. The read loop consumes all six bytes. The next read returns -1
        with EAGAIN, proving the buffer is empty. Only then does the code
        break. If it read just one byte and waited for another event, ET
        might never fire again for the remaining data.
     5. ready is 1 and total is 6. The epoll fd and both sockets are
        closed.

   Output
     "events=1 bytes=6"

   Why it matters
     epoll scales to many file descriptors because the kernel keeps the
     interest list and returns only ready entries; select/poll rescan the
     whole set. Edge-triggered mode reduces wakeups further and is the
     basis of high-performance event loops (nginx, libuv, Redis). It is
     Linux-specific; kqueue and IOCP provide similar readiness/completion
     models elsewhere.

   Caveats
     - With EPOLLET you must drain each fd until EAGAIN, and the fd must
       be nonblocking. Forgetting this stalls the connection permanently.
     - A file descriptor is removed from epoll only after all references
       to the open file description are closed. Duplicating an fd and
       closing one copy does not remove the registration.
     - EPOLLONESHOT is useful for multithreaded loops: it disables the
       registration after one event and requires EPOLL_CTL_MOD to rearm.
     - epoll is not a portable API. Windows IOCP is completion-based, not
       readiness-based, so the event-loop abstraction differs.
     - This demo uses a socketpair; regular files and some devices do not
       support epoll readiness the way sockets do.
     - Edge-triggered code is notoriously race-prone; test with partial
       reads, spurious wakeups, and EINTR.
   ============================================== */
