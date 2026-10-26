#!/usr/bin/env bash
# MAGIC: RLIMIT_NOFILE has a soft ceiling and a hard ceiling; a process may lower its soft limit, and then new opens fail with EMFILE.
set -euo pipefail
export LC_ALL=C

soft=$(ulimit -Sn)
hard=$(ulimit -Hn)
if [[ $soft == unlimited || $hard == unlimited ]] || (( soft <= hard )); then
  within=yes
else
  within=no
fi
printf 'soft_within_hard=%s\n' "$within"

(
  # Lower only the soft limit; plain 'ulimit -n 16' sets both.
  if ! ulimit -Sn 16 2>/dev/null; then
    printf 'skipped: cannot lower soft nofile limit\n'
    exit 0
  fi
  printf 'child_soft=16\n'

  hit=no
  {
    for _ in {1..100}; do
      if ! exec {fd}<>/dev/null; then
        hit=yes
        break
      fi
    done
  } 2>/dev/null
  printf 'hit_emfile=%s\n' "$hit"
)

printf 'parent_unchanged=%s\n' "$([[ $(ulimit -Sn) == "$soft" ]] && echo yes || echo no)"

# ================= EXPLANATION =================
# What it does
#   Reads the soft and hard RLIMIT_NOFILE values, lowers the soft limit
#   to 16 in a subshell, opens /dev/null until the kernel returns
#   EMFILE, and then checks that the parent shell's soft limit is still
#   the original value.
#
# Step by step
#   1. ulimit -Sn prints the soft open-file limit; -Hn prints the hard
#      limit. Numeric comparison, or an "unlimited" result, decides
#      whether the soft limit is within the hard ceiling.
#   2. The parentheses create a subshell. Limits are per-process, so
#      anything changed inside is invisible after it exits.
#   3. ulimit -Sn 16 lowers only the soft limit. Lowering is always
#      allowed; it does not need privilege.
#   4. exec {fd}<>/dev/null asks bash for the next free fd >= 10 and
#      opens /dev/null on it. The loop keeps the fds open; with a limit
#      of 16, new descriptor numbers must stay below 16, so allocation
#      eventually fails.
#   5. The failure is EMFILE ("too many open files"). A tiny limit is
#      deliberate; the loop stops at the first failure and reports it.
#   6. Back in the parent, ulimit -Sn still matches the saved value.
#
# Output
#   soft_within_hard=yes
#   child_soft=16
#   hit_emfile=yes
#   parent_unchanged=yes
#
# Why it matters
#   File descriptors are a bounded per-process resource. Leaking them
#   eventually makes accept, open or pipe fail in a working program.
#   Limits also explain why a daemon under systemd, a container or a
#   shell may have very different maximums from an interactive login.
#
# Caveats
#   - The soft limit may be raised only up to the hard limit. An
#     unprivileged process cannot raise the hard limit; root with
#     CAP_SYS_RESOURCE can, but root in a restricted container may not.
#   - Limits are inherited across fork and exec. A child that lowers
#     its own limit cannot restore it after the hard limit is also
#     reduced; use -S so only the soft limit changes.
#   - The subshell only demonstrates isolation. A real worker must set
#     its limit before it starts opening files.
#   - Fds already open above the new soft limit stay usable; the limit
#     constrains new descriptor numbers. Bash itself keeps its script
#     fd at 255, which is why some fds can exceed a limit of 16.
#   - EMFILE is per-process; ENFILE means the system-wide table is
#     full. A per-process ulimit does not raise the system-wide
#     fs.file-max limit.
#   - Other rlimits (nproc, stack, core, memlock) are separate and have
#     their own privilege rules.
# ==============================================
