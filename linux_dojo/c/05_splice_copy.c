/* MAGIC: copy a file through a pipe with splice, never staging the bytes
   in a user-space buffer. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int splice_all(int from, int to, size_t len)
{
    size_t left = len;
    while (left > 0) {
        ssize_t n = splice(from, NULL, to, NULL, left, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            break;
        left -= (size_t)n;
    }
    if (left != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int main(void)
{
    const char msg[] = "hello splice\n";
    const size_t len = sizeof msg - 1;
    FILE *src_file = tmpfile();
    FILE *dst_file = tmpfile();
    if (src_file == NULL || dst_file == NULL) {
        perror("tmpfile");
        if (src_file)
            fclose(src_file);
        if (dst_file)
            fclose(dst_file);
        return 1;
    }
    int src = fileno(src_file);
    int dst = fileno(dst_file);
    int pipefd[2] = { -1, -1 };
    int rc = 1;

    ssize_t written = write(src, msg, len);
    if (written != (ssize_t)len) {
        if (written < 0)
            perror("write src");
        else
            fprintf(stderr, "write src: short write\n");
        goto out;
    }
    if (lseek(src, 0, SEEK_SET) != 0) {
        perror("lseek src");
        goto out;
    }
    if (pipe2(pipefd, O_CLOEXEC) != 0) {
        perror("pipe2");
        goto out;
    }
    if (splice_all(src, pipefd[1], len) != 0) {
        perror("splice file to pipe");
        goto out;
    }
    if (close(pipefd[1]) != 0) {
        pipefd[1] = -1;
        perror("close pipe write");
        goto out;
    }
    pipefd[1] = -1;
    if (splice_all(pipefd[0], dst, len) != 0) {
        perror("splice pipe to file");
        goto out;
    }
    if (lseek(dst, 0, SEEK_SET) < 0) {
        perror("lseek dst");
        goto out;
    }
    char buf[64];
    ssize_t got = read(dst, buf, len);
    if (got != (ssize_t)len) {
        if (got < 0)
            perror("read dst");
        else
            fprintf(stderr, "read dst: short read\n");
        goto out;
    }
    buf[got] = '\0';
    printf("bytes=%zu\n", len);
    printf("content=%s", buf);
    rc = 0;

out:
    if (pipefd[0] >= 0 && close(pipefd[0]) != 0)
        rc = 1;
    if (pipefd[1] >= 0 && close(pipefd[1]) != 0)
        rc = 1;
    if (fclose(src_file) != 0)
        rc = 1;
    if (fclose(dst_file) != 0)
        rc = 1;
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Creates two unlinked temporary files, writes "hello splice\n" to the
     source, and copies its 13 bytes through a pipe with two splice(2)
     calls. It then reads the destination back and prints it.

   Step by step
     1. tmpfile() creates src and dst and removes their names immediately;
        only the FILE descriptors keep the inodes alive.
     2. write() fills src, and lseek() rewinds it.
     3. pipe2(O_CLOEXEC) creates the kernel pipe. splice(src, NULL,
        pipefd[1], NULL, 13, 0) moves data from the file into pipe
        buffers. No intermediate user-space buffer holds the copy.
     4. splice(pipefd[0], NULL, dst, NULL, 13, 0) moves the pipe buffers
        into the destination page cache. splice_all() loops on short
        transfers in both directions.
     5. lseek(dst, 0, SEEK_SET) and read() are only verification: they
        copy the result into buf so it can be printed. That final read is
        an ordinary user-space copy, after the splice.
     6. The pipe and both temp files are closed on every path; closing
        the last reference removes the unlinked files.

   Output
     bytes=13
     content=hello splice

   Why it matters
     A pipe is a kernel FIFO of page-backed buffers. splice() passes page
     references between a file and the pipe, or copies inside the kernel
     when it must, but never bounces the payload through user space. That
     is the building block behind sendfile-style servers, proxies, and
     log pipelines that need bounded memory.

   Caveats
     - One end of splice() must be a pipe; file-to-file is invalid. The
       pipe is required.
     - Pipe capacity (often 64 KiB) bounds a transfer, and the call
       blocks while full unless SPLICE_F_NONBLOCK is set.
     - Short returns are normal. This code loops; a single unchecked call
       is a common bug.
     - NULL means "use and advance the file's current offset". A non-NULL
       loff_t pointer updates that explicit offset instead.
     - "No user-space copy" is not "no copy anywhere": filesystem, page
       cache, and pipe paths may copy in the kernel. Measure it.
     - tmpfile() needs a writable temporary filesystem; a failure is
       reported and the program exits nonzero.
   ============================================== */
