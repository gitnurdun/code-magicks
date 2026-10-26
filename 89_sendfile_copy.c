/* MAGIC: sendfile copies from the page cache to a socket inside the
   kernel, avoiding a user-space buffer and one copy. */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <string.h>
#if defined(__linux__)
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void)
{
    const char *msg = "zero copy\n";
    int in = open("sendfile_in.txt", O_CREAT | O_RDWR | O_TRUNC, 0600);
    int out = open("sendfile_out.txt", O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (in < 0 || out < 0) { perror("open"); return 1; }
    write(in, msg, strlen(msg));
    off_t off = 0;
    ssize_t n = sendfile(out, in, &off, strlen(msg));
    char buf[32] = {0};
    lseek(out, 0, SEEK_SET);
    read(out, buf, sizeof buf - 1);
    printf("copied=%zd data=%s", n, buf);
    close(in);
    close(out);
    unlink("sendfile_in.txt");
    unlink("sendfile_out.txt");
    return 0;
}
#else
int main(void)
{
    puts("sendfile is Linux-only; this is a compile-time stub");
    return 0;
}
#endif

/* ================= EXPLANATION =================
   What it does
     Copies a small file to another file descriptor with sendfile(2),
     then reads the destination back to prove the bytes arrived. On Linux,
     the kernel moves data from the page cache to the output without
     bouncing it through a user-space buffer.

   Step by step
     1. Two regular files are opened: the source is written with "zero
        copy\n", and the destination starts empty.
     2. sendfile(out, in, &off, count) asks the kernel to copy count bytes
        starting at the source offset. The kernel reads from the page
        cache, writes to the destination's page cache (or socket), and
        updates off. No read()/write() pair copies into user memory.
     3. The return value is the number of bytes copied (10 here). A real
        server would loop because sendfile may copy less than requested.
     4. The destination is rewound and read back with ordinary read();
        the data matches. The input and output files are removed.
     5. The zero-copy win is larger when out is a socket: data can go from
        page cache to NIC via DMA, and the CPU avoids one copy and two
        syscalls.

   Output
     "copied=10 data=zero copy"

   Why it matters
     Web servers and proxies use sendfile to serve static files with less
     CPU and memory bandwidth. It is one of the classic Linux zero-copy
     interfaces, alongside splice, vmsplice, and mmap. The same idea
     appears in Windows TransmitFile and FreeBSD sendfile.

   Caveats
     - sendfile requires in_fd to be a regular file (or a few special
       types); it cannot read from a socket on Linux. For socket-to-socket
       forwarding use splice.
     - It does not work with TLS/SSL, because the data must be encrypted
       in user space; kTLS or a user-space proxy changes the trade-off.
     - The source and destination offsets and partial-copy behaviour must
       be handled in a loop.
     - The page cache may still copy for some filesystems, compression,
       or checksums; "zero-copy" is a path, not a guarantee.
     - The API is Linux-specific. Windows has TransmitFile; macOS has
       sendfile with different semantics.
     - This demo copies file-to-file rather than file-to-socket so it can
       run without a network; the syscall is the same.
   ============================================== */
