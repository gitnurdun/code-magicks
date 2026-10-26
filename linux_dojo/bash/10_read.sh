#!/usr/bin/env bash
# MAGIC: `read -r` keeps backslashes literal, and `|| [[ -n $line ]]` processes
# a final line that has no trailing newline.
set -euo pipefail

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

printf 'one two\nthree four' > "$work/noeol"
while IFS= read -r line || [[ -n $line ]]; do
    printf 'line=<%s>\n' "$line"
done < "$work/noeol"

printf '%s\n' 'a\b' > "$work/backslash"
IFS= read -r raw < "$work/backslash"
IFS= read -r cooked < "$work/backslash"
printf 'raw=<%s>\n' "$raw"
printf 'cooked=<%s>\n' "$cooked"

printf '  padded value  \n' > "$work/spaces"
IFS= read -r padded < "$work/spaces"
printf 'padded=<%s>\n' "$padded"

# ================= EXPLANATION =================
# What it does
#   Reads a two-line file whose last line has no newline, compares `read -r`
#   with plain `read` on a backslash, and preserves leading/trailing spaces by
#   emptying IFS.
#
# Step by step
#   1. The file ends after "three four" with no newline. A plain `while read`
#      loop would discard that last partial line, because read returns nonzero
#      at EOF even though it filled the variable. The `|| [[ -n $line ]]` test
#      accepts the partial line and still stops on a truly empty EOF.
#   2. `IFS=` prevents leading/trailing whitespace from being stripped, and
#      `-r` prevents backslash processing.
#   3. Reading `a\b` with -r gives the two characters backslash and b.
#      Without -r, the backslash escapes b and is removed, so the value is ab.
#   4. Reading a line with two leading and two trailing spaces with IFS= keeps
#      them; the newline itself is still removed.
#   5. The temp directory is removed by the EXIT trap even if the loop fails.
#
# Output
#   line=<one two>
#   line=<three four>
#   raw=<a\b>
#   cooked=<ab>
#   padded=<  padded value  >
#
# Why it matters
#   Almost every "my script ate the last line" or "my backslashes vanished" bug
#   is a missing -r or a missing final-line test. The `IFS= read -r` loop is the
#   canonical safe way to read text line by line: it preserves the line's bytes
#   except the newline and does not glob or split it.
#
# Caveats
#   - `read` still removes the delimiter newline; it does not preserve it.
#   - `[[ -n $line ]]` is only a final-line guard. If the file ends with a
#     newline, the last read returns 1 with an empty line and the test fails, so
#     the loop ends correctly.
#   - A line containing only a NUL byte is not representable in a bash
#     variable; use `read -d ''` with a different delimiter or a binary tool.
#   - `-r` is not "raw mode" for all escapes; it only stops backslash acting as
#     an escape character in read itself.
#   - `IFS= read -r` reads one line, not one record; for arbitrary delimiters
#     use `read -r -d ''` in a loop.
#   - Redirect the loop's input, not the read inside the loop, or the whole
#     file may be consumed by the first command.
# ==============================================
