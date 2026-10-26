/* MAGIC: tell the compiler which side of a branch is hot or cold. */
#include <stdio.h>
#define likely(x)__builtin_expect(!!(x),1)
#define unlikely(x)__builtin_expect(!!(x),0)
int main(void){int n=4;if(unlikely(n<0))puts("error");else if(likely(n%2==0))puts("fast");}

/* ================= EXPLANATION =================
   What it does
     Marks one side of a condition as the expected ("hot") path so the
     compiler can arrange the code for it. The hints are advice only; they
     never change what the program computes.

   Step by step
     1. likely(x) expands to __builtin_expect(!!(x), 1). The !! normalises
        any scalar to 0/1 so the builtin receives exactly a Boolean.
     2. unlikely(x) is the same with expected value 0.
     3. n = 4, so unlikely(n < 0) is false and the error path is skipped.
     4. likely(n % 2 == 0) is true, so "fast" is printed. Because n is a
        compile-time constant here, the optimiser folds the whole branch
        away; in real code n would come from input.

   Output
     "fast"

   Why it matters
     The compiler may lay out the hot path as fall-through, move the cold
     path out of line, and weight its inlining and block-ordering
     decisions accordingly. This keeps the instruction cache dense on the
     path that actually runs, which is the usual goal in kernels and
     parsers.

   Caveats
     - Wrong hints can make code slower; they are not free.
     - Modern CPUs predict from history, so the binary layout matters more
       than the static prediction bit the hint sometimes sets.
     - Profile-guided optimisation (PGO) or __builtin_expect_with_probability
       gives better data than hand-written guesses.
     - The main risk is readability and stale assumptions after refactors.
   ============================================== */
