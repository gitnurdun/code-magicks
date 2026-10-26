/* MAGIC: count set bits in parallel with SWAR arithmetic - no loop and
   no lookup table. WHY: each word holds several independent counters. */
#include <stdint.h>
#include <stdio.h>
static unsigned popcount32(uint32_t x)
{
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (x * 0x01010101u) >> 24;
}
int main(void)
{
    uint32_t x = 0xDEADBEEFu;
    printf("popcount(%08x)=%u\n", x, popcount32(x));
}

/* ================= EXPLANATION =================
   What it does
     Counts the 1 bits of a 32-bit word using SWAR: "SIMD Within A
     Register". The same 32-bit register is treated as 16 independent
     2-bit counters, then 8 4-bit counters, then 4 byte counters.

   Step by step
     1. `x - ((x >> 1) & 0x55555555)` turns each adjacent bit pair into a
        2-bit count (00, 01, or 10) without carries crossing the pairs.
        The mask 0x55555555 isolates the even bit of each pair.
     2. The second line adds neighbouring 2-bit counts, producing four
        4-bit counts per byte; 0x33333333 isolates the low pair.
     3. `x + (x >> 4)` adds the two 4-bit counts in each byte, and the
        0x0F0F0F0F mask keeps the low nibble - now each byte holds the
        popcount of that byte (0..8).
     4. Multiplying by 0x01010101 sums the four byte counters into the
        top byte, because each byte's value is replicated into the next
        carry position. Shifting right 24 extracts that sum.
     5. 0xDEADBEEF has 24 set bits, so the function returns 24.

   Output
     "popcount(deadbeef)=24"

   Why it matters
     Bit counting is used in allocators (free-page counts), bitmap scans,
     chess engines, compression, and database filters. SWAR avoids a
     branch per bit and a table lookup per byte. It is also a beautiful
     example of packing independent lanes into one word - the same idea
     behind SIMD, but using ordinary integer instructions.

   Caveats
     - It is 32-bit-specific; a 64-bit version needs 64-bit masks and a
       final multiply/shift adjustment.
     - Modern x86 has POPCNT and ARM has VCNT; compilers translate
       __builtin_popcount to them. SWAR is a fallback, not automatically
       faster.
     - The final multiply can overflow, which is fine for unsigned
       arithmetic but not for signed int.
     - The input must be unsigned; right-shifting a negative signed value
       is implementation-defined.
   ============================================== */
