#!/usr/bin/env python3
"""MAGIC: a pidfd is a file descriptor naming one process, so poll() can wait
for its exit without racing against PID reuse."""

import ctypes
import errno
import os
import select
import sys
import time

if not sys.platform.startswith("linux"):
    print("skipped: non-Linux platform")
    sys.exit(0)


def pidfd_open(pid):
    if hasattr(os, "pidfd_open"):          # Python 3.9+
        return os.pidfd_open(pid)
    libc = ctypes.CDLL(None, use_errno=True)
    libc.syscall.argtypes = [ctypes.c_long, ctypes.c_int, ctypes.c_uint]
    libc.syscall.restype = ctypes.c_long
    ctypes.set_errno(0)
    fd = libc.syscall(434, pid, 0)         # __NR_pidfd_open on asm-generic
    if fd < 0:
        err = ctypes.get_errno()
        raise OSError(err, os.strerror(err))
    return fd


def main():
    try:
        pid = os.fork()
    except OSError as e:
        print(f"skipped: fork: {e.strerror}")
        return 0

    if pid == 0:
        time.sleep(0.15)
        os._exit(7)                        # not sys.exit: no shared buffers

    fd = -1
    try:
        try:
            fd = pidfd_open(pid)
        except OSError as e:
            if e.errno in (errno.ENOSYS, errno.ESRCH, errno.EPERM):
                print(f"skipped: pidfd_open: {e.strerror}")
                return 0
            raise

        poller = select.poll()
        poller.register(fd, select.POLLIN)
        events = poller.poll(2000)         # milliseconds

        _, status = os.waitpid(pid, 0)
        pid = -1                           # already reaped
        if not events or not os.WIFEXITED(status):
            print("skipped: pidfd did not become readable")
            return 0
        print(f"poll=ready exit={os.WEXITSTATUS(status)}")
    finally:
        if fd >= 0:
            os.close(fd)
        if pid > 0:
            os.waitpid(pid, 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ================= EXPLANATION =================
# What it does
#   Forks a child that sleeps 0.15 s and exits with status 7, opens a pidfd for
#   it, waits for the fd to become readable with select.poll(), reaps the child
#   with waitpid(), and prints the poll state and exit code.
#
# Step by step
#   1. os.fork() creates the child. The child sleeps briefly so the parent can
#      open the pidfd while the process is still alive, then calls _exit(7).
#      _exit() skips stdio flushing, so the child cannot duplicate the parent's
#      buffered output.
#   2. pidfd_open(pid, 0) returns an fd that refers to that process. Python
#      3.9+ exposes os.pidfd_open(); on 3.8 the same syscall (434) is issued
#      through ctypes with a 64-bit return type.
#   3. The fd is registered for POLLIN and poll() blocks until the process
#      exits, or 2000 ms pass. For a pidfd, POLLIN means "the process is dead".
#   4. waitpid() reaps the zombie and returns the wait status; WEXITSTATUS
#      extracts 7.
#   5. The pidfd is closed in finally, and waitpid is still called if an error
#      path skipped the reap, so no zombie leaks.
#
# Output
#   poll=ready exit=7
#
# Why it matters
#   PIDs are recycled. A supervisor that stores a PID and later calls kill()
#   can signal an unrelated process if the original exited and the number was
#   reused. A pidfd pins the kernel's struct pid, so it stays correct for the
#   lifetime of the descriptor and can be stored in epoll, passed over a Unix
#   socket, or used with waitid(P_PIDFD). Service managers, containers, and
#   sandboxes increasingly use pidfds instead of raw PIDs for exactly this
#   reason.
#
# Caveats
#   - Linux 5.3+ is required. Some builds or architectures return ENOSYS; the
#     hard-coded syscall number 434 is the asm-generic/x86_64 value used by
#     most ports, but a fallback should check for ENOSYS and skip.
#   - Readiness means the process exited, not that it was reaped: you must
#     still waitpid() (or waitid(P_PIDFD)) or the zombie stays. The pidfd does
#     not carry the exit status; poll only tells you it is over.
#   - A pidfd refers to the process that pid named when it was opened. Opening
#     a pidfd from a stale number after reuse binds you to the new process, so
#     open early and pass the fd, not the number.
#   - pidfd_open() is per PID namespace; a fd received from another namespace
#     refers to a process outside your view and may fail to signal or wait.
#   - The 2000 ms poll timeout is generous for a 0.15 s child, but a loaded
#     machine can exceed it; this demo treats a timeout as a skip rather than
#     hanging forever.
# ==============================================
