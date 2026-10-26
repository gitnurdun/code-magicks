/* MAGIC: tee duplicates data between two pipes by taking another
   reference to the same kernel pipe buffers, consuming nothing. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int p1[2] = { -1, -1 }, p2[2] = { -1, -1 };
    int rc = 1;
    const char msg[] = "tee me";
    const size_t len = sizeof msg - 1;

    if (pipe2(p1, O_CLOEXEC) != 0) {
        perror("pipe2 p1");
        return 1;
    }
    if (pipe2(p2, O_CLOEXEC) != 0) {
        perror("pipe2 p2");
        goto out;
    }
    ssize_t moved = write(p1[1], msg, len);
    if (moved != (ssize_t)len) {
        if (moved < 0)
            perror("write p1");
        else
            fprintf(stderr, "write p1: short write\n");
        goto out;
    }
    moved = tee(p1[0], p2[1], len, 0);
    if (moved != (ssize_t)len) {
        if (moved < 0)
            perror("tee");
        else
            fprintf(stderr, "tee: short duplicate\n");
        goto out;
    }
    if (close(p1[1]) != 0) {
        perror("close p1 write");
        p1[1] = -1;
        goto out;
    }
    p1[1] = -1;
    if (close(p2[1]) != 0) {
        perror("close p2 write");
        p2[1] = -1;
        goto out;
    }
    p2[1] = -1;

    char a[16], b[16];
    ssize_t na = read(p1[0], a, len);
    if (na != (ssize_t)len) {
        if (na < 0)
            perror("read p1");
        else
            fprintf(stderr, "read p1: short read\n");
        goto out;
    }
    ssize_t nb = read(p2[0], b, len);
    if (nb != (ssize_t)len) {
        if (nb < 0)
            perror("read p2");
        else
            fprintf(stderr, "read p2: short read\n");
        goto out;
    }
    a[na] = '\0';
    b[nb] = '\0';
    printf("a=%s b=%s\n", a, b);
    rc = 0;

out:
    if (p1[0] >= 0 && close(p1[0]) != 0)
        rc = 1;
    if (p1[1] >= 0 && close(p1[1]) != 0)
        rc = 1;
    if (p2[0] >= 0 && close(p2[0]) != 0)
        rc = 1;
    if (p2[1] >= 0 && close(p2[1]) != 0)
        rc = 1;
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Writes "tee me" into pipe p1, duplicates those six bytes into pipe
     p2 with tee(2), then drains both pipes and prints both copies. The
     input bytes are still present after tee.

   Step by step
     1. Two pipe2(O_CLOEXEC) calls create independent pipes p1 and p2.
        Only their read ends will be used for the final reads.
     2. write(p1[1], "tee me", 6) fills the first pipe. A short message
        is written atomically because it is below PIPE_BUF.
     3. tee(p1[0], p2[1], 6, 0) attaches the same pipe-buffer references
        to p2. tee does not advance p1's read pointer, so both pipes now
        contain the bytes.
     4. Both write ends are closed. Reading p1[0] returns "tee me"; the
        tee did not consume it. Reading p2[0] returns the duplicate.
     5. The descriptors are closed on success and on every error path.

   Output
     a=tee me b=tee me

   Why it matters
     tee is the fan-out primitive for pipes. A proxy can copy a request
     body to an audit log and still pass the original stream onward; a
     monitoring daemon can sample a pipe without stealing bytes. The
     duplicate is a reference to the same pages, not a byte-by-byte
     memcpy. That keeps the operation O(1) in data size (though each
     reader gets its own read cursor).

   Caveats
     - Both descriptors must refer to pipes, and they must be different
       pipes; anything else fails with EINVAL.
     - tee does not create data. If the input pipe is empty and writers
       are still open, it blocks; with SPLICE_F_NONBLOCK it can return
       EAGAIN.
     - The output pipe has finite capacity. A full p2 makes tee block or
       return EAGAIN, even if p1 has more data.
     - "No copy" means no payload copy into a user buffer. Each consumer
       still copies from the pipe into its own read() buffer.
     - The duplicates are independent read cursors, not independent
       storage. Draining one pipe does not drain the other; failing to
       drain either can deadlock a producer.
     - SPLICE_F_GIFT is unused for tee; flags mainly affect blocking and
       move hints.
   ============================================== */
