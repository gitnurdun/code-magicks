/* MAGIC: a jump table of label addresses turns bytecode dispatch into
   one indirect jump per instruction. WHY: no switch range check. */
#include <stdio.h>
enum { PUSH, ADD, MUL, PRINT, HALT };
int main(void)
{
    int code[] = {PUSH, 6, PUSH, 7, MUL, PRINT, PUSH, 0, ADD, HALT};
    int stack[16], sp = 0, pc = 0;
    void *ops[] = {&&op_push, &&op_add, &&op_mul, &&op_print, &&op_halt};
#define NEXT goto *ops[code[pc++]]
    NEXT;
op_push: stack[sp++] = code[pc++]; NEXT;
op_add:  sp--; stack[sp - 1] += stack[sp]; NEXT;
op_mul:  sp--; stack[sp - 1] *= stack[sp]; NEXT;
op_print: printf("%d\n", stack[--sp]); NEXT;
op_halt: return 0;
#undef NEXT
}

/* ================= EXPLANATION =================
   What it does
     Implements a tiny stack virtual machine with "threaded code": each
     opcode is an index into a table of label addresses, and dispatch is a
     single computed goto rather than a switch with a bounds check.

   Step by step
     1. The program is the integer array code: push 6, push 7, multiply,
        print, push 0, add, halt. PUSH is followed by its immediate
        operand in the same array.
     2. ops holds the address of each opcode handler using GCC's
        `&&label` extension. The array order matches the enum.
     3. NEXT expands to `goto *ops[code[pc++]]`: read the next opcode,
        advance pc, and jump to its handler. There is no switch, no jump
        table bounds check, and no return to a central dispatch loop.
     4. Each handler finishes with NEXT, so the next instruction is
        entered directly. MUL pops 7 and 6 and pushes 42; PRINT pops and
        prints it; the final PUSH/ADD leaves 42 on the stack, then HALT
        returns from main.
     5. The result is 42.

   Output
     "42"

   Why it matters
     This is the classic "direct threaded interpreter" technique used by
     Forth engines, CPython's older ceval loop, Lua-style VMs, and JIT
     dispatch tables. Chaining jumps lets the CPU's indirect-branch
     predictor learn opcode-to-handler transitions, often beating a
     switch whose targets are less predictable. It also makes the
     instruction set explicit: bytecode is data, handlers are code.

   Caveats
     - `&&label` and computed goto are GCC/Clang extensions, not ISO C.
       MSVC needs a switch or an assembly trampoline.
     - The label table must be kept in sync with the opcode numbering;
       a wrong index jumps into arbitrary code.
     - This is not portable bytecode and offers no sandbox: malformed
       code can jump anywhere. Validate bytecode before dispatch in a
       real VM.
     - Indirect branches can still mispredict; a switch may be faster for
       a small, highly skewed opcode set.
   ============================================== */
