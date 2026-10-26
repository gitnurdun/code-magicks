/* ASM MAGICK #24 - clflush: evict a cache line and watch the reload stall.
   Build: gcc -O2 -std=c11 24_clflush.c -o 24_clflush.exe
   MAGIC: one instruction invalidates a line from every cache level, which
   makes the cost of a cache miss visible on demand. */
#include <stdio.h>
#include <stdint.h>

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi) : : "memory");
    return ((uint64_t)hi << 32) | lo;
}

static inline unsigned load_byte(const unsigned char *p)
{
    unsigned v;
    __asm__ volatile("movzbl (%1), %0" : "=r"(v) : "r"(p) : "memory");
    return v;
}

int main(void)
{
    static unsigned char __attribute__((aligned(64))) line[64];
    line[0] = 0x5A;

    /* Warm it: the first load brings the line into L1. */
    (void)load_byte(line);
    uint64_t t0 = tsc();
    unsigned v1 = load_byte(line);
    uint64_t t1 = tsc();

    /* CLFLUSH invalidates this line from all caches; MFENCE waits for it. */
    __asm__ volatile("clflush (%0)\n\tmfence" : : "r"(line) : "memory");
    uint64_t t2 = tsc();
    unsigned v2 = load_byte(line);
    uint64_t t3 = tsc();

    printf("[24] cached reload : %4llu cycles (value 0x%02x)\n",
           (unsigned long long)(t1 - t0), v1);
    printf("[24] flushed reload: %4llu cycles (value 0x%02x)\n",
           (unsigned long long)(t3 - t2), v2);
    printf("[24] miss penalty : %4llu cycles\n",
           (unsigned long long)(t3 - t2) - (unsigned long long)(t1 - t0));
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     CLFLUSH m8 invalidates the cache line containing the address from
     every level of the cache hierarchy for the executing core, writing
     back dirty data first. CLFLUSHOPT is the same but can be reordered;
     CLWB writes back without invalidating.

   What the demo measures
     The first load warms line[0] into L1. The second load should hit L1
     and take a few cycles. Then CLFLUSH evicts it (and MFENCE waits for
     the flush to complete), so the next load must come from memory,
     paying the full latency. The difference is the cache-miss penalty.

   Why this matters
     - Persistent memory: CLWB/CLFLUSH + SFENCE is how you make stores
       durable; the ordering rules are subtle.
     - Cache-side-channel research: Flush+Reload and Prime+Probe use
       CLFLUSH to create a known state.
     - Self-modifying/JIT code: after writing instructions, flush the
       relevant lines so the instruction fetch sees them.
     - Benchmarks: deliberately cold-starting a data structure to measure
       its real memory behavior.
     - Non-temporal stores: MOVNTI/streaming stores bypass the cache; a
       later CLFLUSH is not needed, but you do need SFENCE for ordering.

   Why the numbers vary
     A reload from L3 is ~40 cycles; from DRAM ~200-300. Which one you see
     depends on whether another core kept the line, the prefetcher, and
     the page mapping. On a VM the hypervisor may emulate CLFLUSH and the
     number becomes meaningless. Run it several times.

   Output on this machine (ranges across runs; TSC granularity/VM vary)
     [24] cached reload : ~30-90 cycles (value 0x5a)
     [24] flushed reload: ~200-550 cycles (value 0x5a)
     [24] miss penalty : ~150-460 cycles

   Gotchas
     - CLFLUSH is a privileged? No, it is unprivileged, but it can be
       intercepted by a hypervisor.
     - It flushes one 64-byte line (or the line size reported by
       CPUID.80000006H); it is not a whole-cache flush.
     - MFENCE after CLFLUSH is needed for ordering, but it also serializes
       the pipeline; measure the flush itself separately if that matters.
     - CLFLUSH on a line that is not present is cheap; the cost appears on
       the next access, not necessarily in the flush instruction.
     - Do not use this as a "clear memory" primitive: it is not
       guaranteed to zero anything and is not a security boundary.
   ============================================== */
