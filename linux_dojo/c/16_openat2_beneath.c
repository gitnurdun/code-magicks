/* MAGIC: openat2 resolves a path inside a directory and refuses to climb out. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__has_include)
# if __has_include(<linux/openat2.h>)
#  include <linux/openat2.h>
#  define HAVE_LINUX_OPENAT2_H 1
# endif
#endif
#ifndef HAVE_LINUX_OPENAT2_H
struct open_how {
    __u64 flags;
    __u64 mode;
    __u64 resolve;
};
#endif
#ifndef RESOLVE_BENEATH
#define RESOLVE_BENEATH 0x08
#endif
#ifndef SYS_openat2
#define SYS_openat2 437
#endif

int main(void)
{
    int dirfd = -1, fd_in = -1, outfd = -1, fd = -1, rc = 1;
    char dir[] = "/tmp/openat2XXXXXX";
    char inside[4096] = "";
    char escape[4096] = "";
    char outside[4096] = "";

    if (mkdtemp(dir) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    if (snprintf(inside, sizeof inside, "%s/inside.txt", dir) >=
            (int)sizeof inside ||
        snprintf(outside, sizeof outside, "%s.out", dir) >=
            (int)sizeof outside) {
        fprintf(stderr, "path too long\n");
        goto out;
    }

    fd_in = open(inside, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd_in < 0) {
        perror("open(inside.txt)");
        goto out;
    }
    if (write(fd_in, "x", 1) != 1) {
        perror("write(inside.txt)");
        goto out;
    }
    if (close(fd_in) != 0) {
        perror("close(inside.txt)");
        fd_in = -1;
        goto out;
    }
    fd_in = -1;

    outfd = open(outside, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (outfd < 0) {
        perror("open(outside)");
        goto out;
    }
    if (write(outfd, "x", 1) != 1) {
        perror("write(outside)");
        goto out;
    }
    if (close(outfd) != 0) {
        perror("close(outside)");
        outfd = -1;
        goto out;
    }
    outfd = -1;

    const char *base = strrchr(outside, '/');
    if (base == NULL ||
        snprintf(escape, sizeof escape, "../%s", base + 1) >=
            (int)sizeof escape) {
        fprintf(stderr, "path too long\n");
        goto out;
    }

    dirfd = open(dir, O_PATH | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0) {
        perror("open(dir)");
        goto out;
    }

    struct open_how how = {
        .flags = O_RDONLY | O_CLOEXEC,
        .mode = 0,
        .resolve = RESOLVE_BENEATH,
    };
    fd = (int)syscall(SYS_openat2, dirfd, "inside.txt", &how, sizeof how);
    if (fd < 0) {
        if (errno == ENOSYS || errno == E2BIG) {
            printf("skipped: openat2\n");
            rc = 0;
            goto out;
        }
        perror("openat2(inside.txt)");
        goto out;
    }

    struct open_how blocked = how;
    int bad = (int)syscall(SYS_openat2, dirfd, escape, &blocked,
                           sizeof blocked);
    int denied = (bad < 0 && errno == EXDEV);
    if (bad >= 0)
        close(bad);

    printf("inside=ok outside_denied=%s\n", denied ? "yes" : "no");
    rc = 0;

out:
    if (fd_in >= 0)
        close(fd_in);
    if (outfd >= 0)
        close(outfd);
    if (fd >= 0)
        close(fd);
    if (dirfd >= 0)
        close(dirfd);
    unlink(inside);
    unlink(outside);
    rmdir(dir);
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Creates a temporary directory with inside.txt and a second file in
     its parent. It opens the directory with O_PATH, then uses openat2
     with RESOLVE_BENEATH to open inside.txt successfully and to reject
     "../outside", which would leave the directory.

   Step by step
     1. mkdtemp makes /tmp/openat2XXXXXX. The outside file is the
        sibling "/tmp/openat2XXXXXX.out", created in the parent
        directory. Both exist before the constrained opens.
     2. open(dir, O_PATH|O_DIRECTORY|O_CLOEXEC) returns a descriptor
        used only as a resolution root. O_PATH does not grant read/write
        access to the directory itself.
     3. struct open_how is zero-filled and given flags, mode, and
        resolve=RESOLVE_BENEATH. openat2 takes its size as an explicit
        ABI version; syscall(SYS_openat2, ...) is used because glibc has
        no wrapper.
     4. The relative path "inside.txt" stays beneath dirfd, so it opens
        and the descriptor is closed during cleanup.
     5. "../openat2XXXXXX.out" has a component outside dirfd.
        RESOLVE_BENEATH detects the escape and fails with EXDEV, which is
        the boolean outside_denied=yes.
     6. Cleanup closes every descriptor and unlinks/rmdirs the exact
        paths, never a recursive shell command.

   Output
     inside=ok outside_denied=yes

   Why it matters
     Untrusted archives, container images, and setuid helpers need to
     open names inside a directory without letting ".." or an absolute
     symlink escape. openat2 moves that policy into one atomic lookup,
     avoiding the check-then-open race of manual path validation.

   Caveats
     - Linux 5.6+ only; old kernels return ENOSYS. A userspace header
       newer than the kernel can return E2BIG, so this file falls back to
       a local struct and skips on either error.
     - RESOLVE_BENEATH rejects any escape (EXDEV). RESOLVE_IN_ROOT
       instead treats dirfd as a temporary root: absolute symlinks are
       reinterpreted under it and ".." is clamped at that root, so it is
       a chroot-like policy rather than a strict "must stay below" one.
     - RESOLVE_BENEATH currently disables magic links, but that is not a
       permanent guarantee. Add RESOLVE_NO_MAGICLINKS (or
       RESOLVE_NO_SYMLINKS) if /proc/self/fd-style links must not escape.
     - It protects path resolution, not later use of an already-open fd.
       The directory tree can still be renamed or swapped after the open.
     - resolve=0 behaves like openat; the protection is opt-in. EAGAIN
       can be returned if the kernel cannot prove ".." is safe, and the
       caller may retry.
   ============================================== */
