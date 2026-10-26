#!/usr/bin/env python3
"""MAGIC: SO_PEERCRED asks the kernel who is on the other end of a Unix socket,
with credentials captured at connect time."""
import os
import socket
import struct
import sys

SO_PEERCRED = getattr(socket, "SO_PEERCRED", 17)


def main():
    if not sys.platform.startswith("linux"):
        print("skipped: non-Linux platform")
        return 0

    left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        size = struct.calcsize("3i")
        raw = left.getsockopt(socket.SOL_SOCKET, SO_PEERCRED, size)
        pid, uid, gid = struct.unpack("3i", raw)

        print(f"pid_match={'yes' if pid == os.getpid() else 'no'}")
        print(f"uid_match={'yes' if uid == os.getuid() else 'no'}")
        print(f"gid_match={'yes' if gid == os.getgid() else 'no'}")
        print(f"cred_size={len(raw)}")
    finally:
        left.close()
        right.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# ================= EXPLANATION =================
# What it does
#   Creates a Unix socketpair and asks the kernel for the peer's pid, uid, and
#   gid via SO_PEERCRED. In a socketpair the peer is the same process, so all
#   three match the caller.
#
# Step by step
#   1. socketpair(AF_UNIX, SOCK_STREAM) returns two connected endpoints.
#   2. SO_PEERCRED is a getsockopt option on an AF_UNIX socket. The kernel
#      fills a struct ucred { pid_t pid; uid_t uid; gid_t gid; }.
#   3. Python's struct format "3i" matches three 32-bit ints, which is the
#      Linux LP64 layout used here.
#   4. The values are compared with the current process. They match because
#      both ends of a socketpair are the same process; a real server would see
#      the client's credentials instead.
#   5. The sockets are closed in finally.
#
# Output
#   pid_match=yes
#   uid_match=yes
#   gid_match=yes
#   cred_size=12
#
# Why it matters
#   SO_PEERCRED is the basis of local authentication: D-Bus, systemd, and
#   container runtimes use it to decide whether a client may talk to a service.
#   The credentials are captured when the connection is established (connect
#   or socketpair), so a process cannot change uid after connecting and fool
#   the server.
#
# Caveats
#   - The credentials are a snapshot at connect time, not a live check. If the
#     peer changes uid/gid later, SO_PEERCRED still reports the old values.
#   - A pid is not a stable identity: it can be recycled after the peer exits.
#     For a stable handle, pair this with SO_PEERPIDFD (Linux 6.5+) or check
#     the process start time.
#   - The 3i layout assumes 32-bit pid/uid/gid, which is the Linux ABI. Do not
#     use it on a non-Linux Unix.
#   - SO_PEERCRED is only meaningful for connected AF_UNIX sockets; on a
#     listening socket it reports the creator of the socket, not a client.
#   - Root can forge some credential-related metadata elsewhere in the system,
#     so treat peer credentials as one input to authorization, not a complete
#     security policy.
# ==============================================
