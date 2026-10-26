/* MAGIC: reverse byte order with shifts, no temporary array. */
#include <stdint.h>
#include <stdio.h>
static uint32_t bswap32(uint32_t x){return(x>>24)|((x>>8)&0xff00)|((x<<8)&0xff0000)|(x<<24);}
int main(void){printf("%08x\n",bswap32(0x12345678));}

/* ================= EXPLANATION =================
   What it does
     Reverses the byte order of a 32-bit value using only shifts, masks,
     and ORs - the standard big-endian <-> little-endian conversion.

   Step by step
     For x = 0x12345678 the four terms isolate one byte each and move it
     to the opposite end:
     1.  x >> 24              = 0x00000012  (byte 3 -> byte 0)
     2.  (x >> 8) & 0xff00    = 0x00003400  (byte 2 -> byte 1)
     3.  (x << 8) & 0xff0000  = 0x00560000  (byte 1 -> byte 2)
     4.  x << 24              = 0x78000000  (byte 0 -> byte 3)
     ORing them gives 0x78563412.
     The masks are needed because the shifts bring neighbouring bytes
     along; without them the results would overlap and corrupt each other.

   Output
     "78563412"

   Why it matters
     On-disk and on-the-wire formats specify byte order, while the CPU has
     its own. A branch-free, allocation-free byte swap is the primitive
     behind htonl/ntohl and protocol serialisation. Modern compilers
     recognise the pattern and emit a single `bswap` instruction.

   Caveats
     - Use uint32_t (unsigned) so the shifts cannot overflow or trigger
       signed-shift undefined behaviour.
     - Prefer __builtin_bswap32, htonl/ntohl, or std::byteswap (C++23) in
       real code; they are clearer and at least as fast.
     - A byte swap is not a conversion between numeric values; it is a
       reinterpretation of the same 32 bits.
     - 64-bit values need the analogous mask/shift pattern, not two
       independent 32-bit swaps.
   ============================================== */
