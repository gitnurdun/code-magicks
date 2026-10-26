#!/usr/bin/env bash
# MAGIC: find compares mtimes to a reference file or an age window, then -printf and -exec turn matches into stable rows.
set -euo pipefail

tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
mkdir "$tmp/recent"

touch -d '2024-01-01 00:00:00 UTC' "$tmp/ref"
printf 'old\n'   > "$tmp/old.txt";   touch -d '2023-12-31 23:00:00 UTC' "$tmp/old.txt"
printf 'edge\n'  > "$tmp/edge.txt";  touch -d '2024-01-01 00:00:00 UTC' "$tmp/edge.txt"
printf 'new\n'   > "$tmp/new.txt";   touch -d '2024-01-01 02:00:00 UTC' "$tmp/new.txt"
printf 'fresh\n' > "$tmp/fresh.txt"; touch -d '2024-01-02 00:00:00 UTC' "$tmp/fresh.txt"

now=$(date +%s)
printf 'recent30\n' > "$tmp/recent/r30.txt"; touch -d "@$((now - 1800))" "$tmp/recent/r30.txt"
printf 'recent90\n' > "$tmp/recent/r90.txt"; touch -d "@$((now - 5400))" "$tmp/recent/r90.txt"

printf '%s\n' 'newer:'
find "$tmp" -maxdepth 1 -type f -newer "$tmp/ref" -printf '%f %s\n' | LC_ALL=C sort

printf '%s\n' 'mmin_60:'
find "$tmp/recent" -maxdepth 1 -type f -mmin -60 -printf '%f %s\n' | LC_ALL=C sort

printf '%s\n' 'exec_plus:'
find "$tmp" -maxdepth 1 -type f -name '*.txt' -exec printf '%s\n' {} + |
  sed 's|^.*/||' | LC_ALL=C sort

# ================= EXPLANATION =================
# What it does
#   Builds a tree with known mtimes, selects files newer than a fixed
#   reference, selects files modified within a 60-minute window, then
#   prints name/size rows and demonstrates -exec ... {} + batching.
#
# Step by step
#   1. touch -d (GNU) sets exact timestamps. ref is the boundary file;
#      old.txt is one hour older, edge.txt is exactly equal, new.txt is
#      two hours newer, fresh.txt is a day newer.
#   2. -newer ref is a strict comparison: mtime must be greater. A file
#      with the same timestamp as ref (edge.txt) is not selected.
#   3. The recent/ files are set relative to the current clock. They
#      are well inside and outside the window (30 vs 90 minutes), so
#      -mmin -60 has a deterministic membership even though its
#      threshold is relative to "now".
#   4. -printf '%f %s\n' prints the basename and byte size. The output
#      is piped through LC_ALL=C sort so find's traversal order does
#      not matter.
#   5. -exec ... {} + collects many paths per invocation, reducing
#      process count compared with {} \;. Here printf receives all
#      paths as arguments and repeats its format; sed strips the
#      temporary directory prefix.
#
# Output
#   newer:
#   fresh.txt 6
#   new.txt 4
#   mmin_60:
#   r30.txt 9
#   exec_plus:
#   edge.txt
#   fresh.txt
#   new.txt
#   old.txt
#
# Why it matters
#   Age-based selection is how cron jobs, backup tools and journals
#   find work. A fixed reference makes the test reproducible; a
#   relative window is unavoidable when the question is "recent", so
#   keep test data far from the boundary and never assert on the exact
#   edge without controlling the clock.
#
# Caveats
#   - -printf and touch -d '@epoch' are GNU extensions; -mmin is a
#     common non-POSIX extension, and -maxdepth is not POSIX either.
#     POSIX has -newer and -mtime.
#   - -mmin is relative to wall-clock now. If the script pauses or the
#     clock jumps, a file can cross the boundary; filesystem timestamp
#     granularity (ext4 nanoseconds, some filesystems 1s or 2s) and
#     NFS clock skew add more error.
#   - find traverses a changing tree; files can be created or removed
#     mid-run. There is no snapshot. Sort the result if it is consumed.
#   - -daystart makes day-based tests count from local midnight today
#     instead of a rolling 24-hour window, and only affects tests after
#     it on the command line; it does not change -newer.
#   - {} + may invoke the command more than once; do not assume one
#     process or one ordered batch.
#   - -newer is strict (>); equality needs a different test.
# ==============================================
