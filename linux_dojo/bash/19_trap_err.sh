#!/usr/bin/env bash
# MAGIC: ERR fires only for untested failures; errtrace carries the trap into
# MAGIC: subshells, and || true marks a failure as handled.
set -uo pipefail
set -E

handler() {
    local fn=${FUNCNAME[1]:-main}
    local src=${BASH_SOURCE[1]:-${BASH_SOURCE[0]}}
    printf 'ERR status=%s func=%s source=%s line=%s\n' \
        "$1" "$fn" "${src##*/}" "${BASH_LINENO[0]}"
}
# Install, then fail inside an inherited subshell on the same source line.
trap 'handler "$?"' ERR; ( false; exit 0 )
printf 'subshell done\n'

false || true
printf 'survived: false || true\n'

printf 'done\n'

# ================= EXPLANATION =================
# What it does
#   Installs an ERR trap that reports the failing status, the enclosing
#   function, the source file, and the line. It demonstrates that `false` on
#   the left of `||` does not trigger ERR, while the same failure inside a
#   subshell does, because errtrace propagates the trap. The failing command is
#   followed by `exit 0` in the subshell, so the script finishes successfully.
#
# Step by step
#   1. `set -E` is the same option as `set -o errtrace`: an ERR trap is
#      inherited by shell functions, command substitutions, and subshells.
#      Without it, the trap would not fire inside ( ... ).
#   2. handler captures $? first, then reads the call stack. FUNCNAME[1] names
#      the function or main context where the trap fired; BASH_SOURCE[1] is the
#      corresponding file; BASH_LINENO[0] is the command's source line.
#   3. The trap is installed and the subshell is launched on one line. The
#      child inherits the trap through -E; false fails, ERR fires, and
#      execution continues to `exit 0`, so the child itself is successful and
#      no second trap runs in the parent. Both the installation and the failing
#      command are on line 14, so the reported number is unambiguous.
#   4. `false || true`: ERR follows the same exemptions as set -e, so a command
#      in a || list that is not the final command does not trigger the trap.
#      The failure is treated as handled.
#   5. The parent never enabled errexit, so it reaches its own final printf and
#      exits 0. The ERR trap is reporting, not terminating.
#
# Output
#   ERR status=1 func=main source=19_trap_err.sh line=14
#   subshell done
#   survived: false || true
#   done
#
# Why it matters
#   ERR traps add diagnostics to code that already continues after failures.
#   errtrace is what makes that diagnostics reach into helper functions and
#   command substitutions instead of silently stopping at the function boundary.
#   A stack-aware handler turns a bare non-zero status into a usable location.
#
# Caveats
#   - ERR is not a signal. It fires after a simple command or pipeline fails,
#     under the same rules that suppress set -e. A command in an if/while test,
#     a ||/&& list (except the final command), or after ! will not fire it.
#   - `|| true` does not "catch" the failure for code that runs earlier inside
#     the command; it only makes that command's result a handled one.
#   - The trap inherits into functions only with -E/errtrace, and into command
#     substitutions only with that plus the shell being non-interactive. A
#     subshell's inherited trap can report an error that the parent never sees.
#   - BASH_LINENO is a source line, not a syscall location. It becomes
#     misleading with eval, sourced files, and generated code; print
#     BASH_SOURCE too, not just the number.
#   - The handler must be short and must not fail: a failing handler can change
#     the status or recurse into the trap. Redirect diagnostics to stderr in
#     real scripts so stdout stays machine-readable.
# ==============================================
