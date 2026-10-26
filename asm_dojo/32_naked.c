/* ASM MAGICK #32 - naked functions: no prologue, no epilogue, no compiler.
   Build: gcc -O2 -std=c11 32_naked.c -o 32_naked.exe
   MAGIC: the function body is exactly the bytes you wrote - GCC emits no
   stack frame, no saves, and no implicit return. */
#include <stdio.h>

__attribute__((naked))
static int answer(void)
{
    __asm__ volatile("movl $42, %eax\n\t"
                     "ret");
}

__attribute__((naked))
static int naked_max(int a __attribute__((unused)),
                     int b __attribute__((unused)))
{
    /* Windows x64: a in ecx, b in edx, result in eax. */
    __asm__ volatile("movl %ecx, %eax\n\t"
                     "cmpl %edx, %ecx\n\t"
                     "cmovll %edx, %eax\n\t"
                     "ret");
}

int main(void)
{
    printf("[32] naked answer() = %d\n", answer());
    printf("[32] naked max(17, 42) = %d, max(42, 17) = %d\n",
           naked_max(17, 42), naked_max(42, 17));
    return 0;
}

/* ================= EXPLANATION =================
   What "naked" means
     A normal function has a prologue (push rbp, mov rbp/rsp, sub rsp) and
     an epilogue (leave, ret), plus spills and unwind metadata. A naked
     function suppresses all of it: the compiler emits only the asm in the
     body. This is how you write a complete function in assembly, or a
     thunk/trampoline that must control the stack exactly.

   Why you would want it
     - Interrupt/service handlers that must preserve every register.
     - Context switches that save/restore a custom frame.
     - Tail-call thunks and ABI adapters (#34).
     - JIT/trampoline entry points that receive arguments in unusual
       registers.
     - Very small leaf functions where the prologue is pure overhead.

   Why it is dangerous
     - The compiler does not know the function clobbers registers unless
       you list them; there is no automatic save/restore. On Windows x64,
       RBX/RBP/RDI/RSI/R12-R15/XMM6-XMM15 are callee-saved and must be
       preserved by you if you touch them.
     - There is no unwind information. An exception or a debugger walk
       through a naked frame may fail or produce a wrong backtrace.
     - You cannot use C statements in the body (the compiler cannot
       generate code around them safely); only asm is allowed.
     - Inlining, sanitizers and profiling can interact badly.
     - The compiler may still emit a `ret` or alignment padding? It should
       not, but always disassemble and check.

   The ABI in one line
     Windows x64: args in RCX, RDX, R8, R9, return in EAX/RAX; caller
     reserves 32 bytes of shadow space; RSP is 16-byte aligned before a
     call. SysV x86-64: args in RDI, RSI, RDX, RCX, R8, R9, return in
     RAX. The same naked body is not portable between them.

   Output
     [32] naked answer() = 42
     [32] naked max(17, 42) = 42, max(42, 17) = 42

   Gotchas
     - `naked` is a GCC/Clang attribute; MSVC uses `__declspec(naked)` with
       different rules.
     - Do not call a naked function that does not preserve callee-saved
       registers from C and expect the caller to survive.
     - The `ret` must be present; without it, execution falls into whatever
       follows.
     - Stack alignment is your responsibility if you call another function.
   ============================================== */
