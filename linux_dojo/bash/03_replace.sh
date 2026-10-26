#!/usr/bin/env bash
# MAGIC: parameter substitution is a glob-based search and replace with
# anchoring, first/all control, and no external sed process.
set -euo pipefail

v='my report / final (v2).txt'

printf 'original=%s\n' "$v"
printf 'space_to_underscore=%s\n' "${v// /_}"
printf 'first_slash=%s\n' "${v/\//-}"
printf 'all_slash=%s\n' "${v//\//-}"
printf 'prefix=%s\n' "${v/#my/YOUR}"
printf 'suffix=%s\n' "${v/%.txt/.md}"
printf 'sanitized=%s\n' "${v//[^A-Za-z0-9._-]/_}"

# ================= EXPLANATION =================
# What it does
#   Rewrites one string six ways: spaces to underscores, the first slash to a
#   dash, every slash to a dash, an anchored prefix and suffix replacement, and
#   a whitelist sanitizer. All of it is parameter expansion.
#
# Step by step
#   1. `${v// /_}` replaces every space. One slash after the variable means
#      first match only; two mean all matches.
#   2. The pattern is a glob, so `/` is literal and needs no escape in a glob;
#      inside `${v/pattern/repl}` the slash separates pattern from replacement,
#      so it is written `\/`.
#   3. `${v/#my/YOUR}` anchors the pattern at the start; `${v/%.txt/.md}`
#      anchors it at the end. `#` and `%` here are anchors, not the length/trim
#      operators.
#   4. `${v//[^A-Za-z0-9._-]/_}` treats the bracket expression as a glob
#      character class and replaces each character outside the whitelist with
#      an underscore. This is a filename sanitizer, not a full path validator.
#   5. Every expansion is quoted so the result is one word even after the
#      spaces become underscores or the string contains glob characters.
#
# Output
#   original=my report / final (v2).txt
#   space_to_underscore=my_report_/_final_(v2).txt
#   first_slash=my report - final (v2).txt
#   all_slash=my report - final (v2).txt
#   prefix=YOUR report / final (v2).txt
#   suffix=my report / final (v2).md
#   sanitized=my_report___final__v2_.txt
#
# Why it matters
#   Substitution handles the common log/name cleanup cases without forking
#   sed/tr, and it composes with the trim operators. It is also the mechanism
#   behind many "safe filename" helpers. The anchored forms are a compact way
#   to change an extension or a known prefix.
#
# Caveats
#   - The pattern is a glob, not a regex. `.` is literal, `*` is a wildcard,
#     and bracket classes follow glob rules. Do not paste a regex here.
#   - `${v//x/y}` replaces overlapping matches left to right; it is not a
#     recursive or simultaneous rewrite.
#   - The replacement is literal except for `&`, which expands to the matched
#     text in bash. Escape it as `\&` when you want a literal ampersand.
#   - A leading `#` or `%` in the pattern is an anchor only in the position
#     shown; a pattern that starts with a literal `#` needs care.
#   - For multibyte locales, character classes operate on characters, but the
#     bracket expression's range is locale-dependent. `LC_ALL=C` gives bytes.
#   - Sanitizing is not the same as validating. `..`, a leading `-`, or a
#     reserved name can survive this whitelist; reject those explicitly.
# ==============================================
