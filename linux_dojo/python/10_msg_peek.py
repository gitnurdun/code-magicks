#!/usr/bin/env python3
"""MAGIC: MSG_PEEK returns a copy without dequeuing, and SO_TIMESTAMP attaches
a kernel receive timestamp as ancillary data."""
import socket
import struct
import sys

SO_TIMESTAMP = getattr(socket, "SO_TIMESTAMP", 29)


def main():
    if not sys.platform.startswith("linux"):
        print("skipped: non-Linux platform")
        return 0

    rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        rx.bind(("127.0.0.1", 0))
        addr = rx.getsockname()

        tx.sendto(b"peekaboo", addr)
        first = rx.recv(64, socket.MSG_PEEK)
        second = rx.recv(64, socket.MSG_PEEK)
        consumed = rx.recv(64)

        rx.setsockopt(socket.SOL_SOCKET, SO_TIMESTAMP, 1)
        tx.sendto(b"stamped", addr)
        msg, ancdata, flags, src = rx.recvmsg(64, socket.CMSG_SPACE(16))

        stamp = None
        for level, ctype, cdata in ancdata:
            if level == socket.SOL_SOCKET and ctype == SO_TIMESTAMP:
                sec, usec = struct.unpack("ll", cdata[:16])
                stamp = (sec, usec)

        print(f"peek_same={'yes' if first == second else 'no'}")
        print(f"consumed={consumed.decode()}")
        print(f"timestamp_present={'yes' if stamp else 'no'}")
        print(f"timestamp_plausible={'yes' if stamp and stamp[0] > 1_000_000_000 else 'no'}")
    finally:
        rx.close()
        tx.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# ================= EXPLANATION =================
# What it does
#   Receives the same UDP datagram twice with MSG_PEEK, then consumes it. It
#   then enables SO_TIMESTAMP and reads the kernel's receive time from the
#   SCM_TIMESTAMP control message.
#
# Step by step
#   1. Two UDP sockets are created; the receiver binds to an ephemeral port on
#      loopback and reports the address.
#   2. The sender queues "peekaboo". MSG_PEEK copies the datagram into user
#      space but leaves it on the receive queue, so the next peek sees the same
#      bytes.
#   3. A normal recv() finally removes the datagram.
#   4. SO_TIMESTAMP makes the kernel record a timeval when the packet is
#      received, not when recvmsg() runs. recvmsg() returns it in ancillary
#      data, one cmsg per datagram.
#   5. struct timeval is two C longs; on 64-bit Linux that is two 8-byte
#      values, which "ll" matches. The 16-byte CMSG_SPACE reserves room for
#      the cmsghdr plus the timeval.
#
# Output
#   peek_same=yes
#   consumed=peekaboo
#   timestamp_present=yes
#   timestamp_plausible=yes
#
# Why it matters
#   MSG_PEEK is how protocol parsers inspect a header before deciding how much
#   body to read, and how proxies avoid a second buffer. SO_TIMESTAMP moves
#   timing into the kernel's receive path, which is far closer to the wire than
#   a user-space clock read after the syscall. The ancillary-data mechanism is
#   the same one used for SCM_RIGHTS fd passing and SCM_CREDENTIALS.
#
# Caveats
#   - MSG_PEEK copies the data; it is not zero-copy and can be expensive for
#     large datagrams.
#   - The datagram stays queued, so a peek loop can starve other readers.
#   - SO_TIMESTAMP gives wall-clock time (CLOCK_REALTIME). For intervals use
#     SO_TIMESTAMPNS/SO_TIMESTAMPING with the appropriate clock.
#   - Ancillary data is per-datagram and truncated if the control buffer is too
#     small. Use CMSG_SPACE(sizeof(struct timeval)) and check MSG_CTRUNC.
#   - The "ll" struct format assumes the LP64 layout used on Linux x86-64 and
#     aarch64. A 32-bit build uses 32-bit longs and a different size.
# ==============================================
