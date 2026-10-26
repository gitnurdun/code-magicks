/* ASM MAGICK #25 - mfence/lfence/sfence: the three orders.
   Build: gcc -O2 -std=c11 25_fences.c -o 25_fences.exe
   MAGIC: fences do not move data; they constrain the order in which the
   memory system is allowed to make it visible. */
#include <stdio.h>
#include <stdint.h>

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi) : : "memory");
    return ((uint64_t)hi << 32) | lo;
}

#define N 2000000

int main(void)
{
    uint64_t t0 = tsc();
    for (int i = 0; i < N; i++)
        __asm__ volatile("mfence" : : : "memory");
    uint64_t t1 = tsc();

    uint64_t t2 = tsc();
    for (int i = 0; i < N; i++)
        __asm__ volatile("lfence" : : : "memory");
    uint64_t t3 = tsc();

    uint64_t t4 = tsc();
    for (int i = 0; i < N; i++)
        __asm__ volatile("sfence" : : : "memory");
    uint64_t t5 = tsc();

    printf("[25] mfence: %6.2f cycles/op\n", (double)(t1 - t0) / N);
    printf("[25] lfence: %6.2f cycles/op\n", (double)(t3 - t2) / N);
    printf("[25] sfence: %6.2f cycles/op\n", (double)(t5 - t4) / N);
    return 0;
}

/* ================= EXPLANATION =================
   What each fence orders
     LFENCE  : loads before it are ordered before loads after it. It also
               serializes instruction execution on Intel (a speculation
               barrier), which is why it is used to make RDTSC precise.
     SFENCE  : stores before it are ordered before stores after it. Needed
               for non-temporal stores (MOVNT*) and write-combining memory.
     MFENCE  : both loads and stores. A full fence.
     None of them flush caches or wait for a store to reach DRAM; they
     order visibility as observed by other cores.

   Why x86 needs them at all
     x86 has a strong memory model (TSO): loads are not reordered with
     loads, stores are not reordered with stores, and loads are not
     reordered with older stores. The two places it still reorders are:
       - a store followed by a load to a different address (StoreLoad);
       - non-temporal/streaming stores.
     MFENCE is the StoreLoad barrier. A locked instruction (XCHG, LOCK ADD,
     CMPXCHG) also implies a full barrier, which is why atomics do not
     need an explicit MFENCE.

   What the numbers mean
     MFENCE is the most expensive because it drains the store buffer and
     waits for the load queue. LFENCE is cheaper but still serializes.
     SFENCE is cheapest: it only orders stores. Exact cycles vary with the
     microarchitecture and with what is in flight.

   Where they appear
     - Seqlocks (#12) and RCU (#51): the writer needs a release barrier
       (a compiler barrier plus a store fence, or an atomic store with
       release semantics).
     - DCLP (#55): the publication must be release-ordered.
     - Non-temporal stores in a game engine's streaming buffer: SFENCE
       before the GPU/consumer reads it.
     - RDTSC profiling: LFENCE on both sides.

   Output on this machine (one run; varies with load and CPU)
     [25] mfence: ~25 cycles/op
     [25] lfence:  ~8 cycles/op
     [25] sfence:  ~3.5 cycles/op

   Gotchas
     - A compiler barrier (`asm volatile("" ::: "memory")`) orders the
       compiler, not the CPU. You usually need both.
     - MFENCE on AMD and Intel has different microarchitectural cost;
       `lock add` is sometimes a cheaper full barrier.
     - LFENCE is not a full barrier for stores; do not use it to publish
       data.
     - On x86, a plain `mov` store followed by a `mov` load can be
       reordered; that is the StoreLoad case MFENCE fixes.
   ============================================== */
