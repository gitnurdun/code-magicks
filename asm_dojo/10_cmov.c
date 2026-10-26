/* ASM MAGICK #10 - cmov: branchless min/max without a jump.
   Build: gcc -O2 -std=c11 10_cmov.c -o 10_cmov.exe
   MAGIC: the condition moves a value, not the instruction pointer, so
   there is no branch to mispredict. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static inline int imin(int a, int b)
{
    int r;
    /* r = a; flags = a-b; if (a >= b) r = b. */
    __asm__ volatile("movl %1, %0\n\t"
                     "cmpl %2, %1\n\t"
                     "cmovgel %2, %0"
                     : "=&r"(r)
                     : "r"(a), "r"(b)
                     : "cc");
    return r;
}

static inline int imax(int a, int b)
{
    int r;
    __asm__ volatile("movl %1, %0\n\t"
                     "cmpl %2, %1\n\t"
                     "cmovlel %2, %0"
                     : "=&r"(r)
                     : "r"(a), "r"(b)
                     : "cc");
    return r;
}

int main(void)
{
    int data[16];
    uint32_t seed = 12345;
    for (int i = 0; i < 16; i++) {
        seed = seed * 1664525u + 1013904223u;
        data[i] = (int)(seed >> 16) - 32768;
    }

    int lo = data[0], hi = data[0];
    int lo_c = data[0], hi_c = data[0];
    for (int i = 1; i < 16; i++) {
        lo = imin(lo, data[i]);
        hi = imax(hi, data[i]);
        if (data[i] < lo_c) lo_c = data[i];
        if (data[i] > hi_c) hi_c = data[i];
    }

    printf("[10] min=%d max=%d (C says %d/%d)\n", lo, hi, lo_c, hi_c);
    printf("[10] cmov(3,9)=%d cmov(9,3)=%d cmov(-4,4)=%d\n",
           imin(3, 9), imin(9, 3), imin(-4, 4));
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     CMOVcc dst, src copies src into dst if the condition flags match.
     It reads the flags set by a previous CMP/TEST and does not modify
     them. The four common forms:
         cmovl  less            (signed)
         cmovge greater/equal   (signed)
         cmovb  below           (unsigned)
         cmovae above/equal     (unsigned)

   Why branchless matters
     A predictable branch is nearly free. An unpredictable one costs
     ~15-20 cycles when mispredicted, and data-dependent min/max over
     random input is the textbook unpredictable branch. CMOV removes the
     control dependency: the CPU executes both sides' address calculation
     and selects a value. For a tight clamp or sort network this is a
     measurable win.

   The catch
     CMOV cannot remove work that has side effects or may fault. You
     cannot use it to guard a load from a null pointer: the load is still
     executed. It only selects between values that are already safe to
     compute. This is exactly why "branchless" code often does more total
     work - it trades wasted ALU cycles for avoided mispredictions.

   Why "=&r" here
     The first instruction writes r. If r shared a register with b, the
     mov would destroy b before the cmp. "&" (early-clobber) forbids that
     overlap. This is demo #02 in action.

   Output
     [10] min=-32585 max=32365 (C says -32585/-32585)
     [10] cmov(3,9)=3 cmov(9,3)=3 cmov(-4,4)=-4

   Gotchas
     - The compiler already generates CMOV for `a < b ? a : b` at -O2;
       hand-written asm is only needed for unusual flag combinations or
       when you must defeat a vectorizer.
     - CMOV on Intel has a dependency on both inputs and the flags; it is
       not always faster than a well-predicted branch.
     - Signed vs unsigned conditions are different instructions; picking
       the wrong one is a silent wrong-answer bug.
     - Floating point has no CMOV; use minss/maxss (which have their own
       NaN semantics) or blends.
   ============================================== */
