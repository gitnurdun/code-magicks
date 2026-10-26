#!/usr/bin/env python3
"""MAGIC: a memfd is an anonymous RAM file whose seals the kernel enforces,
turning it into immutable shared memory or a safe exec payload."""

import sys

if not sys.platform.startswith("linux"):
    print("skipped: non-Linux platform")
    sys.exit(0)

import errno
import fcntl
import mmap
import os

if not hasattr(os, "memfd_create") or not hasattr(os, "MFD_ALLOW_SEALING"):
    print("skipped: memfd_create not available")
    sys.exit(0)

SEAL_NAMES = ("F_ADD_SEALS", "F_SEAL_WRITE", "F_SEAL_SHRINK",
              "F_SEAL_GROW", "F_SEAL_SEAL")
if not all(hasattr(fcntl, name) for name in SEAL_NAMES):
    print("skipped: memfd seals not supported")
    sys.exit(0)

SEALS = (fcntl.F_SEAL_WRITE | fcntl.F_SEAL_SHRINK |
         fcntl.F_SEAL_GROW | fcntl.F_SEAL_SEAL)


def main():
    try:
        # MFD_ALLOW_SEALING must be requested at creation; it cannot be
        # turned on for an existing memfd.
        fd = os.memfd_create("secret", os.MFD_ALLOW_SEALING)
    except OSError as e:
        print(f"skipped: memfd_create: {e.strerror}")
        return 0

    try:
        os.write(fd, b"sealed bytes")

        try:
            fcntl.fcntl(fd, fcntl.F_ADD_SEALS, SEALS)
        except OSError as e:
            print(f"skipped: F_ADD_SEALS: {e.strerror}")
            return 0

        denied = False
        try:
            os.write(fd, b"x")
        except OSError as e:
            if e.errno != errno.EPERM:
                raise
            denied = True

        with mmap.mmap(fd, 12, prot=mmap.PROT_READ) as mm:
            data = mm.read(12)

        print("sealed=yes write_denied={} data={}".format(
            "yes" if denied else "no", data.decode()))
    finally:
        os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ================= EXPLANATION =================
# What it does
#   Creates a 12-byte anonymous file with memfd_create(), seals it against
#   writes, growth, shrinking, and further sealing, then proves that a second
#   write fails with EPERM while a read-only mmap still shows the original
#   bytes.
#
# Step by step
#   1. memfd_create("secret", MFD_ALLOW_SEALING) returns an fd to a file that
#      exists only in kernel memory (backed by tmpfs). "secret" is a label for
#      /proc/<pid>/fd, not a path; the file has no directory entry.
#   2. write() stores b"sealed bytes" (12 bytes).
#   3. fcntl(F_ADD_SEALS) adds four seals in one call:
#        F_SEAL_WRITE  - writes and writable shared mappings fail with EPERM
#        F_SEAL_GROW   - the file cannot get longer
#        F_SEAL_SHRINK - the file cannot get shorter
#        F_SEAL_SEAL   - no further seals may be added
#      MFD_ALLOW_SEALING was required; otherwise F_ADD_SEALS fails EPERM.
#   4. The second write() raises OSError(EPERM), caught and recorded.
#   5. mmap(..., prot=PROT_READ) maps the file read-only; read(12) returns
#      the original bytes. The seal protects the file, not the reader.
#   6. The fd is closed in finally; the mapping is closed by its with block.
#
# Output
#   sealed=yes write_denied=yes data=sealed bytes
#
# Why it matters
#   Sealed memfds are how one process hands another a piece of data that
#   cannot be mutated after the fact: pipe the fd over SCM_RIGHTS, then exec
#   the contents with fexecve()/execveat() without ever writing a file to
#   disk. System services use them for immutable configuration, and runtimes
#   use them to load code into a sandbox. The guarantee comes from the kernel
#   checking the seal on every write, not from convention.
#
# Caveats
#   - The file lives in RAM/tmpfs and is charged to memory; a large memfd is
#     not free, and it disappears when the last fd is closed.
#   - MFD_ALLOW_SEALING must be set when the memfd is created. Seals can never
#     be removed, and F_SEAL_SEAL permanently blocks adding more, so decide
#     the whole set before applying it.
#   - F_SEAL_WRITE fails with EBUSY if the file still has a writable shared
#     mapping; that is why a process should seal before sharing, and why
#     F_SEAL_FUTURE_WRITE exists for mappings already handed out.
#   - Seals do not hide the data: any holder of the fd can read it. They only
#     prevent modification (and the grow/shrink classes of modification).
#   - Needs Linux 3.17+ and a kernel with memfd support; on ENOSYS the program
#     skips. The seal numbers are stable ABI but the Python fcntl constants
#     are only present if the build found them.
# ==============================================
