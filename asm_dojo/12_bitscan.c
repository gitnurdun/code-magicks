/* ASM MAGICK #12 - bsf/bsr and their BMI1 replacements tzcnt/lzcnt.
   Build: gcc -O2 -std=c11 12_bitscan.c -o 12_bitscan.exe
   MAGIC: find the first or last set bit in one instruction - the core of
   allocators, schedulers, chess boards and bitmaps. */
#include <stdio.h>
#include <stdint.h>

static inline uint32_t cpuid_leaf7_ebx(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(7), "c"(0));
    return b;
}

static inline unsigned bsf32(uint32_t x)
{
    unsigned r;
    __asm__ volatile("bsfl %1, %0" : "=r"(r) : "r"(x));
    return r;
}

static inline unsigned bsr32(uint32_t x)
{
    unsigned r;
    __asm__ volatile("bsrl %1, %0" : "=r"(r) : "r"(x));
    return r;
}

static inline unsigned tzcnt32(uint32_t x)
{
    unsigned r;
    __asm__ volatile("tzcntl %1, %0" : "=r"(r) : "r"(x));
    return r;
}

static inline unsigned lzcnt32(uint32_t x)
{
    unsigned r;
    __asm__ volatile("lzcntl %1, %0" : "=r"(r) : "r"(x));
    return r;
}

int main(void)
{
    const uint32_t v = 0x00F00000u;   /* bits 20..23 set */
    int bmi1 = (cpuid_leaf7_ebx() >> 3) & 1;

    printf("[12] v=0x%08x bsf=%u bsr=%u\n", v, bsf32(v), bsr32(v));
    if (bmi1) {
        printf("[12] BMI1: tzcnt=%u lzcnt=%u (for v=0: tzcnt=%u lzcnt=%u)\n",
               tzcnt32(v), lzcnt32(v), tzcnt32(0), lzcnt32(0));
    } else {
        printf("[12] BMI1 not available; tzcnt/lzcnt skipped\n");
    }
    printf("[12] bsf/bsr with zero is undefined; guard it (v!=0 -> %s)\n",
           v ? "call" : "skip");
    return 0;
}

/* ================= EXPLANATION =================
   The four instructions
     BSF  dst, src : index of the least-significant set bit (0..31)
     BSR  dst, src : index of the most-significant set bit
     TZCNT dst, src: count trailing zeros (32 if src == 0)
     LZCNT dst, src: count leading zeros  (32 if src == 0)
     BSF/BSR date back to 386. TZCNT/LZCNT are BMI1 (Haswell+, Zen+).

   Why TZCNT matters even though it looks like BSF
     BSF leaves the destination undefined when the source is zero, so
     every caller needs a test and a branch. TZCNT returns the operand
     width (32), which is exactly the "no bits" answer most algorithms
     want. That removes a branch from the inner loop. On older CPUs the
     TZCNT encoding executes as BSF (REP BSF), so you must check CPUID
     before using it on a zero input - the result would be undefined.

   The classic use: iterate set bits
       while (mask) {
           unsigned i = tzcnt(mask);
           mask &= mask - 1;      // clear the lowest set bit
           use(i);
       }
     This runs once per set bit, not once per machine word. It is the
     foundation of sparse bitsets (#16), ECS archetype masks (#60), and
     allocator free-lists (#61).

   Output
     [12] v=0x00f00000 bsf=20 bsr=23
     [12] BMI1: tzcnt=20 lzcnt=8 (for v=0: tzcnt=32 lzcnt=32)

   Gotchas
     - BSF/BSR are undefined for zero; TZCNT/LZCNT are defined.
     - The 64-bit forms use the full 64-bit operand; mixing `tzcntl` and a
       64-bit value silently looks at the low half only.
     - On some microarchitectures LZCNT/TZCNT have a false dependency on
       the destination register; `xor dst,dst` before can help (or hurt).
     - Bit index vs bit mask confusion is the usual bug: tzcnt returns an
       index, not 1<<index.
   ============================================== */
