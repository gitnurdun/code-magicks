/* MAGIC: describe one message as several buffers instead of concatenating it.
   WHY: scatter/gather I/O preserves existing header and payload storage. */
#if defined(_WIN32)
#include <stddef.h>
struct iovec { void *iov_base; size_t iov_len; };
#else
#include <sys/uio.h>
#endif
#include <stdio.h>
#include <string.h>
int main(void){char h[]="HEAD ",b[]="payload";struct iovec v[]={{h,strlen(h)},{b,strlen(b)}};for(size_t i=0;i<2;i++)fwrite(v[i].iov_base,1,v[i].iov_len,stdout);putchar('\n');}

/* ================= EXPLANATION =================
   What it does
     Describes one logical message as a list of (address, length) pairs
     instead of copying the pieces into one contiguous buffer. This is the
     scatter/gather shape used by readv/writev and sendmsg.

   Step by step
     1. h is the 5-byte header "HEAD " and b is the 7-byte body
        "payload"; they live in separate arrays.
     2. struct iovec holds iov_base (a void * to the bytes) and iov_len
        (how many bytes belong to this segment). The array v has two
        entries, in the order they should appear on the wire.
     3. The loop walks the array and writes each segment with fwrite. A
        real implementation would call writev(fd, v, 2) once, letting the
        kernel gather both segments into the socket/page cache without an
        intermediate copy.
     4. A newline is added after the segments.

   Output
     "HEAD payload"

   Why it matters
     Zero-copy: the header and payload often come from different layers
     (protocol header + application buffer, or several pooled buffers).
     Concatenating them costs a copy and an allocation per message.
     Passing an iovec array lets the kernel copy directly from each source
     into the destination, in one syscall.

   Caveats
     - The real call is POSIX writev/readv (or sendmsg/recvmsg); fwrite is
       used here only to keep the demo portable and runnable.
     - A short write returns the byte count actually written; you must
       advance across the iovec entries and retry the remainder.
     - There is a limit on the number of segments (IOV_MAX, often 1024).
     - The buffers must stay alive and unmodified until the syscall
       returns. iov_base is not const in the POSIX struct.
     - Some file descriptors (e.g. certain devices) do not support
       scatter/gather efficiently.
   ============================================== */
