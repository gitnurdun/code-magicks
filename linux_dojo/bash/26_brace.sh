#!/usr/bin/env bash
# MAGIC: brace expansion is textual and happens before parameter expansion, so
# MAGIC: {1..$n} is literal; seq takes a variable, eval needs validation.
set -euo pipefail

digits=({1..5})
printf 'digits: %s\n' "${digits[*]}"

stepped=({01..10..3})
printf 'stepped: %s\n' "${stepped[*]}"

letters=({a..e})
printf 'letters: %s\n' "${letters[*]}"

product=({a,b}{1,2})
printf 'product: %s\n' "${product[*]}"

n=5
printf 'literal: %s\n' "{1..$n}"
printf 'seq: %s\n' "$(seq -s ' ' 1 "$n")"

if [[ $n =~ ^[0-9]+$ ]]; then
    eval "range=({1..$n})"
    printf 'eval: %s\n' "${range[*]}"
else
    printf 'eval refused: n is not a decimal integer\n'
fi

# ================= EXPLANATION =================
# What it does
#   Prints the common brace forms: a numeric range, a zero-padded stepped
#   range, a letter range, and a Cartesian product. Then it shows the trap:
#   `{1..$n}` does not become a range, and demonstrates seq and a validated
#   eval as the variable-range alternatives.
#
# Step by step
#   1. `{1..5}` expands to 1 2 3 4 5 as separate words, before any command
#      runs. The array assignment captures the five words.
#   2. `{01..10..3}` starts at 01, advances by 3, keeps the width, and stops
#      at 10: 01 04 07 10. `{a..e}` is the character sequence. These forms are
#      bash 4+.
#   3. `{a,b}{1,2}` expands left-to-right: for each first alternative, expand
#      the second, giving a1 a2 b1 b2.
#   4. `"{1..$n}"`: brace expansion runs before parameter expansion, and at
#      that stage the endpoint is the literal text `$n`, not an integer. No
#      range is formed. $n is expanded afterwards, leaving the literal string
#      `{1..5}`; braces are not rescanned.
#   5. seq builds the range at run time from the value of n. It is the clearest
#      choice for a numeric range with a variable end; -s is GNU coreutils.
#   6. eval re-parses a string as shell input, so `{1..$n}` is assembled and
#      then parsed as a brace range. The guard accepts only digits; without it
#      a value such as `1..5}; rm -rf ~; {` would be executed as shell code.
#
# Output
#   digits: 1 2 3 4 5
#   stepped: 01 04 07 10
#   letters: a b c d e
#   product: a1 a2 b1 b2
#   literal: {1..5}
#   seq: 1 2 3 4 5
#   eval: 1 2 3 4 5
#
# Why it matters
#   Brace expansion is one of the few pure-syntax expansions: it happens before
#   variables exist, and it never invokes the shell again. That distinction
#   explains both why it is fast and predictable and why it cannot use a
#   variable as an endpoint. seq and careful eval are the two standard ways to
#   get a run-time range.
#
# Caveats
#   - Brace expansion happens before parameter, command, and arithmetic
#     substitution and before word splitting. There is no second pass.
#   - Quoting suppresses it: `"{1..5}"` is the literal seven-character string.
#   - The step and zero-padding syntax is bash 4+; older shells may treat the
#     braces literally or reject the step.
#   - eval is code execution. Validate the entire value against a strict
#     pattern, prefer seq for numbers, and never interpolate user text. Note
#     that seq itself can treat a leading-dash operand as an option; use
#     `seq -- 1 "$n"` when n is untrusted.
#   - Empty brace lists such as `{,}` produce empty words, which can surprise
#     argument counts. Brace expansion is not globbing and does not touch the
#     filesystem.
# ==============================================
