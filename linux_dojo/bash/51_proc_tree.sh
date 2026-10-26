#!/usr/bin/env bash
# MAGIC: /proc/<pid>/stat puts comm between the first "(" and the last ")";
# MAGIC: everything after it is fixed-width, so field 4 (ppid) is unambiguous.
set -euo pipefail
export LC_ALL=C

[[ -r /proc/$$/stat ]] || { printf 'skipped: /proc is not mounted\n'; exit 0; }
for t in sleep sort awk; do
    command -v "$t" >/dev/null 2>&1 || { printf 'skipped: %s not available\n' "$t"; exit 0; }
done

# Two short-lived children make the tree non-trivial and both are "sleep".
sleep 0.3 &
sleep 0.3 &
sleep 0.02                     # let the children exec before the snapshot

shopt -s nullglob
stats=(/proc/[0-9]*/stat)
if (( ${#stats[@]} == 0 )); then
    printf 'skipped: no /proc/<pid>/stat entries\n'
    exit 0
fi

declare -A kids=() comm_of=() seen=()
for st in "${stats[@]}"; do
    pid=${st#/proc/}
    pid=${pid%/stat}
    # read is a builtin: no transient child appears in the snapshot.
    IFS= read -r line < "$st" || continue
    rest=${line##*)}           # " S ppid ..." after the last ")"
    head=${line%"$rest"}       # "pid (comm)"
    c=${head#* (}              # "comm)"
    c=${c%)}                   # "comm"
    read -r state ppid _ <<< "$rest"
    [[ "$ppid" =~ ^[0-9]+$ ]] || continue
    comm_of[$pid]=$c
    kids[$ppid]+=" $pid"
done

tree_lines=()
walk() {
    local parent=$1 depth=$2 child
    local -a children=()
    read -ra children <<< "${kids[$parent]:-}" || true
    for child in "${children[@]}"; do
        if [[ -n "${seen[$child]:-}" ]]; then
            continue
        fi
        seen[$child]=1
        tree_lines+=("$depth ${comm_of[$child]:-unknown}")
        walk "$child" "$((depth + 1))"
    done
}
walk "$$" 0

printf 'tree:\n'
if (( ${#tree_lines[@]} > 0 )); then
    printf '%s\n' "${tree_lines[@]}" |
        sort -n -k1,1 -k2,2 |
        awk '{ d = $1; $1 = ""; sub(/^ /, ""); printf "  depth=%s comm=%s\n", d, $0 }'
fi
printf 'descendants=%s\n' "${#tree_lines[@]}"
wait

# ================= EXPLANATION =================
# What it does
#   Spawns two short-lived sleep children, walks every /proc/<pid>/stat file,
#   builds a parent-to-children map, and prints the subtree below the script's
#   own PID as depth plus comm. PIDs are never printed, because they change on
#   every run; only the two child names and the descendant count are stable.
#
# Step by step
#   1. /proc/<pid>/stat begins with "pid (comm) state ppid ...". comm is the
#      task name, limited to 16 bytes, and it may contain spaces, parentheses,
#      and even newlines, so splitting on whitespace is wrong.
#   2. The safe split is the last ")" in the line: the numeric fields after it
#      cannot contain ")". rest is "state ppid ...", and ppid is its second
#      field, which is field 4 of the whole line (pid, comm, state, ppid).
#   3. The comm text is everything between the first "(" after the pid and that
#      last ")". Taking the first "(" is safe because the pid is numeric.
#   4. Each pid is mapped to its ppid, producing a multimap from parent to a
#      space-separated list of children. A process that exits between the glob
#      and the read is silently skipped.
#   5. A recursive DFS from $$ follows the map. A seen set prevents duplicate
#      entries and any pathological cycle, and the depth increments per level.
#   6. Lines of "depth comm" are sorted numerically by depth and then by comm,
#      and awk reformats them as "depth=N comm=NAME". The root itself is not
#      printed because its comm is the interpreter or script name.
#   7. wait reaps the two sleep children so the script exits cleanly.
#
# Output
#   tree:
#     depth=1 comm=sleep
#     depth=1 comm=sleep
#   descendants=2
#
# Why it matters
#   Process trees are the basis of supervisors, cgroup placement, and kill
#   policies; getting ppid wrong breaks all of them. The "comm after the last
#   )" rule is the only correct way to parse stat without a real parser, and it
#   is also why tools prefer /proc/<pid>/task/<tid>/children or cgroup.procs for
#   reliable hierarchy walks.
#
# Caveats
#   - The snapshot is racy: processes can fork, exec, or exit between the glob
#     and the read. This script keeps its own children alive with sleep 0.3 and
#     uses only builtins during the walk, but a busy machine can still drop a
#     child.
#   - PIDs are reused. A stale ppid captured just before a parent exited can
#     name an unrelated process by the time the map is built. The seen set
#     avoids loops but cannot detect this.
#   - comm is changed by exec and by prctl(PR_SET_NAME), can be inherited, and
#     is truncated to 15 characters plus NUL. Two different commands can share
#     the same comm, which is why this output is not unique.
#   - A newline inside comm would break the line-oriented read; the kernel
#     permits it, so a hardened parser should read the whole file and split at
#     the last ")" rather than the first newline.
#   - Kernel threads have ppid 2 (kthreadd) or 0 and often no executable; this
#     walk includes them if they are reachable from the chosen root.
#   - /proc/<pid>/stat is world-readable in the default hidepid=0 setup; other
#     settings and PID namespaces change what is visible.
#   - The children of a process are not necessarily its cgroup or session; use
#     those interfaces when the question is about containment, not parentage.
# ==============================================
