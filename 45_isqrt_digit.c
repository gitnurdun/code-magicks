/* MAGIC: integer square root by restoring one result bit at a time.
   WHY: exact floor(sqrt(n)) with no floating point and no division. */
#include <stdint.h>
#include <stdio.h>
static uint32_t isqrt32(uint32_t n)
{
    uint32_t res = 0;
    uint32_t bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}
int main(void)
{
    printf("isqrt(1000000)=%u\n", isqrt32(1000000u));
    printf("isqrt(2)=%u\n", isqrt32(2u));
}

/* ================= EXPLANATION =================
   What it does
     Computes the largest integer r such that r*r <= n, using a bitwise
     "digit-by-digit" method analogous to long division. No sqrt call, no
     floating-point rounding, no division instruction.

   Step by step
     1. The result is built from the most significant bit down. bit starts
        at 2^30, the largest power of four that fits in 32 bits; the first
        loop shifts it down by two until it is <= n.
     2. At each step, the algorithm tentatively adds bit to the result.
        `res + bit` represents the next odd number in the identity
        (r + b)^2 = r^2 + 2rb + b^2. Subtracting it from the running
        remainder is the integer analogue of bringing down a digit.
     3. If the remainder is large enough, the bit is accepted:
        n -= res + bit and res = (res >> 1) + bit.
     4. Otherwise the bit is rejected and res is halved. Either way, bit
        moves two places right, because each step resolves two binary
        digits of the root.
     5. The loop ends after bit reaches zero. For 1,000,000 the result is
        exactly 1000; for 2 it is 1 (floor(sqrt(2))).

   Output
     "isqrt(1000000)=1000"
     "isqrt(2)=1"

   Why it matters
     Embedded code often has no FPU, and even with one, sqrt is correctly
     rounded but comparatively slow. This method is exact for the floor,
     uses only shifts, adds, and compares, and has a fixed 16 iterations
     for 32-bit input. It is the integer counterpart of the fast inverse
     square root: approximate methods are faster, but exact bit methods
     are predictable and portable.

   Caveats
     - The result is floor(sqrt(n)); it is not rounded to nearest.
     - This version is for uint32_t. A 64-bit version starts at 2^62 and
       needs a 64-bit accumulator; otherwise res + bit can overflow.
     - For very small inputs the initial while loop can shift bit to zero,
       which is fine: the second loop is skipped and 0 is returned.
     - Hardware sqrt or a library call is usually faster on modern CPUs;
       use this when you need exact integer semantics or no FPU.
   ============================================== */
