#!/usr/bin/env python3
"""MAGIC: splice() moves bytes between two descriptors through a pipe inside
the kernel, with no user-space buffer at either end."""

import ctypes
import errno
import os
import sys
import tempfile

if not sys.platform.startswith("linux"):
    print("skipped: non-Linux platform")
    sys.exit(0)

libc = ctypes.CDLL(None, use_errno=True)
try:
    libc.splice.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                            ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint]
    libc.splice.restype = ctypes.c_long
except AttributeError:
    print("skipped: libc has no splice")
    sys.exit(0)


def splice(fd_in, fd_out, length):
    ctypes.set_errno(0)
    moved = libc.splice(fd_in, None, fd_out, None, length, 0)
    if moved < 0:
        err = ctypes.get_errno()
        if err == errno.ENOSYS:
            print("skipped: splice not available (ENOSYS)")
            sys.exit(0)
        raise OSError(err, os.strerror(err))
    return moved


def splice_all(fd_in, fd_out, length):
    done = 0
    while done < length:
        moved = splice(fd_in, fd_out, length - done)
        if moved == 0:
            raise OSError(errno.EIO, "splice made no progress")
        done += moved
    return done


def main():
    with tempfile.TemporaryDirectory() as tmp:
        src = os.open(os.path.join(tmp, "src"),
                      os.O_RDWR | os.O_CREAT | os.O_TRUNC, 0o600)
        dst = os.open(os.path.join(tmp, "dst"),
                      os.O_RDWR | os.O_CREAT | os.O_TRUNC, 0o600)
        rfd, wfd = os.pipe()
        try:
            os.write(src, b"splice magic")          # 12 bytes
            os.lseek(src, 0, os.SEEK_SET)

            # File -> pipe, then pipe -> file: the data goes through the
            # kernel pipe buffers, never through a Python bytes object.
            n = splice_all(src, wfd, 12)
            print(f"bytes={n}")
            assert splice_all(rfd, dst, n) == n

            os.lseek(dst, 0, os.SEEK_SET)
            print("content=" + os.read(dst, n).decode())
        except OSError as e:
            print(f"error: splice: {e.strerror}")
            return 1
        finally:
            for fd in (src, dst, rfd, wfd):
                os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ================= EXPLANATION =================
# What it does
#   Writes 12 bytes to a temporary file, then moves them into a pipe with
#   splice() and out of the pipe into a second file with splice(). It prints
#   the byte count and reads the destination back to prove the contents.
#
# Step by step
#   1. ctypes.CDLL(None) opens the already-loaded libc and looks up splice();
#      argtypes use c_int for fds, c_void_p for the optional off_t * offsets,
#      c_size_t for the length, and c_uint for flags. restype is c_long
#      (ssize_t), not the default c_int.
#   2. os.pipe() creates the kernel buffer that both calls share.
#   3. splice(src, NULL, pipe_w, NULL, 12, 0) attaches the source file's pages
#      to the pipe. NULL offsets mean "use and advance the file offset". The
#      return is the number of bytes moved, which can be less than requested,
#      so splice_all() loops until 12 bytes have crossed.
#   4. splice(pipe_r, NULL, dst, NULL, 12, 0) drains the pipe into the
#      destination. At least one end of every splice() must be a pipe; a
#      direct file-to-file splice returns EINVAL.
#   5. lseek(dst, 0) and read() show the bytes arrived unchanged.
#   6. Every fd is closed in finally; the with block then removes the
#      temporary directory.
#
# Output
#   bytes=12
#   content=splice magic
#
# Why it matters
#   read()/write() copy data into and out of user buffers. splice() hands the
#   kernel a pipe as the transfer buffer, so the payload is not mapped into
#   the process at all. File servers, proxies, and copy tools use it (and its
#   cousins sendfile and copy_file_range) to cut CPU and cache pollution on
#   large transfers. A pipe's 64 KiB capacity also gives natural backpressure.
#
# Caveats
#   - A pipe holds a bounded amount (64 KiB by default, tunable up to
#     /proc/sys/fs/pipe-max-size), so large transfers must loop. One call may
#     move fewer bytes than asked, and with a non-blocking pipe it may return
#     EAGAIN.
#   - Splice moves page references when it can, but it is not free and not
#     always zero-copy: SPLICE_F_MOVE is only a hint, and source/destination
#     types constrain it. Measure before claiming a speedup.
#   - Both fds must support splice (regular files, pipes, sockets, some
#     char devices). Not all filesystems implement it; ENOSYS/EINVAL means
#     fall back to read/write.
#   - The offset pointers are optional but architecture-sensitive; passing
#     None is the portable choice. The syscall wraps a pipe buffer, so data
#     is still copied into and out of that buffer -- "no user-space copy", not
#     "no copy".
#   - The error path reads ctypes.get_errno() only after a -1 return;
#     set_errno(0) beforehand is belt-and-braces, not the detection mechanism.
# ==============================================
