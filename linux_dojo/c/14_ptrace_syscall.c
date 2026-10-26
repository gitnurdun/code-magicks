/* MAGIC: ptrace turns every system call of a child into a stop the parent can inspect. */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__x86_64__)
static void reap(pid_t pid)
{
    int status;
    if (kill(pid, SIGKILL) != 0 && errno != ESRCH)
        perror("kill");
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
}
#endif

int main(void)
{
#if !defined(__x86_64__)
    printf("skipped: x86-64 only\n");
    return 0;
#else
    int status;
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
            _exit(1);
        if (raise(SIGSTOP) != 0)
            _exit(1);
        (void)syscall(SYS_getpid);
        (void)syscall(SYS_getppid);
        _exit(0);
    }

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 1;
    }
    if (!WIFSTOPPED(status)) {
        printf("skipped: ptrace\n");
        return 0;
    }
    if (ptrace(PTRACE_SETOPTIONS, pid, NULL,
               (void *)(unsigned long)PTRACE_O_TRACESYSGOOD) < 0) {
        int e = errno;
        reap(pid);
        if (e == EPERM || e == EINVAL || e == ESRCH) {
            printf("skipped: ptrace\n");
            return 0;
        }
        errno = e;
        perror("ptrace(PTRACE_SETOPTIONS)");
        return 1;
    }

    int entering = 1, syscalls = 0;
    long first = -1, second = -1;

    for (;;) {
        if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            perror("ptrace(PTRACE_SYSCALL)");
            reap(pid);
            return 1;
        }
        while (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR)
                continue;
            perror("waitpid");
            reap(pid);
            return 1;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status))
            break;
        if (!WIFSTOPPED(status))
            continue;

        if (WSTOPSIG(status) != (SIGTRAP | 0x80))
            continue; /* ordinary signal or group stop, not a syscall stop */

        if (entering) {
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) {
                perror("ptrace(PTRACE_GETREGS)");
                reap(pid);
                return 1;
            }
            if (syscalls == 0)
                first = (long)regs.orig_rax;
            else if (syscalls == 1)
                second = (long)regs.orig_rax;
            syscalls++;
        }
        entering = !entering;
    }

    printf("syscalls>=2=%s first=%ld second=%ld\n",
           (syscalls >= 2 && first == 39 && second == 110) ? "yes" : "no",
           first, second);
    return 0;
#endif
}

/* ================= EXPLANATION =================
   What it does
     Forks a child that asks to be traced, stops itself, then calls
     getpid and getppid through raw syscall(). The parent stops the child
     at syscall entry and exit, counts entry stops, and reads orig_rax to
     capture the first two syscall numbers: 39 and 110 on x86-64.

   Step by step
     1. The child calls ptrace(PTRACE_TRACEME), which makes its parent
        the tracer and stops it on the next signal. raise(SIGSTOP)
        supplies that signal; the parent's first waitpid sees a stop.
     2. The parent sets PTRACE_O_TRACESYSGOOD. This changes the reported
        signal for syscall stops from SIGTRAP to SIGTRAP|0x80 (0x85), so
        they can be told apart from a real SIGTRAP.
     3. Each PTRACE_SYSCALL resumes the child until the next syscall
        entry or exit, then waitpid reports WIFSTOPPED.
     4. The code toggles entering/exit on every syscall stop. Only entry
        stops are counted, so each system call contributes one count.
     5. At an entry stop, PTRACE_GETREGS fills struct user_regs_struct
        and orig_rax holds the syscall number. At an exit stop orig_rax
        is unchanged but rax holds the return value. getpid is 39 and
        getppid is 110 in the x86-64 table.
     6. After _exit(0) the child reports WIFEXITED, the loop breaks, and
        the parent prints the two numbers it captured.

   Output
     syscalls>=2=yes first=39 second=110

   Why it matters
     This is the stop protocol used by strace, gdb, seccomp tracers, and
     sandbox supervisors. It can observe or rewrite syscall arguments and
     return values without recompiling the target.

   Caveats
     - This file is x86-64 only; syscall numbers and orig_rax are
       architecture-specific. Other architectures print one skip line.
     - Yama ptrace_scope, a seccomp policy, a different UID, or a
       non-dumpable child can block PTRACE_TRACEME/SETOPTIONS. That path
       prints skipped and exits 0.
     - Signal-delivery stops, group stops, and PTRACE_EVENT stops are not
       syscall stops. The toggle must ignore them, or entry/exit pairing
       breaks. Restarted syscalls can also produce extra entry stops.
     - Every stop is two context switches, so traced code is far slower.
     - A tracer cannot trace itself, and setuid/privileged transitions
       can drop the trace relationship.
   ============================================== */
