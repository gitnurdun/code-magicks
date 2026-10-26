/* MAGIC: install a BPF program that inspects every syscall number and
   returns ALLOW or ERRNO before the kernel executes it. */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#include <stdio.h>
#if defined(__linux__)
#include <errno.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
int main(void)
{
    struct sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_getpid, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof filter / sizeof filter[0]),
        .filter = filter,
    };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        perror("PR_SET_NO_NEW_PRIVS");
        return 1;
    }
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        perror("PR_SET_SECCOMP");
        return 1;
    }
    errno = 0;
    long r = syscall(SYS_getpid);
    printf("getpid=%ld errno=%d\n", r, errno);
    return 0;
}
#else
int main(void)
{
    puts("seccomp is Linux-only; this is a compile-time stub");
    return 0;
}
#endif

/* ================= EXPLANATION =================
   What it does
     Installs a seccomp-BPF filter that makes the getpid syscall fail with
     EPERM while allowing every other syscall. The program then calls
     getpid and prints the error.

   Step by step
     1. The BPF program loads the syscall number from seccomp_data.nr.
        If it equals __NR_getpid, it falls through to the ERRNO return;
        otherwise it jumps to the final ALLOW return.
     2. SECCOMP_RET_ERRNO | EPERM tells the kernel to fail the syscall
        with errno EPERM, without running the real getpid implementation.
        SECCOMP_RET_ALLOW lets everything else through.
     3. PR_SET_NO_NEW_PRIVS prevents the process from gaining privileges
        through setuid binaries or other mechanisms. It is required before
        an unprivileged process may install a seccomp filter, and it makes
        the sandbox stick across execve.
     4. PR_SET_SECCOMP with SECCOMP_MODE_FILTER loads the program. From
        then on, every syscall from this thread (and its children) is
        filtered. The filter cannot be removed.
     5. syscall(SYS_getpid) now returns -1 and sets errno to EPERM. The
        program itself continues because exit and write are allowed.

   Output
     "getpid=-1 errno=1"

   Why it matters
     seccomp is the innermost layer of Linux sandboxing: Docker, Chrome,
     systemd, and OpenSSH use it to remove entire syscall classes from a
     process. It is fast (a BPF program per syscall), inherited across
     fork/exec, and irreversible, which makes it a strong containment
     primitive. It complements namespaces, cgroups, and capabilities.

   Caveats
     - This filter is a denylist of one syscall. Real sandboxes use an
       allowlist and validate the architecture (seccomp_data.arch) to
       prevent ABI confusion.
     - Filtering only the syscall number is not enough: arguments can be
       dangerous (pointer, flags, fd). BPF can inspect them, within limits.
     - The filter applies to the calling thread; create it before spawning
       threads or apply it to each one. It is inherited by children.
     - Some syscalls are implemented via others (e.g., glibc wrappers may
       use different numbers); test against the real libc.
     - SECCOMP_RET_KILL_PROCESS is safer for a hard boundary, but harder
       to debug than ERRNO.
     - Seccomp does not control file paths or network destinations; pair
       it with namespaces, Landlock, or an LSM.
     - This uses Linux-specific headers and will not compile on Windows.
   ============================================== */
