#!/usr/bin/env bash
# MAGIC: extglob adds @(...), +(...), ?(...), *(...) and !(...) to the pattern
# MAGIC: language, and the parser must know that before it reads those words.
set -euo pipefail
shopt -s extglob

show() {
    local pat="$1" str="$2" result=no
    if [[ $str == $pat ]]; then
        result=yes
    fi
    printf 'pattern=%s string=%s match=%s\n' "$pat" "${str:-<empty>}" "$result"
}

show '@(foo|bar)' foo
show '@(foo|bar)' bar
show '@(foo|bar)' baz
show '+(ab)' ab
show '+(ab)' abab
show '+(ab)' aba
show '?(x)' ''
show '?(x)' x
show '?(x)' xx
show '*(y)' ''
show '*(y)' y
show '*(y)' yyy
show '*(y)' yx
show '!(tmp|log)' tmp
show '!(tmp|log)' log
show '!(tmp|log)' data

for name in foo baz; do
    case "$name" in
        @(foo|bar)) printf 'case %s -> one-of\n' "$name" ;;
        *)          printf 'case %s -> fallback\n' "$name" ;;
    esac
done

# ================= EXPLANATION =================
# What it does
#   Enables extglob, then exercises each operator: @() exactly one alternative,
#   +() one or more, ?() zero or one, *() zero or more, and !() anything that
#   does not match. The same syntax is used in [[ == ]] and in a case pattern.
#
# Step by step
#   1. shopt -s extglob changes the parser's grammar for the rest of the shell.
#      It must run before the parser reads a compound command containing @(,
#      +(? or !( ; otherwise those characters are a syntax error.
#   2. show receives the pattern as a single-quoted word, so it is not expanded
#      as a filename when the function is called. Inside [[ ]], the unquoted
#      $pat on the right of == is a pattern, and the extglob matcher understands
#      the operators even though they arrived through a variable.
#   3. @(foo|bar) matches exactly foo or bar. + (ab) matches ab, abab, and so
#      on; aba fails because the trailing a is not a complete repetition.
#   4. ?(x) matches the empty string or x, but not xx. *(y) matches the empty
#      string, y, yyy, but not yx.
#   5. !(tmp|log) matches anything except the exact strings tmp and log, so
#      data matches while tmp and log do not.
#   6. The case loop uses a literal @(foo|bar) pattern. foo takes the first
#      branch; baz falls through to *. The pattern is parsed after the shopt
#      call above has taken effect.
#
# Output
#   pattern=@(foo|bar) string=foo match=yes
#   pattern=@(foo|bar) string=bar match=yes
#   pattern=@(foo|bar) string=baz match=no
#   pattern=+(ab) string=ab match=yes
#   pattern=+(ab) string=abab match=yes
#   pattern=+(ab) string=aba match=no
#   pattern=?(x) string=<empty> match=yes
#   pattern=?(x) string=x match=yes
#   pattern=?(x) string=xx match=no
#   pattern=*(y) string=<empty> match=yes
#   pattern=*(y) string=y match=yes
#   pattern=*(y) string=yyy match=yes
#   pattern=*(y) string=yx match=no
#   pattern=!(tmp|log) string=tmp match=no
#   pattern=!(tmp|log) string=log match=no
#   pattern=!(tmp|log) string=data match=yes
#   case foo -> one-of
#   case baz -> fallback
#
# Why it matters
#   Shell patterns are the native matching language of case, [[ ]], parameter
#   expansion, and filename generation. extglob brings alternation, repetition,
#   and negation to that language without invoking grep or a regex engine, so
#   argument validation and dispatch stay fast and dependency-free.
#
# Caveats
#   - extglob is a parse-time switch, not a run-time matcher flag. A function
#     whose body contains @(a|b) must be defined after the shopt call, or the
#     shell fails while parsing it. `bash -O extglob script` enables it before
#     any parsing; an interactive shell inherits an already-parsed line.
#   - The operators use shell pattern syntax, not POSIX extended regex: *
#     means any string, ? one character, and alternation is @(a|b), not (a|b).
#     Ranges use [a-z], not regex classes.
#   - Quoting a pattern makes it literal. To pass a pattern through a variable,
#     leave the right-hand side of [[ == ]] unquoted; quoting $pat compares
#     strings.
#   - extglob also affects pathname expansion, so leaving it on changes what
#     ordinary unquoted words mean. Enable it deliberately, near the top.
#   - A lone !( at the start of a word is not valid without extglob, and some
#     older bash builds parse it differently. State the bash 4+ requirement.
# ==============================================
