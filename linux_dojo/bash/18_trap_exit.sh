#!/usr/bin/env bash
# MAGIC: an EXIT trap runs on every exit path, sees the pending status, and
# MAGIC: keeps that status unless the handler itself calls exit.
set -euo pipefail

work=$(mktemp -d)
cleaned=0
cleanup() {
    local st=$?
    if (( cleaned )); then
        printf 'cleanup again: no-op (status=%s)\n' "$st"
        return 0
    fi
    cleaned=1
    rm -rf -- "$work"
    printf 'cleanup=yes\n'
    printf 'cleanup saw status=%s\n' "$st"
    return 0
}
trap cleanup EXIT
trap -p EXIT

printf 'payload\n' > "$work/payload"
cleanup
printf 'manual-call done\n'

if ( trap 'exit 7' EXIT; exit 3 ); then
    printf 'override did not happen\n'
else
    printf 'trap-exit override status=%s\n' "$?"
fi

exit 3

# ================= EXPLANATION =================
# What it does
#   Installs a guarded cleanup routine as the EXIT trap, prints the installed
#   trap, calls the routine once by hand, and then leaves through `exit 3`.
#   The trap runs again at exit, does no work, and the script still exits 3.
#   A separate subshell shows that a trap which calls exit replaces the status.
#
# Step by step
#   1. mktemp -d creates a private directory; every later path is derived from
#      it, so the cleanup has one checked absolute path to remove.
#   2. `trap cleanup EXIT` registers the function for normal end, explicit exit,
#      and an uncaught `set -e` abort. `trap -p EXIT` shows the shell's stored
#      form, including the shell quoting.
#   3. The cleanup function reads $? first, before any command can clobber it.
#      On the manual call the pending status is 0; in the EXIT trap for exit 3
#      it is 3.
#   4. cleaned=1 is the idempotence guard. The manual call removes the tree and
#      prints cleanup=yes. When the trap is entered at exit, it skips the rm
#      and reports the no-op. This makes accidental double cleanup harmless.
#   5. The subshell installs its own trap that calls `exit 7`, then exits 3.
#      Because the handler calls exit, the subshell's status becomes 7. The
#      parent sees that failure and prints it from the else branch.
#   6. `exit 3` triggers the outer EXIT trap. The handler returns without
#      calling exit, so bash keeps the original status 3.
#
# Output
#   trap -- 'cleanup' EXIT
#   cleanup=yes
#   cleanup saw status=0
#   manual-call done
#   trap-exit override status=7
#   cleanup again: no-op (status=3)
#
#   Exit status: 3.
#
# Why it matters
#   Shell scripts leak temp directories, locks, and background processes when
#   they fail early. An EXIT trap is the one hook that covers normal return,
#   explicit exit, set -e aborts, and most fatal shell errors. Preserving $?
#   lets the caller still see the real failure.
#
# Caveats
#   - EXIT does not run for SIGKILL, and a signal that terminates the shell
#     still runs the EXIT trap only because bash handles the termination; use a
#     separate INT/TERM trap when the child process tree must be stopped.
#   - Calling `exit` inside the EXIT trap replaces the status (7 above) and can
#     recurse if the trap is re-armed incorrectly. Return instead of exiting.
#   - $? is only the pending status at handler entry; save it before any other
#     command, including `local`, runs.
#   - Keep the handler simple and idempotent. It can run while another command
#     is half finished, and a second call must not remove a live path twice.
#   - EXIT traps are not a substitute for a signal trap; an unhandled INT/TERM
#     ends the shell, and the trap then runs with status 128+signal.
# ==============================================
