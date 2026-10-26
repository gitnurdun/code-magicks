/* MAGIC: adjacent floats are adjacent integers; their bit patterns reveal
   the exponent, mantissa, and the size of one ULP. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static uint32_t fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
int main(void)
{
    float x = 1.0f;
    float y = nextafterf(x, 2.0f);
    printf("%08x -> %08x gap=%.9g\n", fbits(x), fbits(y), (double)(y - x));
}

/* ================= EXPLANATION =================
   What it does
     Reads the IEEE-754 binary32 representation of 1.0f, asks for the next
     representable float above it, and prints the bit patterns and the gap
     between the two values.

   Step by step
     1. fbits() uses memcpy to copy the four bytes of the float into a
        uint32_t. A pointer cast would violate strict aliasing; memcpy is
        the portable bit-cast and compilers optimise it away.
     2. 1.0f is sign 0, exponent 127 (biased), mantissa 0. In hex that is
        0x3F800000: 0 01111111 00000000000000000000000.
     3. nextafterf(1.0f, 2.0f) returns the smallest float strictly greater
        than 1.0f. It increments the mantissa by one, giving 0x3F800001.
     4. The difference is 2^-23, approximately 1.1920929e-07: one unit in
        the last place (ULP) at this exponent. At 2.0f the ULP doubles to
        2^-22, because the exponent increased.
     5. Printing the bits makes the abstract "floating point" concrete:
        finite floats are ordered like integers (for non-negative values),
        and the spacing between them changes with the exponent.

   Output
     "3f800000 -> 3f800001 gap=1.1920929e-07"

   Why it matters
     ULP arithmetic is how numerical code reasons about precision,
     tolerance, and rounding. Comparing floats with `==` is often wrong
     because two mathematically equal computations can differ by a few
     ULPs; nextafter is used for correctly-rounded bounds and for
     stepping through the representable values. Bit inspection also
     underpins fast approximations, NaN boxing, and serialization.

   Caveats
     - This assumes IEEE-754 binary32 (sizeof(float) == 4), true on
       mainstream platforms but not guaranteed by C.
     - For negative floats, the integer ordering is reversed; use
       sign-magnitude adjustments before comparing bit patterns.
     - NaN and infinity do not obey the normal exponent rules; nextafter
       has special cases for them.
     - `y - x` is exact here because both are powers of two, but in
       general subtraction can round.
     - Use double or long double when 24 bits of significand are not
       enough; the ULP grows with magnitude.
   ============================================== */
