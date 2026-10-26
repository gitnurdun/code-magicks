/* MAGIC: a macro can evaluate each argument once and infer its type.
   CAVEAT: ({ }) and __typeof__ are GNU C extensions. */
#include <stdio.h>
#define MAX(a,b)({__typeof__(a)_a=(a);__typeof__(b)_b=(b);_a>_b?_a:_b;})
int main(void){int x=3;printf("%d\n",MAX(x++,10));}

/* ================= EXPLANATION =================
   What it does
     MAX(a, b) is a macro that behaves like a type-generic function: it
     evaluates each argument exactly once, keeps the result's natural
     type, and needs no separate macro per type.

   Step by step
     1. MAX(x++, 10) expands to a GCC statement expression: ({ ... }).
        Its value is the value of the last expression inside the block.
     2. __typeof__(a) _a = (a); creates a temporary of exactly a's type
        and initialises it once. The same is done for b. Declaring the
        temporaries is what prevents the classic MAX(x++, 10) bug of
        incrementing x twice.
     3. _a > _b ? _a : _b yields the larger value, converted to the
        common type of the two operands.
     4. x starts at 3, so _a = 3 (x becomes 4), _b = 10, and 10 wins.

   Output
     "10"

   Why it matters
     It shows the standard C trick behind type-safe macro "functions"
     such as the kernel's min/max and swap helpers: capture the arguments
     in typed locals, then operate on the locals. The side-effect count
     and the types are preserved without writing a function per type.

   Caveats
     - ({ ... }) and __typeof__ are GNU C extensions (GCC/Clang); they are
       not ISO C and are not accepted by MSVC. Compile as -std=gnu11.
     - The comparison can promote both sides to a common type; mixed
       signed/unsigned arguments can still surprise you.
     - A statement expression cannot be used at file scope, and jumping
       into it (goto) is not allowed.
   ============================================== */
