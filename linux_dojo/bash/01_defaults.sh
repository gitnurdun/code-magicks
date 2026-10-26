#!/usr/bin/env bash
# MAGIC: the colon changes whether "empty" counts as missing, in all four
# default/assign/error/alternate parameter expansions.
set -uo pipefail

show() { printf '%-5s = <%s>\n' "$1" "$2"; }

unset u
e=""
v="value"

show 'u:-' "${u:-fallback}"   # unset  -> fallback
show 'u-'  "${u-fallback}"    # unset  -> fallback
show 'e:-' "${e:-fallback}"   # empty  -> fallback
show 'e-'  "${e-fallback}"    # empty  -> empty (set)
show 'v:-' "${v:-fallback}"
show 'v:+' "${v:+set}"
show 'e:+' "${e:+set}"
show 'u:+' "${u:+set}"

[[ -z ${u+x} ]] && echo 'u_is_unset=yes'
[[ -n ${e+x} ]] && echo 'e_is_set=yes'

x=${u:=assigned}
printf 'after_assign u=<%s> x=<%s>\n' "$u" "$x"
y=${e=assigned}
printf 'after_noassign e=<%s> y=<%s>\n' "$e" "$y"

if ( unset z; : "${z:?missing}" ) 2>/dev/null; then
    echo 'colon_question=no-error'
else
    echo 'colon_question=error'
fi
if ( e2=""; : "${e2?ok}" ) 2>/dev/null; then
    echo 'plain_question_empty=ok'
else
    echo 'plain_question_empty=error'
fi

# ================= EXPLANATION =================
# What it does
#   Exercises the four parameter-expansion operators (default, assign, error,
#   alternate) with and without the colon, on unset, empty, and non-empty
#   variables. The output is the difference table.
#
# Step by step
#   1. `u` is unset, `e` is set to the empty string, `v` is non-empty.
#   2. `${u:-fallback}` and `${u-fallback}` both yield fallback, because unset
#      is missing under either rule.
#   3. `${e:-fallback}` yields fallback but `${e-fallback}` yields empty: the
#      colon makes an empty value count as missing.
#   4. `:=` and `=` assign the fallback when the test succeeds. `${u:=...}`
#      writes to u; `${e=...}` does not, because e is set (to empty).
#   5. `:+` substitutes only when the value is non-empty (with colon) or set
#      (without). `${e:+set}` is empty; `${u:+set}` is empty too.
#   6. `${z:?message}` aborts the current shell with an error when z is unset.
#      `${e2?message}` does not, because e2 is set even though it is empty.
#
# Output
#   u:-   = <fallback>
#   u-    = <fallback>
#   e:-   = <fallback>
#   e-    = <>
#   v:-   = <value>
#   v:+   = <set>
#   e:+   = <>
#   u:+   = <>
#   u_is_unset=yes
#   e_is_set=yes
#   after_assign u=<assigned> x=<assigned>
#   after_noassign e=<> y=<>
#   colon_question=error
#   plain_question_empty=ok
#
# Why it matters
#   These four operators are bash's null-coalescing, default-assignment, assert,
#   and conditional-value primitives. The colon is the difference between
#   "missing" and "present but empty", which is exactly the distinction that
#   configuration parsing needs: an empty override is usually a deliberate
#   value, not an absent one.
#
# Caveats
#   - `${var:?}` exits a non-interactive shell. In a function it unwinds the
#     whole shell, not just the call; use an explicit `if` when that is too
#     abrupt.
#   - Word splitting and globbing apply to the expansion result only when it is
#     unquoted. Quote it.
#   - `${var=word}` assigns the literal word; `${var:=$(cmd)}` runs cmd only
#     when the assignment is needed.
#   - Under `set -u`, `${var}` is an error but `${var:-}` and `${var-}` are
#     safe. This is why defensive scripts use the colon forms.
#   - Bash expands the word lazily, so side effects in the default value only
#     happen on the taken branch.
# ==============================================
