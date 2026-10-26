#!/usr/bin/env bash
# MAGIC: `exec {fd}<>file` lets bash pick a descriptor above 10, and
# `exec 3<&0` is a save/restore of an existing stream.
set -euo pipefail

work=$(mktemp)
trap 'rm -f -- "$work"' EXIT

exec {fd}<>"$work"
printf 'fd_gt_10=%s\n' "$([[ $fd -gt 10 ]] && echo yes || echo no)"
printf 'first line\n' >&"$fd"

exec {rfd}<"$work"
IFS= read -r line <&"$rfd"
printf 'content=%s\n' "$line"
exec {rfd}<&-
exec {fd}>&-

exec 3<&0
printf 'saved_fd3=%s\n' "$([[ -e /dev/fd/3 ]] && echo yes || echo no)"
exec 0<&3 3<&-
printf 'restored_stdin=%s\n' "$([[ -e /dev/fd/0 ]] && echo yes || echo no)"

# ================= EXPLANATION =================
# What it does
#   Opens one file read-write on a bash-allocated descriptor above 10, writes a
#   line, reads it back through a second allocated descriptor, then saves and
#   restores stdin on fd 3.
#
# Step by step
#   1. `exec {fd}<>"$work"` opens the file for reading and writing. The braces
#      make bash choose a free descriptor >= 10 and store its number in fd. The
#      redirection persists because exec applies it to the shell, with no
#      command.
#   2. `printf ... >&"$fd"` writes at the current offset (0). The `<>` open is
#      important: `>` alone would truncate and be write-only.
#   3. A second allocation `{rfd}<` opens the same path for reading at offset 0,
#      so the data just written is visible without a seek.
#   4. `exec {rfd}<&-` and `exec {fd}>&-` close the allocated descriptors. The
#      brace syntax works for closing too, so the script does not need to know
#      the numbers.
#   5. `exec 3<&0` duplicates stdin onto fd 3. The test checks that /dev/fd/3
#      exists. `exec 0<&3 3<&-` restores stdin and closes the saved copy.
#   6. The final test distinguishes a tty from a pipe/closed fd without
#      consuming input.
#
# Output
#   fd_gt_10=yes
#   content=first line
#   saved_fd3=yes
#   restored_stdin=yes
#
# Why it matters
#   Dynamic descriptors free the script from picking numbers and colliding with
#   inherited fds. The save/restore idiom is how a function can temporarily
#   redirect stdin or stdout without leaking the change to its caller. Combined
#   with a lock or a socket, it is the basis of shell resource management.
#
# Caveats
#   - `{var}` allocation is bash 4.1+. The descriptor is not close-on-exec by
#     default, so children inherit it; close it or use `{var}>&-` before exec.
#   - `<>` does not truncate, but the file offset is shared by the descriptor.
#     Reads and writes advance the same offset.
#   - A second open of the same file has an independent offset; do not assume
#     the first descriptor's position carries over.
#   - The `/dev/fd/3` test is Linux-specific (it resolves through /proc). On
#     other systems use `[[ -e /dev/fd/3 ]]` only if the symlink exists.
#   - Restoring stdin is not the same as restoring terminal state. Use `stty -g`
#     and `stty "$saved"` for echo/canonical-mode changes.
#   - If the script is run with fd 3 already open, this clobbers it. Save and
#     restore 3 as well, or use a dynamic descriptor for the saved copy.
# ==============================================
