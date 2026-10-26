#!/usr/bin/env bash
# MAGIC: paste glues columns side by side, cut slices fields or characters, tr squeezes bytes, and tac reverses lines.
set -euo pipefail
export LC_ALL=C

tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT

printf '%s\n' \
  'root:x:0:0:root:/root:/bin/bash' \
  'daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin' \
  'nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin' > "$tmp/passwd"

cut -d: -f1 "$tmp/passwd" > "$tmp/users"
cut -d: -f3 "$tmp/passwd" > "$tmp/uids"

printf '%s\n' 'paste_csv:'
paste -d, "$tmp/users" "$tmp/uids"

printf '%s\n' 'paste_pairs:'
printf '%s\n' w x y z | paste - - | tr '\t' '|'

printf '%s\n' 'cut_fields:'
cut -d: -f1,3 "$tmp/passwd"

printf '%s\n' 'cut_chars:'
cut -c1-4 "$tmp/users"

squeezed=$(printf '%s\n' 'a   b    c' | tr -s ' ')
printf 'squeezed=[%s]\n' "$squeezed"

printf '%s\n' 'tac_reverse:'
tac "$tmp/users"

# ================= EXPLANATION =================
# What it does
#   Treats an embedded /etc/passwd-like block as columns: paste puts
#   usernames beside uids, cut selects fields 1 and 3 and the first four
#   characters, tr squeezes spaces, and tac reverses the line order.
#
# Step by step
#   1. The passwd block is written to a temp file. Each line has seven
#      colon-separated fields: name, password, uid, gid, gecos, home,
#      shell.
#   2. cut -d: -f1 and -f3 extract columns 1 and 3 into two files.
#      paste -d, reads them in parallel and joins each pair with a
#      comma, preserving line order.
#   3. 'paste - -' names standard input twice, so paste consumes two
#      lines per output row and separates the columns with a tab. tr
#      turns that tab into '|' for visibility.
#   4. cut -f1,3 on the original prints fields in file order, not in
#      the order given: field 1 then field 3, separated by ':'.
#   5. cut -c1-4 keeps character positions 1 through 4 of each name.
#      With LC_ALL=C these are bytes, which is exactly what this ASCII
#      data needs.
#   6. tr -s ' ' squeezes each run of spaces to one. tac reverses the
#      order of lines, not the characters within them.
#
# Output
#   paste_csv:
#   root,0
#   daemon,1
#   nobody,65534
#   paste_pairs:
#   w|x
#   y|z
#   cut_fields:
#   root:0
#   daemon:1
#   nobody:65534
#   cut_chars:
#   root
#   daem
#   nobo
#   squeezed=[a b c]
#   tac_reverse:
#   nobody
#   daemon
#   root
#
# Why it matters
#   Column extraction from passwd-like records is a daily shell task.
#   paste is the inverse of cut for simple layouts, and both work on
#   streams without loading a file into memory. tr and tac cover the
#   byte-level and line-level transformations that complete the toolkit.
#
# Caveats
#   - cut -d takes one delimiter character, not a string. Empty fields
#     are still fields, and cut cannot reorder output: -f3,1 prints
#     field 1 first because it always follows input order.
#   - cut -c positions are characters in the current locale; -b
#     positions are bytes. Under LC_ALL=C one character is one byte,
#     but in UTF-8 the two differ and -b can split a codepoint. Even -c
#     counts combining marks separately, so it is not display width.
#   - tr operates on bytes and byte ranges, not multibyte characters;
#     squeezing an ASCII space is safe, but character classes and
#     ranges can behave differently under locales.
#   - paste pairs lines by position and uses tabs by default. With an
#     odd number of lines, 'paste - -' leaves the final field empty.
#   - tac reverses lines, not fields or characters. rev is a separate
#     util-linux tool that reverses characters in each line.
#   - The passwd-like block is synthetic; do not parse a real
#     /etc/passwd with cut if the file can contain escaped colons or
#     NIS entries. Use getent passwd for real accounts.
# ==============================================
