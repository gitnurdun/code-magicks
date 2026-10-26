/* MAGIC: getdents64 returns raw variable-length directory records, not
   a friendly array of names. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef SYS_getdents64
#define SYS_getdents64 217
#endif

struct linux_dirent64 {
    ino64_t d_ino;
    off64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

static int cmp_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int touch(const char *path)
{
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);

    if (fd < 0)
        return -1;
    return close(fd);
}

int main(void)
{
    char tmpl[] = "/tmp/getdents_XXXXXX";
    char alpha[128], beta[128], subdir[128];
    char *names[64];
    char buf[4096] __attribute__((aligned(8)));
    int dfd = -1, count = 0, i, rc = 0;

    if (mkdtemp(tmpl) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(alpha, sizeof alpha, "%s/alpha", tmpl);
    snprintf(beta, sizeof beta, "%s/beta", tmpl);
    snprintf(subdir, sizeof subdir, "%s/subdir", tmpl);
    if (touch(alpha) != 0 || touch(beta) != 0 ||
        mkdir(subdir, 0700) != 0) {
        perror("create entries");
        rc = 1;
        goto cleanup;
    }

    dfd = open(tmpl, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd < 0) {
        perror("open directory");
        rc = 1;
        goto cleanup;
    }

    for (;;) {
        long nread = syscall(SYS_getdents64, dfd, buf, sizeof buf);
        long pos = 0;

        if (nread < 0) {
            perror("getdents64");
            rc = 1;
            goto cleanup;
        }
        if (nread == 0)
            break;
        while (pos < nread) {
            struct linux_dirent64 *d =
                (struct linux_dirent64 *)(buf + pos);

            if (d->d_reclen < offsetof(struct linux_dirent64, d_name) + 1 ||
                pos + d->d_reclen > nread) {
                fprintf(stderr, "malformed getdents64 record\n");
                rc = 1;
                goto cleanup;
            }
            if (count < (int)(sizeof names / sizeof names[0])) {
                names[count] = strdup(d->d_name);
                if (names[count] == NULL) {
                    perror("strdup");
                    rc = 1;
                    goto cleanup;
                }
                count++;
            }
            pos += d->d_reclen;
        }
    }

    qsort(names, (size_t)count, sizeof names[0], cmp_name);
    printf("entries=%d names=", count);
    for (i = 0; i < count; i++)
        printf("%s%s", i ? "," : "", names[i]);
    putchar('\n');

cleanup:
    for (i = 0; i < count; i++)
        free(names[i]);
    if (dfd >= 0)
        close(dfd);
    unlink(alpha);
    unlink(beta);
    rmdir(subdir);
    rmdir(tmpl);
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Creates a temporary directory containing two regular files and one
     subdirectory, opens it, and reads it with the raw getdents64
     syscall.  It walks the variable-length records, copies the names,
     sorts them, and prints entries=5 names=.,..,alpha,beta,subdir.

   Step by step
     1. mkdtemp() creates a unique 0700 directory.  alpha and beta are
        created with O_CREAT|O_EXCL, subdir with mkdir(0700).
     2. open(dir, O_RDONLY|O_DIRECTORY) gives a directory fd.  read(2)
        on this fd would fail EISDIR; the only reader is getdents64.
     3. getdents64(fd, buf, 4096) fills the buffer with whole records
        and returns the byte count, or 0 at end of directory.  Records
        never straddle a call, so each returned buffer can be walked.
     4. Each record is struct linux_dirent64: 8-byte inode, 8-byte
        d_off, 2-byte d_reclen, 1-byte d_type, then a NUL-terminated
        name, padded to an 8-byte boundary.  The next record starts
        exactly d_reclen bytes later.
     5. The code validates d_reclen before advancing, copies d_name with
        strdup, and stops on a malformed record.  It keeps at most 64
        names; the demo has five.
     6. qsort with strcmp sorts bytewise: ".", "..", "alpha", "beta",
        "subdir".  Directory order from the kernel was arbitrary.
     7. Descriptors are closed and every created entry is removed
        (unlink files, rmdir subdir, rmdir the temp directory).

   Output
     entries=5 names=.,..,alpha,beta,subdir

   Why it matters
     ls, find, rsync and every package manager ultimately consume this
     format.  Knowing it explains why readdir() carries an opaque
     offset, why a directory can change under you between two reads, and
     why d_type is only a hint.  It is also the foundation of
     getdents64-based scanners that avoid one stat() per entry.

   Caveats
     - The names are sorted by strcmp here, not by the filesystem.  ext4
       with dir_index returns hash order; tmpfs returns insertion-ish
       order.  Never depend on readdir order.
     - d_off is a filesystem cookie, not a byte offset.  It is passed
       back via lseek to resume a scan, but its value is meaningless to
       userspace and can change between calls on some filesystems.
     - d_type can be DT_UNKNOWN (for example on some network or
       older/odd filesystems).  It is not a substitute for fstatat();
       DT_REG and DT_DIR are hints only, and a file can change type
       between the read and the stat.
     - getdents64 is not standard C.  glibc's readdir() wraps it and
       adds buffering; using the syscall directly means handling short
       reads, EINTR, and buffer sizing yourself.
     - The inode numbers are filesystem-specific; hard links share one,
       and some filesystems synthesize them.  Do not use d_ino as a
       global identity.
     - A directory modified during the scan may yield entries twice or
       miss entries; there is no snapshot without filesystem support.
     - "." and ".." are normally present but are not guaranteed by the
       VFS contract; the printed count can differ on unusual mounts.
   ============================================== */
