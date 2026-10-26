#!/usr/bin/env python3
"""MAGIC: RLIMIT_NOFILE is per-process, so a forked child can lower its own
soft limit until open() fails with EMFILE without touching the parent."""

import sys

if not sys.platform.startswith("linux"):
    print("skipped: non-Linux platform")
    sys.exit(0)

import errno
import os
import resource


def child_test():
    _, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
    target = 16 if hard == resource.RLIM_INFINITY or hard >= 16 else hard
    resource.setrlimit(resource.RLIMIT_NOFILE, (target, hard))
    child_soft = resource.getrlimit(resource.RLIMIT_NOFILE)[0]

    held = []
    try:
        while True:
            try:
                held.append(os.open("/dev/null", os.O_RDONLY))
            except OSError as e:
                if e.errno != errno.EMFILE:
                    raise
                break
        print("hit_emfile=yes")
        print(f"child_soft={child_soft}")
    finally:
        for fd in held:
            os.close(fd)


def main():
    before = resource.getrlimit(resource.RLIMIT_NOFILE)

    try:
        pid = os.fork()
    except OSError as e:
        print(f"skipped: fork: {e.strerror}")
        return 0

    if pid == 0:
        try:
            child_test()
        finally:
            sys.stdout.flush()          # _exit() would drop buffered output
        os._exit(0)

    _, status = os.waitpid(pid, 0)
    after = resource.getrlimit(resource.RLIMIT_NOFILE)
    ok = os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0
    print("parent_unchanged={}".format(
        "yes" if before == after and ok else "no"))
    soft = after[0]
    high = soft == resource.RLIM_INFINITY or soft >= 1024
    print("parent_soft>=1024={}".format("yes" if high else "no"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ================= EXPLANATION =================
# What it does
#   Reads the RLIMIT_NOFILE soft/hard pair, forks, and has the child lower its
#   soft limit to 16 and open /dev/null until the kernel answers EMFILE. The
#   child reports that it hit the limit; the parent, after waitpid(), proves
#   its own limits are untouched.
#
# Step by step
#   1. resource.getrlimit(RLIMIT_NOFILE) returns (soft, hard). The soft limit
#      is what the kernel enforces; the hard limit is the ceiling the soft
#      limit can be raised to.
#   2. os.fork() duplicates the process. Limits are per-process state and are
#      inherited by the child at fork.
#   3. The child calls setrlimit((16, hard)): lowering the soft limit needs no
#      privilege, and the hard limit is left as it was (or the target is
#      clamped if the hard limit is already below 16).
#   4. The child opens /dev/null in a loop, keeping each fd, until os.open()
#      raises OSError(EMFILE). EMFILE means "this process's fd table is full".
#   5. The child prints "hit_emfile=yes" and "child_soft=16", closes the held
#      fds, flushes stdout, and calls os._exit(0) so the parent's buffered
#      stdio is not duplicated.
#   6. The parent waits, re-reads its limit, and confirms it is byte-for-byte
#      what it was before the fork, then checks it is at least 1024.
#
# Output
#   hit_emfile=yes
#   child_soft=16
#   parent_unchanged=yes
#   parent_soft>=1024=yes
#
# Why it matters
#   File descriptors are a finite per-process resource, and an unbounded
#   server can exhaust them under load. Service managers (systemd's
#   LimitNOFILE, Docker's --ulimit) set limits before exec; daemons often
#   raise their own soft limit toward the hard limit at startup. A supervisor
#   can also fork a constrained worker with a low limit so a bug in the worker
#   cannot exhaust the host. The parent/child split here is the whole point:
#   limits are inherited, not shared.
#
# Caveats
#   - Limits are per-process, not per-user or per-system. The child's change
#     does not affect the parent, its siblings, or the parent's later exec.
#     The child's own later exec, however, inherits the lowered soft limit.
#     System-wide exhaustion shows up as ENFILE, not EMFILE.
#   - Raising the soft limit is allowed up to the hard limit; raising the hard
#     limit needs CAP_SYS_RESOURCE. Root can therefore escape a low soft limit,
#     so a limit is a safety net, not a security boundary, unless the process
#     is also de-privileged.
#   - Existing descriptors are not closed when the limit is lowered. Soft 16
#     includes fds 0, 1, and 2, so the child can open only 13 more in the usual
#     case; do not read "16" as 16 new files.
#   - A high RLIMIT_NOFILE does not make select() safe: fd_set is capped at
#     FD_SETSIZE (1024) on typical builds, so use poll() or epoll() for large
#     fd values.
#   - Containers and service managers may set different limits before exec,
#     so the value here is only this process's. A hard limit of RLIM_INFINITY
#     (reported as -1 by Python) must be clamped before use, which this child
#     does.
# ==============================================
