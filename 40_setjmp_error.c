/* MAGIC: setjmp saves a stack context; longjmp restores it, unwinding
   the C stack without running destructors. WHY: exceptions before C++. */
#include <stdio.h>
#include <setjmp.h>
static jmp_buf env;
static void fail(int code) { longjmp(env, code); }
int main(void)
{
    int rc = setjmp(env);
    if (rc == 0) {
        puts("try");
        fail(42);
        puts("unreachable");
    } else {
        printf("caught %d\n", rc);
    }
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Implements a non-local jump: setjmp records the current execution
     context, and longjmp restores it from a nested call, abandoning every
     intermediate stack frame.

   Step by step
     1. setjmp(env) saves the stack pointer, instruction pointer, and
        callee-saved registers in the jmp_buf. It returns 0 on the direct
        path.
     2. The `if (rc == 0)` branch runs the "try" body and prints "try".
     3. fail(42) calls longjmp(env, 42). This does not return to fail;
        it replaces the machine context with the one saved by setjmp.
        Control reappears at the setjmp call as if it had just returned.
     4. This time setjmp returns the value passed to longjmp, 42, so the
        else branch prints "caught 42". Because longjmp never returns
        through fail's frame, the puts after fail is never executed.
     5. A longjmp value of 0 is changed to 1 by the standard, so 0 is
        reserved to mean "first return".

   Output
     "try"
     "caught 42"

   Why it matters
     setjmp/longjmp is the portable C mechanism behind error unwinding in
     interpreters, coroutine libraries, and old exception systems. It
     shows that the C stack is just a pointer plus saved registers: a
     "return" can be manufactured without unwinding frames normally. It
     is also the reason C++ exceptions cannot safely pass through a
     longjmp, and why `goto` cannot cross functions.

   Caveats
     - Locals modified between setjmp and longjmp and not declared
       volatile have indeterminate values after the jump. Keep state in
       volatile locals or globals.
     - Destructors, cleanup attributes, and stack unwinding do not run.
       File descriptors, locks, and allocations leak.
     - longjmp from a signal handler is only safe for a restricted set of
       signals and functions (sigsetjmp/siglongjmp).
     - Do not longjmp into a function that has already returned; the
       saved context is dangling.
     - C++ objects with non-trivial destructors make this undefined.
   ============================================== */
