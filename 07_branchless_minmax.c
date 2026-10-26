/* MAGIC: choose min with a sign-derived mask instead of a branch.
   CAVEAT: this teaching form assumes two's-complement and no overflow. */
#include <stdio.h>
static int min_i(int a,int b){int d=a-b,m=d>>31;return b+(d&m);}
int main(void){printf("%d\n",min_i(8,3));}

/* ================= EXPLANATION =================
   What it does
     Computes min(a, b) without an if/else or ?: expression, using the
     sign bit of the difference as a 0-or-all-ones mask.

   Step by step
     1. d = a - b. If a is smaller, d is negative; otherwise it is zero or
        positive.
     2. m = d >> 31. On a 32-bit int with arithmetic right shift this is
        -1 (all bits set, 0xFFFFFFFF) for a negative d, and 0 otherwise.
     3. d & m keeps d when it is negative and yields 0 when it is not.
     4. b + (d & m) is therefore b + (a - b) == a when a < b, and b when
        b <= a. min_i(8, 3): d = 5, m = 0, result 3.

   Output
     "3"

   Why it matters
     Branchless selection compiles to a compare plus a conditional move
     (cmov) or a bitwise sequence. On unpredictable data that avoids
     branch mispredictions, and it can be vectorised across lanes where a
     branchy min cannot. Compilers often generate this shape themselves
     for simple int min/max.

   Caveats
     - This teaching form assumes 32-bit int, two's complement, and an
       arithmetic right shift. Right-shifting a negative value is
       implementation-defined in C, though every mainstream compiler does
       arithmetic shift.
     - a - b can overflow for extreme values (UB for signed int); use
       unsigned arithmetic or __builtin_sub_overflow for production code.
     - On modern CPUs a predictable branch is often faster than the
       extra arithmetic; measure.
   ============================================== */
