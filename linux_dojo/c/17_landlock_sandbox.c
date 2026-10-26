/* MAGIC: Landlock confines this process to one directory with an
   unprivileged, stacked LSM ruleset. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__has_include)
# if __has_include(<linux/landlock.h>)
#  include <linux/landlock.h>
#  define HAVE_LANDLOCK_H 1
# endif
#endif

#ifndef HAVE_LANDLOCK_H
struct landlock_ruleset_attr {
    __u64 handled_access_fs;
};
struct landlock_path_beneath_attr {
    __u64 allowed_access;
    __s32 parent_fd;
} __attribute__((packed));
#endif

#ifndef LANDLOCK_CREATE_RULESET_VERSION
#define LANDLOCK_CREATE_RULESET_VERSION (1U << 0)
#endif
#ifndef LANDLOCK_RULE_PATH_BENEATH
#define LANDLOCK_RULE_PATH_BENEATH 1
#endif
#ifndef LANDLOCK_ACCESS_FS_WRITE_FILE
#define LANDLOCK_ACCESS_FS_WRITE_FILE (1ULL << 1)
#endif
#ifndef LANDLOCK_ACCESS_FS_REMOVE_DIR
#define LANDLOCK_ACCESS_FS_REMOVE_DIR (1ULL << 4)
#endif
#ifndef LANDLOCK_ACCESS_FS_REMOVE_FILE
#define LANDLOCK_ACCESS_FS_REMOVE_FILE (1ULL << 5)
#endif
#ifndef LANDLOCK_ACCESS_FS_MAKE_DIR
#define LANDLOCK_ACCESS_FS_MAKE_DIR (1ULL << 7)
#endif
#ifndef LANDLOCK_ACCESS_FS_MAKE_REG
#define LANDLOCK_ACCESS_FS_MAKE_REG (1ULL << 8)
#endif
#ifndef LANDLOCK_ACCESS_FS_MAKE_SYM
#define LANDLOCK_ACCESS_FS_MAKE_SYM (1ULL << 12)
#endif
#ifndef __NR_landlock_create_ruleset
#define __NR_landlock_create_ruleset 444
#endif
#ifndef __NR_landlock_add_rule
#define __NR_landlock_add_rule 445
#endif
#ifndef __NR_landlock_restrict_self
#define __NR_landlock_restrict_self 446
#endif

#define FS_ACCESS (LANDLOCK_ACCESS_FS_WRITE_FILE | \
                   LANDLOCK_ACCESS_FS_REMOVE_FILE | \
                   LANDLOCK_ACCESS_FS_REMOVE_DIR | \
                   LANDLOCK_ACCESS_FS_MAKE_REG | \
                   LANDLOCK_ACCESS_FS_MAKE_DIR | \
                   LANDLOCK_ACCESS_FS_MAKE_SYM)

int main(void)
{
    char tmpl[] = "/tmp/landlock_XXXXXX";
    char inside[128], outside[128];
    struct landlock_ruleset_attr attr = { .handled_access_fs = FS_ACCESS };
    struct landlock_path_beneath_attr rule;
    long abi;
    int rfd = -1, dfd = -1, status;
    pid_t pid;

    abi = syscall(__NR_landlock_create_ruleset, NULL, 0,
                  LANDLOCK_CREATE_RULESET_VERSION);
    if (abi < 0) {
        if (errno == ENOSYS || errno == EOPNOTSUPP) {
            printf("skipped: landlock not supported\n");
            return 0;
        }
        perror("landlock_create_ruleset(version)");
        return 1;
    }
    if (abi < 1) {
        printf("skipped: landlock abi %ld\n", abi);
        return 0;
    }

    if (mkdtemp(tmpl) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    dfd = open(tmpl, O_PATH | O_CLOEXEC);
    if (dfd < 0) {
        perror("open");
        rmdir(tmpl);
        return 1;
    }
    rfd = (int)syscall(__NR_landlock_create_ruleset, &attr, sizeof attr, 0);
    if (rfd < 0) {
        if (errno == EOPNOTSUPP || errno == ENOSYS) {
            printf("skipped: landlock disabled\n");
            close(dfd);
            rmdir(tmpl);
            return 0;
        }
        perror("landlock_create_ruleset");
        close(dfd);
        rmdir(tmpl);
        return 1;
    }
    rule.allowed_access = FS_ACCESS;
    rule.parent_fd = dfd;
    if (syscall(__NR_landlock_add_rule, rfd, LANDLOCK_RULE_PATH_BENEATH,
                &rule, 0) != 0) {
        perror("landlock_add_rule");
        close(rfd);
        close(dfd);
        rmdir(tmpl);
        return 1;
    }

    snprintf(inside, sizeof inside, "%s/inside", tmpl);
    snprintf(outside, sizeof outside, "/tmp/landlock_outside_%ld",
             (long)getpid());

    /* The parent stays unrestricted so it can remove the temporary tree. */
    pid = fork();
    if (pid < 0) {
        perror("fork");
        close(rfd);
        close(dfd);
        rmdir(tmpl);
        return 1;
    }
    if (pid == 0) {
        int fd, ok_in, denied_out = 0;

        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
            printf("skipped: prctl(PR_SET_NO_NEW_PRIVS): %s\n",
                   strerror(errno));
            fflush(stdout);
            _exit(0);
        }
        if (syscall(__NR_landlock_restrict_self, rfd, 0) != 0) {
            printf("skipped: landlock_restrict_self: %s\n", strerror(errno));
            fflush(stdout);
            _exit(0);
        }
        close(rfd);
        close(dfd);

        fd = open(inside, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        ok_in = fd >= 0;
        if (fd >= 0)
            close(fd);

        fd = open(outside, O_CREAT | O_WRONLY, 0600);
        if (fd < 0 && errno == EACCES)
            denied_out = 1;
        else if (fd >= 0) {
            close(fd);
            unlink(outside);
        }
        printf("inside=%s outside_denied=%s\n",
               ok_in ? "ok" : "failed", denied_out ? "yes" : "no");
        fflush(stdout);
        _exit(0);
    }

    if (waitpid(pid, &status, 0) < 0)
        perror("waitpid");
    unlink(inside);
    unlink(outside);
    close(rfd);
    close(dfd);
    if (rmdir(tmpl) != 0)
        perror("rmdir");
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Builds a Landlock ruleset that handles six filesystem access types,
     allows them only below a fresh mkdtemp directory, and enforces the
     ruleset with landlock_restrict_self().  A forked child then creates
     a file inside the directory (allowed) and tries to create one in
     /tmp (denied with EACCES).  It prints "inside=ok outside_denied=yes".

   Step by step
     1. landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION)
        is the ABI probe.  It returns 1 for the first Landlock ABI
        (Linux 5.13) and higher on newer kernels; it does not create
        anything.
     2. The ruleset attribute names the access classes to mediate.  For
        ABI v1 these are WRITE_FILE, REMOVE_FILE, REMOVE_DIR, MAKE_REG,
        MAKE_DIR and MAKE_SYM.  Handling an access means: deny it
        everywhere, except where a rule allows it.
     3. A LANDLOCK_RULE_PATH_BENEATH rule grants those accesses under one
        directory, passed as an O_PATH descriptor in
        landlock_path_beneath_attr.parent_fd (a packed struct: 8-byte
        access mask then 32-bit fd).
     4. PR_SET_NO_NEW_PRIVS is mandatory: Landlock will not restrict a
        process that could still gain privileges through setuid/exec.
     5. landlock_restrict_self() stacks the ruleset onto the calling
        thread's credential.  It is irreversible and unprivileged.
     6. After the child applies the restriction, open() inside the
        allowed tree checks MAKE_REG/WRITE_FILE and succeeds; open() in
        /tmp checks MAKE_REG on an unallowed parent and gets EACCES.
     7. The unrestricted parent waits, then unlinks the file and removes
        the temporary directory.  Landlock would have denied rmdir of
        the directory itself because REMOVE_DIR on /tmp is handled but
        not allowed.

   Output
     inside=ok outside_denied=yes

   Why it matters
     Landlock lets ordinary processes build useful filesystem sandboxes
     without root, seccomp filters, or user namespaces.  The ruleset is
     inherited across fork() and execve(), so a launcher can confine a
     whole service tree; systemd and container runtimes can stack it
     under their own policy.  Because it is an LSM, it composes with
     seccomp (which filters syscalls, not paths).

   Caveats
     - Needs CONFIG_SECURITY_LANDLOCK and Landlock enabled on the kernel
       command line (lsm=...,landlock); otherwise the probe returns
       ENOSYS or EOPNOTSUPP and the program prints "skipped".
     - ABI v1 cannot mediate rename/link between hierarchies (REFER,
       ABI v2), truncate (ABI v3) or ioctl on devices (ABI v5).  Asking
       for such bits on an older kernel fails with EINVAL.
     - The restriction is per-thread: landlock_restrict_self() confines
       only the caller.  Other threads of the same process keep their
       old credential unless they call it too; children created after
       the call inherit the domain.
     - It does not revoke descriptors already opened: an fd opened for
       writing before the sandbox can still be written.
     - There is no "unrestrict".  The parent here is deliberately left
       unsandboxed to do cleanup, which also demonstrates the boundary.
     - mkdtemp uses 0700, but Landlock still denies by policy, not by
       file permissions; running as root does not bypass it.
   ============================================== */
