/* ASM MAGICK #31 - computed goto: a jump table of label addresses.
   Build: gcc -O2 -std=c11 31_computed_goto.c -o 31_computed_goto.exe
   MAGIC: the opcode is an index; the "switch" is one indirect jump and the
   handlers chain directly into each other. */
#include <stdio.h>

enum { PUSH, ADD, MUL, PRINT, HALT };

int main(void)
{
    int code[] = {PUSH, 6, PUSH, 7, MUL, PRINT, PUSH, 0, ADD, HALT};
    int stack[16], sp = 0, pc = 0;
    void *ops[] = {&&op_push, &&op_add, &&op_mul, &&op_print, &&op_halt};

#define NEXT goto *ops[code[pc++]]
    NEXT;

op_push:  stack[sp++] = code[pc++]; NEXT;
op_add:   sp--; stack[sp - 1] += stack[sp]; NEXT;
op_mul:   sp--; stack[sp - 1] *= stack[sp]; NEXT;
op_print: printf("[31] %d\n", stack[--sp]); NEXT;
op_halt:  return 0;
#undef NEXT
}

/* ================= EXPLANATION =================
   What the CPU executes
     `goto *ops[code[pc++]]` compiles to something like:
         mov  eax, [code + pc*4]
         inc  pc
         jmp  [ops + rax*8]
     That is an indirect branch. Each handler ends with another indirect
     branch, so the machine never returns to a central dispatch loop. This
     is "direct threaded code".

   Why it can be faster than switch
     A switch on a dense range compiles to a jump table too, but it still
     has a bounds check and a common dispatch point. Threaded code removes
     the bounds check and lets the branch predictor learn the transition
     from one opcode to the next (e.g., PUSH is usually followed by PUSH).
     For interpreters with a small, stable opcode set this can be a real
     win; for a large or unpredictable set it can lose.

   The label-address table
     `&&label` is a GCC/Clang extension that yields the address of a label.
     The array `ops` is initialized at run time (or by a relocation) with
     those addresses. The enum values must match the array order, or the
     VM jumps into the wrong handler.

   Where it appears
     - Forth engines and bytecode interpreters (older CPython, Lua-style
       dispatch, Wasm interpreters).
     - Regex engines (Thompson NFA with threaded dispatch).
     - Packet filters and protocol state machines.
     - Game scripting VMs and shader interpreters.

   Output
     [31] 42

   Gotchas
     - This is not portable C: MSVC has no computed goto (use a switch or
       an asm trampoline), and the label table is easy to desynchronize.
     - There is no sandbox: a corrupt opcode jumps anywhere. Validate
       bytecode before dispatch if it is untrusted.
     - Indirect branches can mispredict; the win is workload-dependent.
     - The handler labels are ordinary code; falling through without NEXT
       executes the next handler by accident.
   ============================================== */
