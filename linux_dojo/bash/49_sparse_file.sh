#!/usr/bin/env bash
# MAGIC: a sparse file has logical length without allocated blocks; truncate
# MAGIC: creates holes, fallocate fills them, and punch-hole frees them again.
set -euo pipefail

for t in truncate stat du dd fallocate; do
    command -v "$t" >/dev/null 2>&1 || { printf 'skipped: %s not available\n' "$t"; exit 0; }
done

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
img=$work/sparse.bin

truncate -s 64M "$img" || { printf 'skipped: cannot create a 64M sparse file\n'; exit 0; }

read -r stat_size stat_blocks < <(stat -c '%s %b' "$img")
apparent_bytes=$stat_size
allocated_bytes=$(( stat_blocks * 512 ))
du_allocated_kib=$(du -k "$img" | awk '{ print $1 }')
du_apparent_kib=$(du -k --apparent-size "$img" | awk '{ print $1 }')

if (( apparent_bytes <= allocated_bytes )); then
    printf 'skipped: filesystem did not make a sparse file\n'
    exit 0
fi

printf 'apparent_bytes=%s\n' "$apparent_bytes"
printf 'apparent_kib=%s\n' "$(( apparent_bytes / 1024 ))"
printf 'du_allocated_kib=%s\n' "$du_allocated_kib"
printf 'du_apparent_kib=%s\n' "$du_apparent_kib"
printf 'stat_blocks=%s\n' "$stat_blocks"
printf 'apparent_gt_allocated=yes\n'

alloc=$work/alloc.bin
if ! fallocate -l 4M "$alloc"; then
    printf 'skipped: fallocate -l failed\n'
    exit 0
fi
dd if=/dev/zero of="$alloc" bs=1M count=4 conv=notrunc status=none
after_blocks=$(stat -c %b "$alloc")
if (( after_blocks == 0 )); then
    printf 'skipped: fallocate did not allocate blocks\n'
    exit 0
fi
printf 'fallocate_after_blocks_gt_0=yes\n'

before_punch=$after_blocks
if fallocate --punch-hole --keep-size --offset 1048576 --length 1048576 "$alloc" 2>/dev/null; then
    after_punch=$(stat -c %b "$alloc")
    printf 'punch_hole=yes\n'
    if (( after_punch < before_punch )); then
        printf 'punch_released_blocks=yes\n'
    else
        printf 'punch_released_blocks=no\n'
    fi
else
    printf 'punch_hole=skipped: filesystem rejects FALLOC_FL_PUNCH_HOLE\n'
fi

# ================= EXPLANATION =================
# What it does
#   Creates a 64 MiB file with truncate, which sets the logical length without
#   writing data blocks. It compares apparent size, allocated blocks, du and
#   du --apparent-size, then makes fallocate reserve a real 4 MiB extent and
#   finally punches a 1 MiB hole out of it. Only normalised counts and booleans
#   are printed, because block sizes and filesystem allocation policies differ.
#
# Step by step
#   1. truncate -s 64M sets the inode's size to 67108864, but no data blocks
#   2. stat %s is the apparent (logical) size. stat %b is the number of 512-byte
#      blocks actually allocated, not the file size and not the filesystem
#      block size. Multiplying %b by 512 gives the allocated byte count.
#   3. du -k walks and sums allocated blocks, so it reports about 0 KiB.
#      du --apparent-size -k sums the logical lengths instead, so it reports
#      65536 KiB. Both describe the same inode from different angles.
#   4. fallocate -l 4M asks the filesystem to reserve 4 MiB of extents. Writing
#      4 MiB of zeros over them with dd converts the unwritten extents into
#      ordinary data so the next step has something to release. %b is now 8192
#      (4 MiB / 512).
#   5. fallocate --punch-hole --offset 1M --length 1M with --keep-size tells the
#      filesystem to deallocate that range without changing st_size. The blocks
#      are freed, reads return zeros, and %b drops. --keep-size is important:
#      otherwise util-linux may adjust the apparent size.
#   6. If the filesystem rejects punch-hole, the script prints a skipped line
#      instead of failing; the sparse demonstration above still stands.
#
# Output
#   apparent_bytes=67108864
#   apparent_kib=65536
#   du_allocated_kib=0
#   du_apparent_kib=65536
#   stat_blocks=0
#   apparent_gt_allocated=yes
#   fallocate_after_blocks_gt_0=yes
#   punch_hole=yes
#   punch_released_blocks=yes
#
# Why it matters
#   Sparse files make VM images, container layers, and database files cheap to
#   create and copy, but only if every tool in the path understands holes. cp,
#   tar, rsync, and scp have sparse-aware modes; a naive copy expands a 64 MiB
#   hole into 64 MiB of zeros. punch-hole is how filesystems reclaim space for
#   deleted records without changing the file layout.
#
# Caveats
#   - Sparse support is filesystem-specific. ext4, XFS, Btrfs, and tmpfs support
#     holes; FAT, exFAT, and many network filesystems do not and will allocate
#     real blocks. The script skips if the test file came out non-sparse.
#   - stat %b counts 512-byte units, independent of the filesystem block size.
#     Do not compare it directly with du's KiB output.
#   - fallocate may allocate unwritten extents that read as zeros but are not
#     yet data; some filesystems account for them differently until written.
#   - punch-hole typically requires offset and length aligned to the filesystem
#     block size. 1 MiB is aligned on ordinary filesystems but not guaranteed
#     everywhere.
#   - du reports allocation in units of the filesystem block size; --apparent-size
#     reports logical bytes. Neither is a substitute for stat when you need inode
#     metadata.
#   - Copying with cat, dd without conv=sparse, or a tar without --sparse will
#     materialise the holes.
# ==============================================
