/* MAGIC: turn a blocked signal into a readable file descriptor and consume
   it with read(), so no signal handler ever runs. */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>

int main(void)
{
    sigset_t mask, old;
    if (sigemptyset(&mask) != 0 ||
        sigaddset(&mask, SIGUSR1) != 0 ||
        sigaddset(&mask, SIGTERM) != 0) {
        perror("sigset");
        return 1;
    }
    if (sigprocmask(SIG_BLOCK, &mask, &old) != 0) {
        perror("sigprocmask block");
        return 1;
    }

    int sfd = -1;
    int rc = 1;
    sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd < 0) {
        if (errno == ENOSYS || errno == EINVAL) {
            printf("skipped: signalfd unavailable (%s)\n", strerror(errno));
            rc = 0;
        } else {
            perror("signalfd");
        }
        goto out;
    }
    if (kill(getpid(), SIGUSR1) != 0) {
        perror("kill");
        goto out;
    }

    struct pollfd pfd = { .fd = sfd, .events = POLLIN };
    int ready;
    do {
        ready = poll(&pfd, 1, 5000);
    } while (ready < 0 && errno == EINTR);
    if (ready != 1 || !(pfd.revents & POLLIN)) {
        if (ready < 0)
            perror("poll");
        else
            fprintf(stderr, "poll: signal did not arrive\n");
        goto out;
    }

    struct signalfd_siginfo si;
    ssize_t n = read(sfd, &si, sizeof si);
    if (n != (ssize_t)sizeof si) {
        if (n < 0)
            perror("read signalfd");
        else
            fprintf(stderr, "read signalfd: short read\n");
        goto out;
    }
    int match = ((pid_t)si.ssi_pid == getpid());
    printf("signo=%u pid=match=%s\n", si.ssi_signo,
           match ? "yes" : "no");
    rc = (si.ssi_signo == SIGUSR1 && match) ? 0 : 1;

out:
    if (sfd >= 0 && close(sfd) != 0)
        rc = 1;
    if (sigprocmask(SIG_SETMASK, &old, NULL) != 0) {
        perror("sigprocmask restore");
        rc = 1;
    }
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Blocks SIGUSR1 and SIGTERM, creates a signalfd for that mask, sends
     SIGUSR1 to itself, waits for the descriptor to become readable, and
     reads one struct signalfd_siginfo. It prints the signal number and
     whether the queued sender PID matches this process, without printing
     the PID itself. Then it closes the fd and restores the old mask.

   Step by step
     1. sigemptyset/sigaddset build a mask containing SIGUSR1 and
        SIGTERM. sigprocmask(SIG_BLOCK, ...) blocks both and saves the
        previous mask in old.
     2. signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC) creates a
        descriptor fed by signals in that mask. -1 means "use the
        caller's signal mask" rather than an existing fd.
     3. kill(getpid(), SIGUSR1) queues the signal. Because it is blocked,
        no handler or default action runs; it remains pending for the
        signalfd.
     4. poll() waits for POLLIN. The signalfd becomes readable when a
        matching signal is pending.
     5. read() copies one fixed-size signalfd_siginfo and removes that
        signal from the queue. ssi_signo is the signal number; ssi_pid is
        the sending process. The code compares ssi_pid with getpid() and
        prints only yes/no.
     6. SIGUSR1 is 10 on x86 and x86-64. The program prints the actual
        ssi_signo, so it stays correct on architectures that number it
        differently. The old signal mask is restored at exit.

   Output
     signo=10 pid=match=yes

   Why it matters
     signalfd turns asynchronous signal delivery into the same
     poll/read loop used for sockets and pipes. That avoids fragile
     async-signal-safe code and lets an event loop handle signals and I/O
     together. It is Linux-specific; portability layers often use a
     self-pipe instead. Blocking the signal is what makes the fd the
     signal's delivery path.

   Caveats
     - signalfd is Linux-specific (2.6.22+) and needs _GNU_SOURCE and
       <sys/signalfd.h>.
     - The signal must be blocked in every thread that could receive it;
       otherwise the kernel may deliver it normally. This single-threaded
       demo uses sigprocmask, not pthread_sigmask.
     - Standard signals coalesce: two pending SIGUSR1s may produce one
       read. Real-time signals queue multiple instances (up to
       RLIMIT_SIGPENDING), and signalfd returns them one by one.
     - SIGTERM is blocked here only to show a multi-signal mask; nothing
       sends it. Restoring the mask lets normal SIGTERM handling resume.
     - read() consumes one signalfd_siginfo per signal. A short buffer
       fails with EINVAL; this code checks for exactly sizeof the struct.
     - The pid field is the sender's PID (or 0 for kernel-generated
       signals), not an authentication token. Compare it, do not trust
       it.
   ============================================== */
