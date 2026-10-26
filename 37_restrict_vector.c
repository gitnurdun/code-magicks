/* MAGIC: `restrict` promises two pointers never alias, so the compiler may
   vectorise a loop it would otherwise have to run one element at a time.
   WHY: aliasing is a semantic constraint before it is an optimisation. */
#include <stdio.h>
#include <stddef.h>
static void axpy(size_t n, float a,
                 const float *restrict x,
                 float *restrict y)
{
    for (size_t i = 0; i < n; i++)
        y[i] = a * x[i] + y[i];
}
int main(void)
{
    float x[4] = {1, 2, 3, 4};
    float y[4] = {10, 20, 30, 40};
    axpy(4, 2.0f, x, y);
    for (int i = 0; i < 4; i++) printf("%.0f%c", y[i], i == 3 ? '\n' : ' ');
}

/* ================= EXPLANATION =================
   What it does
     Computes y = a*x + y (the BLAS "axpy" operation) while telling the
     compiler that x and y point to disjoint objects. With that promise it
     can load several x values, multiply and add them in parallel, and
     store the results without worrying that a store through y changes a
     later load through x.

   Step by step
     1. `const float *restrict x` means: during this function call, any
        object accessed through x is not accessed through any other
        pointer. `float *restrict y` makes the same promise for y. The
        const only says the function will not write through x.
     2. Without restrict, the compiler must assume x[i] and y[i] may be
        the same address. It therefore has to finish each iteration's
        store before the next iteration's load - a loop-carried
        dependency that blocks SIMD.
     3. With restrict, iterations are independent. At -O2/-O3 GCC can
        emit vector loads, multiply-adds, and stores, processing 4 or 8
        floats per instruction when the target supports it.
     4. The loop runs four times, producing 2*1+10=12, 2*2+20=24,
        2*3+30=36, and 2*4+40=48.

   Output
     "12 24 36 48"

   Why it matters
     Aliasing analysis is one of the biggest differences between scalar
     and vector code. `restrict` is a zero-cost, standard C99 contract
     that unlocks it in numerical kernels, image processing, audio, and
     game math. C++ has no standard equivalent (compilers accept
     __restrict), which is one reason C remains common in hot loops.

   Caveats
     - Passing overlapping arrays is undefined behaviour, not "the
       compiler will notice". If x and y overlap, remove restrict or use
       an explicitly ordered scalar loop.
     - restrict is a promise about all accesses in the block, including
       pointers derived from the restricted pointer. A second alias can
       silently invalidate it.
     - The compiler is never required to vectorise; check the generated
       assembly or use -fopt-info-vec.
     - Vectorisation can change floating-point rounding unless the
       compiler is allowed to reassociate; use -ffast-math only when the
       numerical contract permits it.
   ============================================== */
