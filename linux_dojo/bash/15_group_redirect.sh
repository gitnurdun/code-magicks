#!/usr/bin/env bash
# MAGIC: one redirection can cover many commands, but braces stay in this shell,
# MAGIC: parentheses fork, and >(tee) is an asynchronous helper you must wait for.
set -euo pipefail

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

# Brace group: runs here, so x survives; the whole group writes the file.
x=before
{ x=inside; printf 'brace x=%s\n' "$x"; } > "$work/brace.txt"
printf 'after-brace x=%s\n' "$x"

# Subshell: same redirection, but the assignment happens in a child.
( x=sub; printf 'subshell x=%s\n' "$x" ) > "$work/sub.txt"
printf 'after-subshell x=%s\n' "$x"

# One redirection, several commands.
{ printf 'one\n'; printf 'two\n'; } > "$work/many.txt"
printf 'many-lines=%s\n' "$(wc -l < "$work/many.txt")"

# |& is bash's shorthand for 2>&1 | (bash 4+).
{ printf 'stdout-line\n'; printf 'stderr-line\n' >&2; } |& LC_ALL=C sort

# Process substitution: tee is a concurrent child, not a filter in this shell.
exec 3>&1
exec > >(tee "$work/tee.log")
printf 'captured one\n'
printf 'captured two\n'
exec 1>&3 3>&-
wait
printf 'tee-log:\n'
sed 's/^/  /' "$work/tee.log"

# ================= EXPLANATION =================
# What it does
#   Shows the three ways to put several commands behind one redirection:
#   a brace group in the current shell, a parenthesised subshell, and a
#   process substitution whose tee runs beside the script. It also uses |&
#   to feed both stdout and stderr into one pipe.
#
# Step by step
#   1. { ...; } > file runs its list in the current shell. x changes to
#      "inside" and stays changed after the group; only stdout is redirected.
#      The spaces after { and the ; before } are required syntax.
#   2. ( ... ) > file runs the list in a forked child. The child's x becomes
#      "sub", but the parent still sees "before"; the child's exit status is
#      the status of the parenthesised command.
#   3. The third group sends two printf calls through one file descriptor;
#      the shell opens the file once, not once per command.
#   4. |& is exactly 2>&1 |, so both streams enter sort. Merging loses the
#      original stream identity and any interleaving guarantee.
#   5. exec 3>&1 saves the real stdout. exec > >(tee log) replaces fd 1 with
#      the write end of a pipe to a concurrently running tee. tee copies to
#      the saved stdout and to the log.
#   6. exec 1>&3 3>&- restores stdout and closes the saved descriptor, so tee
#      sees EOF. wait then reaps the helper (bash 5.1+ reliably waits for
#      process substitutions), and only afterwards is the log read.
#
# Output
#   after-brace x=inside
#   after-subshell x=before
#   many-lines=2
#   stderr-line
#   stdout-line
#   captured one
#   captured two
#   tee-log:
#     captured one
#     captured two
#
# Why it matters
#   Redirection is a property of a file descriptor, not of a command. Grouping
#   lets a whole block share one open file, one pipe, or one lock. Process
#   substitution turns a filter into a side process while the script keeps its
#   own stdout, which is the standard way to log and display at the same time.
#
# Caveats
#   - Process substitution is asynchronous: tee starts at fork time, and the
#     script continues immediately. Close the write end and wait before reading
#     the log; without that, the log can be empty or partial. The shell does not
#     expose tee's exit status through exec, and a dead tee gives the writer
#     SIGPIPE/EPIPE.
#   - bash 5.1 added reliable `wait` for process substitutions. On bash 4.x/5.0,
#     `wait` may return before tee has flushed; use a named FIFO plus its PID
#     when older bash must be supported.
#   - The saved fd 3 is inherited by children unless marked close-on-exec.
#     Redirecting the whole shell with exec persists until explicitly restored.
#   - |& merges descriptors before the pipe, so ordering between the two
#     streams is not preserved; use separate pipes if stream identity matters.
#   - `tee` output can lag behind direct writes to the same terminal; wait for
#     it before printing a summary, as this script does.
# ==============================================
