/* ASM MAGICK #28 - the stack frame: rsp, rbp, and the return address.
   Build: gcc -O2 -std=c11 28_stack_frame.c -o 28_stack_frame.exe
   MAGIC: a function call is a convention, not a language feature: the
   stack pointer, the frame pointer and a saved return address. */
#include <stdio.h>
#include <stdint.h>

__attribute__((noinline))
static void probe(void)
{
    int local = 0x1234;
    uintptr_t rsp, rbp;
    __asm__ volatile("movq %%rsp, %0" : "=r"(rsp));
    __asm__ volatile("movq %%rbp, %0" : "=r"(rbp));

    printf("[28] &local-rsp = %lld, rbp-rsp = %lld\n",
           (long long)((char *)&local - (char *)rsp),
           (long long)((char *)rbp - (char *)rsp));
    printf("[28] return address = %p (inside main)\n",
           __builtin_return_address(0));
    printf("[28] local = 0x%x, frame = %p\n", local,
           __builtin_frame_address(0));
}

int main(void)
{
    probe();
    return 0;
}

/* ================= EXPLANATION =================
   The call sequence (System V and Windows x64)
       call target        ; pushes the return address, jumps
       ...
     target:
       push rbp           ; save the caller's frame pointer
       mov  rbp, rsp      ; establish a stable frame base
       sub  rsp, N        ; reserve locals
       ...
       leave              ; mov rsp, rbp; pop rbp
       ret                ; pops the return address
     The return address is at [rsp] on entry, and at [rbp+8] after the
     standard prologue. Locals live below rbp, at negative offsets.

   Why the demo's numbers look odd
     At -O2 GCC may omit the frame pointer entirely (Windows x64 defaults
     to frame-pointer omission), so rbp is just another general-purpose
     register and `rbp-rsp` is not a frame size. The local is usually at a
     small negative offset from rsp, or kept in a register. That is the
     point: the frame layout is an ABI and optimization detail, not a
     language guarantee.

   Windows x64 specifics
     - The first four integer args are in RCX, RDX, R8, R9; the rest on the
       stack.
     - The caller reserves 32 bytes of "shadow space" for the callee to
       spill the four register args.
     - RSP must be 16-byte aligned before a call, so it is 8 mod 16 at
       function entry.
     - RBP, RBX, RDI, RSI, R12-R15 and XMM6-XMM15 are callee-saved.
     - There is no red zone (unlike SysV), so a leaf function may not write
       below RSP.

   Why it matters
     - Stack unwinding and exception handling need to know the frame
       layout; hand-written asm must provide unwind info (`.seh_*` or
       `.cfi_*`) or a crash will not produce a correct backtrace.
     - Coroutines (#21) save/restore rsp; setjmp/longjmp (#30) saves rip
       and rsp; a stack switch is just `mov rsp, new_stack`.
     - Stack overflow detection relies on the guard page below rsp.

   Output (varies)
     [28] &local-rsp = 44, rbp-rsp = 48
     [28] return address = 0x... (inside main)
     [28] local = 0x1234, frame = 0x...
     GCC kept a frame pointer in this build and placed the local in the
     call-shadow area above rsp; a different compiler or flag set can
     produce negative offsets or no rbp frame at all.

   Gotchas
     - Never return a pointer to a local; the frame disappears on return.
     - `__builtin_return_address(0)` can be wrong with inlining, tail
       calls, or omitted frame pointers; compile that function with
       `-fno-omit-frame-pointer` if you need reliable walks.
     - The stack grows downward on x86, so subtracting allocates.
     - Reading above rsp (the caller's frame) is legal only within the
       current stack object; doing it across functions is undefined.
   ============================================== */
