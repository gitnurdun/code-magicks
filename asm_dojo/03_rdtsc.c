/* ASM MAGICK #03 - rdtsc: the cycle counter, and why it needs a fence.
   Build: gcc -O2 -std=c11 03_rdtsc.c -o 03_rdtsc.exe
   MAGIC: a 64-bit free-running counter split across two 32-bit registers. */
#include <stdio.h>
#include <stdint.h>

static inline uint64_t tsc_serialized(void)
{
    uint32_t lo, hi;
    /* lfence prevents the CPU from moving later loads before rdtsc;
       "memory" prevents the compiler from moving C accesses across it. */
    __asm__ volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi) : : "memory");
    return ((uint64_t)hi << 32) | lo;
}

int main(void)
{
    const int N = 1000000;

    uint64_t t0 = tsc_serialized();
    for (volatile int i = 0; i < N; i++) {
        /* empty loop: the volatile index forces the compare/branch */
    }
    uint64_t t1 = tsc_serialized();

    uint64_t t2 = tsc_serialized();
    for (int i = 0; i < N; i++)
        __asm__ volatile("nop");
    uint64_t t3 = tsc_serialized();

    printf("[03] %d volatile-loop iterations : %llu cycles (%.2f/iter)\n",
           N, (unsigned long long)(t1 - t0), (double)(t1 - t0) / N);
    printf("[03] %d nop iterations           : %llu cycles (%.2f/iter)\n",
           N, (unsigned long long)(t3 - t2), (double)(t3 - t2) / N);
    printf("[03] raw tsc now                 : %llu\n",
           (unsigned long long)tsc_serialized());
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     RDTSC loads the 64-bit time-stamp counter into EDX:EAX. On x86-64 the
     upper halves of rax/rdx are zeroed, so `shl rdx,32; or rax,rdx` builds
     the full value. The counter increments at a constant rate (the "TSC
     frequency", often the nominal core clock), not at the current core
     frequency - so it measures wall time, not cycles executed.

   Why lfence
     RDTSC is not a serializing instruction. The CPU may execute it before
     earlier instructions or after later ones, producing intervals that are
     too short or negative. LFENCE (or CPUID, or a locked instruction)
     drains the pipeline. Measure like this:
         lfence; rdtsc; <work>; lfence; rdtsc
     and subtract. Without the fences the numbers are indicative only.

   What the output means
     A volatile loop of 1e6 iterations is roughly one load, one add, one
     compare and one branch per iteration, but the volatile load forces a
     real memory access, so it is much slower than an empty register loop.
     Cycles/iteration above 1 is expected; on this machine the loop is the
     dominant cost, not rdtsc.

   Why kernel/game people care
     - Profiling without a syscall or timer interrupt.
     - Frame-time budgeting: read TSC at frame start/end.
     - Spin-wait timeouts and backoff.
     - Seeding cheap randomness (never for crypto).
     The invariant TSC (CPUID.80000001H:EDX[27]) is synchronized across
     cores on most modern x86, which is what makes it usable for timelines.

   Gotchas
     - Frequency varies by SKU and turbo state; calibrate against
       QueryPerformanceCounter or clock_gettime if you need nanoseconds.
     - On some virtual machines and older CPUs the TSC is not invariant or
       not synchronized between cores - never use raw TSC deltas across
       cores for ordering.
     - RDTSC is unprivileged but can be trapped (CR4.TSD); a hypervisor may
       emulate or fault it.
     - It is not a serializing barrier. Use lfence/rdtscp as appropriate.
     - Do not use it for security decisions: it is observable and predictable.
   ============================================== */
