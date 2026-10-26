/* ASM MAGICK #21 - pmaddwd: multiply 16-bit pairs and add them, the dot
   product primitive.
   Build: gcc -O2 -std=c11 21_pmaddwd.c -o 21_pmaddwd.exe
   MAGIC: eight multiplies and four adds collapse into one instruction -
   the inner loop of every integer dot product. */
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    int16_t a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int16_t b[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    int32_t out[4] = {0};

    __asm__ volatile(
        "movdqu %1, %%xmm0\n\t"
        "movdqu %2, %%xmm1\n\t"
        "pmaddwd %%xmm1, %%xmm0\n\t"   /* 4 x (a0*b0 + a1*b1) */
        "movdqu %%xmm0, %0"
        : "=m"(out)
        : "m"(a), "m"(b)
        : "xmm0", "xmm1", "memory");

    long sum = 0;
    for (int i = 0; i < 4; i++) sum += out[i];
    printf("[21] pmaddwd lanes = {%d,%d,%d,%d}\n",
           out[0], out[1], out[2], out[3]);
    printf("[21] dot product   = %ld\n", sum);
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     PMADDWD xmm1, xmm2/m128 treats each register as eight signed 16-bit
     integers, multiplies corresponding pairs, and adds adjacent products
     into four signed 32-bit results:
         dst.dword[i] = src1.word[2i] * src2.word[2i]
                      + src1.word[2i+1] * src2.word[2i+1]
     The 32-bit result prevents overflow for the common 16x16+16x16 case
     (maximum magnitude 2 * 32768 * 32768 = 2^31, which just fits).

   What the demo computes
     Pair (1,2) with (10,20): 10 + 40 = 50
     Pair (3,4) with (30,40): 90 + 160 = 250
     Pair (5,6) with (50,60): 250 + 360 = 610
     Pair (7,8) with (70,80): 490 + 640 = 1130
     Sum = 2040.

   Why it is a classic
     Before FMA and before wide integer dot-product instructions, PMADDWD
     was how you did 16-bit audio, video and fixed-point math:
       - audio filters: multiply a window by samples and accumulate;
       - video codecs: SAD/SATD are siblings, PMADDWD does the transform;
       - game math: fixed-point dot products for lighting and physics;
       - neural networks: the original quantized inference kernels.
     It is still the core of many int16 inference and DSP kernels because
     it does 16 multiplies and 8 adds' worth of work in one op.

   The horizontal sum
     PMADDWD produces four partial sums. To get the scalar dot product you
     still need a horizontal add: PADDD twice with PSHUFD, or PHADDD
     (SSSE3), or a store and scalar sum (shown here for clarity). The
     horizontal step is often the bottleneck in short vectors.

   Output
     [21] pmaddwd lanes = {50,250,610,1130}
     [21] dot product   = 2040

   Gotchas
     - Signed 16-bit inputs only. For unsigned, xor with 0x8000 and correct
       the bias, or widen to 32-bit first.
     - The 32-bit accumulation can overflow if you chain many PMADDWD
       results; use PADDD with saturation awareness or widen.
     - SSE2 baseline; available on every x86-64.
     - The order of the pairs is fixed: (0,1), (2,3), ... . Use PSHUFB to
       reorder if your data layout differs.
   ============================================== */
