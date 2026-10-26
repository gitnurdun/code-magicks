/* ASM MAGICK #20 - pshufb: a 16-way byte permutation in one instruction.
   Build: gcc -O2 -std=c11 20_pshufb.c -o 20_pshufb.exe
   MAGIC: a mask register selects, per output byte, any input byte - or
   zero - in a single pass. */
#include <stdio.h>
#include <stdint.h>

static inline uint32_t cpuid_leaf1_ecx(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(1), "c"(0));
    return c;
}

int main(void)
{
    unsigned char in[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    unsigned char mask[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                              7, 6, 5, 4, 3, 2, 1, 0};
    unsigned char out[16] = {0};

    if ((cpuid_leaf1_ecx() >> 9) & 1) {   /* SSSE3 */
        __asm__ volatile(
            "movdqu %1, %%xmm0\n\t"
            "movdqu %2, %%xmm1\n\t"
            "pshufb %%xmm1, %%xmm0\n\t"
            "movdqu %%xmm0, %0"
            : "=m"(out)
            : "m"(in), "m"(mask)
            : "xmm0", "xmm1", "memory");
        printf("[20] reversed bytes:");
        for (int i = 0; i < 16; i++) printf(" %02x", out[i]);
        printf("\n");
    } else {
        printf("[20] SSSE3 not available; pshufb would #UD\n");
    }
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     PSHUFB xmm1, xmm2/m128 builds a new 16-byte register by table lookup:
         for i in 0..15:
             index = src2.byte[i] & 0x0F
             if (src2.byte[i] & 0x80) dst.byte[i] = 0
             else                     dst.byte[i] = src1.byte[index]
     It is a byte-wise permute with an optional zeroing feature, all in one
     instruction. The mask is the *second* operand; the data is the first.

   What the demo does
     The mask is [15,14,...,0], so every output byte selects the opposite
     input byte. One instruction reverses a 16-byte string. With different
     masks you can broadcast, rotate, interleave, do UTF-8 validation
     (via lookup tables), or implement a 16-entry LUT without branches.

   Why it is everywhere
     - Base64 encode/decode: rearrange 3-byte groups into 4-byte lanes.
     - UTF-8/UTF-16 transcoding and validation.
     - Compression: byte-level entropy coding.
     - Game engines: swizzling vertex attributes, Morton codes, texture
       channel packing.
     - Hash tables: fast small-key comparisons and shuffles.
     The key property is that the permutation is data, not code: no branch
     per byte, no loop, constant latency.

   Output
     [20] reversed bytes: 0f 0e 0d 0c 0b 0a 09 08 07 06 05 04 03 02 01 00

   Gotchas
     - SSSE3 (CPUID.1:ECX[9]); #UD on SSE2-only CPUs. AVX2 has VPSHUFB
       with 256-bit registers, but the lane-crossing version needs
       VPERMB (AVX-512) or a different shuffle.
     - The high bit of each mask byte zeroes that output byte; it is not
       an index bit. Forgetting this produces surprising zeros.
     - PSHUFB operates within 128-bit lanes; with AVX2, the same mask is
       applied independently to each 128-bit lane, not across the full
       256 bits.
     - The mask must be in a register or memory; there is no immediate
       form.
   ============================================== */
