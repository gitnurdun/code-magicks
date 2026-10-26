/* ASM MAGICK #34 - the Windows x64 calling convention, by hand.
   Build: gcc -O2 -std=c11 34_abi_thunk.c -o 34_abi_thunk.exe
   MAGIC: four arguments in RCX/RDX/R8/R9, 32 bytes of shadow space, a
   16-byte-aligned stack, and a call - all visible in one naked function. */
#include <stdio.h>

__attribute__((noinline, used))
int sum4(int a, int b, int c, int d)
{
    return a + b + c + d;
}

__attribute__((naked))
static int call_sum4(void)
{
    __asm__ volatile(
        "subq $40, %rsp\n\t"     /* 32 shadow + 8 to realign */
        "movl $1, %ecx\n\t"
        "movl $2, %edx\n\t"
        "movl $3, %r8d\n\t"
        "movl $4, %r9d\n\t"
        "call sum4\n\t"
        "addq $40, %rsp\n\t"
        "ret");
}

int main(void)
{
    printf("[34] direct sum4(1,2,3,4) = %d\n", sum4(1, 2, 3, 4));
    printf("[34] via naked asm thunk   = %d\n", call_sum4());
    return 0;
}

/* ================= EXPLANATION =================
   Windows x64 argument passing
     The first four integer/pointer arguments go in RCX, RDX, R8, R9, in
     that order. The fifth and later go on the stack, at [rsp+32],
     [rsp+40], ... . The caller must reserve 32 bytes of "shadow space"
     (also called home space) immediately above the return address, even
     if the callee takes fewer than four arguments. The callee may spill
     its register arguments there.
     The return value goes in RAX/EAX; floats use XMM0-XMM3.

   Stack alignment
     At the instruction after a `call`, RSP % 16 == 8 (the call pushed the
     8-byte return address). Before the next `call`, RSP must be 16-byte
     aligned. This thunk subtracts 40: 8 - 40 = -32, which is 0 mod 16,
     and 32 of those bytes are the shadow space. Then it sets the four
     argument registers and calls sum4. After the return, it adds 40 and
     returns; the result is already in EAX.

   Why a naked thunk
     This is how you call a function with a different signature, build a
     closure/trampoline, or adapt a callback. A normal C function would
     also work, but the compiler hides the ABI; the naked version shows
     the exact contract.

   SysV x86-64 for contrast
     RDI, RSI, RDX, RCX, R8, R9; no shadow space; the stack is 16-byte
     aligned before the call, so a callee sees RSP % 16 == 8. A thunk
     written for Windows will pass the wrong arguments on Linux and vice
     versa - this is the classic cross-platform asm bug.

   Output
     [34] direct sum4(1,2,3,4) = 10
     [34] via naked asm thunk   = 10

   Gotchas
     - Forgetting the shadow space lets the callee spill into your locals.
     - Forgetting the alignment fix causes #GP or SSE faults in the callee
       (Windows requires 16-byte alignment for XMM spills).
     - Variadic functions (printf) require AL = number of vector registers
       used; a hand-written call to printf must set AL (0 if no floats).
     - The callee may clobber RAX, RCX, RDX, R8-R11, XMM0-XMM5; the
       caller-saved set is not preserved.
     - A naked function has no unwind info; an exception through it is not
       guaranteed to unwind correctly.
   ============================================== */
