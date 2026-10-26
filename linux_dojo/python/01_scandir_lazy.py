#!/usr/bin/env python3
"""MAGIC: os.scandir() answers entry types from the d_type byte that
getdents64() already returned, so most names never need a stat() call."""

import os
import stat
import sys
import tempfile

if not sys.platform.startswith("linux"):
    print("skipped: non-Linux platform")
    sys.exit(0)


def kind_from_mode(mode):
    if stat.S_ISLNK(mode):
        return "symlink"
    if stat.S_ISDIR(mode):
        return "dir"
    if stat.S_ISREG(mode):
        return "file"
    return "other"


def main():
    with tempfile.TemporaryDirectory() as tmp:
        for name in ("alpha.txt", "beta.log"):
            with open(os.path.join(tmp, name), "w") as f:
                f.write(name)
        os.mkdir(os.path.join(tmp, "subdir"))
        os.symlink("alpha.txt", os.path.join(tmp, "link.txt"))

        # One getdents64 stream; classification reads the cached type byte.
        with os.scandir(tmp) as it:
            entries = sorted(it, key=lambda e: e.name)
            saw_symlink = False
            for entry in entries:
                if entry.is_symlink():
                    saw_symlink = True
                    kind = "symlink"
                elif entry.is_dir(follow_symlinks=False):
                    kind = "dir"
                elif entry.is_file(follow_symlinks=False):
                    kind = "file"
                else:
                    kind = "other"

                # Cross-check with an explicit fstatat(): same answer, one stat.
                st = os.stat(entry.path, follow_symlinks=False)
                assert kind == kind_from_mode(st.st_mode)
                print(f"{entry.name}:{kind}")

        print("symlink=yes" if saw_symlink else "symlink=no")
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ================= EXPLANATION =================
# What it does
#   Builds a temporary directory holding two regular files, a subdirectory,
#   and a relative symlink, then lists it with os.scandir(). Each entry is
#   classified without a stat() where the filesystem supplies d_type, and the
#   result is printed as sorted "name:type" lines followed by "symlink=yes".
#
# Step by step
#   1. tempfile.TemporaryDirectory() creates and later removes the tree; the
#      context manager is the try/finally.
#   2. os.scandir() wraps getdents64(). On Linux each directory record carries
#      a d_type byte, so the directory itself is read once. Entries are
#      classified while the iterator (and its directory fd) is still open.
#   3. is_symlink() reads the cached DT_LNK bit. is_file()/is_dir() with
#      follow_symlinks=False consult the cached d_type (DT_REG, DT_DIR) and do
#      not call stat() unless d_type is DT_UNKNOWN.
#   4. os.stat(..., follow_symlinks=False) is called explicitly as a check; it
#      is one fstatat() per entry, exactly the cost d_type avoided.
#   5. st_mode bits map back to the same "file"/"dir"/"symlink" strings.
#   6. sorted() makes the listing stable; the final line records that a
#      symlink was seen.
#
# Output
#   alpha.txt:file
#   beta.log:file
#   link.txt:symlink
#   subdir:dir
#   symlink=yes
#
# Why it matters
#   A recursive walk over a million files is dominated by metadata syscalls,
#   not by name bytes. On ext4, XFS, Btrfs, and tmpfs the type is already in
#   the directory entry, so scandir() removes roughly one stat() per file from
#   tools like find, du, and tree. That is why Python's os.walk() was
#   rewritten on top of scandir() in Python 3.5, and why directory walkers
#   that call stat() on every entry are slower than they need to be.
#
# Caveats
#   - The fast path is a filesystem property, not a guarantee. Some network
#     filesystems (notably NFS) and some FUSE implementations return
#     DT_UNKNOWN, and CPython then silently falls back to fstatat() for every
#     entry. The output is the same; only the syscall count grows.
#   - d_type is a snapshot from getdents64() time. A file can be replaced or
#     change type afterwards, and the cached type will be stale (TOCTOU).
#   - follow_symlinks=False changes the semantics: a symlink to a directory is
#     reported as "symlink", not "dir", and is_file() on a dangling symlink is
#     False rather than an error.
#   - This is the Linux getdents64 path; other platforms and network
#     filesystems may implement scandir() differently.
# ==============================================
