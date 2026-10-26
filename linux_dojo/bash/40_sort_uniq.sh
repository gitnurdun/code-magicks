#!/usr/bin/env bash
# MAGIC: C-locale sort gives byte order and -s keeps input order for equal keys, while uniq only collapses adjacent duplicates.
set -euo pipefail

tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT

printf '%s\n' 'b:2' 'a:10' 'c:1' 'd:2' > "$tmp/nums"
printf '%s\n' 'red' 'blue' 'red' 'green' 'blue' 'blue' > "$tmp/words"
printf '%s\n' 'apple' 'banana' 'cherry' > "$tmp/a"
printf '%s\n' 'banana' 'cherry' 'date' > "$tmp/b"

printf '%s\n' 'by_field_2:'
LC_ALL=C sort -s -t: -k2,2n "$tmp/nums"

printf '%s\n' 'counts_desc:'
LC_ALL=C sort "$tmp/words" | uniq -c | sed 's/^ *//' | sort -rn

printf '%s\n' 'common:'
LC_ALL=C comm -12 <(LC_ALL=C sort "$tmp/a") <(LC_ALL=C sort "$tmp/b")

printf 'uniq_adjacent=%d\n' "$(uniq "$tmp/words" | wc -l)"
printf 'sort_u=%d\n' "$(LC_ALL=C sort -u "$tmp/words" | wc -l)"

# ================= EXPLANATION =================
# What it does
#   Sorts a key:value file numerically by the second field with a
#   stable tie order, counts words with uniq -c and orders the counts
#   descending, intersects two sorted files with comm -12, and compares
#   adjacent-only uniq with globally unique sort -u.
#
# Step by step
#   1. LC_ALL=C makes sort compare raw bytes. In other locales case,
#      punctuation and accented letters can collate differently.
#   2. -t: sets the field separator; -k2,2n means "field 2 only,
#      numeric". -s disables the last-resort whole-line comparison, so
#      equal keys keep their input order (b:2 before d:2).
#   3. uniq -c counts runs of identical adjacent lines. sort first is
#      mandatory; the sed strips GNU's leading padding from the count.
#   4. sort -rn reorders those "count word" lines by the leading
#      number, largest first.
#   5. comm compares two sorted streams and prints column 3, lines
#      common to both. -12 suppresses file1-only and file2-only.
#   6. uniq on the unsorted words finds only the final adjacent pair of
#      "blue" lines: 5 lines remain. sort -u removes all duplicates,
#      sorted globally: 3 unique words.
#
# Output
#   by_field_2:
#   c:1
#   b:2
#   d:2
#   a:10
#   counts_desc:
#   3 blue
#   2 red
#   1 green
#   common:
#   banana
#   cherry
#   uniq_adjacent=5
#   sort_u=3
#
# Why it matters
#   Most text pipelines are set operations, and their correctness
#   depends on collation and on whether the comparison is global or
#   adjacency-based. Pinning LC_ALL=C and knowing when -s matters makes
#   the result reproducible across machines.
#
# Caveats
#   - Locale changes collation: dictionary order, case folding and
#     ignoring punctuation are all possible. comm requires both inputs
#     sorted with the same collation it uses, or it reports disorder or
#     silently compares differently.
#   - uniq only collapses adjacent equal lines. Without a preceding
#     sort, duplicates separated by another line survive.
#   - sort's default last-resort comparison breaks ties by comparing
#     whole lines, so equal keys can move. -s is stable but is not
#     required by POSIX.
#   - sort -u removes duplicates after sorting; uniq -u prints only
#     lines that occur once. They are not the same operation.
#   - sort -n on "3 blue" parses the leading number; use -k when the
#     numeric field is not first, and remember -n does not imply -s.
#   - comm and process substitution need bash; both inputs must use
#     the same collation and be fully sorted, with no unsorted tail.
# ==============================================
