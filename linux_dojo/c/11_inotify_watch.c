/* MAGIC: inotify turns filesystem changes into a queue of typed records read from one fd. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <time.h>
#include <unistd.h>

int main(void)
{
    int fd = -1, wd = -1, rc = 1;
    char dir[] = "/tmp/inotifyXXXXXX";
    char a[PATH_MAX] = "";
    char b[PATH_MAX] = "";

    fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOSYS || errno == EINVAL) {
            printf("skipped: inotify\n");
            return 0;
        }
        perror("inotify_init1");
        return 1;
    }
    if (mkdtemp(dir) == NULL) {
        perror("mkdtemp");
        close(fd);
        return 1;
    }
    snprintf(a, sizeof a, "%s/a.txt", dir);
    snprintf(b, sizeof b, "%s/b.txt", dir);

    wd = inotify_add_watch(fd, dir, IN_CREATE | IN_MODIFY | IN_MOVED_FROM |
                                        IN_MOVED_TO | IN_DELETE);
    if (wd < 0) {
        perror("inotify_add_watch");
        goto out;
    }

    int f = open(a, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (f < 0) {
        perror("open(a.txt)");
        goto out;
    }
    if (write(f, "x", 1) != 1) {
        perror("write(a.txt)");
        close(f);
        goto out;
    }
    if (close(f) != 0) {
        perror("close(a.txt)");
        goto out;
    }
    if (rename(a, b) != 0) {
        perror("rename(a.txt,b.txt)");
        goto out;
    }
    if (unlink(b) != 0) {
        perror("unlink(b.txt)");
        goto out;
    }

    struct timespec ts = { .tv_sec = 0, .tv_nsec = 50 * 1000 * 1000 };
    while (nanosleep(&ts, &ts) < 0) {
        if (errno != EINTR) {
            perror("nanosleep");
            goto out;
        }
    }

    int events = 0, got_create = 0, got_moved = 0, got_delete = 0;
    char last[NAME_MAX + 1] = "";
    char buf[4096];

    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            perror("read(inotify)");
            goto out;
        }
        if (n == 0)
            break;

        for (ssize_t off = 0;
             off + (ssize_t)sizeof(struct inotify_event) <= n; ) {
            struct inotify_event *ev = (struct inotify_event *)(buf + off);
            size_t len = sizeof(struct inotify_event) + (size_t)ev->len;
            if (off + (ssize_t)len > n)
                break; /* truncated record: should not happen with a 4K read */

            events++;
            if (ev->mask & IN_CREATE)
                got_create = 1;
            if (ev->mask & (IN_MOVED_FROM | IN_MOVED_TO))
                got_moved = 1;
            if (ev->mask & IN_DELETE)
                got_delete = 1;
            if (ev->len > 0)
                snprintf(last, sizeof last, "%s", ev->name);

            off += (ssize_t)len;
        }
    }

    printf("events>=4=%s create=%s moved=%s delete=%s final=%s\n",
           events >= 4 ? "yes" : "no", got_create ? "yes" : "no",
           got_moved ? "yes" : "no", got_delete ? "yes" : "no", last);
    rc = 0;

out:
    if (fd >= 0)
        close(fd);
    unlink(a);
    unlink(b);
    rmdir(dir);
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Creates a private temporary directory, watches it with inotify,
     then creates a.txt, writes one byte, renames it to b.txt, and
     deletes it. It drains the event queue and checks that create, move,
     and delete records all appeared, naming the last event.

   Step by step
     1. inotify_init1(IN_NONBLOCK|IN_CLOEXEC) returns one fd. The event
        queue belongs to that inotify instance, not to each watch.
     2. inotify_add_watch returns a watch descriptor: a small integer
        scoped to the instance, not a file descriptor. It records the
        directory path and selected mask.
     3. The operations run in order. Each read() returns one or more
        records, each laid out as struct inotify_event followed by a
        NUL-terminated name padded to the next alignment boundary. The
        correct stride is sizeof(struct inotify_event) + ev->len.
     4. rename(a,b) produces IN_MOVED_FROM for a.txt and IN_MOVED_TO for
        b.txt, sharing a cookie that pairs the two halves. unlink(b)
        produces IN_DELETE, so the last queued name is b.txt.
     5. After a 50 ms pause the queue is drained until EAGAIN. Cleanup
        uses unlink/rmdir on the exact paths, never a recursive shell
        command.

   Output
     events>=4=yes create=yes moved=yes delete=yes final=b.txt

   Why it matters
     This is the kernel-side notification API behind hot reload,
     file sync, build watchers, and security monitors. It avoids polling
     and gives the name, mask, and cookie in one record stream.

   Caveats
     - inotify is not recursive: each directory needs its own watch.
     - Records coalesce when identical (same wd, mask, cookie, name) and
       the previous one is still unread. A burst of writes may become one
       IN_MODIFY; IN_MODIFY means "something changed", not a byte count.
     - The queue is bounded by fs.inotify.max_queued_events; overflow
       produces IN_Q_OVERFLOW and lost events. Watches are limited by
       fs.inotify.max_user_watches.
     - rename pairing depends on the cookie, not on names; a move from
       another watched directory has only one visible half.
     - Name fields are not paths and are only valid inside the returned
       record. Events can arrive for a file that has already been
       deleted. Network and some overlay filesystems may not report all
       local changes.
   ============================================== */
