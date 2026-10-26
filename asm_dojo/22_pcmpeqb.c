/* ASM MAGICK #22 - pcmpeqb + pmovmskb: compare 16 bytes and get a bitmask.
   Build: gcc -O2 -std=c11 22_pcmpeqb.c -o 22_pcmpeqb.exe
   MAGIC: 16 byte comparisons collapse into one 16-bit mask; a bit scan
   finds the first mismatch without a loop. */
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    unsigned char a[16] = "hello world!!!!";
    unsigned char b[16] = "hello WORLD!!!!";   /* differs at index 6..10 */
    unsigned char av[16], bv[16];
    for (int i = 0; i < 16; i++) { av[i] = a[i]; bv[i] = b[i]; }

    uint32_t mask = 0;
    __asm__ volatile(
        "movdqu %1, %%xmm0\n\t"
        "movdqu %2, %%xmm1\n\t"
        "pcmpeqb %%xmm1, %%xmm0\n\t"   /* 0xff per equal byte */
        "pmovmskb %%xmm0, %0"          /* collect the 16 sign bits */
        : "=r"(mask)
        : "m"(av), "m"(bv)
        : "xmm0", "xmm1", "memory");

    uint32_t diff = (~mask) & 0xFFFFu;
    int first = diff ? __builtin_ctz(diff) : -1;
    printf("[22] equal mask = 0x%04x, diff mask = 0x%04x\n", mask & 0xFFFFu, diff);
    printf("[22] first mismatch at index %d\n", first);
    return 0;
}

/* ================= EXPLANATION =================
   The two instructions
     PCMPEQB xmm1, xmm2 compares 16 byte pairs and writes 0xFF for equal,
     0x00 for different, in each byte of the destination.
     PMOVMSKB r32, xmm takes the most-significant bit of each of the 16
     bytes and packs them into the low 16 bits of a general-purpose
     register. Since PCMPEQB produces all-ones or all-zeros, those MSBs
     are exactly the comparison results.

   String length 16
     `a` is initialized from a string literal that is exactly 15 visible
     characters plus the terminating NUL = 16 bytes. Writing
     `unsigned char a[16] = "hello world!!!!"` is legal: the array is
     exactly large enough for the 15 chars and the NUL. The comparison
     includes the NUL byte, which is what makes the masks 0xffff for equal
     buffers.

   Why this is fast
     A scalar loop does 16 loads, 16 compares and 16 branches (or a
     branchless byte loop). SIMD does two loads, one compare and one mask
     move. Then a single BSF/TZCNT (#12) finds the first differing byte,
     and you can process the mismatch with a scalar path. This is the
     kernel of memcmp, strcmp, UTF-8 validation, and hash-table key
     comparison.

   Generalizing: MOVEMASK + TZCNT
     The pattern
         pcmpeqb / pcmpgtb / pcmpistri
         pmovmskb
         tzcnt / bsf
     turns a vector predicate into a scalar index in three instructions.
     AVX2 does 32 bytes at a time with VPMOVMSKB; AVX-512 uses mask
     registers directly.

   Output
     [22] equal mask = 0xf83f, diff mask = 0x07c0
     [22] first mismatch at index 6

   Gotchas
     - PCMPEQB compares bytes, not characters; case-insensitive comparison
       needs a case-fold pass first.
     - Reading past the end of a buffer is a real fault risk. Use masked
       loads (AVX-512) or ensure 16 bytes are readable; `memcmp` may read
       up to a word past the mismatch on some implementations, which is
       why the C standard requires accessible objects.
     - PMOVMSKB writes a 32-bit register but only the low 16 bits are
       meaningful for 128-bit operands; mask before using.
     - SSE2 baseline, no feature check needed on x86-64.
   ============================================== */
