/* ASM MAGICK #06 - pause: the polite spin.
   Build: gcc -O2 -std=c11 06_pause.c -o 06_pause.exe
   MAGIC: one hint instruction turns a cache-line hammer into a
   power-friendly, SMT-friendly wait. */
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
    const int N = 1000000;

    uint64_t t0 = tsc();
    for (int i = 0; i < N; i++)
        __asm__ volatile("pause");
    uint64_t t1 = tsc();

    uint64_t t2 = tsc();
    for (int i = 0; i < N; i++)
        __asm__ volatile("nop");
    uint64_t t3 = tsc();

    printf("[06] %d pause : %8llu cycles (%.1f each)\n",
           N, (unsigned long long)(t1 - t0), (double)(t1 - t0) / N);
    printf("[06] %d nop   : %8llu cycles (%.1f each)\n",
           N, (unsigned long long)(t3 - t2), (double)(t3 - t2) / N);
    return 0;
}

/* ================= EXPLANATION =================
   What PAUSE does
     On pre-Skylake Intel it is a 2-byte REP NOP that mostly just delays a
     few cycles. On Skylake and later it waits roughly 140 cycles; AMD Zen
     uses a shorter delay. Its jobs:
       - de-pipeline the spin loop, so the core stops speculatively
         hammering the lock's cache line;
       - save power and leave execution resources to the SMT sibling;
       - avoid the memory-order violation machine clear that a tight
         test-and-set loop can trigger when the lock is released.

   Why it is not a no-op
     A spin loop doing `lock cmpxchg` in a tight loop generates a cache
     line in Modified state and a stream of coherence traffic. Every failed
     attempt also costs a full locked operation. Adding PAUSE spaces the
     attempts out, cutting contention dramatically. This is why every
     serious spinlock is "test with a plain load, then pause, then try the
     atomic only when it looks free".

   Reading the numbers
     The pause loop is intentionally much slower per iteration than the nop
     loop. That is the point: the waiting thread burns fewer coherence
     resources. If you are measuring throughput of a lock, compare total
     wall time and cache-miss counts, not just cycles inside pause.

   A better spinlock shape
       while (atomic_load_explicit(&lock, memory_order_relaxed))
           __builtin_ia32_pause();
       if (atomic_exchange(&lock, 1)) { ... slow path ... }
     The first loop does not write the line; only the final exchange does.
     Add an exponential backoff and then a futex/WaitOnAddress sleep.

   Gotchas
     - PAUSE is x86-only. ARM has YIELD; RISC-V has no exact equivalent.
     - On a single-core machine, spinning without yielding can deadlock
       progress if the lock holder was preempted.
     - The duration is microarchitectural and not contractual; never use
       PAUSE as a calibrated delay.
     - Do not put PAUSE in a loop that also does useful work; it is a hint,
       not a scheduler.
   ============================================== */
