/* MAGIC: process_vm_readv copies another process's memory directly, with no pipe and no shared map. */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

char secret[] = "child secret";

static void stop_child(pid_t pid)
{
    if (kill(pid, SIGKILL) != 0 && errno != ESRCH)
        perror("kill");
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
        ;
}

int main(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        if (printf("child_ready=yes\n") < 0 || fflush(stdout) != 0)
            _exit(1);
        for (;;)
            pause();
    }

    struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };
    while (nanosleep(&ts, &ts) < 0) {
        if (errno != EINTR) {
            perror("nanosleep");
            stop_child(pid);
            return 1;
        }
    }

    size_t len = strlen(secret);
    char local[sizeof secret];
    struct iovec local_iov = { .iov_base = local, .iov_len = len };
    struct iovec remote_iov = { .iov_base = secret, .iov_len = len };

    ssize_t n = process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);
    if (n < 0) {
        int e = errno;
        stop_child(pid);
        if (e == EPERM) {
            printf("skipped: process_vm_readv EPERM\n");
            return 0;
        }
        if (e == ENOSYS) {
            printf("skipped: process_vm_readv unsupported\n");
            return 0;
        }
        errno = e;
        perror("process_vm_readv");
        return 1;
    }

    if ((size_t)n < sizeof local)
        local[n] = '\0';
    else
        local[sizeof local - 1] = '\0';

    printf("bytes=%zd data=%s\n", n, local);
    stop_child(pid);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Places a 12-byte string in a global array, forks, lets the child
     announce readiness and sleep, then reads the child's copy of that
     array with one process_vm_readv call. It prints the byte count and
     the copied text, then kills and reaps the child.

   Step by step
     1. secret[] lives in the parent's data segment. fork() duplicates
        the whole address space, so the child has the same bytes at the
        same virtual address. No address is printed or guessed.
     2. The child prints child_ready=yes and fflush(stdout), then pauses.
        The parent sleeps 100 ms so the child is definitely resident and
        waiting before the read.
     3. process_vm_readv(child, local_iov, 1, remote_iov, 1, 0) copies
        from the child's iovec to the parent's iovec. flags must be 0;
        the return value is the number of bytes read and may be partial.
     4. The parent reads exactly strlen("child secret") == 12 bytes, not
        the terminating NUL, then adds its own NUL to local[].
     5. stop_child kills with SIGKILL and reaps with waitpid, so no
        zombie or running child is left behind.

   Output
     bytes=12 data=child secret

   Why it matters
     This is a one-copy cross-process memory primitive used by
     debuggers, profilers, checkpoint/restore, and fast message passing.
     It avoids inventing a shared mapping or serialising through a pipe.

   Caveats
     - Permission is a ptrace access-mode check
       (PTRACE_MODE_ATTACH_REALCREDS): same real/effective/saved UID and
       GID, target dumpable, and any LSM (Yama, SELinux, AppArmor) must
       allow it. A non-dumpable or setuid child, a different UID, or
       Yama ptrace_scope 2/3 produces EPERM.
     - The remote address is valid only because fork copied the address
       space. A pointer from a different process is meaningless; for a
       non-forked target it must be discovered another way.
     - Needs Linux 3.2+ and CONFIG_CROSS_MEMORY_ATTACH; older kernels
       return ENOSYS. Readv/writev checks are per page, so a range that
       crosses an unmapped page can return a partial count.
     - Results are not atomic. The target can change the memory while
       the copy runs, and the target can exit between fork and read.
     - The copy still costs one copy into the caller's address space; it
       is not zero-copy despite avoiding kernel bounce buffers.
   ============================================== */
