/* MAGIC: floating-point exponent bits provide a strong inverse-sqrt guess.
   WHY: an old speed trick; modern hardware may prefer 1/sqrtf. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
static float invsqrt(float x){uint32_t i;float half=0.5f*x;memcpy(&i,&x,4);i=0x5f3759df-(i>>1);memcpy(&x,&i,4);return x*(1.5f-half*x*x);}
int main(void){printf("%.6f\n",invsqrt(4.0f));}

/* ================= EXPLANATION =================
   What it does
     Approximates 1/sqrt(x) with integer bit manipulation plus one Newton
     iteration - the famous "fast inverse square root" (Quake III).

   Step by step
     1. memcpy(&i, &x, 4) copies the IEEE-754 float bits into a uint32_t.
        The memcpy is essential: writing through a pointer cast would
        violate strict aliasing.
     2. The float layout is sign | 8-bit exponent | 23-bit mantissa. Taking
        i >> 1 halves the exponent in the integer domain, which
        approximates taking the square root; the constant 0x5f3759df
        corrects the bias and the mantissa error. This is really a piecewise
        linear approximation of log2(x).
     3. i = 0x5f3759df - (i >> 1) produces a float whose value is within a
        few percent of 1/sqrt(x). The bits are copied back with memcpy.
     4. One Newton-Raphson step refines the guess. For y = 1/sqrt(a), the
        update is
            y = y * (1.5 - 0.5 * a * y * y),
        where a is the original input. The code saves half = 0.5f * a
        before overwriting x with the guess, then returns
        x * (1.5f - half * x * x). Using the guess in place of a applies
        the wrong correction and produces about 0.697 for x = 4. For
        a = 4 this lands very close to 0.5.

   Output
     "0.499154" (float rounding of a value mathematically near 0.5)

   Why it matters
     It is a beautiful demonstration that IEEE-754 bit patterns are
     meaningful integers, and that a cheap approximate guess plus one
     Newton step beats a full sqrt on hardware that lacks a fast
     reciprocal-square-root instruction. It is also a cautionary tale
     about magic constants.

   Caveats
     - Modern CPUs have rsqrtss/rsqrtps (and 1.0f/sqrtf is often
       vectorised and accurate); this trick is rarely a win today.
     - Accuracy after one iteration is about 1%; use more Newton steps or
       a proper sqrt for precision work.
     - The constant, the bit layout, and sizeof(float) == 4 are
       assumptions. It does not handle x == 0, negative x, NaN, or Inf.
     - The exact printed value depends on float rounding; do not hard-code
       it.
   ============================================== */
