/* MAGIC: check bounds before addition, avoiding signed-overflow UB.
   WHY: media and embedded math often wants clamp, not wraparound. */
#include <limits.h>
#include <stdio.h>
static int sat_add(int a,int b){if(b>0&&a>INT_MAX-b)return INT_MAX;if(b<0&&a<INT_MIN-b)return INT_MIN;return a+b;}
int main(void){printf("%d\n",sat_add(INT_MAX,1));}

/* ================= EXPLANATION =================
   What it does
     Adds two signed ints and clamps the result to INT_MIN/INT_MAX instead
     of wrapping. The bounds are checked before the addition, so the
     addition itself never overflows.

   Step by step
     1. If b > 0, overflow is possible only when a > INT_MAX - b. In that
        case the exact sum would exceed INT_MAX, so return INT_MAX. The
        comparison is safe because INT_MAX - b is representable for any
        positive b.
     2. If b < 0, overflow is possible only when a < INT_MIN - b, and the
        function returns INT_MIN. INT_MIN - b is safe here because b is
        negative, so the result moves toward zero.
     3. Otherwise a + b is guaranteed to fit and is returned.
     4. sat_add(INT_MAX, 1): b is positive and a > INT_MAX - 1, so the
        first branch clamps to INT_MAX = 2147483647.

   Output
     "2147483647"

   Why it matters
     Signed integer overflow is undefined behaviour in C, so the usual
     "add then check the sign" idiom is not valid. Media code, audio
     mixing, embedded control, and colour math often want saturation:
     clamp at the representable limit rather than wrap around.

   Caveats
     - This is signed-only. Unsigned arithmetic is defined to wrap, so
       saturating unsigned add needs a different check.
     - __builtin_add_overflow(a, b, &out) (GCC/Clang) or C23's
       <stdckdint.h> expresses the same test more clearly.
     - Types narrower than int are promoted to int during the addition;
       cast the result back if the API expects e.g. int16_t.
     - Saturation hides errors; make sure clamping is the desired policy
       rather than a silent workaround.
   ============================================== */
