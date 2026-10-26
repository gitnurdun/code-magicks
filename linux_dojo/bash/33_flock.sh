#!/usr/bin/env bash
# MAGIC: flock locks an open file description, so a child that opens the lockfile again is denied while an inherited fd is not.
set -euo pipefail

tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
lock=$tmp/lock

exec 9>"$lock"

if flock -n 9; then printf 'parent_acquired=yes\n'; else printf 'parent_acquired=no\n'; fi

# 1. The child inherits fd 9: same open file description, so flock succeeds.
if bash -c 'flock -n 9' 2>/dev/null; then
  printf 'inherited_fd=acquired\n'
else
  printf 'inherited_fd=denied\n'
fi

# 2. The child opens the path again: a new open file description, denied.
if bash -c 'exec 8>"$1"; flock -n 8' _ "$lock" 2>/dev/null; then
  printf 'separate_open=acquired\n'
else
  printf 'separate_open=denied\n'
fi

exec 9>&-

# 3. Last fd to the locked description is closed, so the lock is gone.
if bash -c 'exec 8>"$1"; flock -n 8' _ "$lock" 2>/dev/null; then
  printf 'after_release=acquired\n'
else
  printf 'after_release=denied\n'
fi

# ================= EXPLANATION =================
# What it does
#   Takes a nonblocking exclusive flock on a lockfile in the parent,
#   then probes it from three children: one inheriting the locked fd,
#   one opening the path independently, and one doing so after the
#   parent closes its fd. The inherited-fd case succeeds; the
#   independent open is denied until the lock is released.
#
# Step by step
#   1. exec 9>"$lock" opens the file and keeps fd 9 open in this shell
#      for the rest of the script.
#   2. flock -n 9 takes LOCK_EX without blocking and returns 0. The
#      lock belongs to the open file description (OFD), not to the
#      number 9 and not to the process.
#   3. bash -c 'flock -n 9' inherits fd 9. It asks for a lock on the
#      same OFD, which the kernel already considers owned, so it is a
#      successful no-op rather than a second, independent lock.
#   4. The next child opens the path with its own fd 8, creating a new
#      OFD. flock -n 8 finds the OFD locked and fails with EWOULDBLOCK;
#      stderr is suppressed so only the boolean is printed.
#   5. exec 9>&- closes the parent's last reference to the OFD. The
#      kernel releases the lock, and a new independent open succeeds.
#
# Output
#   parent_acquired=yes
#   inherited_fd=acquired
#   separate_open=denied
#   after_release=acquired
#
# Why it matters
#   flock is the simple way to serialize cron jobs, package managers
#   and shell workers without a daemon. Understanding that it follows
#   the open file description explains both safe patterns (pass the
#   locked fd to a worker) and surprising tests (a child inheriting the
#   fd appears to "acquire" a lock it already holds).
#
# Caveats
#   - flock is advisory. It excludes only other flock users; it does
#     not stop plain reads/writes, and it does not interact with POSIX
#     fcntl locks.
#   - Locks attach to the open file description. dup, fork and exec
#     share it; a fresh open() creates a competing one. Closing every
#     fd for that description is what releases the lock.
#   - flock support on NFS depends on kernel and mount; on some setups
#     it is emulated or unreliable. Test on the actual filesystem.
#   - Do not unlink or rename the lockfile while holding the lock: a
#     new process opens a new inode and locks that instead.
#   - The lockfile contents are irrelevant. -n is nonblocking; without
#     it the command waits indefinitely.
#   - The exit status shows denial, but only the kernel lock is
#     reliable; a failed flock prints to stderr, which is hidden here.
# ==============================================
