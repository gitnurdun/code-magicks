/* MAGIC: a De Bruijn sequence maps an isolated low bit to its index with
   one multiply and one table load. WHY: every 5-bit window is unique. */
#include <stdint.h>
#include <stdio.h>
static const unsigned char index32[32] = {
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};
static unsigned ctz32(uint32_t x)
{
    return index32[((x & (0u - x)) * 0x077CB531u) >> 27];
}
int main(void)
{
    uint32_t x = 0x00000008u;
    printf("ctz(%08x)=%u\n", x, ctz32(x));
    x = 0x80000000u;
    printf("ctz(%08x)=%u\n", x, ctz32(x));
}

/* ================= EXPLANATION =================
   What it does
     Counts trailing zero bits (finds the position of the lowest set bit)
     without a loop: isolate the bit, multiply by a magic constant, use
     the top five bits as an index into a 32-entry table.

   Step by step
     1. `x & (0u - x)` isolates the lowest set bit. For x = 8 (0b1000),
        the result is 8; for 0x80000000 it is 0x80000000.
     2. 0x077CB531 is a De Bruijn sequence B(2, 5): a cyclic sequence in
        which every possible 5-bit string occurs exactly once. Multiplying
        the isolated power of two by this constant shifts a unique 5-bit
        window into the high bits for each bit position.
     3. `>> 27` extracts the top five bits, a value 0..31.
     4. index32 maps that window value to the original bit index. For 8
        the window is 3, and the table returns 3. For 0x80000000 it
        returns 31.
     5. The table is constructed for this exact constant; changing the
        constant without regenerating the table breaks the mapping.

   Output
     "ctz(00000008)=3"
     "ctz(80000000)=31"

   Why it matters
     This is the classic branch-free "find first set" used in allocators,
     schedulers, and bitboards before every CPU had a CTZ instruction. It
     shows that a multiplication can perform a tiny perfect hash: the
     De Bruijn property turns a bit position into a unique table index.
     The same technique builds minimal perfect hash functions.

   Caveats
     - ctz(0) is undefined; the isolated bit is zero and the table index
       is meaningless. Check for zero first.
     - The table and constant are 32-bit; 64-bit needs B(2, 6), a 64-bit
       constant, and a 64-entry table.
     - Modern compilers expose __builtin_ctz / TZCNT; use them when
       available. They are faster and handle zero only if you guard it.
     - The multiply relies on unsigned wrap-around, which is defined in C.
   ============================================== */
