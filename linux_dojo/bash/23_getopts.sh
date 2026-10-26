#!/usr/bin/env bash
# MAGIC: getopts is a small state machine over short options, with OPTIND and
# MAGIC: OPTARG as its state; a leading ':' in the spec makes errors reportable.
set -euo pipefail

parse() {
    local opt a=0 b=0 c=
    OPTIND=1
    while getopts ':abc:' opt; do
        case "$opt" in
            a) a=1; printf 'opt=a OPTARG=%s\n' "${OPTARG:-<none>}" ;;
            b) b=1; printf 'opt=b OPTARG=%s\n' "${OPTARG:-<none>}" ;;
            c) c=$OPTARG; printf 'opt=c OPTIND=%s OPTARG=%s\n' "$OPTIND" "$OPTARG" ;;
            :) printf 'missing argument for -%s (OPTARG=%s)\n' "$OPTARG" "$OPTARG"; return 2 ;;
            \?) printf 'unknown option (OPTARG=%s)\n' "${OPTARG:-<none>}"; return 2 ;;
        esac
    done
    shift $((OPTIND - 1))
    printf 'parsed a=%s b=%s c=%s rest=[%s] OPTIND=%s\n' "$a" "$b" "$c" "$*" "$OPTIND"
}

parse -a -b -c value
printf 'after first: OPTIND=%s\n' "$OPTIND"

parse -ab -- -c
printf 'after combined: OPTIND=%s\n' "$OPTIND"

parse -a
printf 'reparse: OPTIND=%s\n' "$OPTIND"

if parse -c; then
    printf 'unexpected success\n'
else
    printf 'missing-arg status=%s shell-alive=yes\n' "$?"
fi

# ================= EXPLANATION =================
# What it does
#   Parses separate, combined, and valued short options, stops at --, prints
#   OPTIND/OPTARG where they change, resets OPTIND and parses again, and reports
#   a missing option argument without terminating the shell.
#
# Step by step
#   1. getopts is given `:abc:`. The leading colon selects silent mode: errors
#      are returned as ':' (missing argument) or '?' (unknown option) rather
#      than printed, and OPTARG names the offending option.
#   2. The a and b cases set flags and show the unset OPTARG. In the c case
#      OPTARG carries the value and OPTIND has advanced past both -c and value.
#      The final summary prints the OPTIND at which the loop stopped.
#   3. getopts stops at the first non-option and at --. `-ab -- -c` consumes
#      both bundled flags and the terminator; -c is left as an operand, not
#      parsed again. shift $((OPTIND-1)) removes only the parsed options.
#   4. OPTIND is global state: getopts never resets it. The function assigns
#      OPTIND=1 at entry, which is what makes the third call start over.
#   5. getopts reads the current function's positional parameters. parse -c
#      has only -c and no argument, so it returns ':' with OPTARG=c. The
#      function returns 2; the caller's if keeps that failure from reaching
#      set -e, and the shell stays alive.
#
# Output
#   opt=a OPTARG=<none>
#   opt=b OPTARG=<none>
#   opt=c OPTIND=5 OPTARG=value
#   parsed a=1 b=1 c=value rest=[] OPTIND=5
#   after first: OPTIND=5
#   opt=a OPTARG=<none>
#   opt=b OPTARG=<none>
#   parsed a=1 b=1 c= rest=[-c] OPTIND=3
#   after combined: OPTIND=3
#   opt=a OPTARG=<none>
#   parsed a=1 b=0 c= rest=[] OPTIND=2
#   reparse: OPTIND=2
#   missing argument for -c (OPTARG=c)
#   missing-arg status=2 shell-alive=yes
#
# Why it matters
#   Option parsing is where a script's interface lives. getopts is portable,
#   needs no external tool, supports bundling and attached values, and leaves
#   operands untouched after --. Doing it in a function keeps the parser's
#   state and the caller's positional parameters separate.
#
# Caveats
#   - getopts has no long-option support. --verbose is not parsed; -- alone is
#     the end-of-options marker. Long options need a hand-written loop or a
#     different tool.
#   - It stops at the first non-option; unlike GNU getopt, it does not permute
#     options past operands. Decide where -- belongs and document it.
#   - OPTIND is global. Reset it before each independent parse, and do not
#     expect a local declaration to isolate it. OPTARG is unset for flags.
#   - Missing arguments and unknown options return a status; with set -e an
#     unguarded call exits the script. Handle the ':'/'?' cases and return a
#     status the caller tests.
#   - OPTARG for an attached value (-cvalue) works, but a value beginning with
#     '-' can be mistaken for an option unless attached or preceded by --.
# ==============================================
