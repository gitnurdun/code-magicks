#!/usr/bin/env bash
# MAGIC: an associative array counts words in one pass, and `[[ -v map[key] ]]`
# tests membership without expanding the value.
set -euo pipefail

declare -A count
text='the quick brown fox jumps over the lazy dog the fox'

for word in $text; do          # intentional split on default IFS
    (( count[$word]++ )) || true
done

printf 'unique=%d\n' "${#count[@]}"
printf 'the=%d fox=%d\n' "${count[the]}" "${count[fox]}"

# Hash iteration order is unspecified. Sort the keys for stable output.
mapfile -t keys < <(printf '%s\n' "${!count[@]}" | LC_ALL=C sort)
for key in "${keys[@]}"; do
    printf '%s=%d\n' "$key" "${count[$key]}"
done

if [[ -v 'count[fox]' ]]; then
    printf 'has_fox=yes\n'
else
    printf 'has_fox=no\n'
fi

if [[ -v 'count[cat]' ]]; then
    printf 'has_cat=yes\n'
else
    printf 'has_cat=no\n'
fi

# ================= EXPLANATION =================
# What it does
#   Counts word frequencies with an associative array, prints the unique count
#   and two lookups, then lists every key/value pair in sorted order and tests
#   membership with `-v`.
#
# Step by step
#   1. `declare -A count` creates an associative array. Unlike an indexed
#      array, its subscripts are arbitrary strings, not arithmetic expressions.
#   2. The unquoted `$text` is deliberate: it splits on IFS into words. For
#      arbitrary text use `read -ra` with a controlled IFS instead.
#   3. `(( count[$word]++ ))` increments the value; an absent key starts at 0.
#      The `|| true` absorbs the status 1 that an arithmetic command returns
#      when its result is 0, which would otherwise trip `set -e`.
#   4. `${#count[@]}` is the number of distinct keys, not the number of words.
#   5. `"${!count[@]}"` expands to the key list. The order is hash order and is
#      not stable across bash versions or insertion sequences, so the keys are
#      piped through sort before display.
#   6. `[[ -v 'count[fox]' ]]` asks whether the element exists. Quoting the
#      whole subscript prevents pathname expansion and keeps `[` `]` literal.
#
# Output
#   unique=8
#   the=3 fox=2
#   brown=1
#   dog=1
#   fox=2
#   jumps=1
#   lazy=1
#   over=1
#   quick=1
#   the=3
#   has_fox=yes
#   has_cat=no
#
# Why it matters
#   One associative array replaces sort/uniq pipelines for in-memory counting
#   and gives O(1) membership tests. It is the natural structure for
#   configuration maps, seen-sets, and per-key aggregation in shell scripts.
#
# Caveats
#   - Bash 4.0+ only; macOS's bash 3.2 cannot do this without a workaround.
#   - Key order is unspecified. Never rely on insertion order; sort the keys.
#   - Subscripts undergo expansion, so a key containing `]`, `[`, or glob
#     characters can be misparsed. Quote the subscript and, for hostile input,
#     validate the key.
#   - An empty key is legal but awkward; reject it at the boundary.
#   - `(( ... ))` returns 1 when the arithmetic value is 0. Under `set -e`,
#     write `(( count[$word]++ )) || true` or use `count[$word]=$(( ... ))`.
#   - Associative arrays are not exported to child processes; they are shell
#     state, not environment.
# ==============================================
