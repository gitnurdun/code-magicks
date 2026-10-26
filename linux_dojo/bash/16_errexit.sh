#!/usr/bin/env bash
# MAGIC: set -e ignores failures in tested contexts, and `local x=$(false)`
# MAGIC: returns the status of local (zero), not of the substitution.
set -uo pipefail

demo() (
    set -e
    printf 'errexit on\n'

    if false; then :; fi
    printf 'continued: if\n'

    while false; do :; done
    printf 'continued: while\n'

    false || printf 'continued: or\n'
    false && printf 'unreachable\n'
    printf 'continued: and-not-last\n'

    ! false
    printf 'continued: bang\n'

    mask() {
        local v
        local v=$(false)
        printf 'continued: local-mask status=%s\n' "$?"
    }
    mask

    printf 'about to fail outside any test\n'
    false
    printf 'unreachable: after bare false\n'
)
demo
printf 'demo-status=%s\n' "$?"
printf 'script-status=0\n'

# ================= EXPLANATION =================
# What it does
#   Turns on errexit inside a throwaway subshell and walks through the places
#   where a failing command does not trigger it. The final bare false does
#   abort the subshell, proving that -e is active; because the parent never
#   enabled -e, the parent can report the subshell's status and exit 0.
#
# Step by step
#   1. demo is a subshell body, so its deliberate failure cannot kill the
#      script. Inside it, `set -e` is the default under test.
#   2. A command used as an if condition or a while/until condition is exempt:
#      its status is the thing being tested. Execution continues.
#   3. In a && or || list, every command except the one following the final
#      operator is exempt. `false || ...` and `false && ...` therefore survive;
#      the && branch is simply not run.
#   4. `! false` inverts the status, and a command whose status is inverted is
#      exempt.
#   5. `local v=$(false)` is the trap. The substitution fails, but the command
#      whose status errexit examines is the local builtin, which returns 0.
#      The failure disappears. Declare first, then assign on a separate line in
#      a context where the status is checked, so the failure stays visible.
#   6. A bare false outside every tested context finally aborts the subshell
#      with status 1. The parent prints that status and then exits 0.
#
# Output
#   errexit on
#   continued: if
#   continued: while
#   continued: or
#   continued: and-not-last
#   continued: bang
#   continued: local-mask status=0
#   about to fail outside any test
#   demo-status=1
#   script-status=0
#
# Why it matters
#   `set -e` is a shorthand for "abort on an unhandled simple-command failure",
#   not an error handler. It has no message, no cleanup, and no stack; it is
#   silently disabled across large parts of normal shell code. Scripts that
#   need reliable failure handling use explicit checks and an ERR or EXIT trap.
#
# Caveats
#   - The exemptions are structural, not value-based: a function called from an
#     if condition has errexit disabled throughout its body, even if it calls
#     `set -e` itself.
#   - `local`, `declare`, `export` and `readonly` with an assignment return
#     their own status and mask the command substitution's status. Declare
#     first, assign on a separate line.
#   - Pipelines without pipefail report only the last stage, so an earlier
#     failure is invisible to -e. With pipefail, any stage can abort.
#   - Command substitutions do not inherit errexit unless
#     `shopt -s inherit_errexit` (bash 4.4+) is set.
#   - A failing command inside a trap does not necessarily change the exit
#     status. Use ERR/EXIT traps plus explicit checks for cleanup and reporting.
# ==============================================
