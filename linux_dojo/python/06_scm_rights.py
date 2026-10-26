#!/usr/bin/env python3
"""MAGIC: SCM_RIGHTS passes an open file description through a Unix socket;
the kernel installs a duplicate fd in the receiving process."""

import array
import os
import socket
import sys
import tempfile

if not sys.platform.startswith("linux"):
    print("skipped: non-Linux platform")
    sys.exit(0)


def main():
    with tempfile.TemporaryDirectory() as tmp:
        fd = os.open(os.path.join(tmp, "payload"),
                     os.O_RDWR | os.O_CREAT | os.O_TRUNC, 0o600)
        left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        rfd = -1
        try:
            os.write(fd, b"fd passing")
            fds = array.array("i", [fd])

            # The fd travels as ancillary data; the payload byte is the data.
            left.sendmsg([b"x"],
                         [(socket.SOL_SOCKET, socket.SCM_RIGHTS, fds)])

            msg, ancdata, _flags, _addr = right.recvmsg(
                1, socket.CMSG_SPACE(fds.itemsize))
            assert msg == b"x"
            for level, ctype, cdata in ancdata:
                if level == socket.SOL_SOCKET and ctype == socket.SCM_RIGHTS:
                    got = array.array("i")
                    got.frombytes(cdata[:fds.itemsize])
                    rfd = got[0]
            if rfd < 0:
                print("skipped: no SCM_RIGHTS descriptor received")
                return 0

            os.lseek(rfd, 0, os.SEEK_SET)
            data = os.read(rfd, 64)
            a, b = os.fstat(rfd), os.fstat(fd)
            same = (a.st_dev, a.st_ino) == (b.st_dev, b.st_ino)
            print("received={} same_inode={}".format(
                data.decode(), "yes" if same else "no"))
        finally:
            if rfd >= 0:
                os.close(rfd)
            left.close()
            right.close()
            os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ================= EXPLANATION =================
# What it does
#   Creates a temporary file, writes "fd passing", sends its descriptor over a
#   Unix socketpair as an SCM_RIGHTS control message, receives it on the other
#   end, and reads the file through the received descriptor. It compares
#   st_dev/st_ino rather than fd numbers, because the numbers differ by design.
#
# Step by step
#   1. socketpair(AF_UNIX, SOCK_STREAM) gives two connected endpoints. There
#      is no address, no listen(), and no network.
#   2. os.open() gives the sender a descriptor for the file; os.write() fills
#      it. The descriptor number is private to this process.
#   3. sendmsg() carries ordinary data (b"x") plus ancillary data. The tuple
#      (SOL_SOCKET, SCM_RIGHTS, array.array("i", [fd])) asks the kernel to
#      duplicate fd into the receiving process. The array of ints is the ABI
#      payload: one or more raw fd numbers.
#   4. recvmsg(1, CMSG_SPACE(4)) reserves room for the byte and for one
#      control message. The kernel returns the received data and the cmsg,
#      whose body is an array of new fd numbers in our process.
#   5. The received fd refers to the same open file description: same inode,
#      same offset, same open flags. lseek(rfd, 0) and read() show the text.
#   6. fstat() on both fds confirms identical st_dev and st_ino. Every fd and
#      socket is closed in finally; the temporary file is then removed.
#
# Output
#   received=fd passing same_inode=yes
#
# Why it matters
#   Fd passing is capability transfer. A privileged parent opens a resource
#   (a device, a socket, a file it alone can reach), then hands the capability
#   to a sandboxed child without giving it a path or the right to open one.
#   Wayland, D-Bus, systemd socket activation, and container runtimes all rely
#   on SCM_RIGHTS. The receiver gets a real descriptor it can read, write,
#   poll, or pass on again.
#
# Caveats
#   - The receiver must close every fd it gets. Python's recvmsg() does not
#     close them for you, and a dropped control message leaks descriptors
#     until the process exits or hits RLIMIT_NOFILE.
#   - The race with fork/exec: a received fd is not close-on-exec unless you
#     pass MSG_CMSG_CLOEXEC to recvmsg() (Linux) or set FD_CLOEXEC afterwards.
#     Passing fds to a child that execs without CLOEXEC leaks them into the
#     new program.
#   - SCM_RIGHTS works only over AF_UNIX sockets, not TCP. A single message can
#     carry at most SCM_MAX_FD (253) descriptors and is still limited by the
#     receiver's RLIMIT_NOFILE.
#   - The kernel installs a duplicate descriptor for the same open file
#     description; it does not copy the file contents. The receiver shares the
#     offset and status flags, so one process's lseek() or O_NONBLOCK change
#     can surprise the other.
#   - Matching st_dev/st_ino proves the same inode, not necessarily the same
#     open file description (two separate opens of one file would also match).
#     Here the fd was duplicated, so it really is one description.
# ==============================================
