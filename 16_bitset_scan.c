/* MAGIC: isolate the lowest set bit, then map it to an index.
   CAVEAT: ctz(0) is undefined, so check zero first. */
#include <stdint.h>
#include <stdio.h>
int main(void){uint32_t bits=0x90;if(!bits)return 0;uint32_t low=bits&-bits;printf("bit=%u\n",__builtin_ctz(low));}

/* ================= EXPLANATION =================
   What it does
     Finds the index of the lowest set bit in a bitset word: isolate that
     bit, then count the trailing zeros.

   Step by step
     1. bits = 0x90 = binary 1001 0000: bits 7 and 4 are set. The zero
        check is mandatory because both the isolation and ctz(0) are
        undefined for zero.
     2. bits & -bits isolates the lowest set bit. Negating a two's
        complement number flips all bits and adds one, so every lower bit
        becomes 0 and the lowest 1 stays 1. Here 0x90 & 0x70 = 0x10.
     3. __builtin_ctz(low) counts trailing zeros in 0x10, which is 4. That
        is exactly the position of the bit. A compiler maps this to `tzcnt`
        or `bsf` on x86 and to `rbit`+`clz` elsewhere.
     4. Printing gives "bit=4".

   Output
     "bit=4"

   Why it matters
     This is the inner loop of allocators, schedulers, and bloom-filter
     scans: find the next available slot or set member in a word in a
     couple of instructions, instead of looping bit by bit.

   Caveats
     - ctz is undefined for 0; guard it, as the code does. Some CPUs also
       define the result of tzcnt on 0 (32), but that is not portable.
     - Use unsigned types; the -bits trick on signed values can overflow.
     - The ctz result is an int, so cast to the counter type if needed.
     - C23 offers stdc_countr_zero for a standard spelling.
     - For a full bitmap, iterate words and clear the lowest bit with
       bits &= bits - 1 to visit only set bits.
   ============================================== */
