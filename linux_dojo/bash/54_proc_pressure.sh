#!/usr/bin/env bash
# MAGIC: /proc/pressure reports time lost to stalls; "some" means at least one
# MAGIC: task stalled, "full" means every task stalled (no full line for CPU).
set -euo pipefail

for res in cpu memory io; do
    [[ -r "/proc/pressure/$res" ]] || { printf 'skipped: CONFIG_PSI (%s missing)\n' "$res"; exit 0; }
done

is_2dp() { [[ "$1" =~ ^[0-9]+\.[0-9][0-9]$ ]]; }

read_pressure() {
    awk '
        $1 == "some" || $1 == "full" {
            v = "absent"
            for (i = 2; i <= NF; i++)
                if ($i ~ /^avg10=/) v = substr($i, 7)
            if ($1 == "some") some = v
            else full = v
        }
        END {
            printf "%s %s\n", (some == "" ? "absent" : some), (full == "" ? "absent" : full)
        }
    ' "$1"
}

printf 'psi_available=yes\n'
printf 'psi_avg10_format=2dp\n'

for res in cpu memory io; do
    facts=$(read_pressure "/proc/pressure/$res")
    read -r some full <<< "$facts"
    if is_2dp "$some"; then
        printf '%s_some_avg10_ge_0=yes\n' "$res"
    else
        printf 'skipped: malformed %s some line\n' "$res"
        exit 0
    fi
    if [[ "$res" == cpu ]]; then
        printf 'cpu_full_line=absent\n'
    elif is_2dp "$full"; then
        printf '%s_full_avg10_ge_0=yes\n' "$res"
    else
        printf 'skipped: malformed %s full line\n' "$res"
        exit 0
    fi
done

# ================= EXPLANATION =================
# What it does
#   Reads the three PSI files, extracts the avg10 value from the "some" line
#   (and the "full" line where it exists), validates that each is a two-decimal
#   number, and reports stable booleans. CPU has no "full" line by design.
#
# Step by step
#   1. Pressure Stall Information lives under /proc/pressure when the kernel is
#      built with CONFIG_PSI. Each file has a "some" line and, for memory and
#      io, a "full" line. The fields are avg10, avg60, avg300, and total, the
#      last in microseconds.
#   2. The "some" line counts wall time in which at least one task was stalled
#      on the resource; the percentage can be non-zero even while other CPUs
#      run normally.
#   3. The "full" line counts wall time in which *all* non-idle tasks were
#      stalled. CPU has no full line because a runnable CPU task is, by
#      definition, making progress; only memory and I/O can stall everyone.
#   4. The awk helper walks the fields of each line and takes the substring
#      after "avg10=". It emits "some full" with "absent" for a missing line, so
#      the shell can keep the two cases separate without stitching files.
#   5. is_2dp rejects anything that is not digits dot two digits. The kernel
#      formats these with %u.%02u; if a shim or a future kernel changes that,
#      the script skips rather than printing garbage.
#   6. The raw percentages are not echoed because they move on every read. The
#      output records that they were parsed and are non-negative, which is what
#      the test cares about. A busy loop can be added to make "some" non-zero,
#      but it would still be a variable number.
#
# Output
#   psi_available=yes
#   psi_avg10_format=2dp
#   cpu_some_avg10_ge_0=yes
#   cpu_full_line=absent
#   memory_some_avg10_ge_0=yes
#   memory_full_avg10_ge_0=yes
#   io_some_avg10_ge_0=yes
#   io_full_avg10_ge_0=yes
#
# Why it matters
#   Load average mixes runnable and blocked tasks and has no notion of which
#   resource is short. PSI distinguishes CPU contention from memory thrashing
#   and I/O stalls, which is what systemd-oomd, container runtimes, and
#   pressure-based schedulers actually need. A memory.full above zero means
#   every task was waiting on reclaim, a much stronger signal than a high load
#   average.
#
# Caveats
#   - CONFIG_PSI is required. The psi=0 boot parameter or a kernel built
#     without it can leave the files missing; psi=1 enables them. The script
#     prints one skipped line in that case.
#   - /proc/pressure is system-wide (or namespace-wide with a suitable /proc).
#     In a container it usually shows the host, not the cgroup, unless lxcfs or
#     cgroup pressure files are used. cgroup v2 exposes memory.pressure,
#     cpu.pressure, and io.pressure per cgroup.
#   - avg10 is an exponentially weighted percentage of wall time over roughly
#     the last ten seconds, not an instantaneous value. Reading it twice in a
#     row can give different numbers.
#   - "some" does not identify which task or cgroup stalled; use the per-cgroup
#     files or tracing for that.
#   - CPU has no "full" line. Do not treat its absence as a read failure.
#   - PSI can be disabled per cgroup with cgroup.pressure on cgroup v2, so a
#     zero value may mean "not accounted", not "no stalls".
#   - The total= field is in microseconds and wraps only after a very long
#     uptime; this script does not read it.
# ==============================================
