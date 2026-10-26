#!/usr/bin/env python3
"""MAGIC: /proc/<pid>/stat puts a free-form comm inside parentheses, so parse
between the first '(' and the last ')' and count fields only after that."""

import os
import subprocess
import sys

if not sys.platform.startswith("linux"):
    print("skipped: non-Linux platform")
    sys.exit(0)


def read_stat(pid):
    """Return (comm, ppid) for pid, or None if the process vanished."""
    try:
        with open(f"/proc/{pid}/stat", "rb") as f:
            data = f.read()
    except OSError:
        return None
    lpar = data.find(b"(")
    rpar = data.rfind(b")")
    if lpar < 0 or rpar < 0:
        return None
    comm = data[lpar + 1:rpar].decode("utf-8", "replace")
    fields = data[rpar + 2:].split()
    # fields[0] is state (field 3 of the line); fields[1] is ppid (field 4).
    if len(fields) < 2:
        return None
    return comm, int(fields[1])


def main():
    # sleep keeps the children alive; even after exit they stay zombies
    # (and in /proc) until wait(), so the snapshot below is not racing them.
    kids = [subprocess.Popen(["sleep", "0.3"]) for _ in range(2)]
    try:
        me = os.getpid()
        table = {}
        for name in os.listdir("/proc"):
            if name.isdigit():
                got = read_stat(int(name))
                if got is not None:
                    table[int(name)] = got

        children = {}
        for pid, (_comm, ppid) in table.items():
            children.setdefault(ppid, []).append(pid)

        lines = []
        stack = [(me, 0)]
        while stack:
            pid, depth = stack.pop()
            # The root is the interpreter; label it "python" for stable output.
            comm = "python" if pid == me else table[pid][0]
            lines.append((depth, comm))
            for child in children.get(pid, ()):
                stack.append((child, depth + 1))

        for depth, comm in sorted(lines):
            print(f"{depth} {comm}")
        print(f"children={len(children.get(me, ()))}")
    finally:
        for p in kids:
            p.terminate()
        for p in kids:
            p.wait()
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ================= EXPLANATION =================
# What it does
#   Starts two `sleep 0.3` children, scans every numeric entry in /proc, and
#   parses /proc/<pid>/stat to build a parent -> children map. It then walks
#   that map from its own PID and prints each reachable process as a sorted
#   "depth comm" line, ending with the number of direct children.
#
# Step by step
#   1. subprocess.Popen() forks and execs sleep twice, so both are real
#      children of this process.
#   2. For each /proc/<pid>/stat, read the line as bytes. The format is
#      "pid (comm) state ppid ...". comm is free-form: it may contain spaces,
#      parentheses, and newlines, and is truncated to 15 bytes.
#   3. Therefore the only safe anchors are the FIRST '(' and the LAST ')'.
#      Everything between them is comm; everything after ") " is space-
#      separated numeric fields.
#   4. In that tail, split()[0] is state (field 3 of the whole line) and
#      split()[1] is ppid (field 4 overall). Python indices are shifted by
#      three because pid, comm, and the ')' are gone.
#   5. Build {ppid: [pid, ...]} and depth-first walk from os.getpid(). The root
#      is printed at depth 0; its comm is normalised to "python".
#   6. Children are terminated and reaped in finally so no zombies survive.
#
# Output
#   0 python
#   1 sleep
#   1 sleep
#   children=2
#
# Why it matters
#   /proc is the process table as a filesystem: the kernel exposes state that
#   no portable library call does, at the cost of a text format with one nasty
#   corner (comm). Getting the comm/ppid split right is the difference between
#   a working process tree and a parser that breaks on a process named
#   "foo) bar" or "(x)". Supervisors and container runtimes do exactly this.
#
# Caveats
#   - The comm field is attacker-influenced and can contain ')' and newlines;
#     a hand-rolled split() or a regex on the whole line is wrong. rfind(')')
#     is required: the closing delimiter is the last ')' on the line even when
#     comm itself ends in ')'. Parse from the anchors, never from the left.
#   - PID reuse is real: a process can exit between listdir() and open(), and
#     the number can be recycled before our open(). We only lose the entry or
#     attach the wrong process for one snapshot; a pidfd opened before the exit
#     would pin the right process, but discovery itself still races.
#   - The scan is a snapshot, not a lock. Processes can fork or exit mid-walk.
#   - /proc must be mounted (it is in normal Linux userlands and containers),
#     and a PID namespace hides processes outside it.
#   - The 0.3 s sleep only keeps the children alive long enough to be seen;
#     they are terminated if still sleeping and always reaped in finally.
# ==============================================
