/* MAGIC: reverse 32 bits with five masked swaps, no loop and no table.
   WHY: divide-and-conquer bit permutation is branch-free. */
#include <stdint.h>
#include <stdio.h>
static uint32_t reverse32(uint32_t x)
{
    x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
    x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
    x = ((x >> 4) & 0x0F0F0F0Fu) | ((x & 0x0F0F0F0Fu) << 4);
    x = ((x >> 8) & 0x00FF00FFu) | ((x & 0x00FF00FFu) << 8);
    x = (x >> 16) | (x << 16);
    return x;
}
int main(void)
{
    uint32_t x = 0x12345678u;
    printf("%08x -> %08x\n", x, reverse32(x));
}

/* ================= EXPLANATION =================
   What it does
     Reverses the order of all 32 bits in an unsigned 32-bit integer using
     five stages of masked swaps. Bit 0 becomes bit 31, bit 1 becomes bit
     30, and so on.

   Step by step
     1. The first line swaps adjacent bits: the mask 0x55555555 selects
        even positions, shifts one copy right and the other left, then ORs
        them. Pairs (0,1), (2,3), ... are exchanged.
     2. The second line swaps 2-bit groups using 0x33333333; the third
        swaps nibbles using 0x0F0F0F0F; the fourth swaps bytes using
        0x00FF00FF.
     3. The last line swaps the two 16-bit halves with a plain shift and
        OR. After stage k, each block of 2^k bits has been reversed and
        moved to its mirror position.
     4. For 0x12345678 the stages produce 0x1E6A2C48, which is the exact
        bit-reversal: 0001 0010 ... becomes ... 0100 1000.

   Output
     "12345678 -> 1e6a2c48"

   Why it matters
     This is the classic Hacker's Delight bit-reversal. It is branch-free,
     constant-time, and uses only shifts, ANDs, ORs, and constants, so it
     vectorises well and has no cache footprint. It appears in FFT bit-
     reversal permutations, CRC/bitstream code, cryptography, and
     endianness-related bit twiddling.

   Caveats
     - It is exactly 32-bit. Use a 64-bit variant with 0x5555... masks for
       uint64_t, or a byte table for arbitrary widths.
     - The constants assume two's-complement unsigned arithmetic, which C
       guarantees for unsigned types.
     - Modern compilers may recognise the idiom and emit a dedicated
       bit-reverse instruction if the target has one (ARM RBIT).
     - Reversing bits is not the same as reversing bytes: 0x12345678
       byte-reverses to 0x78563412, while this gives 0x1E6A2C48.
   ============================================== */
