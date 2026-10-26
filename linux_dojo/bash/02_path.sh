#!/usr/bin/env bash
# MAGIC: strip prefixes and suffixes with #, ##, % and %% instead of spawning
# basename, dirname, and sed.
set -euo pipefail

p=/var/log/nginx/access.log.gz

printf 'path=%s\n' "$p"
printf 'dir=%s\n' "${p%/*}"          # shortest suffix matching /*
printf 'base=%s\n' "${p##*/}"        # longest prefix matching */
base=${p##*/}
printf 'stem=%s\n' "${base%.*}"      # shortest suffix matching .*
printf 'ext=%s\n' "${base##*.}"      # longest prefix matching *.
printf 'stem_all=%s\n' "${base%%.*}" # longest suffix matching .*

rest=${p#/}
printf 'root_component=%s\n' "${rest%%/*}"

plain=README
no_slash=plain.txt
d=${no_slash%/*}
[[ $d == "$no_slash" ]] && d=.
printf 'no_slash_dir=%s\n' "$d"
printf 'dotless_ext=%s\n' "${plain##*.}"
printf 'ext_matches_base=%s\n' "$([[ ${plain##*.} == "$plain" ]] && echo yes || echo no)"

# ================= EXPLANATION =================
# What it does
#   Splits /var/log/nginx/access.log.gz into directory, basename, stem, and
#   extension using only parameter expansion, then handles a name with no
#   slash and a name with no dot.
#
# Step by step
#   1. `${p%/*}` removes the shortest suffix matching `/*`, giving the
#      directory. `${p##*/}` removes the longest prefix matching `*/`, giving
#      the basename. The doubled operator is the greedy one.
#   2. `${base%.*}` removes the last `.ext`; `${base##*.}` keeps only the
#      extension; `${base%%.*}` removes everything from the first dot.
#   3. `${p#/}` removes one leading slash, then `${rest%%/*}` keeps the first
#      path component (`var`).
#   4. When there is no slash, `${no_slash%/*}` returns the whole string. The
#      explicit comparison turns that into `.`, matching dirname's convention.
#   5. With no dot, `${plain##*.}` returns README unchanged. The test
#      `ext_matches_base=yes` makes that edge case visible instead of silently
#      treating the whole name as an extension.
#
# Output
#   path=/var/log/nginx/access.log.gz
#   dir=/var/log/nginx
#   base=access.log.gz
#   stem=access.log
#   ext=gz
#   stem_all=access
#   root_component=var
#   no_slash_dir=.
#   dotless_ext=README
#   ext_matches_base=yes
#
# Why it matters
#   These are pure shell operations: no fork, no external command, no locale
#   surprises from basename/sed. They are the fastest path surgery available in
#   a shell and work in a minimal container where coreutils may be absent.
#
# Caveats
#   - Patterns are globs, not regexes. `*` matches any string, `?` one
#     character, `[...]` a class. A `*` in the pattern does not cross `/` in
#     path context only because the pattern itself contains `/`.
#   - `${p%/*}` on `/var` gives an empty string, not `/`; special-case the
#     single-component case if you need dirname semantics.
#   - Hidden files: `${base%.*}` on `.bashrc` gives an empty stem, because the
#     leading dot is treated as the separator. Decide explicitly what a dotfile
#     means.
#   - `${base##*.}` returns the whole name when there is no dot; do not assume
#     the result is a real extension.
#   - These operations are byte/character oriented; filenames with newlines are
#     fine, but always quote the result.
# ==============================================
