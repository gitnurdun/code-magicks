#!/usr/bin/env bash
# MAGIC: `IFS=, read -ra` splits on an exact delimiter and preserves empty
# fields; the default IFS also trims runs of whitespace.
set -euo pipefail

line='alpha,,gamma  delta'
IFS=, read -ra parts <<< "$line"
printf 'csv_count=%d\n' "${#parts[@]}"
printf 'csv=<%s><%s><%s>\n' "${parts[0]}" "${parts[1]}" "${parts[2]}"

IFS=, read -ra lead <<< ",a"
printf 'leading_count=%d first=<%s>\n' "${#lead[@]}" "${lead[0]}"

IFS=, read -ra trail <<< "a,,b"
printf 'double_count=%d middle=<%s>\n' "${#trail[@]}" "${trail[1]}"

s='  one   two  three  '
read -r a b c <<< "$s"
printf 'default=<%s><%s><%s>\n' "$a" "$b" "$c"

IFS= read -r keep <<< '  keep  '
printf 'no_split=<%s>\n' "$keep"

# ================= EXPLANATION =================
# What it does
#   Splits CSV-like text with IFS=, showing that an empty field survives, then
#   shows how the default IFS collapses whitespace and how an empty IFS keeps a
#   line whole.
#
# Step by step
#   1. `IFS=,` applies only to the read builtin on that command. `read -ra
#      parts` splits the line into the array parts on commas.
#   2. "alpha,,gamma  delta" has three fields: alpha, an empty field, and
#      "gamma  delta". The two spaces inside the last field are preserved
#      because the delimiter is a comma, not whitespace.
#   3. A leading comma produces an empty first field; a doubled comma produces
#      an empty middle field. Both are real array elements, not skipped.
#   4. With the default IFS (space, tab, newline), read splits on runs of
#      whitespace and strips leading/trailing whitespace. The three leading and
#      two trailing spaces disappear, and the three internal spaces collapse.
#   5. `IFS=` disables splitting entirely, so `read -r keep` keeps the leading
#      and trailing spaces as part of the value.
#
# Output
#   csv_count=3
#   csv=<alpha><><gamma  delta>
#   leading_count=2 first=<>
#   double_count=3 middle=<>
#   default=<one><two><three>
#   no_split=<  keep  >
#
# Why it matters
#   IFS is the shell's field separator, and setting it per-command is the
#   cleanest way to parse delimited records without an external tool. Knowing
#   that IFS whitespace collapses while non-whitespace delimiters preserve empty
#   fields is what separates a correct parser from a lossy one.
#
# Caveats
#   - `IFS=, read ...` sets IFS only for that command. `IFS=,; read ...` changes
#     it for the rest of the shell, often a bug.
#   - Only the first character of IFS is used to join `"${a[*]}"`; all
#     characters are used for splitting.
#   - With IFS=, a trailing comma does not create a final empty field in bash's
#     read; handle the terminator explicitly if your format requires it.
#   - `read` strips the trailing newline and returns nonzero at EOF. In a loop,
#     use `|| [[ -n $line ]]` to catch a final line without a newline.
#   - Unquoted expansion still word-splits on IFS after read; quote your values.
#   - For real CSV (quoted commas, embedded newlines), IFS is the wrong tool;
#     use a CSV parser.
# ==============================================
