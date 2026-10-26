#!/usr/bin/env bash
# MAGIC: /proc/loadavg is five fields; the averages count runnable *and*
# MAGIC: uninterruptible tasks, so load per CPU is not a CPU-utilisation metric.
set -euo pipefail
export LC_ALL=C

for f in /proc/loadavg /proc/uptime; do
    [[ -r "$f" ]] || { printf 'skipped: %s not readable\n' "$f"; exit 0; }
done

is_num() { [[ "$1" =~ ^[0-9]+([.][0-9]+)?$ ]]; }
is_2dp() { [[ "$1" =~ ^[0-9]+\.[0-9][0-9]$ ]]; }

read -ra lavg < /proc/loadavg
read -ra uptime_fields < /proc/uptime
printf 'proc_loadavg_fields=%s\n' "${#lavg[@]}"
printf 'proc_uptime_fields=%s\n' "${#uptime_fields[@]}"

load1=${lavg[0]}
load5=${lavg[1]}
load15=${lavg[2]}
procs=${lavg[3]}
uptime=${uptime_fields[0]}

if ! is_num "$load1" || ! is_num "$load5" || ! is_num "$load15" || ! is_num "$uptime"; then
    printf 'skipped: non-numeric /proc fields\n'
    exit 0
fi
if [[ ! "$procs" =~ ^[0-9]+/[0-9]+$ ]]; then
    printf 'skipped: unexpected running/total field\n'
    exit 0
fi

running=${procs%/*}
total=${procs#*/}
if (( running > total )); then
    printf 'skipped: running exceeds total\n'
    exit 0
fi

cpus=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')
[[ "$cpus" =~ ^[1-9][0-9]*$ ]] || cpus=1

# Divide inside awk so this works without floating-point bash arithmetic.
read -r p1 p5 p15 < <(awk -v a="$load1" -v b="$load5" -v c="$load15" -v n="$cpus" \
    'BEGIN { printf "%.2f %.2f %.2f\n", a / n, b / n, c / n }')
if ! is_2dp "$p1" || ! is_2dp "$p5" || ! is_2dp "$p15"; then
    printf 'skipped: bad per-CPU format\n'
    exit 0
fi

printf 'cpus_online_ge_1=yes\n'
printf 'load_1m_per_cpu_2dp=yes\n'
printf 'load_5m_per_cpu_2dp=yes\n'
printf 'load_15m_per_cpu_2dp=yes\n'
printf 'running_le_total=yes\n'
printf 'uptime_seconds_ge_0=yes\n'

# ================= EXPLANATION =================
# What it does
#   Parses /proc/loadavg and /proc/uptime, checks their field counts and numeric
#   formats, divides the three load averages by the number of online CPUs, and
#   validates that each quotient is a two-decimal number. The raw values are
#   deliberately not echoed: they change on every read, so the output reports
#   stable facts instead.
#
# Step by step
#   1. /proc/loadavg has five fields: 1-, 5-, and 15-minute exponentially
#      decaying averages, then "runnable/total", then the last PID created.
#      The first three are decimal numbers; the fourth is a slash-separated
#      pair of integers.
#   2. /proc/uptime has two fields: seconds since boot (with hundredths) and
#      cumulative idle seconds. Only the first is used here.
#   3. read -ra splits each line on whitespace into an array, which gives a
#      cheap field count and makes the individual fields explicit.
#   4. The numeric guard rejects anything that is not digits with an optional
#      fractional part. That matters on lxcfs-backed /proc, where a simulator
#      can return different text, and on malformed test fixtures.
#   5. getconf _NPROCESSORS_ONLN reports the CPUs online now, not the maximum
#      configured. If getconf is missing or returns nonsense, the script falls
#      back to 1.
#   6. awk performs the floating-point division and rounds to two decimals with
#      %.2f. Bash cannot do floating-point arithmetic natively. The regex check
#      then proves the formatting succeeded before it is reported.
#
# Output
#   proc_loadavg_fields=5
#   proc_uptime_fields=2
#   cpus_online_ge_1=yes
#   load_1m_per_cpu_2dp=yes
#   load_5m_per_cpu_2dp=yes
#   load_15m_per_cpu_2dp=yes
#   running_le_total=yes
#   uptime_seconds_ge_0=yes
#
# Why it matters
#   Load average is a scheduler statistic, not a CPU gauge. It adds tasks in
#   TASK_RUNNING and TASK_UNINTERRUPTIBLE to nr_running/nr_uninterruptible and
#   applies fixed decay constants (1, 5, 15 minutes). A machine waiting on a
#   slow NFS or disk mount can have a high load with an idle CPU. Per-CPU
#   normalisation is only a rough comparison; a load of 1.00 on one CPU and on
#   eight CPUs mean very different things.
#
# Caveats
#   - Uninterruptible sleep (D state) counts toward load. A process blocked in
#     the kernel for I/O inflates load even though no CPU is busy.
#   - The averages are exponential, not window averages. They are not directly
#     comparable to top's instantaneous %CPU or to /proc/stat jiffy deltas.
#   - Linux only counts runnable and uninterruptible tasks; it does not include
#     tasks in interruptible sleep (S), which on other Unixes may count.
#   - In a container, /proc/loadavg is normally the host's view unless lxcfs or
#     a similar shim is mounted. nproc may also reflect the host or a cpuset.
#   - getconf can be absent in a minimal image; the fallback to 1 is a guess,
#     not the real CPU count.
#   - The "running/total" field counts tasks/threads, not processes, and is a
#     snapshot that may already be stale by the time it is printed.
#   - The raw numbers are hidden here on purpose; a real monitor should export
#     them as gauges rather than parse human text.
# ==============================================
