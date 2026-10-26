#!/usr/bin/env bash
# MAGIC: cmp reports the first differing byte but exits 1 on difference, so a
# MAGIC: healthy comparison under set -e must turn that status into data.
set -euo pipefail

for t in cmp dd od; do
    command -v "$t" >/dev/null 2>&1 || { printf 'skipped: %s not available\n' "$t"; exit 0; }
done

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
a=$work/a.bin
b=$work/b.bin
printf 'ABCDEFGH' > "$a"
printf 'ABXDEFGH' > "$b"        # differs at byte position 3 (0-based offset 2)

# cmp -l prints "position old new": 1-based position, octal byte values.
printf 'cmp_l:\n'
cmp -l "$a" "$b" 2>/dev/null | awk '{ printf "pos=%s a_oct=%s b_oct=%s\n", $1, $2, $3 }' || true

status=0
cmp -s "$a" "$b" || status=$?
printf 'cmp_s_status=%s\n' "$status"

printf 'a_hex='
od -An -tx1 -v "$a" | tr -d ' \n'
printf '\n'
printf 'b_hex='
od -An -tx1 -v "$b" | tr -d ' \n'
printf '\n'

# First line of cmp -l is the first differing byte, 1-based.
first=$( { cmp -l "$a" "$b" || true; } | awk 'NR == 1 { print $1; exit }')
offset=$((first - 1))
printf 'first_diff_offset=%s\n' "$offset"

# Copy b's byte over a's byte at the same offset; dd positions are 0-based.
dd if="$b" of="$a" bs=1 skip="$offset" seek="$offset" count=1 conv=notrunc status=none
if cmp -s "$a" "$b"; then
    printf 'fixed=yes\n'
else
    printf 'fixed=no\n'
fi

# ================= EXPLANATION =================
# What it does
#   Creates two 8-byte files that differ in exactly one byte, compares them
#   three ways, converts the first differing offset from cmp's 1-based position
#   to a 0-based seek, and repairs the difference by copying one byte with dd.
#
# Step by step
#   1. cmp -l lists every differing byte as "<position> <old> <new>", with the
#      position counted from 1 and the byte values printed in octal. Here the
#      third byte is C (0103 octal) in a and X (0130 octal) in b.
#   2. cmp exits 1 when the files differ. That is not an error, but set -e and
#      pipefail would abort on it, so the call is guarded with `|| true`.
#   3. cmp -s says nothing and reports the same status: 0 identical, 1 differ,
#      2 trouble. Capturing the status with `|| status=$?` is the only way to
#      distinguish "different" from "could not read".
#   4. od -An -tx1 -v prints the bytes in hex; -v stops od from collapsing
#      repeated lines to "*", which would hide data in a longer file.
#   5. `cmp -l | awk 'NR==1 {print $1}'` extracts the 1-based position 3.
#      Subtracting one gives dd's 0-based offset 2.
#   6. dd copies one byte from b at offset 2 to a at offset 2, with conv=notrunc
#      so a's 8-byte length survives. The final cmp -s is now status 0.
#
# Output
#   cmp_l:
#   pos=3 a_oct=103 b_oct=130
#   cmp_s_status=1
#   a_hex=4142434445464748
#   b_hex=4142584445464748
#   first_diff_offset=2
#   fixed=yes
#
# Why it matters
#   cmp is the byte-exact comparison used under rsync, patch verification, and
#   image builds, and its exit status is a three-way result. Scripts that write
#   `if cmp a b; then` are fine; scripts that call it bare under set -e abort on
#   the normal "files differ" case. The dd repair is the primitive behind binary
#   delta patching: locate an offset, then pwrite the replacement bytes.
#
# Caveats
#   - The cmp exit status is 0, 1, or 2; 2 means an I/O or usage error. Do not
#     treat every nonzero result as "different" if correctness matters.
#   - cmp -l prints octal values, not hex or decimal. It reports all differences,
#     so a large pair of files can produce a lot of output; `cmp` without -l
#     prints only the first one.
#   - The offset could not be parsed if cmp produced no output (identical
#     files); this script assumes a known difference.
#   - dd fsync is not implied. After the patch, a crash may lose it, and readers
#     can observe the half-written file. Use a copy plus rename for atomicity.
#   - The files are tiny here; bs=1 with count=1 is fine, but patching many
#     bytes this way is one syscall pair per byte.
#   - `cmp -l ... || true` also swallows genuine read errors from cmp; inspect
#     stderr when that matters.
# ==============================================
