/* MAGIC: the compiler executes a parser while compiling the program. */
#include <cstdio>
template<size_t N>constexpr unsigned sum(const char(&s)[N],size_t i=0){return i+1==N?0:(unsigned)s[i]+sum(s,i+1);}
int main(){constexpr auto n=sum("abc");static_assert(n==294);std::printf("%u\n",n);}

/* ================= EXPLANATION =================
   What it does
     Runs a tiny parser at compile time: a constexpr function recurses over
     a string literal and sums its characters, and a static_assert proves
     the compiler computed the result without running the program.

   Step by step
     1. `template<size_t N> constexpr unsigned sum(const char (&s)[N],
        size_t i = 0)` accepts an array by reference, so N is deduced as
        the literal's size including the terminating NUL (4 for "abc").
     2. The base case is i + 1 == N, i.e. the last element, which is the
        '\0' terminator; it contributes 0 and stops the recursion.
     3. Otherwise it returns (unsigned)s[i] + sum(s, i + 1): the current
        character value plus the rest. For "abc" that is 97 + 98 + 99.
     4. main() forces evaluation in a constant expression:
        `constexpr auto n = sum("abc")`. The static_assert(n == 294) is
        checked while compiling, so a wrong parser would fail the build.
     5. The program then prints the already-computed constant.

   Output
     "294"

   Why it matters
     constexpr moves work from run time to compile time: lookup tables,
     string hashes, format validation, and parsers can be executed by the
     compiler, producing constants with zero runtime cost. static_assert
     turns the computation into a compile-time test. C++20's consteval and
     constinit make the intent even stricter.

   Caveats
     - Recursion depth and constexpr operation counts are limited by the
       compiler; deep or long inputs may hit the limit.
     - N includes the NUL terminator, so the base case must exclude it;
       off-by-one here is easy.
     - char may be signed; for non-ASCII bytes the sum can be negative
       before the cast to unsigned. Use unsigned char for byte semantics.
     - This is a toy parser; real compile-time parsing usually uses
       fixed-size arrays and avoids unbounded recursion.
   ============================================== */
