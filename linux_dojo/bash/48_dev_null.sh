#!/usr/bin/env bash
# MAGIC: /dev/null discards, /dev/zero generates, /dev/full fails writes with
# MAGIC: ENOSPC - character devices that look like files but have no size.
set -euo pipefail

for dev in /dev/null /dev/zero /dev/full; do
    [[ -c "$dev" ]] || { printf 'skipped: %s is not a character device\n' "$dev"; exit 0; }
done
for t in dd od stat truncate; do
    command -v "$t" >/dev/null 2>&1 || { printf 'skipped: %s not available\n' "$t"; exit 0; }
done

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

# Writes to /dev/null succeed and store nothing; its size is not a byte count.
printf 'discard me' > /dev/null
printf 'null_size=%s\n' "$(stat -c %s /dev/null)"

# /dev/zero is an infinite supplier of NUL bytes; take exactly four.
zero_hex=$(dd if=/dev/zero bs=1 count=4 status=none | od -An -tx1 -v | tr -d ' \n')
printf 'zero_hex=%s\n' "$zero_hex"
printf 'zero_bytes=%s\n' "$(( ${#zero_hex} / 2 ))"

# /dev/full accepts open() but every write returns ENOSPC. Catch dd's status.
full_status=0
printf 'x' | dd of=/dev/full bs=1 count=1 status=none 2>/dev/null || full_status=$?
if (( full_status != 0 )); then
    printf 'full_failed=yes\n'
else
    printf 'full_failed=no\n'
fi

# ftruncate/truncate on a character device is EINVAL; on a regular file it works.
if truncate -s 0 /dev/null 2>/dev/null; then
    printf 'truncate_device_ok=yes\n'
else
    printf 'truncate_device_ok=no\n'
fi

regular=$work/regular.dat
printf '0123456789' > "$regular"
truncate -s 0 "$regular"
printf 'regular_size_after_truncate=%s\n' "$(stat -c %s "$regular")"

# ================= EXPLANATION =================
# What it does
#   Exercises the three classic character devices: /dev/null swallows a write,
#   /dev/zero supplies four NUL bytes, and /dev/full fails a write with ENOSPC.
#   It then shows that truncate works on a regular file but not on a device
#   node.
#
# Step by step
#   1. /dev/null is a character device whose write method accepts and discards
#      bytes and whose read method returns EOF. Its st_size is 0 both before and
#      after the write; size is not a measure of what was written.
#   2. /dev/zero's read method fills the buffer with zero bytes and never runs
#      out. dd stops after count=4, and bs=1 makes those four separate 1-byte
#      reads. od prints 00 00 00 00.
#   3. /dev/full's write method returns -ENOSPC no matter how much room the
#      backing filesystem has. dd reports a write error and exits nonzero; the
#      script converts that status into the boolean full_failed=yes.
#   4. truncate(2) and ftruncate(2) change the length field of a regular file.
#      The kernel rejects them with EINVAL for a character device, so the
#      /dev/null call exits nonzero and the print says truncate_device_ok=no.
#   5. The same truncate -s 0 on a regular file sets its size to 0; the file
#      still exists, with its inode and name unchanged.
#
# Output
#   null_size=0
#   zero_hex=00000000
#   zero_bytes=4
#   full_failed=yes
#   truncate_device_ok=no
#   regular_size_after_truncate=0
#
# Why it matters
#   Device nodes are inodes with a major/minor number and a driver behind them,
#   not containers of bytes. /dev/null is the standard sink for discarded output
#   and the standard empty input; /dev/zero is used to allocate zero-filled
#   files and to test reads; /dev/full is the reliable way to test ENOSPC
#   handling without filling a disk. Confusing a device with a regular file
#   leads to meaningless stat sizes and failed seek/truncate calls.
#
# Caveats
#   - Device semantics belong to the driver. /dev/null reads return EOF,
#     /dev/zero reads never end, and /dev/full reads return zeros while writes
#     fail. Not every character device behaves this way.
#   - st_size on a device node is 0 (or driver-defined) and says nothing about
#     capacity or data. Use the driver's own ioctls for that.
#   - The shell performs redirections before running the command. A write to
#     /dev/full can fail inside printf or inside dd; both failures are visible
#     only through the exit status, which is why the script checks dd's.
#   - /dev/full may be absent in a minimal container; the script then prints one
#     skipped line instead of pretending the test ran.
#   - dd without conv=fsync/iflag=fullblock may report short I/O. For four bytes
#     and bs=1 that is not a problem, but large copies need fullblock and a
#     status check.
#   - Creating device nodes needs CAP_MKNOD and a filesystem that permits them;
#     opening one can still be blocked by a device cgroup.
# ==============================================
