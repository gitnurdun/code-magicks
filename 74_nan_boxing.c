/* MAGIC: store a double, a pointer, or an integer in one 64-bit word by
   hiding the non-doubles in the NaN payload space. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PAYLOAD 0x0000FFFFFFFFFFFFull
#define TAG_PTR 0x7FF9000000000000ull
#define TAG_INT 0x7FFA000000000000ull
typedef union { uint64_t bits; double d; void *p; } Value;
static Value box_ptr(void *p)
{
    Value v;
    v.bits = TAG_PTR | ((uintptr_t)p & PAYLOAD);
    return v;
}
static Value box_int(int i)
{
    Value v;
    v.bits = TAG_INT | ((uint32_t)i);
    return v;
}
static int is_ptr(Value v) { return (v.bits >> 48) == 0x7FF9; }
static int is_int(Value v) { return (v.bits >> 48) == 0x7FFA; }
static void *as_ptr(Value v) { return (void *)(uintptr_t)(v.bits & PAYLOAD); }
int main(void)
{
    int x = 42;
    Value p = box_ptr(&x);
    Value i = box_int(7);
    Value d;
    d.d = 3.5;
    printf("sizeof=%zu ptr=%d int=%d double=%.1f\n",
           sizeof(Value), *(int *)as_ptr(p), (int)(i.bits & 0xFFFFFFFFu), d.d);
    printf("tags: p=%d i=%d d=%d\n", is_ptr(p), is_int(i), is_ptr(d));
}

/* ================= EXPLANATION =================
   What it does
     Packs three kinds of value into a single 64-bit union: an IEEE-754
     double in its natural bit pattern, and pointers or small integers in
     the payload of a quiet NaN. The top 16 bits act as a tag.

   Step by step
     1. A double uses all 64 bits, but a quiet NaN has an exponent of all
        ones and a nonzero mantissa. The tag patterns 0x7FF9 and 0x7FFA
        are NaN encodings; the low 48 bits are free for a payload.
     2. box_ptr stores the pointer's low 48 bits under TAG_PTR. On x86-64,
        user-space addresses fit in 48 bits; the upper 16 are sign
        extension, not part of the address.
     3. box_int stores a 32-bit integer under TAG_INT. A signed value is
        sign-extended or encoded by the caller; this demo uses a small
        positive value.
     4. Ordinary doubles such as 3.5 have top bits 0x400C, so they never
        match a tag. is_ptr/is_int distinguish the boxed values; anything
        else is treated as a double.
     5. The pointer round-trips through the low bits and dereferences back
        to 42. The double remains 3.5. sizeof(Value) is 8, so a dynamic
        value costs one word.

   Output
     "sizeof=8 ptr=42 int=7 double=3.5"
     "tags: p=1 i=1 d=0"

   Why it matters
     NaN boxing is a classic interpreter technique: a dynamically typed
     value is one machine word, so arrays of values are compact and
     cache-friendly, and unboxing a double is free. JavaScript engines,
     Lisp/Scheme runtimes, and some game scripting VMs use variants of it.
     It is a concrete example of reinterpreting floating-point bit
     patterns as tagged data.

   Caveats
     - A real double NaN whose payload happens to match a tag would be
       misread. Engines canonicalise NaNs or reserve specific patterns.
     - Pointers wider than 48 bits (future x86-64 LA57, tagged pointers,
       CHERI, MTE) do not fit; pointer compression or a different scheme
       is required.
     - Type-punning through a union is implementation-defined in C,
       though supported by mainstream compilers; memcpy is the portable
       bit-cast.
     - The tag check only looks at 16 bits; validate and mask carefully.
     - Unboxing must be type-correct. Treating a tagged pointer as a
       double (or vice versa) is undefined behaviour and a security bug.
     - This is not a garbage collector: boxed pointers still need root
       tracking and write barriers.
   ============================================== */
