#!/usr/bin/env bash
# MAGIC: dd seek= writes at a byte offset, and conv=notrunc keeps the bytes
# MAGIC: after the write instead of truncating the file at the seek point.
set -euo pipefail

for t in dd od stat tail; do
    command -v "$t" >/dev/null 2>&1 || { printf 'skipped: %s not available\n' "$t"; exit 0; }
done

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
file=$work/image.bin

printf 'ABCDEFGHIJKLMNOP' > "$file"        # exactly 16 bytes, no newline
printf 'size_before=%s\n' "$(stat -c %s "$file")"

# Overwrite 4 bytes starting at offset 4; the file must stay 16 bytes.
printf 'WXYZ' | dd of="$file" bs=1 seek=4 conv=notrunc status=none
printf 'size_after_patch=%s\n' "$(stat -c %s "$file")"

printf 'patched_hex='
od -An -tx1 -v "$file" | tr -d ' \n'
printf '\n'

printf 'patched_c='
od -An -c -v "$file" | tr -s ' ' | sed 's/^ //' | tr -d '\n'
printf '\n'

# The same write without conv=notrunc: dd truncates the output first.
trunc=$work/truncated.bin
printf 'ABCDEFGHIJKLMNOP' > "$trunc"
printf 'WXYZ' | dd of="$trunc" bs=1 seek=4 status=none
printf 'truncating_size=%s\n' "$(stat -c %s "$trunc")"

# Offsets one past the old EOF extend the file rather than making a hole.
printf '!' | dd of="$file" bs=1 seek=16 conv=notrunc status=none
printf 'size_after_eof=%s\n' "$(stat -c %s "$file")"
printf 'last_byte='
tail -c 1 "$file"
printf '\n'

# ================= EXPLANATION =================
# What it does
#   Builds a 16-byte file, overwrites four bytes in the middle with dd while
#   keeping the length, dumps the result as hex and as characters, shows what
#   the same dd does without conv=notrunc, and then appends one byte at the
#   old end-of-file.
#
# Step by step
#   1. printf writes "ABCDEFGHIJKLMNOP" with no newline, so stat reports 16.
#   2. dd opens the file for writing and seeks to byte 4. By default dd opens
#      its output with O_TRUNC, which would make the file empty. conv=notrunc
#      suppresses that truncation, so only the four addressed bytes change.
#   3. bs=1 means every read and write is one byte. seek=4 is an output offset,
#      not a byte count; the four bytes of input land at offsets 4..7.
#   4. od -An suppresses the address column, -tx1 prints hex bytes and -c prints
#      escaped characters. tr squeezes and strips the padding so the dump fits
#      on one labelled line.
#   5. The second file gets the same write without conv=notrunc. dd truncates it
#      to zero, seeks to 4 (leaving a four-byte hole of NULs), writes WXYZ, and
#      the resulting size is 8.
#   6. Writing one byte at offset 16, exactly the current size, extends the file
#      to 17 bytes. Seeking beyond the size would instead create a hole.
#
# Output
#   size_before=16
#   size_after_patch=16
#   patched_hex=414243445758595a494a4b4c4d4e4f50
#   patched_c=A B C D W X Y Z I J K L M N O P
#   truncating_size=8
#   size_after_eof=17
#   last_byte=!
#
# Why it matters
#   dd is the portable way to edit a fixed offset without rewriting a whole
#   file: boot sectors, disk images, container layers, and binary headers.
#   conv=notrunc is the difference between an in-place patch and a destructive
#   rewrite, and it is easy to forget because a normal shell redirection does
#   truncate.
#
# Caveats
#   - bs=1 performs one syscall per byte and is very slow for large patches;
#     use bs=4096 with skip/seek in block units, or a language with pwrite.
#   - dd may perform short reads and short writes. Check its exit status and the
#     stderr byte counts; status=none hides those counts.
#   - A seek past EOF followed by a write creates a sparse hole, not zeroed
#     data blocks. Tools that copy the file must understand holes.
#   - The patch is not atomic and is not serialised with readers. A crash or a
#     concurrent writer can leave a half-written record; write to a copy and
#     rename when that matters.
#   - status=none and stat -c are GNU coreutils; od -c escape style is POSIX.
#   - The output assumes the bytes are printable ASCII. od -c would print
#     escapes such as \0 and \n for other values.
# ==============================================
