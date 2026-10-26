#!/usr/bin/env bash
# MAGIC: redirections are applied left to right; `>file 2>&1` sends both
# streams to the file, while `2>&1 >file` sends only stdout there.
set -euo pipefail

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

demo() {
    printf 'stdout\n'
    printf 'stderr\n' >&2
}

demo > "$work/a" 2>&1

# 2>&1 first duplicates the group's stdout (the terminal), then >file moves
# stdout. The outer 2> captures the stderr that stayed on the terminal.
{ demo 2>&1 > "$work/b"; } 2> "$work/b.err"

printf 'a:\n'
sed 's/^/  /' "$work/a"
printf 'b:\n'
sed 's/^/  /' "$work/b"
printf 'b_err:\n'
sed 's/^/  /' "$work/b.err"
printf 'merged:\n'
{ demo; } |& sort

# ================= EXPLANATION =================
# What it does
#   Runs one function that writes to stdout and stderr under three redirection
#   orders, then prints the resulting files and the `|&` merge.
#
# Step by step
#   1. `demo > a 2>&1`: bash opens a for stdout, then duplicates fd 2 from the
#      new stdout. Both streams point at a, so a contains "stdout" then
#      "stderr" in write order.
#   2. `demo 2>&1 > b`: fd 2 is duplicated from the current stdout (the
#      terminal) first, then fd 1 is redirected to b. stdout goes to b; stderr
#      still goes to the terminal. The outer `2> b.err` on the brace group
#      captures that terminal stderr, making the split visible.
#   3. Order matters because each redirection mutates the descriptor table as
#      it is parsed, not as the command runs.
#   4. `|&` is shorthand for `2>&1 |`: stderr is duplicated onto the pipe's
#      write end before the pipe is connected, so both streams enter sort.
#   5. sort's output shows that the two lines were merged into one stream; the
#      original stream identity is gone.
#
# Output
#   a:
#     stdout
#     stderr
#   b:
#     stdout
#   b_err:
#     stderr
#   merged:
#     stderr
#     stdout
#
# Why it matters
#   This is the most common redirection bug in shell scripts: logging stdout
#   and stderr to a file looks like it works, but the error output still hits
#   the terminal because the order was reversed. Understanding the left-to-right
#   descriptor duplication is also required for `exec 3>&1 1>file 2>&3`
#   save/swap tricks.
#
# Caveats
#   - `2>&1` duplicates the descriptor at that moment. Moving stdout later does
#     not move the earlier copy.
#   - `&>` and `>file 2>&1` are equivalent in bash, but `&>` is not POSIX.
#   - `|&` merges stderr before the pipe, so a downstream consumer cannot tell
#     which stream a line came from. Use process substitution or separate pipes
#     when the distinction matters.
#   - Buffering can reorder the two streams in a file even when the write calls
#     are ordered; stdout is block-buffered to a file, stderr is unbuffered.
#   - `exec` redirections persist for the rest of the shell; a command
#     redirection lasts only for that command.
# ==============================================
