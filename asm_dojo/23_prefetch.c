/* ASM MAGICK #23 - prefetch: hide memory latency by asking early.
   Build: gcc -O2 -std=c11 23_prefetch.c -o 23_prefetch.exe
   MAGIC: a hint instruction moves a cache line before the load needs it,
   turning a stall into overlap. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi) : : "memory");
    return ((uint64_t)hi << 32) | lo;
}

#define N (8u * 1024u * 1024u)   /* 32 MiB of ints: far beyond L2/L3 */

static uint64_t sum_plain(const int *a, size_t n)
{
    uint64_t s = 0;
    for (size_t i = 0; i < n; i += 16) s += (unsigned)a[i];
    return s;
}

static uint64_t sum_prefetch(const int *a, size_t n)
{
    uint64_t s = 0;
    for (size_t i = 0; i < n; i += 16) {
        /* Prefetch one KiB ahead: 16 ints = one cache line per iteration. */
        __asm__ volatile("prefetcht0 1024(%0)" : : "r"(a + i));
        s += (unsigned)a[i];
    }
    return s;
}

int main(void)
{
    int *a = malloc(N * sizeof *a);
    if (!a) { puts("[23] allocation failed"); return 1; }
    for (size_t i = 0; i < N; i++) a[i] = (int)(i * 2654435761u);

    uint64_t t0 = tsc();
    uint64_t s1 = sum_plain(a, N);
    uint64_t t1 = tsc();
    uint64_t t2 = tsc();
    uint64_t s2 = sum_prefetch(a, N);
    uint64_t t3 = tsc();

    printf("[23] plain    : %8llu cycles (sum %llu)\n",
           (unsigned long long)(t1 - t0), (unsigned long long)s1);
    printf("[23] prefetch : %8llu cycles (sum %llu)\n",
           (unsigned long long)(t3 - t2), (unsigned long long)s2);
    printf("[23] speedup  : %.2fx\n",
           (double)(t1 - t0) / (double)(t3 - t2));
    free(a);
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     PREFETCHT0 m8 brings the line containing the address into all cache
     levels (L1/L2/L3), as a hint. PREFETCHT1/T2 target higher levels, and
     PREFETCHNTA minimizes cache pollution. The CPU may ignore all of them;
     they are not ordered with respect to loads and never fault on a bad
     address (they are dropped).

   Why prefetch helps
     A DRAM access costs ~200-300 cycles. If the loop simply loads a[i]
     and waits, the core stalls. If it issues a prefetch for a[i + K]
     before the load of a[i], the line is in flight while useful work
     continues. The ideal distance K covers the memory latency times the
     loop throughput: for a pointer chase, 8-16 lines ahead; for a
     bandwidth-bound stream, the hardware prefetcher already does this and
     software prefetch may not help.

   Why it can hurt
     - Prefetching data you never use wastes bandwidth and cache.
     - Too far ahead evicts useful lines; too close does nothing.
     - The hardware stream prefetcher is usually better for contiguous
       arrays. Software prefetch shines for pointer chasing, hash probes,
       and irregular gathers where the hardware cannot predict.
     - On a VM or a shared machine, the timing is noisy; benchmark with
       the real data structure.

   The demo's shape
     The stride is 16 ints = 64 bytes, so each iteration touches exactly
     one new cache line. That makes the memory system the bottleneck and
     makes the prefetch distance easy to reason about. The two sums must
     match; if they don't, the measurement is invalid.

   Output (varies by machine)
     [23] plain    : ~2-4 cycles per line plus DRAM latency effects
     [23] prefetch : often similar; on this machine the hardware prefetcher
                     already handles the stream, so the gain is small
     [23] speedup  : ~1.0-1.3x

   Gotchas
     - Prefetch is a hint; it is not a fence and does not wait.
     - It does not fault, but it can still consume bandwidth and power.
     - The address must be readable in the page-table sense? No: the CPU
       drops prefetches that would fault, but this is not a license to
       prefetch wild pointers in a security boundary.
     - Use `prefetchw` (3DNow!/PREFETCHW) to request exclusive ownership
       before a write, avoiding an RFO stall later.
   ============================================== */
