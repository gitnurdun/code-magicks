#!/usr/bin/env python3
"""MAGIC: SO_REUSEPORT lets several listening sockets share one port, and the
kernel picks a listener per connection by hashing the 4-tuple."""

import errno
import socket
import sys

if not sys.platform.startswith("linux"):
    print("skipped: non-Linux platform")
    sys.exit(0)

HOST = "127.0.0.1"


def listener(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        s.bind((HOST, port))
        s.listen(16)
    except OSError as e:
        s.close()
        if e.errno in (errno.ENOPROTOOPT, errno.EADDRINUSE):
            print(f"skipped: SO_REUSEPORT unavailable: {e.strerror}")
            return None
        raise
    return s


def main():
    first = listener(0)
    if first is None:
        return 0
    port = first.getsockname()[1]
    second = listener(port)
    if second is None:
        first.close()
        return 0

    clients = []
    try:
        for _ in range(8):
            clients.append(socket.create_connection((HOST, port), timeout=1))

        accepted = 0
        while accepted < 8:
            got = 0
            for s in (first, second):
                s.settimeout(0.2)
                try:
                    conn, _ = s.accept()
                except socket.timeout:
                    continue
                conn.close()
                accepted += 1
                got += 1
            if got == 0:
                break
        print(f"accepted={accepted}")
    finally:
        for c in clients:
            c.close()
        first.close()
        second.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ================= EXPLANATION =================
# What it does
#   Opens two TCP listeners on 127.0.0.1 with SO_REUSEPORT. The first binds to
#   port 0 (ephemeral) and learns the real port from getsockname(); the second
#   binds to that same port. Eight clients connect, and the program accepts
#   across both listeners, printing the total.
#
# Step by step
#   1. Both sockets set SO_REUSEPORT before bind. Without it, the second bind
#      to the same address/port fails with EADDRINUSE.
#   2. The first binds to port 0, so the kernel picks a free port; getsockname()
#      returns it, and the second socket binds that exact port.
#   3. Both call listen(16), forming one reuseport group. Every socket in the
#      group must have the option and normally the same effective UID.
#   4. Eight clients connect over loopback. For each incoming SYN the kernel
#      hashes the connection's 4-tuple (source IP/port, destination IP/port)
#      and selects one listening socket; the connection lands in that socket's
#      accept queue.
#   5. The accept loop serves both sockets with a 0.2 s timeout, closing each
#      connection immediately and counting. A round with no accepts means both
#      queues are empty, so it stops.
#   6. Everything is closed in finally; no listener outlives the demo.
#
# Output
#   accepted=8
#
# Why it matters
#   A single accept() loop is a scalability bottleneck: every worker contends
#   on one queue. SO_REUSEPORT lets each worker own a listener and its own
#   queue, while the kernel spreads connections. nginx, gunicorn, and many
#   service managers use this to run N workers without a shared acceptor.
#   Each connection is pinned to one listener for its lifetime, so a worker
#   sees the same connection from SYN to close.
#
# Caveats
#   - Needs Linux 3.9+. On older kernels the setsockopt fails with
#     ENOPROTOOPT (or bind with EADDRINUSE); the program prints one skip line
#     and exits 0.
#   - Distribution is not guaranteed even. It is a hash, not round-robin: with
#     eight connections all eight can land on one listener (the probability is
#     small but real), and short-lived flows can produce lopsided counts. The
#     program reports the total, not a per-socket split, for that reason.
#   - The hash normally includes a per-network random seed; matching it across
#     reboots or hosts is not a contract. eBPF reuseport programs can override
#     the selection but are outside this demo.
#   - SO_REUSEPORT is not SO_REUSEADDR. REUSEADDR relaxes TIME_WAIT binding;
#     REUSEPORT deliberately creates a load-balancing group. All members of a
#     group must set it before bind, and a member without it gets EADDRINUSE.
#   - SO_REUSEPORT for TCP balances at connect time. A long-lived connection
#     never migrates between listeners, so a busy connection pins one worker.
# ==============================================
