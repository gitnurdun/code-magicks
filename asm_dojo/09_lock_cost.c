/* ASM MAGICK #09 - the price of LOCK: 5 million increments, twice.
   Build: gcc -O2 -std=c11 09_lock_cost.c -o 09_lock_cost.exe
   MAGIC: the same add, one prefix byte apart, is an order of magnitude
   slower because LOCK buys global visibility. */
#include <stdio.h>
#include <stdint.h>

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi) : : "memory");
    return ((uint64_t)hi << 32) | lo;
}

int main(void)
{
    const int N = 5000000;
    long x = 0;

    uint64_t t0 = tsc();
    for (int i = 0; i < N; i++)
        __asm__ volatile("incl %0" : "+m"(x) : : "cc", "memory");
    uint64_t t1 = tsc();

    uint64_t t2 = tsc();
    for (int i = 0; i < N; i++)
        __asm__ volatile("lock incl %0" : "+m"(x) : : "cc", "memory");
    uint64_t t3 = tsc();

    double plain = (double)(t1 - t0) / N;
    double locked = (double)(t3 - t2) / N;
    printf("[09] plain inc : %8.2f cycles/op (total %llu)\n",
           plain, (unsigned long long)(t1 - t0));
    printf("[09] lock  inc : %8.2f cycles/op (total %llu)\n",
           locked, (unsigned long long)(t3 - t2));
    printf("[09] ratio     : %.1fx   (x=%ld)\n", locked / plain, x);
    return 0;
}

/* ================= EXPLANATION =================
   What LOCK actually does
     A normal `inc [mem]` is a load, an add, and a store. Between the load
     and the store another core can write the same address, so the result
     is a lost update. LOCK changes the microarchitectural protocol:
       - it asserts the LOCK# signal (or, for cacheable WB memory, takes
         exclusive ownership of the line and uses a cache-lock protocol);
       - no other core can read or write that line until the operation
         commits;
       - it acts as a full fence for the executing core.
     The cost is not the arithmetic; it is draining the store buffer and
     waiting for coherence ownership.

   Why measure even without threads
     On an uncontended line the locked op still serializes against the
     store buffer and often costs ~10-30 cycles instead of <1. Seeing that
     gap is the first step to understanding why "just use atomics" is not
     free and why sharding (#52) exists.

   The numbers are noisy
     Interrupts, frequency scaling and the OS scheduler perturb a 5M-loop
     measurement. The ratio is the stable part: typically 7x-40x depending
     on the microarchitecture and load. Do not hard-code the cycles.

   How to avoid the cost
     - Reduce sharing: per-thread or per-shard counters, summed on read.
     - Use relaxed atomics where ordering is not required.
     - Batch: accumulate locally, publish once.
     - Prefer read-mostly schemes (seqlock #12, RCU #51) for data that is
       rarely written.
     - Beware false sharing (#14): two independent atomics on one cache
       line contend just as hard.

   Gotchas
     - `lock` is only valid with certain instructions (add, adc, and, btc,
       btr, bts, cmpxchg, dec, inc, neg, not, or, sbb, sub, xor, xadd,
       xchg). Random prefixes are #UD.
     - The performance is a property of the cache line and the machine,
       not of the source line.
     - On a VM, LOCK can be emulated and much slower; measure there too.
   ============================================== */
