#!/usr/bin/env bash
# MAGIC: mapfile turns a stream into an array in one builtin, and -d '' splits on NUL instead of newline.
set -euo pipefail

# 1. One element per line; -t strips each trailing newline.
mapfile -t lines < <(printf 'alpha\nbeta\ngamma\n')
printf 'lines: count=%d\n' "${#lines[@]}"
for i in "${!lines[@]}"; do
  printf '  [%d]=%s\n' "$i" "${lines[$i]}"
done

# 2. -s skips, -n caps the number of elements read.
mapfile -t -s 1 -n 2 window < <(printf 'one\ntwo\nthree\nfour\n')
printf 'window: count=%d\n' "${#window[@]}"
for i in "${!window[@]}"; do
  printf '  [%d]=%s\n' "$i" "${window[$i]}"
done

# 3. -d '' splits on NUL, the only byte that cannot appear in a path.
mapfile -t -d '' names < <(printf '%s\0' 'a b' $'c\nd' 'plain')
printf 'nul: count=%d\n' "${#names[@]}"
for i in "${!names[@]}"; do
  printf '  [%d]=%q\n' "$i" "${names[$i]}"
done

# 4. readarray is a synonym for mapfile.
readarray -t copy < <(printf 'x\ny\n')
printf 'readarray: count=%d second=%s\n' "${#copy[@]}" "${copy[1]}"

# 5. A pipe puts mapfile in a subshell, so its array vanishes.
piped=()
printf 'piped\n' | mapfile -t piped
printf 'piped: count=%d\n' "${#piped[@]}"

# ================= EXPLANATION =================
# What it does
#   Reads records from stdin into an array with the bash builtins
#   mapfile and readarray. -t strips the delimiter, -s skips leading
#   records, -n limits how many are stored, and -d '' makes NUL the
#   delimiter - the only safe separator for arbitrary file paths.
#
# Step by step
#   1. < <(printf ...) is process substitution: the producer runs in
#      the background and mapfile reads its pipe as standard input.
#   2. Without -t each element keeps its trailing newline. With -t the
#      delimiter is removed. A final record without a delimiter is
#      still stored; a missing final newline is not an error.
#   3. -s 1 drops the first record; -n 2 stores at most two more.
#   4. -d '' changes the delimiter byte to NUL. Filenames may contain
#      spaces and newlines but never NUL, so the list round-trips.
#   5. %q is used only to display control characters without breaking
#      the terminal; it is not part of the storage format.
#   6. readarray is the same builtin under a synonym.
#   7. A pipeline element runs in a subshell. 'printf | mapfile'
#      therefore fills an array that the parent never sees; redirection
#      or process substitution keeps it in the current shell.
#
# Output
#   lines: count=3
#     [0]=alpha
#     [1]=beta
#     [2]=gamma
#   window: count=2
#     [0]=two
#     [1]=three
#   nul: count=3
#     [0]=a\ b
#     [1]=$'c\nd'
#     [2]=plain
#   readarray: count=2 second=y
#   piped: count=0
#
# Why it matters
#   Shell loops over command substitution silently mangle whitespace,
#   strip trailing newlines, and cannot carry NUL bytes. mapfile keeps
#   a stream intact and makes the array the unit of data, which is the
#   correct shape for filenames and structured records.
#
# Caveats
#   - -d is bash 4.4+. In older bash, mapfile only splits on newline.
#   - Without -t the newline is part of each value; comparisons and
#     printf output then show an unexpected trailing newline.
#   - Command substitution $(...) cannot hold NUL and strips trailing
#     newlines; use mapfile with process substitution instead.
#   - Arrays can be sparse: unset 'a[1]' makes ${#a[@]} count only the
#     remaining elements, while the highest index is still 2. ${!a[@]}
#     lists the surviving indexes, not 0..n-1.
#   - mapfile/readarray and %q are bash-specific, not POSIX sh.
#   - The pipeline-subshell rule changes if the nonstandard lastpipe
#     shopt is enabled; then the last stage runs in the current shell.
#   - A huge stream is held in memory; mapfile is not streaming.
# ==============================================
