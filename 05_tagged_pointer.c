/* MAGIC: aligned pointers have spare low bits for tiny tags.
   CAVEAT: only use bits guaranteed clear by the target alignment. */
#include <stdint.h>
#include <stdio.h>
struct item{int value;};
static uintptr_t pack(struct item*p,unsigned tag){return(uintptr_t)p|(tag&1);}
int main(void){struct item x={99};uintptr_t z=pack(&x,1);printf("%d tag=%lu\n",((struct item*)(z&~(uintptr_t)1))->value,(unsigned long)(z&1));}

/* ================= EXPLANATION =================
   What it does
     Stores one bit of metadata in the low bit of a pointer, relying on
     the fact that the pointed-to object is more aligned than 1 byte, so
     that bit is always zero in a valid address.

   Step by step
     1. struct item contains an int, so alignof(struct item) is 4 on
        typical targets. A valid struct item * is therefore a multiple of
        4 and its bottom two bits are zero (malloc gives at least 8 or 16
        byte alignment; a stack object gets 4). The example only needs one
        of those spare bits.
     2. pack() casts the pointer to uintptr_t and ORs in (tag & 1). The
        mask keeps the tag inside the bit that is known to be free.
     3. Before every dereference the tag must be cleared:
        z & ~(uintptr_t)1 restores the address, and the cast turns it back
        into struct item *. The outer parentheses around the cast matter:
        -> binds tighter than a cast, so (struct item*)(z & ~1)->value
        would parse as a cast of the member access and fail to compile.
        Once the untagged pointer is formed, -> reads value == 99.
     4. z & 1 recovers the tag, printed as 1.

   Output
     "99 tag=1"

   Why it matters
     Pointer tagging packs a small flag (locked, marked, node colour,
     small-integer kind) into the pointer itself. This saves a word per
     object, avoids extra branches, and is used by lock-free algorithms,
     garbage collectors, and small-pointer optimisation in interpreters.

   Caveats
     - Only bits guaranteed clear by the alignment may be used; check
       alignof/alignment of the real allocator. On 32-bit systems a
       malloc'd pointer usually has 3-4 free low bits.
     - Converting a pointer to an integer is implementation-defined, and
       dereferencing a tagged pointer is undefined behaviour; always
       untag first.
     - Hardware tag checking (CHERI, Arm MTE), ASan, and some compilers
       assume clean pointers, so tagged values must never escape as real
       pointers.
     - High bits are not free: they may be used for sign extension or
       canonical-address checks.
   ============================================== */
