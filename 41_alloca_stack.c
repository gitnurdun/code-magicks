/* MAGIC: allocate a variable-sized array by moving the stack pointer,
   then let the function return reclaim it. WHY: no malloc, no free. */
#include <stdio.h>
#if defined(_WIN32)
#include <malloc.h>
#define STACK_ALLOC(n) _alloca(n)
#else
#include <alloca.h>
#define STACK_ALLOC(n) alloca(n)
#endif
int main(void)
{
    int n = 5;
    int *a = STACK_ALLOC((size_t)n * sizeof *a);
    for (int i = 0; i < n; i++) a[i] = i + 1;
    int sum = 0;
    for (int i = 0; i < n; i++) sum += a[i];
    printf("sum=%d cap=%d\n", sum, n);
}

/* ================= EXPLANATION =================
   What it does
     Allocates an array whose size is known only at run time from the
     current function's stack frame. The memory is valid until the
     function returns, then disappears with the frame - no free call.

   Step by step
     1. n is 5, so the requested size is 5 * sizeof(int) = 20 bytes.
     2. STACK_ALLOC expands to _alloca on Windows/MinGW and alloca on
        POSIX. Both are compiler builtins that subtract from the stack
        pointer (with alignment adjustment) and return the old pointer.
     3. The loop writes 1..5 into the new array. The array is ordinary
        writable memory; only its lifetime and location are unusual.
     4. The second loop sums the values to 15.
     5. main returns, the stack pointer is restored, and the 20 bytes are
        instantly reusable by the next call. No allocator metadata was
        touched.

   Output
     "sum=15 cap=5"

   Why it matters
     Stack allocation is the cheapest dynamic allocation: one pointer
     subtraction, excellent locality, and automatic reclamation. It is
     used for small temporary buffers in parsers, formatting routines,
     and numeric kernels. C99 variable-length arrays (VLAs) express the
     same idea with array syntax; alloca predates them and remains
     available as an extension. It also explains why deep recursion or a
     huge VLA can overflow the stack.

   Caveats
     - alloca is not ISO C (C23 made VLA support optional; C11 made it
       optional too). Behaviour on allocation failure is undefined - it
       usually crashes rather than returning NULL.
     - Never return a pointer to alloca'd memory, and never use it in a
       loop without bounded iterations: each call consumes stack until
       the function returns.
     - Large or attacker-controlled sizes are a stack-overflow denial of
       service. Keep it to small, bounded buffers.
     - The memory is not zeroed and has no destructor.
     - A VLA's size expression is evaluated once, but its lifetime ends
       at the enclosing block, not necessarily the whole function.
   ============================================== */
