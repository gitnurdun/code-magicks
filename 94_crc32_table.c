/* MAGIC: CRC-32 is a table-driven polynomial remainder. Build the table
   once, then one XOR, shift, and lookup per byte. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t table[256];
static void make_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
}
static uint32_t crc32(const unsigned char *data, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) c = table[(c ^ *data++) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
int main(void)
{
    make_table();
    const char *s = "123456789";
    printf("crc32(%s)=%08x\n", s, crc32((const unsigned char *)s, strlen(s)));
}

/* ================= EXPLANATION =================
   What it does
     Computes the standard CRC-32 (IEEE 802.3) checksum of "123456789"
     using a 256-entry lookup table generated at startup. The expected
     check value is 0xCBF43926.

   Step by step
     1. CRC is polynomial division over GF(2). The reflected polynomial
        0xEDB88320 is the bit-reversed form of 0x04C11DB7, matching the
        usual Ethernet/zlib convention.
     2. make_table computes, for every possible byte, the remainder after
        eight shift-and-XOR steps. This is the same work the bitwise
        algorithm would do per byte, precomputed once.
     3. crc32 initialises the register to all ones, then for each byte
        XORs it into the low 8 bits and looks up the table entry for that
        index. The shift and XOR advance the polynomial remainder by one
        byte.
     4. The final register is inverted (XOR 0xFFFFFFFF), which is part of
        the standard CRC-32 definition. Without the initial and final
        inversions, the value would differ.
     5. For the nine ASCII bytes of "123456789", the result is CBF43926.

   Output
     "crc32(123456789)=cbf43926"

   Why it matters
     CRCs detect accidental corruption cheaply and predictably: a good
     CRC catches all single-bit errors, all odd-bit errors, and bursts up
     to its width. They are used in network frames, ZIP/PNG, disk
     sectors, and filesystem metadata. The lookup-table trick trades 1 KB
     of memory for a large speedup over bit-at-a-time division.

   Caveats
     - CRC-32 detects accidental errors, not deliberate tampering. An
       attacker can compute a new CRC after modifying data; use a MAC or
       cryptographic hash for integrity.
     - There are many CRC-32 variants with different initial values,
       reflection, and final XOR. "CRC-32" alone is ambiguous; this is the
       IEEE/zlib variant.
     - The table is generated for the reflected polynomial; a different
       polynomial needs a different table.
     - This implementation is byte-at-a-time. Slicing-by-8/16 or hardware
       CRC instructions are much faster.
     - CRC collisions exist: different messages can have the same
       checksum, and the space is only 32 bits.
     - The table is global mutable state; build it once before concurrent
       use or make it const at compile time.
   ============================================== */
