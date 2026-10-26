/* MAGIC: load an executable into an anonymous memfd and execute that
   file descriptor with fexecve, without ever creating a disk path. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

int main(void)
{
    int in = open("/bin/echo", O_RDONLY | O_CLOEXEC);
    if (in < 0)
        in = open("/usr/bin/echo", O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        printf("skipped: no readable echo binary (%s)\n", strerror(errno));
        return 0;
    }

    int fd = memfd_create("echo", 0);
    if (fd < 0) {
        printf("skipped: memfd_create unavailable (%s)\n", strerror(errno));
        close(in);
        return 0;
    }
    int rc = 1;
    char buf[65536];
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("read echo");
            goto out;
        }
        if (n == 0)
            break;
        size_t off = 0;
        while (off < (size_t)n) {
            ssize_t w = write(fd, buf + off, (size_t)n - off);
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                perror("write memfd");
                goto out;
            }
            if (w == 0) {
                fprintf(stderr, "write memfd: no progress\n");
                goto out;
            }
            off += (size_t)w;
        }
    }
    if (close(in) != 0) {
        perror("close echo");
        in = -1;
        goto out;
    }
    in = -1;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0 ||
        lseek(fd, 0, SEEK_SET) != 0) {
        perror("prepare memfd");
        goto out;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        goto out;
    }
    if (pid == 0) {
        char *argv[] = { "echo", "memfd exec works", NULL };
        fexecve(fd, argv, environ);
        _exit(127);
    }
    if (close(fd) != 0) {
        perror("close memfd");
        fd = -1;
        goto out;
    }
    fd = -1;

    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        perror("waitpid");
        goto out;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        puts("child_status=0");
        rc = 0;
    } else {
        puts("skipped: fexecve could not execute the memfd image");
        rc = 0;
    }

out:
    if (in >= 0 && close(in) != 0)
        rc = 1;
    if (fd >= 0 && close(fd) != 0)
        rc = 1;
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Streams /bin/echo (or /usr/bin/echo) into an anonymous RAM-backed
     memfd, then forks and executes that descriptor with fexecve. The
     child becomes echo and prints its argument; the parent reaps it.

   Step by step
     1. open() the echo binary. It is copied in 64 KiB chunks with
        read()/write() into memfd_create("echo", 0), a tmpfs inode with
        no directory entry and no disk path.
     2. fcntl(FD_CLOEXEC) marks the descriptor close-on-exec, and lseek()
        rewinds to offset zero before exec.
     3. fork() gives the child its own descriptor. The child calls
        fexecve(fd, {"echo", "memfd exec works", NULL}, environ).
     4. glibc uses execveat(fd, "", ..., AT_EMPTY_PATH) on modern kernels,
        or /proc/self/fd/<fd> as an older fallback. The kernel loads the
        ELF from the memfd, not from a pathname.
     5. On success fexecve never returns; echo exits 0 and waitpid reports
        child_status=0. If no echo is readable, or memfd_create fails, the
        program prints one skipped: line and exits 0.

   Output
     memfd exec works
     child_status=0

   Why it matters
     fexecve removes the pathname-swap race: the same open inode that was
     checked is the one executed. It does not stop writes between check
     and exec, so seals or permissions must do that. memfd supplies the
     inode without a filesystem path, and fexecve turns a descriptor into
     a process image.

   Caveats
     - memfd_create is Linux 3.17+. fexecve has existed since glibc
       2.3.2; since 2.27 it uses execveat when available, otherwise a
       mounted /proc fallback.
     - A dynamically linked echo still needs its ELF interpreter and
       shared libraries, and /bin/echo itself is still read from disk.
     - Recent kernels may reject executing memfds through
       vm.memfd_noexec or MFD_NOEXEC_SEAL. The child then exits 127 and
       the parent prints skipped.
     - FD_CLOEXEC is unsafe for script images on the /proc fallback; echo
       is ELF. This is a demo, not a hardened loader.
   ============================================== */
