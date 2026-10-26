#!/usr/bin/env python3
"""MAGIC: statx exposes birth time and a mask of which fields the filesystem
actually filled in, through one extensible syscall."""
import ctypes
import os
import sys
import tempfile

AT_FDCWD = -100
AT_SYMLINK_NOFOLLOW = 0x100
STATX_BASIC_STATS = 0x7FF
STATX_BTIME = 0x800
SYS_STATX = {"x86_64": 332, "aarch64": 291, "arm64": 291}


class StatxTimestamp(ctypes.Structure):
    _fields_ = [
        ("tv_sec", ctypes.c_int64),
        ("tv_nsec", ctypes.c_uint32),
        ("reserved", ctypes.c_int32),
    ]


class Statx(ctypes.Structure):
    _fields_ = [
        ("stx_mask", ctypes.c_uint32),
        ("stx_blksize", ctypes.c_uint32),
        ("stx_attributes", ctypes.c_uint64),
        ("stx_nlink", ctypes.c_uint32),
        ("stx_uid", ctypes.c_uint32),
        ("stx_gid", ctypes.c_uint32),
        ("stx_mode", ctypes.c_uint16),
        ("spare0", ctypes.c_uint16 * 1),
        ("stx_ino", ctypes.c_uint64),
        ("stx_size", ctypes.c_uint64),
        ("stx_blocks", ctypes.c_uint64),
        ("stx_attributes_mask", ctypes.c_uint64),
        ("stx_atime", StatxTimestamp),
        ("stx_btime", StatxTimestamp),
        ("stx_ctime", StatxTimestamp),
        ("stx_mtime", StatxTimestamp),
        ("stx_rdev_major", ctypes.c_uint32),
        ("stx_rdev_minor", ctypes.c_uint32),
        ("stx_dev_major", ctypes.c_uint32),
        ("stx_dev_minor", ctypes.c_uint32),
        ("stx_mnt_id", ctypes.c_uint64),
        ("stx_dio_mem_align", ctypes.c_uint32),
        ("stx_dio_offset_align", ctypes.c_uint32),
        ("spare3", ctypes.c_uint64 * 12),
    ]


def main():
    if not sys.platform.startswith("linux"):
        print("skipped: non-Linux platform")
        return 0
    nr = SYS_STATX.get(os.uname().machine)
    if nr is None:
        print("skipped: unknown architecture for SYS_statx")
        return 0

    libc = ctypes.CDLL(None, use_errno=True)
    path = None
    try:
        fd, path = tempfile.mkstemp(prefix="statx_")
        os.write(fd, b"hello")
        os.close(fd)

        buf = Statx()
        ctypes.set_errno(0)
        ret = libc.syscall(
            ctypes.c_long(nr),
            ctypes.c_int(AT_FDCWD),
            ctypes.c_char_p(path.encode()),
            ctypes.c_int(AT_SYMLINK_NOFOLLOW),
            ctypes.c_uint(STATX_BASIC_STATS | STATX_BTIME),
            ctypes.byref(buf),
        )
        if ret != 0:
            err = ctypes.get_errno()
            print(f"skipped: statx failed ({os.strerror(err)})")
            return 0

        regular = (buf.stx_mode & 0o170000) == 0o100000
        print(f"btime_present={'yes' if buf.stx_mask & STATX_BTIME else 'no'}")
        print(f"size={buf.stx_size}")
        print(f"mode_regular={'yes' if regular else 'no'}")
        print(f"mask_has_basic={'yes' if buf.stx_mask & STATX_BASIC_STATS else 'no'}")
    finally:
        if path:
            try:
                os.unlink(path)
            except FileNotFoundError:
                pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# ================= EXPLANATION =================
# What it does
#   Calls statx(2) through ctypes with STATX_BTIME and prints whether the
#   filesystem supplied a birth time, along with the size and file type. The
#   stx_mask field says which parts of the structure are valid.
#
# Step by step
#   1. SYS_statx is 332 on x86-64 and 291 on arm64; the syscall is used
#      directly so the demo does not depend on a recent glibc wrapper.
#   2. struct statx is laid out exactly as the kernel UAPI header defines it:
#      32-bit mask/blksize, 64-bit attributes, four 16-byte timestamps, and
#      the device/mount identifiers. ctypes must match that layout byte for
#      byte or every field after the first mistake is wrong.
#   3. The mask argument requests basic stats plus btime. The kernel may return
#      fewer bits than requested; the result's stx_mask is the source of truth.
#   4. AT_SYMLINK_NOFOLLOW stats the link itself, matching lstat semantics.
#   5. The temp file is removed in finally, even on the skip paths.
#
# Output
#   btime_present=yes
#   size=5
#   mode_regular=yes
#   mask_has_basic=yes
#
# Why it matters
#   stat(2) cannot grow: its structure is frozen by the ABI. statx adds birth
#   time, mount IDs, direct-I/O alignment, and a validity mask without breaking
#   old binaries. The mask is the important idea: a filesystem can say "I do
#   not track btime" instead of returning a zero that looks like 1970.
#
# Caveats
#   - btime is optional. ext4 and xfs usually provide it; some network and
#     virtual filesystems do not, and the demo prints btime_present=no.
#   - The ctypes layout is LP64-specific. It is correct for Linux x86-64 and
#     aarch64, but the spare fields and alignment must be rechecked on a new
#     ABI, and a 32-bit build needs different alignment.
#   - os.stat() in Python already exposes st_birthtime on some platforms, but
#     it hides the mask; statx is how you distinguish "not supported" from
#     "actually zero".
#   - The syscall number table is deliberately small. Unknown architectures
#     skip instead of guessing.
# ==============================================
