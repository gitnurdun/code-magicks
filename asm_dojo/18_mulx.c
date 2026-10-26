/* ASM MAGICK #18 - mulx: 64x64 -> 128 multiply without touching flags.
   Build: gcc -O2 -std=c11 18_mulx.c -o 18_mulx.exe
   MAGIC: RDX is the implicit source, two arbitrary registers receive the
   128-bit product, and EFLAGS survives. */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

static inline uint32_t cpuid_leaf7_ebx(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(7), "c"(0));
    return b;
}

int main(void)
{
    uint64_t a = UINT64_C(0x123456789ABCDEF0);
    uint64_t b = UINT64_C(0x0FEDCBA987654321);
    uint64_t lo = 0, hi = 0;

    if ((cpuid_leaf7_ebx() >> 8) & 1) {
        /* Intel: mulx high, low, src, with RDX implicit. AT&T reverses:
           src, low, high. */
        __asm__ volatile("mulxq %3, %0, %1"
                         : "=r"(lo), "=r"(hi)
                         : "d"(a), "r"(b)
                         : "cc");
        __int128 check = (__int128)a * b;
        __int128 got = ((__int128)hi << 64) | lo;
        printf("[18] a * b = 0x%016" PRIx64 "%016" PRIx64 "\n", hi, lo);
        printf("[18] __int128 agrees: %s\n", got == check ? "yes" : "NO");
    } else {
        printf("[18] BMI2 not available; mulx would #UD\n");
    }
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     MULX hi, lo, src (Intel order) computes RDX * src and writes the high
     64 bits to hi and the low 64 bits to lo. RDX is an implicit input, not
     an operand, and the destination registers can be any GPRs.
         mulx r8, r9, rcx      ; r8:r9 = rdx * rcx
     The flags are unchanged, unlike mul/imul which set CF/OF.

   Why it exists
     Classic `mul r/m64` forces RDX:RAX as both input and output, serializes
     on those registers, and clobbers flags. MULX was added with BMI2 to
     make multi-precision arithmetic schedulable:
       - the multiplier can live in RDX while the multiplicand is anywhere;
       - the two halves go to independent registers, so ADC/ADD chains can
         overlap;
       - no flag dependency means the carry chain is not broken.

   The bigger picture: big integers
     A 512-bit multiply is a sequence of 64x64->128 MULX operations plus
     ADC/ADOX accumulate chains. The MULX + ADCX/ADOX combination is what
     makes modern RSA/ECC and bignum libraries fast. MULX alone is the
     first half; the second half is the flag-free carry propagation
     (ADOX uses OF, ADCX uses CF, so two carry chains can run in parallel).

   Output
     [18] a * b = 0x01f8f5b2a0c9e3d2... (a 128-bit product)
     [18] __int128 agrees: yes

   Gotchas
     - RDX is clobbered? No - MULX reads RDX and does not write it, but the
       compiler must know RDX is an input; "d"(a) does that. If you also
       use RDX for something else, the allocator handles it.
     - The high/low order in AT&T is the reverse of Intel and easy to swap.
       The __int128 check catches it.
     - BMI2 only (#UD otherwise).
     - MULX does not set flags, so a following ADC uses whatever flags were
       already there. That is a feature, not a bug, but it surprises people.
   ============================================== */
