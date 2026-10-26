/* MAGIC: PR_SET_CHILD_SUBREAPER makes orphaned grandchildren reparent
   to this process instead of PID 1, so waitpid(-1) reaps them. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PR_SET_CHILD_SUBREAPER
#define PR_SET_CHILD_SUBREAPER 36
#endif

int main(void)
{
    pid_t child;
    int reaped = 0;

    if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0) {
        if (errno == EINVAL) {
            printf("skipped: PR_SET_CHILD_SUBREAPER needs Linux 3.4\n");
            return 0;
        }
        perror("prctl(PR_SET_CHILD_SUBREAPER)");
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        pid_t grandchild = fork();

        if (grandchild < 0)
            _exit(1);
        if (grandchild == 0) {
            usleep(100000);
            _exit(0);
        }
        /* The child exits at once, orphaning the sleeping grandchild. */
        _exit(0);
    }

    for (;;) {
        int status;
        pid_t p = waitpid(-1, &status, 0);

        if (p > 0) {
            reaped++;
            continue;
        }
        if (p < 0 && errno == EINTR)
            continue;
        if (p < 0 && errno == ECHILD)
            break;
        perror("waitpid");
        return 1;
    }
    printf("reaped=%d\n", reaped);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Marks itself as a child subreaper, forks a child, and lets that
     child fork a grandchild which sleeps for 100 ms.  The child exits
     immediately, so the grandchild is orphaned.  Instead of being
     reparented to PID 1, it is reparented to the subreaper.  The
     original process then calls waitpid(-1) until ECHILD and counts two
     successful reaps: its own child and the adopted grandchild.

   Step by step
     1. prctl(PR_SET_CHILD_SUBREAPER, 1) sets a per-process flag in the
        kernel.  The option is Linux 3.4+; on older kernels it fails
        with EINVAL and this program prints a skip.
     2. fork() creates the child.  The child immediately forks the
        grandchild, which sleeps 100 ms and exits, then the child calls
        _exit(0) without waiting.
     3. When a process dies, the kernel walks up its ancestor chain to
        find the nearest living ancestor marked as a subreaper.  If none
        exists, orphans go to PID 1.  Here the grandparent is marked, so
        the grandchild becomes its child.
     4. The grandparent blocks in waitpid(-1).  It first reaps the
        immediate child, then, roughly 100 ms later, the adopted
        grandchild.  It keeps calling until waitpid returns -1/ECHILD,
        which means there are no children left.
     5. reaped is 2; the program prints it.

   Output
     reaped=2

   Why it matters
     PID 1 is the universal reaper, but no ordinary process wants to
     depend on PID 1 seeing its daemons.  A supervisor that sets
     PR_SET_CHILD_SUBREAPER can track every orphan in its subtree,
     reap it, and avoid zombies without becoming PID 1, without a PID
     namespace, and without root.  systemd sets this flag (PID 1 has the
     behavior inherently), and container runtimes and session managers
     follow the same pattern so double-forked daemons are still
     collected and attributed to the right service cgroup.

   Caveats
     - It only affects orphaned *descendants*.  It does not let a
       process wait for unrelated processes, and it does not change
       signal delivery for children that still have a living parent.
     - The flag must be set before the orphan is created; setting it
       after the child dies is too late.
     - If a closer subreaper exists, that ancestor adopts the orphan,
       not this one.  Nested supervisors therefore form a chain.
     - waitpid(-1) returns ECHILD only when the caller has no children
       and no adoptable orphans left; if the program has other children,
       it must not assume a fixed reaped count.  This demo forks nothing
       else.
     - The adopted child still belongs to its original process group
       and session; subreaping changes the parent link, not the job
       control or cgroup membership.
     - The 100 ms sleep is a race-avoidance device for the demo.  A real
       event loop would not depend on it; it would use SIGCHLD or a
       pidfd.
   ============================================== */
