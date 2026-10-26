#!/usr/bin/env bash
# MAGIC: /proc/<pid>/fd is a directory of magic links; an unlinked file stays
# MAGIC: reachable through its descriptor and the link grows a "(deleted)" tag.
set -euo pipefail

[[ -d /proc/$$/fd ]] || { printf 'skipped: /proc/<pid>/fd not available\n'; exit 0; }
command -v readlink >/dev/null 2>&1 || { printf 'skipped: readlink not available\n'; exit 0; }

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

keep=$work/kept.txt
printf 'kept\n' > "$keep"
exec 9<"$keep"

# Compare device+inode of the descriptor target with the path, not strings.
if [[ "/proc/$$/fd/9" -ef "$keep" ]]; then
    printf 'fd9_points_to_file=yes\n'
else
    printf 'fd9_points_to_file=no\n'
fi

# 0, 1, 2, this descriptor, and possibly bash's script-descriptor slot.
count=$(ls -1 "/proc/$$/fd" | wc -l)
if (( count >= 4 )); then
    printf 'fd_count_ge_4=yes\n'
else
    printf 'fd_count_ge_4=no\n'
fi

gone=$work/gone.txt
printf 'gone\n' > "$gone"
exec 8<"$gone"
rm -f -- "$gone"
if [[ "$(readlink "/proc/$$/fd/8")" == *" (deleted)" ]]; then
    printf 'deleted_marker=yes\n'
else
    printf 'deleted_marker=no\n'
fi

# In bash, /proc/self resolves to the shell, so both names reach the same dir.
if [[ /proc/self/fd -ef "/proc/$$/fd" ]]; then
    printf 'proc_self_fd_is_current_fd_dir=yes\n'
else
    printf 'proc_self_fd_is_current_fd_dir=no\n'
fi

exec 9<&- 8<&-

# ================= EXPLANATION =================
# What it does
#   Inspects the file-descriptor directory of the running shell: opens a known
#   file on fd 9, counts the entries, opens another file and unlinks it while
#   the descriptor is still open, and finally checks that /proc/self/fd is the
#   shell's own descriptor directory. Only booleans are printed, because the
#   temp paths and the entry count vary by shell and caller.
#
# Step by step
#   1. /proc/<pid>/fd is a directory whose entries are symbolic links named by
#      descriptor number. Each target is the path the file was opened with.
#   2. exec 9<file opens the file read-only in the current shell, so it is the
#      shell's fd 9, not a child's. stat("/proc/$$/fd/9") follows the magic link
#      and reaches the same inode as the original path, so -ef is true.
#   3. Listing the directory shows at least stdin, stdout, stderr, and fd 9.
#      bash may also hold the script file open on a high descriptor (often 255),
#      so the exact count is not stable; only "at least four" is asserted.
#   4. Opening a file and then unlinking it leaves the inode alive because the
#      descriptor is a reference. The kernel appends " (deleted)" to the link
#      target so the path is visibly stale even though the fd still works.
#   5. The /proc/self symlink is magic: it resolves relative to whichever
#      process performs the lookup. Bash's [[ -ef ]] stat call is made by the
#      shell itself, so /proc/self/fd is /proc/$$/fd and the inode comparison
#      succeeds.
#   6. Closing fds 8 and 9 drops the last references; the unlinked file's blocks
#      are freed then, and the temp directory is removed by the EXIT trap.
#
# Output
#   fd9_points_to_file=yes
#   fd_count_ge_4=yes
#   deleted_marker=yes
#   proc_self_fd_is_current_fd_dir=yes
#
# Why it matters
#   This directory is how lsof, ss, and debuggers map sockets and files back to
#   processes, and it is the standard way to recover a file that was deleted
#   while a daemon still had it open: open /proc/<pid>/fd/N or copy through it.
#   It also explains why `df` can show no space freed after rm: the inode is
#   still referenced by a descriptor.
#
# Caveats
#   - The listing and any later readlink are separate snapshots. A process can
#     close an fd and reuse the number in between, so /proc/<pid>/fd is racy for
#     other processes. This script examines its own single-threaded shell, which
#     keeps the window closed.
#   - Reading another user's /proc/<pid>/fd needs ptrace-style credentials;
#     hidepid= mounts can hide the directory entirely.
#   - The " (deleted)" suffix is a kernel convention, not part of the path. A
#     file can also be renamed or replaced and the old path will no longer name
#     the inode the descriptor holds.
#   - The entry count includes descriptors inherited from the caller and bash
#     internals; never assert an exact number.
#   - The deleted file's space is reclaimed only when every descriptor and
#     every hard link is gone.
# ==============================================
