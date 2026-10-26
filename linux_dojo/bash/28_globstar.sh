#!/usr/bin/env bash
# MAGIC: globstar makes ** recursive, while nullglob and failglob decide what an
# MAGIC: unmatched pattern expands to - nothing, or a fatal expansion error.
set -euo pipefail

work=$(mktemp -d)
trap 'cd /; rm -rf -- "$work"' EXIT
mkdir -p "$work/a/b" "$work/c"
printf 'root\n' > "$work/root.txt"
printf 'hidden\n' > "$work/.hidden.txt"
printf 'one\n' > "$work/a/one.txt"
printf 'nested hidden\n' > "$work/a/.nested.txt"
printf 'two\n' > "$work/a/b/two.txt"
printf 'log\n' > "$work/c/three.log"

cd "$work"
shopt -s globstar nullglob

printf 'globstar **/*.txt:\n'
files=(**/*.txt)
printf '  %s\n' "${files[@]}" | LC_ALL=C sort
printf 'count=%s\n' "${#files[@]}"

printf 'nullglob:\n'
( missing=(nope/*.txt); printf '  count=%s\n' "${#missing[@]}" )

printf 'failglob:\n'
( shopt -u nullglob; shopt -s failglob; : nope/*.txt ) 2>/dev/null \
    || printf '  failglob: expansion error\n'

shopt -s dotglob
printf 'dotglob **/*.txt:\n'
all=(**/*.txt)
printf '  %s\n' "${all[@]}" | LC_ALL=C sort
printf 'count=%s\n' "${#all[@]}"

# ================= EXPLANATION =================
# What it does
#   Builds a small tree with plain, nested, and hidden .txt files, then shows
#   recursive `**` matching, the difference between nullglob and failglob, and
#   how dotglob adds hidden names. Paths are made relative and sorted, and only
#   counts are printed besides the relative names.
#
# Step by step
#   1. mktemp -d creates a private root; the tree is created relative to it and
#      the script cds inside, so no absolute temp path is printed.
#   2. globstar makes ** a recursive component: **/*.txt matches files in the
#      top directory and at any depth. With no dotglob, names beginning with a
#      dot are ignored, so only three files match.
#   3. The array captures the expansion, then sort normalizes the filesystem's
#      order. LC_ALL=C keeps the ordering byte-wise and locale-independent.
#   4. nullglob removes a pattern that matches nothing. `nope/*.txt` becomes an
#      empty word list and the array length is 0; without nullglob the literal
#      pattern would remain as one word.
#   5. failglob is the opposite policy: no match is an expansion error. The
#      demonstration runs in a subshell with nullglob cleared, so the error
#      aborts only that child and the parent can report it and continue.
#   6. dotglob lets * and ** match leading-dot names. The same **/*.txt now
#      finds both hidden files as well as the three plain ones, for five.
#
# Output
#   globstar **/*.txt:
#     a/b/two.txt
#     a/one.txt
#     root.txt
#   count=3
#   nullglob:
#     count=0
#   failglob:
#     failglob: expansion error
#   dotglob **/*.txt:
#     .hidden.txt
#     a/.nested.txt
#     a/b/two.txt
#     a/one.txt
#     root.txt
#   count=5
#
# Why it matters
#   Recursive globbing is the shortest correct way to select files by name in
#   the shell, and the three shopt policies decide whether a missing match is
#   silence, a literal string, or a hard error. Choosing the policy explicitly
#   removes a whole class of "operated on the literal pattern" bugs.
#
# Caveats
#   - globstar requires bash 4.0+. `**` must be a complete path component; it
#     is not a regex and does not match across `/` inside a component.
#   - Recursive globbing walks and stats the tree in the shell before the
#     command runs. On a huge tree it is slow, uses memory proportional to the
#     matches, sorts them, and can exceed ARG_MAX when passed to an external
#     command. Prefer find -print0 plus a while read loop for large trees.
#   - Symlink behaviour: ** lists a symlink to a directory as an entry but does
#     not recursively descend through it, which avoids loops but means files
#     behind the link are not found. Use find -L or resolve the link explicitly
#     when following is required.
#   - nullglob and failglob are global shell options, not per-command. A
#     subshell contains failglob's fatal expansion error; in the parent it can
#     terminate the script.
#   - dotglob makes * match hidden names but still excludes . and ..; files
#     skipped by a glob are not an error unless failglob is set.
# ==============================================
