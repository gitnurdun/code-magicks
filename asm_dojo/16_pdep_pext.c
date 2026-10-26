/* ASM MAGICK #16 - pdep/pext: gather and scatter bits like a permutation
   network.
   Build: gcc -O2 -std=c11 16_pdep_pext.c -o 16_pdep_pext.exe
   MAGIC: two instructions move an arbitrary subset of bits in one pass -
   no loop over set bits, no lookup table. */
#include <stdio.h>
#include <stdint.h>

static inline uint32_t cpuid_leaf7_ebx(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(7), "c"(0));
    return b;
}

int main(void)
{
    uint64_t src = 0xAB;          /* 1010 1011 */
    uint64_t mask = 0x0F0F;       /* keep bits in the low nibble of each byte */

    uint64_t dep = 0, ext = 0;
    if ((cpuid_leaf7_ebx() >> 8) & 1) {
        /* Intel: pdep dest, src, mask. AT&T reverses: mask, src, dest. */
        __asm__ volatile("pdepq %2, %1, %0" : "=r"(dep) : "r"(src), "r"(mask));
        __asm__ volatile("pextq %2, %1, %0" : "=r"(ext) : "r"(dep), "r"(mask));
        printf("[16] pdep(0x%llx, 0x%llx) = 0x%llx\n",
               (unsigned long long)src, (unsigned long long)mask,
               (unsigned long long)dep);
        printf("[16] pext(0x%llx, 0x%llx) = 0x%llx (round trip)\n",
               (unsigned long long)dep, (unsigned long long)mask,
               (unsigned long long)ext);
    } else {
        printf("[16] BMI2 not available; pdep/pext would #UD\n");
    }
    return 0;
}

/* ================= EXPLANATION =================
   PDEP (parallel bits deposit)
     Takes the low N bits of src and deposits them, in order, into the
     positions selected by mask. Bits outside the mask are zero.
         src  = 0b1011
         mask = 0b0101
         pdep = 0b0101   (bit0->bit0, bit1->bit2)
     It is the inverse of PEXT.

   PEXT (parallel bits extract)
     Collects the bits selected by mask into a contiguous low value:
         src  = 0b1101
         mask = 0b0101
         pext = 0b11
     Together they implement arbitrary bit permutations, bit-field
     packing, and "compress" operations in one instruction.

   Why this is magic
     Without BMI2, packing 8 fields or compressing a sparse bitset requires
     a loop over set bits (tzcnt + shift, #12) or a lookup table. PDEP/PEXT
     are constant-time and branchless, which makes them suitable for
     cryptography (constant-time selection), compression, and bitboard
     move generation. They are also the primitive behind "bit manipulation
     as a sorting network" tricks.

   The catch: latency
     On Intel Haswell through Skylake, PDEP/PEXT are 3 cycles. On AMD Zen 1
     and Zen 2 they are microcoded and extremely slow (~18-300 cycles), so
     a "one instruction" implementation can lose to a loop. On Zen 3+ they
     are fast again. This is a rare case where the instruction exists but
     the performance is not portable - always benchmark on the target.

   Output
     [16] pdep(0xab, 0xf0f) = 0xa0b
     [16] pext(0xa0b, 0xf0f) = 0xab (round trip)

   Gotchas
     - Both are BMI2 (CPUID.7.0:EBX[8]); they #UD on older CPUs.
     - The mask is not required to be contiguous, but the result is only
       defined in terms of the set bits; "holes" are zero.
     - AT&T operand order is the reverse of Intel's; a swapped mask and
       src produces plausible but wrong values.
     - PDEP/PEXT have no 8/16-bit forms; use 32/64-bit registers.
   ============================================== */
