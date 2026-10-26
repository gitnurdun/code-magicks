#!/usr/bin/env bash
# MAGIC: a trapped TERM stops the child, disarms itself against re-entry, and
# MAGIC: lets wait report 128+signal so the script can exit 143 on purpose.
set -euo pipefail

child=
on_signal() {
    trap '' INT TERM
    printf 'signal=%s\n' "$1"
    if [[ -n $child ]]; then
        kill -TERM "$child" 2>/dev/null || true
    fi
}
trap 'on_signal TERM' TERM
trap 'on_signal INT' INT

sleep 5 &
child=$!
printf 'waiting\n'
kill -TERM $$
if wait "$child"; then
    rc=0
else
    rc=$?
fi
printf 'child_stopped=yes status=%s\n' "$rc"
exit "$rc"

# ================= EXPLANATION =================
# What it does
#   Starts a long sleep, sends SIGTERM to the script itself, and handles it:
#   the trap stops the child, wait returns 128+15, and the script exits 143.
#   The trap ignores further INT/TERM while it runs, so it cannot re-enter.
#
# Step by step
#   1. The trap is installed before sleep starts. A caught signal is reset to
#      its default disposition across exec, so the child sleep still dies on
#      SIGTERM. (An ignored signal, trap '', would be inherited and survive.)
#   2. `sleep 5 &` creates the child and $! records its PID.
#   3. kill -TERM $$ delivers TERM to the shell. Because TERM has a handler,
#      bash runs it at the next command boundary instead of dying.
#   4. The handler first executes `trap '' INT TERM`, turning both signals into
#      ignored ones. A second signal now cannot invoke the handler again.
#   5. It kills the child. The shell then continues to wait, which reaps the
#      child and returns 143 (128 + SIGTERM). `wait` may instead be interrupted
#      by the signal and return 143 directly; both paths give the same status.
#   6. The script prints the normalized result and exits with the same 143.
#
# Output
#   waiting
#   signal=TERM
#   child_stopped=yes status=143
#
#   Exit status: 143.
#
# Why it matters
#   Supervisors and wrapper scripts must translate a termination request into
#   an orderly shutdown of their children. wait's 128+N convention is how the
#   shell reports "the child died from signal N", and the wrapper's own exit
#   status preserves that information for whatever started it.
#
# Caveats
#   - Signals are delivered asynchronously. The handler runs between commands,
#     not in the middle of printf, so long-running builtins delay it. A handler
#     must not assume the script is at a convenient point.
#   - Never re-arm the same handler before killing children or waiting: a second
#     signal can re-enter it and double-kill or deadlock. Ignore or block first.
#   - SIGKILL cannot be trapped, and SIGSTOP cannot be handled. A process stuck
#     in uninterruptible I/O may not run the handler until the syscall returns.
#   - Killing one PID does not kill its process tree. For a pipeline or a child
#     with its own children, use a process group and `kill -- -PGID`, or have the
#     child forward the signal.
#   - `wait` returning 143 does not prove the child is gone if the wait was
#     interrupted before the kill completed; reap with wait (or kill -0) before
#     declaring shutdown complete. Here the handler kills first and the main
#     wait reaps, so the printed status is stable.
# ==============================================
