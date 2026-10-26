#!/usr/bin/env bash
# MAGIC: GNU timeout returns 124 when it fired, but 124 alone proves nothing because the child may have chosen it too.
set -euo pipefail

tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT

set +e
timeout 0.2 sleep 5
s_timeout=$?
set -e

set +e
timeout --signal=KILL 0.2 sleep 5
s_kill=$?
set -e

set +e
timeout --preserve-status 0.2 sleep 5
s_preserved=$?
set -e

set +e
timeout --foreground 0.2 sleep 5
s_foreground=$?
set -e

set +e
timeout 0.2 bash -c 'sleep 5 && : > "$1"' _ "$tmp/ran_timeout"
s_fired=$?
timeout 2 bash -c ': > "$1"; exit 124' _ "$tmp/ran_child"
s_child=$?
set -e

printf 'plain_timeout=%d\n' "$s_timeout"
printf 'signal_kill=%d\n' "$s_kill"
printf 'preserve_status=%d\n' "$s_preserved"
printf 'foreground=%d\n' "$s_foreground"
printf 'fired_rc=%d marker=%s\n' "$s_fired" "$([[ -e $tmp/ran_timeout ]] && echo present || echo absent)"
printf 'child_rc=%d marker=%s\n' "$s_child" "$([[ -e $tmp/ran_child ]] && echo present || echo absent)"

# ================= EXPLANATION =================
# What it does
#   Runs GNU timeout in several modes and prints each exit status: the
#   default 124, SIGKILL's 137, --preserve-status's child signal status,
#   and --foreground's normal 124. It then shows that a child which
#   exits 124 on its own is indistinguishable by status alone, and uses
#   a completion marker to tell the two cases apart.
#
# Step by step
#   1. set +e is required around every probe: timeout's nonzero status
#      is the measurement, not an error.
#   2. timeout 0.2 sleep 5 sends SIGTERM after 200 ms and exits 124.
#   3. --signal=KILL sends SIGKILL instead; the reported status is
#      128+9=137.
#   4. --preserve-status reports what the child did instead of 124:
#      killed by SIGTERM is 128+15=143.
#   5. --foreground changes process-group handling, not the status: the
#      command still times out with 124.
#   6. The last two runs both finish with status 124. The first child is
#      killed before it can touch ran_timeout; the second creates
#      ran_child and exits 124 itself. The marker is the evidence.
#
# Output
#   plain_timeout=124
#   signal_kill=137
#   preserve_status=143
#   foreground=124
#   fired_rc=124 marker=absent
#   child_rc=124 marker=present
#
# Why it matters
#   Scripts commonly branch on rc == 124 to mean "timed out". That is a
#   convention, not a proof. When a command can legitimately exit 124,
#   record a side effect (a marker file, a log line) or use
#   --preserve-status and a known signal; otherwise the timeout and the
#   command are indistinguishable after the fact.
#
# Caveats
#   - These flags and fractional durations are GNU coreutils. POSIX has
#     no timeout utility; BSD timeout and busybox differ in statuses.
#   - timeout uses 124 for its own timeout only when --preserve-status
#     is off; a child can also return 124, and with --preserve-status a
#     timeout can surface as 143 or 137, exactly like a child killed by
#     the same signal.
#   - 125 means timeout itself failed, 126 command found but not
#     executable, 127 command not found. Do not fold those into 124.
#   - The default signals the command's whole process group; with
#     --foreground only the direct child is signalled and grandchildren
#     may survive. --kill-after is needed for children that ignore TERM.
#   - SIGKILL cannot be caught, so the target gets no cleanup. Timing
#     is subject to scheduler delay, not an exact deadline.
# ==============================================
