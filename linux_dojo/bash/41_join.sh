#!/usr/bin/env bash
# MAGIC: join is a streaming merge that assumes both inputs are already sorted by the key; it never sorts them for you.
set -euo pipefail

tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT

printf '%s\n' '1,alice' '2,bob' '4,carol' > "$tmp/left"
printf '%s\n' '1,admin' '2,user' '3,guest' > "$tmp/right"
printf '%s\n' '2,bob' '1,alice' '4,carol' > "$tmp/unsorted"

printf '%s\n' 'inner:'
LC_ALL=C join -t, -1 1 -2 1 "$tmp/left" "$tmp/right"

printf '%s\n' 'left_outer:'
LC_ALL=C join -t, -a 1 -e NULL -o 0,1.2,2.2 -1 1 -2 1 "$tmp/left" "$tmp/right"

printf '%s\n' 'unsorted:'
LC_ALL=C join -t, --nocheck-order -1 1 -2 1 "$tmp/unsorted" "$tmp/right"

# ================= EXPLANATION =================
# What it does
#   Joins two small comma-separated files on field 1: an inner join,
#   then a left outer join with missing fields filled by NULL and only
#   selected columns printed. Finally it joins an unsorted left file to
#   show that a match is silently lost.
#
# Step by step
#   1. Both files are already sorted by key: left has 1,2,4 and right
#      has 1,2,3. join reads them as streams and merges in one pass.
#   2. -t, sets the one-character field separator. -1 1 and -2 1 say
#      the join key is field 1 in both files.
#   3. The inner join prints the key once, then the remaining fields of
#      file 1, then the remaining fields of file 2.
#   4. -a 1 keeps every file-1 row, including unmatched key 4. -e NULL
#      replaces empty output fields. -o 0,1.2,2.2 selects the join key,
#      field 2 of file 1, and field 2 of file 2.
#   5. The unsorted file has 2 before 1. join's merge has already
#      advanced the right stream past key 1 by the time it reads key 1,
#      so that pair disappears. --nocheck-order suppresses GNU's
#      disorder warning, making the data loss look clean.
#
# Output
#   inner:
#   1,alice,admin
#   2,bob,user
#   left_outer:
#   1,alice,admin
#   2,bob,user
#   4,carol,NULL
#   unsorted:
#   2,bob,user
#
# Why it matters
#   join is the efficient relational primitive in coreutils: one
#   streaming pass, no index, no SQL engine. That performance is bought
#   with a strict precondition. Unsorted input does not get resorted or
#   fixed; it produces a wrong answer.
#
# Caveats
#   - Both inputs must be sorted by the join key with the same
#     collation join uses. LC_ALL=C plus a byte sort is the simplest
#     reproducible choice.
#   - GNU join can diagnose disorder and exit nonzero, especially when
#     it sees unpairable lines. --nocheck-order suppresses the check
#     (and is the default in some versions), but it never sorts the
#     input and never restores the lost rows.
#   - Keys are compared as text/collation values, not numbers. Sorting
#     with sort -n can order 10 before 2 and make join miss pairs;
#     zero-pad numeric keys or sort the way join compares.
#   - Duplicate keys create a cross product of matching rows, which is
#     usually not what a naive caller expects.
#   - -t accepts one character. Without it, fields are separated by
#     runs of blanks and leading blanks are handled specially.
#   - -a 1 keeps unmatched left rows; -v 1 prints only those rows. Use
#     --header to pass a header line through without matching it.
# ==============================================
