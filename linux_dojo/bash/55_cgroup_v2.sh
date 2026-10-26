#!/usr/bin/env bash
# MAGIC: cgroup v2 is one unified hierarchy; limits live in per-cgroup files
# MAGIC: and the literal "max" means unlimited, not a very large number.
set -euo pipefail

[[ -r /sys/fs/cgroup/cgroup.controllers ]] || { printf 'skipped: cgroup v2 not mounted\n'; exit 0; }
[[ -r /proc/self/cgroup ]] || { printf 'skipped: /proc/self/cgroup not available\n'; exit 0; }

# In v2 the single entry is "0::/path"; v1 lines have a controller in field 2.
cgpath=$(awk -F: '$1 == "0" && $2 == "" { print $3; exit }' /proc/self/cgroup)
[[ -n "$cgpath" ]] || { printf 'skipped: no cgroup v2 entry in /proc/self/cgroup\n'; exit 0; }

cgdir="/sys/fs/cgroup${cgpath}"
for f in memory.max memory.current pids.max pids.current cpu.weight; do
    [[ -r "$cgdir/$f" ]] || { printf 'skipped: %s not available\n' "$f"; exit 0; }
done

is_uint() { [[ "$1" =~ ^[0-9]+$ ]]; }

IFS= read -r mem_max < "$cgdir/memory.max"
IFS= read -r mem_cur < "$cgdir/memory.current"
IFS= read -r pids_max < "$cgdir/pids.max"
IFS= read -r pids_cur < "$cgdir/pids.current"
IFS= read -r cpu_weight < "$cgdir/cpu.weight"

mem_le=no
if [[ "$mem_max" == max ]]; then
    mem_le=yes
elif is_uint "$mem_cur" && is_uint "$mem_max" && (( mem_cur <= mem_max )); then
    mem_le=yes
fi

pids_le=no
if [[ "$pids_max" == max ]]; then
    pids_le=yes
elif is_uint "$pids_cur" && is_uint "$pids_max" && (( pids_cur <= pids_max )); then
    pids_le=yes
fi

weight_ok=no
if is_uint "$cpu_weight" && (( cpu_weight >= 1 && cpu_weight <= 10000 )); then
    weight_ok=yes
fi

printf 'cgroup_v2=yes\n'
printf 'cgroup_path_parsed=yes\n'
printf 'memory_current_le_max=%s\n' "$mem_le"
printf 'pids_current_le_max=%s\n' "$pids_le"
printf 'cpu_weight_in_range=%s\n' "$weight_ok"

# ================= EXPLANATION =================
# What it does
#   Detects the cgroup v2 unified hierarchy, finds the reading process's own
#   cgroup from /proc/self/cgroup, reads its memory and pids limits plus CPU
#   weight, and reports normalised booleans. It never prints absolute byte
#   counts or weights, because those differ per host and container.
#
# Step by step
#   1. The presence of /sys/fs/cgroup/cgroup.controllers marks a v2 mount root;
#      v1 has no such file there and mounts each controller separately, such as
#      /sys/fs/cgroup/memory.
#   2. /proc/self/cgroup lists the process's memberships. A v2 line is
#      "0::/path"; field 1 is the hierarchy id (0 for v2), field 2 is empty, and
#      field 3 is the path relative to the cgroup namespace root.
#   3. /sys/fs/cgroup plus that path is the directory holding the process's
#      control files. If the path is "/", the directory is the root itself.
#   4. memory.max and pids.max are either a decimal integer or the literal
#      "max". Treating "max" as a number would fail or, worse, parse as 0. The
#      script handles it explicitly and then compares current against the limit
#      only when the limit is finite.
#   5. memory.current includes page cache, so it can be close to the limit
#      without the workload being out of memory. pids.current counts tasks
#      (threads), not processes. cpu.weight is in 1..10000 with a default of
#      100; it is a relative share among siblings, not a CPU count.
#   6. The output is booleans so it stays stable across machines. memory.max and
#      pids.max may be "max" here or a number; either way the comparison is
#      valid, and a finite max is checked numerically.
#
# Output
#   cgroup_v2=yes
#   cgroup_path_parsed=yes
#   memory_current_le_max=yes
#   pids_current_le_max=yes
#   cpu_weight_in_range=yes
#
# Why it matters
#   cgroup v2 replaced the per-controller v1 trees with one hierarchy in which
#   every process belongs to exactly one cgroup. That makes delegation and
#   accounting coherent: memory, pids, CPU, and I/O all describe the same set of
#   tasks. Container runtimes, systemd, and Kubernetes all program these files,
#   and reading them is how an application discovers its real limits.
#
# Caveats
#   - A container may have a limit set on an ancestor cgroup, not on its own.
#     Reading only /proc/self/cgroup's directory then shows "max" while the
#     container is still capped. Walk the parents and take the minimum for the
#     effective limit.
#   - memory.max = max means no limit at this level, not infinite physical
#     memory; the host and its ancestors still apply theirs.
#   - memory.current includes page cache and kernel memory charged to the
#     cgroup. It is not RSS and can be reclaimed under pressure.
#   - pids.max counts threads. A multithreaded process can hit it with few
#     processes; pids.current can transiently exceed a lowered max until tasks
#     exit.
#   - cpu.weight is not a quota. Use cpu.max for a bandwidth cap and cpu.weight
#     only for relative sharing. Its files appear only when the cpu controller
#     is enabled at and above this cgroup.
#   - cgroup v1 uses different file names (memory.limit_in_bytes,
#     pids.max in a separate mount, cpu.shares) and a process may belong to
#     different cgroups per controller. This script deliberately skips v1.
#   - /sys/fs/cgroup is often mounted read-only in containers; reading is fine,
#     writing limits is not.
#   - Migration and offline races: a process can move cgroups while its files
#     are being read, so the values may not all come from one instant.
# ==============================================
