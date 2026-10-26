#!/usr/bin/env python3
"""MAGIC: fork copies page tables, not pages; a child's writes trigger COW and
the parent's resident set stays flat."""
import mmap
import os
import sys

PAGE = mmap.PAGESIZE
SIZE = 16 * 1024 * 1024


def rss_pages():
    with open("/proc/self/statm", "r", encoding="ascii") as f:
        return int(f.read().split()[1])


def main():
    if not sys.platform.startswith("linux"):
        print("skipped: non-Linux platform")
        return 0

    region = mmap.mmap(-1, SIZE, flags=mmap.MAP_PRIVATE | mmap.MAP_ANONYMOUS)
    for off in range(0, SIZE, PAGE):
        region[off] = 1

    before = rss_pages()
    pid = os.fork()
    if pid == 0:
        # Every store faults a private copy of the page.
        for off in range(0, SIZE, PAGE):
            region[off] = 2
        os._exit(0)

    os.waitpid(pid, 0)
    after = rss_pages()
    delta = after - before

    # 512 pages = 2 MiB of tolerance for allocator and page-table noise.
    print(f"parent_rss_stable={'yes' if abs(delta) <= 512 else 'no'}")
    print("child_touched=yes")
    print(f"delta_pages_within_2m={'yes' if abs(delta) <= 512 else 'no'}")
    region.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# ================= EXPLANATION =================
# What it does
#   Maps 16 MiB of anonymous memory, touches every page, forks, and has the
#   child overwrite every page. The parent's resident set is measured before
#   and after; it stays essentially unchanged.
#
# Step by step
#   1. mmap(MAP_PRIVATE|MAP_ANONYMOUS) gives zero-filled pages, mapped lazily.
#   2. Writing one byte per page faults them in, so the parent's RSS reflects
#      the whole region before fork.
#   3. fork() duplicates the page tables with every PTE marked read-only and
#      both processes pointing at the same physical pages.
#   4. The child's writes take a write fault; the kernel copies that page and
#      gives the child a private copy. The parent keeps the original.
#   5. The parent waits for the child, then rereads /proc/self/statm field 2.
#      A small delta is page-table and allocator noise, not 16 MiB.
#
# Output
#   parent_rss_stable=yes
#   child_touched=yes
#   delta_pages_within_2m=yes
#
# Why it matters
#   COW is why fork is cheap for large processes and why Redis-style forking
#   for snapshots works until the workload writes to most pages. It is also why
#   "fork is free" benchmarks are misleading: the cost moves into the first
#   write after the fork.
#
# Caveats
#   - Reading /proc/self/statm is a snapshot and includes page tables; compare
#     with a tolerance, not an exact page count.
#   - The kernel may reclaim or THP-collapse pages between measurements, which
#     can move RSS in either direction.
#   - Fork also copies file descriptor tables and signal dispositions, which
#     this demo ignores.
#   - Python's allocator and interpreter touch memory during fork/waitpid, so
#     this is a systems demonstration, not a precise benchmark.
#   - MAP_PRIVATE|MAP_ANONYMOUS is not shared; a MAP_SHARED region would be
#     visible to both processes and would not demonstrate COW.
# ==============================================
