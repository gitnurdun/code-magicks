#!/usr/bin/env bash
# MAGIC: find -print0 and xargs -0 keep filenames intact because NUL, unlike newline, cannot occur in a path.
set -euo pipefail

tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT

: > "$tmp/plain.txt"
: > "$tmp/with space.txt"
: > "$tmp/"$'new\nline.txt'
: > "$tmp/-dash.txt"

printf '%s\n' 'safe_loop:'
i=0
while IFS= read -r -d '' f; do
  i=$((i + 1))
  printf '  [%d]=%q\n' "$i" "${f##*/}"
done < <(find "$tmp" -maxdepth 1 -type f -print0 | LC_ALL=C sort -z)
printf 'safe_count=%d\n' "$i"

xargs_count=$(
  find "$tmp" -maxdepth 1 -type f -print0 | LC_ALL=C sort -z |
    xargs -0 -n1 bash -c 'printf "xargs=%q\n" "$1"' _ | wc -l
)
printf 'xargs_count=%d\n' "$xargs_count"

broken=0
for f in $(find "$tmp" -maxdepth 1 -type f); do
  broken=$((broken + 1))
done
printf 'unquoted_for_count=%d\n' "$broken"

printf 'newline_hex='
printf '%s' $'new\nline.txt' | od -An -v -tx1 | tr -s ' ' | sed 's/^ //; s/ $//'

# ================= EXPLANATION =================
# What it does
#   Creates names containing a space, a newline and a leading dash,
#   then enumerates them with a NUL-delimited find pipeline and with
#   xargs -0. A deliberately naive for loop over $(find) is counted
#   for contrast, and the newline byte in one name is hex-dumped.
#
# Step by step
#   1. mktemp -d makes a private directory; the EXIT trap removes it.
#   2. find -print0 writes each path followed by a NUL byte. Since NUL
#      cannot appear in a filename, the stream is unambiguous even
#      when names contain spaces or newlines.
#   3. LC_ALL=C sort -z orders whole NUL records bytewise, so find's
#      filesystem-dependent traversal order does not leak into output.
#   4. read -r -d '' reads up to NUL without backslash processing;
#      IFS= stops whitespace trimming. The loop reads from process
#      substitution, so i is updated in the current shell.
#   5. xargs -0 consumes the same NUL stream and -n1 runs one bash
#      per name. bash's %q makes each name one printable line.
#   6. for f in $(find ...) splits on IFS (space, tab, newline) and
#      then glob-expands each word: four names become six words.
#   7. od shows the newline as byte 0a inside the otherwise ordinary
#      name "new\nline.txt".
#
# Output
#   safe_loop:
#     [1]=-dash.txt
#     [2]=$'new\nline.txt'
#     [3]=plain.txt
#     [4]=with\ space.txt
#   safe_count=4
#   xargs_count=4
#   unquoted_for_count=6
#   newline_hex=6e 65 77 0a 6c 69 6e 65 2e 74 78 74
#
# Why it matters
#   Filenames are byte strings, not lines. Any tool that separates
#   records with newline or splits on whitespace will silently corrupt
#   ordinary names. The -print0/-0 pair is the standard fix in shell
#   pipelines, and NUL is also what -z and read -d '' consume.
#
# Caveats
#   - -print0, -z and xargs -0 are GNU extensions (also supported by
#     BSD/macOS); POSIX find/xargs have no NUL-safe equivalent.
#   - 'find ... | while read' runs the loop in a subshell, so counters
#     vanish after the pipeline. Process substitution avoids that.
#   - Command substitution strips trailing newlines, and unquoted
#     expansion performs both word splitting and pathname expansion;
#     'for f in $(find ...)' is unsafe even for simple ASCII names.
#   - A leading dash is data here, but if a name is later passed as an
#     argument, put it after -- or prefix it with ./ so it is not read
#     as an option.
#   - xargs batches arguments and has an ARG_MAX-derived limit; -0
#     fixes parsing, not arbitrary command-line size.
#   - The newline is real data (0a), not an escape sequence; %q only
#     makes it visible in this output.
# ==============================================
