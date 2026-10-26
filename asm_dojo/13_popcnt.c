/* ASM MAGICK #13 - popcnt: one instruction to count set bits.
   Build: gcc -O2 -std=c11 13_popcnt.c -o 13_popcnt.exe
   MAGIC: a whole SWAR reduction collapses into one 3-byte instruction. */
#include <stdio.h>
#include <stdint.h>

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi) : : "memory");
    return ((uint64_t)hi << 32) | lo;
}

static inline unsigned hw_popcnt(uint64_t x)
{
    uint64_t r;
    __asm__ volatile("popcntq %1, %0" : "=r"(r) : "r"(x));
    return (unsigned)r;
}

static int has_popcnt(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(1), "c"(0));
    return (c >> 23) & 1;   /* CPUID.1:ECX[23] */
}

/* SWAR: divide and conquer, counting pairs, nibbles, bytes, then summing
   the byte counts with one multiply. This is the classic portable fallback. */
static unsigned swar_popcnt(uint64_t x)
{
    x = x - ((x >> 1) & UINT64_C(0x5555555555555555));
    x = (x & UINT64_C(0x3333333333333333)) +
        ((x >> 2) & UINT64_C(0x3333333333333333));
    x = (x + (x >> 4)) & UINT64_C(0x0F0F0F0F0F0F0F0F);
    return (unsigned)((x * UINT64_C(0x0101010101010101)) >> 56);
}

int main(void)
{
    const uint64_t sample = UINT64_C(0xF0F0F0F0F0F0F0F0);
    const int hw = has_popcnt();

    if (hw)
        printf("[13] popcnt(0x%016llx) = %u (swar %u)\n",
               (unsigned long long)sample, hw_popcnt(sample), swar_popcnt(sample));
    else
        printf("[13] POPCNT unavailable; swar(0x%016llx) = %u\n",
               (unsigned long long)sample, swar_popcnt(sample));

    uint64_t seed = 0x9E3779B97F4A7C15u;
    unsigned bad = 0;
    for (int i = 0; i < 100000; i++) {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
        if (hw && hw_popcnt(seed) != swar_popcnt(seed)) bad++;
    }
    if (hw)
        printf("[13] 100000 random values: %u mismatches\n", bad);
    else
        printf("[13] POPCNT unavailable; hardware comparison skipped\n");

    if (!hw) {
        printf("[13] hardware timing skipped (no POPCNT)\n");
        return 0;
    }

    const int N = 20000000;
    uint64_t acc = 0;
    uint64_t t0 = tsc();
    for (int i = 0; i < N; i++)
        acc += hw_popcnt((uint64_t)i);
    uint64_t t1 = tsc();
    uint64_t t2 = tsc();
    uint64_t acc2 = 0;
    for (int i = 0; i < N; i++)
        acc2 += swar_popcnt((uint64_t)i);
    uint64_t t3 = tsc();

    printf("[13] hardware: %.2f cycles/op  (acc=%llu)\n",
           (double)(t1 - t0) / N, (unsigned long long)acc);
    printf("[13] SWAR    : %.2f cycles/op  (acc=%llu, ratio %.1fx)\n",
           (double)(t3 - t2) / N, (unsigned long long)acc2,
           (double)(t3 - t2) / (double)(t1 - t0));
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     POPCNT dst, src counts the 1 bits in src (16/32/64-bit) and writes the
     count to dst. It is SSE4.2-era (CPUID.1:ECX[23]) on Intel, SSE4a on
     some AMD parts - check CPUID before using it in portable code.

   How SWAR works
     The fallback is a population count using only shifts, masks, adds and
     one multiply:
       1. Treat each 2-bit field as a counter: x - ((x>>1) & 0x55..).
       2. Add adjacent 2-bit counters into 4-bit fields.
       3. Add adjacent 4-bit counters into bytes.
       4. Multiply by 0x0101.. so the byte sums land in the top byte; the
          high byte is the total.
     It is branchless and constant-time, which matters in cryptography
     (popcount is used in constant-time code and in bitset cardinality).

   Why one instruction wins
     The hardware version has latency ~3 and throughput 1/cycle; SWAR is
     ~12 dependent ALU ops. In this demo the loop and the accumulator
     dependency dilute the difference (about 1.1-1.7 vs 2.2-2.3 cycles/op),
     but in a dependency-free kernel the gap is much larger, and the freed
     issue slots can be used for other work.

   Where it appears
     - Bitset cardinality (#16), sparse ECS masks (#60).
     - Bloom filters (#93): count of probes / load factor.
     - Chess engines: piece population per side.
     - Compression and error correction: Hamming weight.
     - HyperLogLog (#100): rank = leading zeros, a cousin of popcount.

   Output
     [13] popcnt(0xf0f0f0f0f0f0f0f0) = 32 (swar 32)
     [13] 100000 random values: 0 mismatches
     [13] hardware: ~1.1-1.7 cycles/op
     [13] SWAR    : ~2.2-2.3 cycles/op (ratio ~1.4-2.1x on this machine)
     (Timing varies; a tight standalone kernel shows a larger gap.)

   Gotchas
     - The compiler may already emit POPCNT for __builtin_popcountll when
       -mpopcnt is enabled, or call a libgcc helper otherwise. Check the
       disassembly, not the source.
     - POPCNT has no false dependency on Intel; on some AMD it writes the
       full destination.
     - For a 32-bit value use popcntl; the 64-bit form counts the zero-
       extended high bits too (they are zero, so same answer, slightly
       different encoding).
   ============================================== */
