#!/usr/bin/env python3
"""MAGIC: an AF_UNIX address beginning with a NUL byte lives in the abstract
namespace, with no filesystem entry and no permissions to manage."""
import os
import socket
import sys


def main():
    if not sys.platform.startswith("linux"):
        print("skipped: non-Linux platform")
        return 0

    name = b"\0linux_dojo_abstract_%d" % os.getpid()
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    conn = None
    try:
        server.bind(name)
        server.listen(1)

        client.connect(name)
        conn, _ = server.accept()

        client.sendall(b"hello")
        data = conn.recv(16)
        print("abstract=yes")
        print(f"echo={data.decode()}")
        starts_with_nul = name[:1] == b"\x00"
        print(f"name_starts_with_nul={'yes' if starts_with_nul else 'no'}")
    finally:
        for sock in (conn, client, server):
            if sock is not None:
                sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# ================= EXPLANATION =================
# What it does
#   Binds an AF_UNIX stream socket to an abstract address (a leading NUL byte),
#   connects a second socket to the same name, and echoes one message. No file
#   appears in the filesystem.
#
# Step by step
#   1. sockaddr_un normally holds a pathname. If sun_path[0] is NUL, the rest
#      of the bytes are a name in the abstract namespace instead of a path.
#   2. bind() registers that name in the current network namespace. There is no
#      inode, no directory entry, and no chmod/chown to worry about.
#   3. connect() looks the name up the same way. The kernel matches the full
#      byte string, including the trailing bytes after the NUL.
#   4. accept() returns a connected stream; the exchange is ordinary Unix
#      socket I/O.
#   5. Closing the last socket releases the name. A process crash cannot leave
#      a stale socket file behind, because there was never a file.
#
# Output
#   abstract=yes
#   echo=hello
#   name_starts_with_nul=yes
#
# Why it matters
#   Abstract sockets are the cleanest way for local services to meet without
#   touching the filesystem: no permissions race, no stale socket after a
#   crash, no path-length juggling. systemd, D-Bus, and container runtimes use
#   them for local control channels. Their scoping is the network namespace,
#   which also makes them useful for per-container isolation.
#
# Caveats
#   - Abstract sockets are Linux-specific. BSD/macOS need a filesystem socket
#     and explicit unlink-on-start.
#   - The name is scoped to the network namespace, not the mount namespace; a
#     process in another netns cannot see or connect to it.
#   - There is no permission check on connect. Any process in the namespace
#     that knows the name can connect; use SO_PEERCRED and authorization logic.
#   - The address is a byte string, not a NUL-terminated C string. Python's
#     bytes form preserves embedded NULs, but C callers must set the length
#     correctly.
#   - The name is released when the last reference closes, but a listening
#     socket that is inherited by a child keeps the name alive.
# ==============================================
