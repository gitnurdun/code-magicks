#!/usr/bin/env bash
# MAGIC: bash's /dev/tcp is a redirection parsed by bash itself, so a socket can be opened like a file, written to, and closed.
set -euo pipefail

if ! command -v python3 >/dev/null 2>&1; then
  printf 'skipped: python3 not found\n'
  exit 0
fi

tmp=$(mktemp -d)
listener=
trap 'if [[ -n $listener ]]; then kill "$listener" 2>/dev/null || true; fi; rm -rf -- "$tmp"' EXIT
portfile=$tmp/port

python3 -c '
import socket, sys
s = socket.socket()
s.bind(("127.0.0.1", 0))
s.listen(1)
with open(sys.argv[1], "w") as f:
    f.write(str(s.getsockname()[1]))
conn, _ = s.accept()
data = conn.recv(1)
conn.close()
s.close()
with open(sys.argv[1] + ".byte", "wb") as f:
    f.write(data)
' "$portfile" 2>/dev/null &
listener=$!

for _ in {1..100}; do
  if [[ -s $portfile ]]; then break; fi
  sleep 0.02
done
if [[ ! -s $portfile ]]; then
  printf 'skipped: listener did not bind\n'
  exit 0
fi

port=$(cat "$portfile")

if port=$port timeout 2 bash -c 'exec 3<>/dev/tcp/127.0.0.1/$port && printf x >&3' 2>/dev/null; then
  connected=yes
else
  connected=no
fi

if [[ $connected == no ]]; then
  kill "$listener" 2>/dev/null || true
fi
wait "$listener" 2>/dev/null || true
listener=

if [[ -f $portfile.byte && $(<"$portfile.byte") == x ]]; then
  byte_sent=yes
else
  byte_sent=no
fi

# The listener is gone, so this loopback port now refuses connections.
if port=$port timeout 2 bash -c 'exec 3<>/dev/tcp/127.0.0.1/$port' 2>/dev/null; then
  refused=no
else
  refused=yes
fi

printf 'connected=%s\n' "$connected"
printf 'byte_sent=%s\n' "$byte_sent"
printf 'refused=%s\n' "$refused"

# ================= EXPLANATION =================
# What it does
#   Starts a one-shot TCP listener with python3 on 127.0.0.1 port 0,
#   reads the assigned port from a temp file, connects with bash's
#   /dev/tcp redirection, sends one byte, then reconnects after the
#   listener is gone to observe a refused connection.
#
# Step by step
#   1. python3 is optional; if it is absent the script prints one
#      skipped: line and exits 0. The listener binds an ephemeral port
#      (port 0), listens, and writes the chosen port to $portfile.
#   2. The port file is polled until nonempty. Polling is in 20 ms
#      steps, up to about two seconds.
#   3. 'exec 3<>/dev/tcp/127.0.0.1/$port' opens a read-write socket on
#      fd 3. The $port value is exported into the child's environment
#      by the assignment prefix. 'printf x >&3' writes one byte.
#   4. The listener accepts, reads one byte, writes it to port.byte,
#      and exits. wait reaps it, so the value is available.
#   5. The same port is probed again after the listening socket has
#      closed. There is no listener, so the local connect fails; the
#      script reports refused=yes.
#
# Output
#   connected=yes
#   byte_sent=yes
#   refused=yes
#
# Why it matters
#   /dev/tcp lets a pure-bash script perform an HTTP health check or a
#   port probe without curl, nc or socat. Bash parses the path and makes
#   the socket calls itself; there is no /dev/tcp file on disk, but the
#   kernel still implements the TCP connection underneath.
#
# Caveats
#   - /dev/tcp is a bash feature, not a device node and not a file.
#     POSIX sh, dash and older bashes do not implement it; bash must be
#     built with net redirections (the default on most distros).
#   - It is TCP only: no UDP, no TLS, no connection timeout. A firewall
#     that silently drops packets leaves the connect hanging, so wrap
#     the probe in timeout(1).
#   - refused=yes here means "the connect attempt failed". On loopback
#     with no listener that is ECONNREFUSED, but elsewhere it could be
#     a timeout, DNS failure or unreachable host. The boolean does not
#     distinguish them.
#   - The listener is test scaffolding. Calling Python from a shell
#     demo is fine, but production probes should use a real client.
#   - Port 0 is deliberate: the kernel assigns a free ephemeral port,
#     avoiding collisions with services already on fixed ports.
# ==============================================
