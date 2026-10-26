/* MAGIC: a tail call becomes a returned thunk; one loop runs the thunks,
   so recursion never grows the C stack. */
#include <stdio.h>
typedef struct bounce bounce;
struct bounce {
    bounce (*fn)(int, int);
    int n, acc;
    int done, value;
};
static bounce fact(int n, int acc)
{
    if (n <= 1) return (bounce){NULL, 0, acc, 1, acc};
    return (bounce){fact, n - 1, acc * n, 0, 0};
}
static int run(bounce b)
{
    while (!b.done) b = b.fn(b.n, b.acc);
    return b.value;
}
int main(void)
{
    printf("fact(5)=%d\n", run(fact(5, 1)));
}

/* ================= EXPLANATION =================
   What it does
     Computes factorial with a trampoline. Instead of calling fact()
     recursively and growing the stack, each step returns a small struct
     describing the next call. run() repeatedly executes those structs in
     a loop until one is marked done.

   Step by step
     1. fact(5, 1) does not recurse. It returns {fact, 4, 5, 0, 0}: call
        fact again with n = 4 and acc = 5. The C stack unwinds immediately.
     2. run() sees done == 0 and calls b.fn(4, 5), which returns
        {fact, 3, 20, 0, 0}. The loop continues, replacing b each time.
     3. Each step multiplies acc by n: 5, 20, 60, 120. At n = 1, fact
        returns {NULL, 0, 120, 1, 120}; no further call is needed.
     4. run() sees done == 1 and returns b.value = 120. Only one C frame
        for fact and one for run exist at any time, regardless of n.
     5. A real trampoline may use function pointers plus a union of
        argument types, or heap-allocated closures; this struct is the
        minimal shape.

   Output
     "fact(5)=120"

   Why it matters
     Tail calls are a control-flow primitive: a call in tail position can
     reuse the caller's frame. Scheme requires proper tail calls; C does
     not guarantee them, and deep recursion can overflow the stack.
     Trampolines implement tail calls portably and are used in interpreters
     for languages with deep recursion, in event loops, and in CPS
     compilers. They also make each pending step an explicit object, which
     can be paused, serialised, or scheduled.

   Caveats
     - This is not a general tail-call optimisation. Only calls returned
       directly can bounce; a call followed by more work cannot.
     - Each bounce may allocate if the thunk is heap-allocated. This demo
       returns by value to avoid allocation, but real closures often need
       the heap.
     - The loop adds a function-pointer indirect call and a struct copy per
       step; a compiler's own tail-call optimisation is faster when
       available.
     - A trampoline changes the shape of the code; debugging and stack
       traces no longer show the logical recursion.
     - C's `return f(x);` is not guaranteed to be a tail call; GCC/Clang
       may optimise it at -O2, but not across translation units or with
       certain ABIs.
   ============================================== */
