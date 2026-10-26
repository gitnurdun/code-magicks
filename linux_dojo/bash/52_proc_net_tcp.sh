#!/usr/bin/env bash
# MAGIC: /proc/net/tcp prints addresses as host-endian hex and state 0A means
# MAGIC: LISTEN; the table is per network namespace, not a global view.
set -euo pipefail

[[ -r /proc/net/tcp ]] || { printf 'skipped: /proc/net/tcp not available\n'; exit 0; }

work=$(mktemp -d)
py_pid=''
cleanup() {
    if [[ -n "$py_pid" ]]; then
        kill "$py_pid" 2>/dev/null || true
        wait "$py_pid" 2>/dev/null || true
    fi
    rm -rf -- "$work"
}
trap cleanup EXIT

tables=(/proc/net/tcp)
if [[ -r /proc/net/tcp6 ]]; then
    tables+=(/proc/net/tcp6)
fi
count_listen() {
    awk 'FNR > 1 && $4 == "0A"' "$@" | wc -l
}

portfile=$work/port
if command -v python3 >/dev/null 2>&1; then
    python3 - "$portfile" 2>/dev/null <<'PY' &
import socket, sys, time
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(("127.0.0.1", 0))
s.listen(1)
with open(sys.argv[1], "w", encoding="ascii") as f:
    f.write(str(s.getsockname()[1]) + "\n")
time.sleep(2)
PY
    py_pid=$!
    for ((i = 0; i < 100; i++)); do
        [[ -s "$portfile" ]] && break
        sleep 0.02
    done
fi

port=''
if [[ -s "$portfile" ]]; then
    IFS= read -r port < "$portfile" || true
fi

listener_found=no
if [[ "$port" =~ ^[0-9]+$ ]] && (( port >= 1 && port <= 65535 )); then
    want=$(printf '0100007F:%04X' "$port")
    if awk -v want="$want" 'FNR > 1 && $4 == "0A" && $2 == want { found = 1 } END { exit(found ? 0 : 1) }' /proc/net/tcp; then
        listener_found=yes
        entry=$want
    fi
fi

listen_entries=$(count_listen "${tables[@]}")

printf 'tcp_table_readable=yes\n'
if (( listen_entries >= 1 )); then
    printf 'listen_entries_ge_1=yes\n'
else
    printf 'listen_entries_ge_1=no\n'
fi
if command -v python3 >/dev/null 2>&1; then
    printf 'listener_found=%s\n' "$listener_found"
    if [[ "$listener_found" == yes ]]; then
        hex=${entry%%:*}
        printf 'listener_address=%d.%d.%d.%d\n' \
            "$((16#${hex:6:2}))" "$((16#${hex:4:2}))" \
            "$((16#${hex:2:2}))" "$((16#${hex:0:2}))"
        printf 'listener_state=0A\n'
    fi
else
    printf 'listener_probe=skipped: python3 not available\n'
fi

# ================= EXPLANATION =================
# What it does
#   Parses /proc/net/tcp and /proc/net/tcp6 directly, decodes the hex local
#   address and port of LISTEN sockets, starts a python3 listener on
#   127.0.0.1:0, and finds that exact socket in the kernel table. Only a
#   normalised address and booleans are printed; the ephemeral port is not.
#
# Step by step
#   1. Each data line of /proc/net/tcp has the local address in field 2 and the
#      state in field 4, after a header line. The address is 4 bytes of IPv4
#      written as 8 uppercase hex digits, then ":", then the port in 4 hex
#      digits. State 0A is TCP_LISTEN.
#   2. Linux prints the address as a 32-bit value in host byte order, so on
#      little-endian x86 127.0.0.1 appears as 0100007F. The port, however, is
#      printed in network (big-endian) order, so 0x1F90 is 8080. That asymmetry
#      is why a naive hex-to-IP conversion reverses the octets.
#   3. Python binds an IPv4 socket to 127.0.0.1 port 0, which asks the kernel
#      for any free ephemeral port, calls listen(), writes the chosen port to a
#      temp file, and sleeps. The shell polls for that file.
#   4. The shell formats the port as %04X and builds the exact local key
#      "0100007F:PORT", then asks awk whether a line with state 0A and that
#      local address exists. If it does, the socket is LISTEN in this network
#      namespace.
#   5. The address octets are recovered with bash substring expansion and
#      arithmetic base conversion: bytes 4..3..2..1 of the string become
#      127.0.0.1.
#   6. The listener is killed by the EXIT trap. Without python3 the script still
#      counts LISTEN entries and reports the probe as skipped.
#
# Output
#   tcp_table_readable=yes
#   listen_entries_ge_1=yes
#   listener_found=yes
#   listener_address=127.0.0.1
#   listener_state=0A
#
# Why it matters
#   Before ss and lsof, this was the only way to see TCP state, and it is still
#   the fallback in minimal containers and recovery shells. Parsing it teaches
#   the kernel's address representation, also used by /proc/net/udp and eBPF
#   tools. The namespace scoping explains why containers see their own table.
#
# Caveats
#   - The hex address is in host byte order per 32-bit word. On a big-endian
#     machine 127.0.0.1 would appear as 7F000001, so hard-coding the reversal is
#     only correct for little-endian systems. The port is always big-endian.
#   - /proc/net/tcp is IPv4; /proc/net/tcp6 holds IPv6 in 32 hex digits, four
#     host-order words. This script decodes only the v4 listener.
#   - The table shows the network namespace of the reading process. A socket in
#     another namespace or an ancestor is invisible; use nsenter or setns to
#     look there.
#   - The port could be reused between listen() and the scan. Matching the exact
#     loopback address narrows it, but the table is a snapshot with no handle.
#   - TIME_WAIT, SYN_RECV, and other states are not listeners; filter on 0A.
#   - The inode field is the reliable join key to /proc/<pid>/fd; not used here.
#   - Listening on 127.0.0.1 is deliberate: it cannot accept remote traffic.
# ==============================================
