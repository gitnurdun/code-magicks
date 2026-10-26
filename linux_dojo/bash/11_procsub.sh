#!/usr/bin/env bash
# MAGIC: `<(cmd)` and `>(cmd)` are /dev/fd paths backed by a concurrently
# running process, so a command can read from or write to another command.
set -euo pipefail

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

if diff <(printf 'a\nb\nc\n') <(printf 'a\nB\nc\n') > "$work/diff"; then
    printf 'diff=identical\n'
else
    printf 'diff:\n'
    sed 's/^/  /' "$work/diff"
fi

printf 'paste:\n'
paste <(printf '1\n2\n') <(printf 'x\ny\n') | sed 's/^/  /'

printf 'tee-lines=' 
tee >(wc -l > "$work/lines") <<< $'a\nb\nc' >/dev/null
for _ in {1..100}; do
    [[ -s $work/lines ]] && break
    sleep 0.01
done
cat "$work/lines"

# ================= EXPLANATION =================
# What it does
#   Runs diff against two process substitutions, pastes two command outputs
#   side by side, and uses tee's process-substitution branch to count lines
#   while the main output is discarded.
#
# Step by step
#   1. `<(...)` starts a command with its stdout connected to a pipe and
#      substitutes a path such as /dev/fd/63. The consumer opens that path and
#      reads the command's output as if it were a file.
#   2. diff therefore compares two live streams, not two files on disk. It
#      exits 1 when they differ, so the `if` captures that status instead of
#      letting `set -e` abort.
#   3. `>(...)` is the reverse: the command's stdin is a pipe, and the path can
#      be written to. `tee >(wc -l > file)` writes the input both to stdout
#      (discarded) and to the wc process.
#   4. The wc process runs concurrently. The `for` loop waits for its output
#      file to appear and be non-empty before reading it. `wait` is not enough
#      on older bash versions, and even on newer ones the file can lag.
#   5. The temp directory is removed on exit.
#
# Output
#   diff:
#     2c2
#     < b
#     ---
#     > B
#   paste:
#     1	x
#     2	y
#   tee-lines=3
#
# Why it matters
#   Process substitution lets a program that insists on filenames participate
#   in a pipeline without a named FIFO or temp file. It is the clean way to
#   diff remote command output, to tee into a log filter, and to feed multiple
#   consumers from one producer. On Linux the /dev/fd entries resolve through
#   /proc/self/fd, which is why this is a kernel-backed trick.
#
# Caveats
#   - Bash-specific. POSIX sh has no process substitution; ksh and zsh have
#     their own variants with different lifetime rules.
#   - The path is a pipe, not a seekable file. Programs that lseek, stat for a
#     size, or reopen by path may fail or see a different process.
#   - Process substitution is asynchronous. Reading a file written by a
#     `>(...)` branch requires waiting for that process, not just for the
#     producer.
#   - A `>(...)` consumer that exits early can give the writer SIGPIPE/EPIPE.
#   - File descriptor numbers are not stable API; do not parse /dev/fd/63.
#   - If the consumer never opens the path, the producer can block on a full
#     pipe. Keep both sides moving.
# ==============================================
