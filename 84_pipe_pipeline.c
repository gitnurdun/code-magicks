/* MAGIC: a pipe is a kernel byte buffer with two file descriptors - one
   writes, one reads - and EOF appears when every writer closes. */
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#define READ  _read
#define WRITE _write
#define CLOSE _close
#else
#include <unistd.h>
#define READ  read
#define WRITE write
#define CLOSE close
#endif
int main(void)
{
    int fds[2];
#if defined(_WIN32)
    if (_pipe(fds, 4096, _O_BINARY) != 0) { perror("_pipe"); return 1; }
#else
    if (pipe(fds) != 0) { perror("pipe"); return 1; }
#endif
    const char *msg = "hello pipe\n";
    WRITE(fds[1], msg, (unsigned)strlen(msg));
    CLOSE(fds[1]);
    char buf[64];
    int n = (int)READ(fds[0], buf, sizeof buf - 1);
    if (n < 0) { perror("read"); return 1; }
    buf[n] = '\0';
    printf("pipe said: %s", buf);
    CLOSE(fds[0]);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Creates an anonymous one-way pipe, writes a short message into the
     write end, closes that end, and reads the bytes back from the read
     end. The message comes out exactly as written, including its newline.

   Step by step
     1. pipe() (POSIX) or _pipe() (Windows CRT) returns two descriptors.
        fds[0] is the read end; fds[1] is the write end. The kernel owns a
        bounded buffer between them.
     2. WRITE copies "hello pipe\n" into that buffer. For a small message
        the write completes immediately; if the buffer were full, the
        writer would block until a reader drained it.
     3. CLOSE(fds[1]) closes the only write end. A read that drains the
        buffer now returns 0, the pipe's EOF. If another process still
        held a copy of the write end, EOF would not appear.
     4. READ copies the available bytes into buf and returns the count.
        A single read is not guaranteed to return the whole message; a
        robust loop would continue until EOF.
     5. The program NUL-terminates buf and prints it. Because the write
        end is closed and the message is short, the read sees all 11
        bytes.

   Output
     "pipe said: hello pipe"

   Why it matters
     Pipes are the composition primitive of Unix shells, process
     supervisors, and logging systems. They provide backpressure (a full
     buffer blocks the writer), decouple producer and consumer, and let
     unrelated processes communicate without shared memory. The same
     descriptor abstraction underlies sockets and files.

   Caveats
     - A pipe is one-way. Bidirectional communication needs two pipes or a
       socketpair.
     - It is a byte stream, not a message queue: boundaries are not
       preserved, and a read may return a partial message or several
       messages.
     - Buffer capacity is finite. A parent that writes a large amount
       before reading can deadlock with a child that does the same.
     - SIGPIPE (POSIX) is delivered when writing with no readers; ignore
       it or handle EPIPE.
     - On Windows, CRT descriptors and Win32 HANDLEs are different
       namespaces; use _open_osfhandle or CreatePipe when mixing APIs.
     - Close every unused end, especially in forked children, or EOF will
       never arrive.
   ============================================== */
