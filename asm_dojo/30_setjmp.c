/* ASM MAGICK #30 - setjmp/longjmp: a non-local goto that restores the
   machine state saved by setjmp.
   Build: gcc -O2 -std=c11 30_setjmp.c -o 30_setjmp.exe
   MAGIC: longjmp does not unwind; it reloads rip, rsp and the callee-saved
   registers, teleporting execution back to a saved point. */
#include <stdio.h>
#include <setjmp.h>

static jmp_buf env;

__attribute__((noinline, noreturn))
static void deep(int n)
{
    if (n == 0)
        longjmp(env, 7);      /* returns from setjmp with value 7 */
    deep(n - 1);
}

int main(void)
{
    volatile int marker = 1;
    int r = setjmp(env);

    if (r == 0) {
        printf("[30] setjmp returned 0: jumping 5 frames deep\n");
        deep(5);
        printf("[30] unreachable\n");
    } else {
        printf("[30] longjmp returned %d, marker=%d\n", r, marker);
    }
    return 0;
}

/* ================= EXPLANATION =================
   What setjmp saves
     On x86-64, jmp_buf holds the registers that must survive a call:
       - RIP (the return point)
       - RSP (the stack pointer at setjmp)
       - RBP, RBX, R12-R15 (callee-saved GPRs)
       - on Windows, XMM6-XMM15 (callee-saved SIMD)
       - the signal mask on some systems (sigsetjmp/siglongjmp)
     It does not save the whole stack. longjmp restores those registers and
     jumps to the saved RIP, so all frames between the two points are
     abandoned, not unwound.

   Why the return value is weird
     setjmp returns 0 on the direct call. When longjmp(env, v) runs, it
     restores the saved state and makes setjmp appear to return v. If v is
     0, longjmp substitutes 1, because 0 is reserved for "direct return".
     That is why the code branches on r == 0.

   volatile is mandatory
     A local modified between setjmp and longjmp and read after the jump
     must be volatile, or its value is indeterminate (it may live in a
     callee-saved register that longjmp restores to the old value, or in a
     stack slot that is still there). `marker` is volatile for exactly this
     reason.

   Where it is used
     - C error handling without exceptions: Lua's protected calls, CPython's
       interpreter loop, and many parsers use setjmp for "throw".
     - Coroutines and green threads: save the stack pointer, switch to
       another stack, longjmp back.
     - Test harnesses and sandbox escape paths.

   Output
     [30] setjmp returned 0: jumping 5 frames deep
     [30] longjmp returned 7, marker=1

   Gotchas
     - longjmp does not run C++ destructors, close files, or release locks.
       It is a leak generator if used carelessly.
     - setjmp must be called in a function that is still active when
       longjmp runs; jumping into a returned function is undefined.
     - The jmp_buf must remain valid (not a local of a returned frame).
     - The value passed to longjmp is an int; passing 0 becomes 1.
     - setjmp is a macro; do not take its address or wrap it in a way that
       hides the call from the compiler.
     - Signal handlers need sigsetjmp/siglongjmp to restore the signal mask.
   ============================================== */
