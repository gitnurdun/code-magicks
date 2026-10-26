/* ASM MAGICK #19 - paddd: four 32-bit adds in one instruction.
   Build: gcc -O2 -std=c11 19_sse_paddd.c -o 19_sse_paddd.exe
   MAGIC: SIMD is just a wider register file; the same add opcode works on
   4 lanes at once. */
#include <stdio.h>

int main(void)
{
    int a[4] = {1, 2, 3, 4};
    int b[4] = {10, 20, 30, 40};
    int out[4] = {0};

    __asm__ volatile(
        "movdqu %1, %%xmm0\n\t"      /* unaligned 16-byte load */
        "movdqu %2, %%xmm1\n\t"
        "paddd  %%xmm1, %%xmm0\n\t"  /* 4 x int32 lane-wise add */
        "movdqu %%xmm0, %0"
        : "=m"(out)
        : "m"(a), "m"(b)
        : "xmm0", "xmm1", "memory");

    printf("[19] {1,2,3,4} + {10,20,30,40} = {%d,%d,%d,%d}\n",
           out[0], out[1], out[2], out[3]);
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     PADDD xmm1, xmm2/m128 adds four packed 32-bit integers:
         dst.lane[i] = dst.lane[i] + src.lane[i]   (i = 0..3)
     There are saturating (PADDSW/PADDUSB) and byte/word variants, plus
     AVX2's 256-bit VPADDD. The operation is lane-wise; carries do not
     propagate between lanes.

   What the asm does
     1. MOVDQU loads 16 unaligned bytes from a into xmm0. (MOVDQA would
        fault on a non-16-byte-aligned address; the arrays here are only
        4-byte aligned.)
     2. MOVDQU loads b into xmm1.
     3. PADDD adds xmm1 to xmm0, four ints at once.
     4. MOVDQU stores the 16-byte result into out.
     The "memory" clobber and the "=m" output tell GCC that out changes;
     without them the compiler could keep a stale copy in a register.

   Why SIMD matters in engines
     Particle updates, matrix math, skinning, audio mixing and BVH slab
     tests all have this shape: independent elements, same operation.
     Four int adds per instruction is a 4x reduction in issue slots, and
     the loads/stores are also wider. The real win in game code is often
     fewer instructions and better memory bandwidth, not just ALU
     throughput.

   Output
     [19] {1,2,3,4} + {10,20,30,40} = {11,22,33,44}

   Gotchas
     - Alignment: MOVDQA requires 16-byte alignment; MOVDQU does not but
       can be slower when it crosses a cache line. Modern CPUs mostly
       don't care.
     - SIMD lanes are not a substitute for a loop when the data is not
       contiguous; gather/scatter is expensive.
     - The compiler may vectorize the same loop automatically. Check the
       disassembly before hand-writing intrinsics or asm.
     - Mixing SSE and AVX without VZEROUPPER can cause a transition
       penalty; if the rest of the program uses AVX, use VEX-encoded
       instructions.
     - xmm0-xmm5 are volatile on Windows x64; xmm6-xmm15 are callee-saved.
       If you clobber xmm6+, the compiler must save it (and it will, if
       you list it as a clobber).
   ============================================== */
