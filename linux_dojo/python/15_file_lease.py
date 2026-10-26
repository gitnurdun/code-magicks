#!/usr/bin/env python3
"""MAGIC: a write lease makes a conflicting open() block in the kernel until
the lease holder releases it or the lease-break timer fires."""
import os
import signal
import sys
import tempfile
import time


def main():
    if not sys.platform.startswith("linux"):
        print("skipped: non-Linux platform")
        return 0
    try:
        import fcntl
    except ImportError:
        print("skipped: fcntl unavailable")
        return 0
    f_setlease = getattr(fcntl, "F_SETLEASE", 1024)
    f_unlck = getattr(fcntl, "F_UNLCK", 2)
    f_wrlck = getattr(fcntl, "F_WRLCK", 1)

    signal.signal(signal.SIGIO, signal.SIG_IGN)
    fd, path = tempfile.mkstemp(prefix="lease_")
    try:
        os.write(fd, b"lease")
        os.lseek(fd, 0, os.SEEK_SET)
        try:
            fcntl.fcntl(fd, f_setlease, f_wrlck)
        except OSError as exc:
            print(f"skipped: F_SETLEASE failed ({exc.strerror})")
            return 0

        ready_r, ready_w = os.pipe()
        pid = os.fork()
        if pid == 0:
            os.close(ready_r)
            os.close(fd)  # do not inherit the lease-holding description
            try:
                other = os.open(path, os.O_WRONLY)
                os.write(ready_w, b"opened")
                os.close(other)
            except OSError:
                os.write(ready_w, b"error")
            os._exit(0)

        os.close(ready_w)
        time.sleep(0.2)
        done, _ = os.waitpid(pid, os.WNOHANG)
        blocked = done == 0

        fcntl.fcntl(fd, f_setlease, f_unlck)
        result = os.read(ready_r, 16)
        os.close(ready_r)
        os.waitpid(pid, 0)

        print(f"blocked={'yes' if blocked else 'no'}")
        print(f"released={'yes' if result == b'opened' else 'no'}")
    finally:
        try:
            fcntl.fcntl(fd, f_setlease, f_unlck)
        except OSError:
            pass
        os.close(fd)
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# ================= EXPLANATION =================
# What it does
#   Takes a write lease on a temp file, forks a child that tries to open the
#   same file for writing, observes that the child is blocked, releases the
#   lease, and confirms the child then opens the file.
#
# Step by step
#   1. F_SETLEASE with F_WRLCK asks the kernel to notify this process before
#      another process opens the file for read or write. The caller must own
#      the file or have CAP_LEASE, and the file must not be mmapped by others.
#   2. The child is forked and attempts open(path, O_WRONLY). Because a write
#      lease is held, the open sleeps in the kernel instead of returning.
#   3. After 200 ms, waitpid(WNOHANG) returns 0, proving the child has not
#      finished. It is blocked in open, not in user space.
#   4. F_UNLCK drops the lease. The blocked open completes and the child writes
#      "opened" to the pipe, which the parent reads.
#   5. SIGIO is ignored because a lease break normally delivers it. The kernel
#      also starts a lease-break timer; if the holder does not release in time,
#      the kernel forcibly breaks the lease.
#
# Output
#   blocked=yes
#   released=yes
#
# Why it matters
#   Leases let a process cache or lock a file without polling: the kernel tells
#   you when someone else wants it. They underlie NFS client caching and are
#   useful for "I am the only writer" daemons. The kernel-side blocking is the
#   key difference from flock/fcntl locks, which only advise cooperating
#   processes; a lease is enforced against ordinary open().
#
# Caveats
#   - Leases are Linux-specific and require ownership or CAP_LEASE; the demo
#     prints skipped on EPERM.
#   - A lease is broken by any conflicting open, even one that later fails; the
#     holder gets SIGIO and should release promptly or the kernel breaks it
#     after /proc/sys/fs/lease-break-time seconds.
#   - Leases are per-inode, not per-file-description, and do not compose with
#     mandatory locking or some network filesystems.
#   - The demo ignores SIGIO, so it does not show the notification path. In a
#     real daemon, handle SIGIO and drop the lease before doing more I/O.
#   - If the child is blocked and the parent crashes without releasing, the
#     kernel's break timer eventually rescues the child.
# ==============================================
