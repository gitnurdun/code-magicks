/* ASM MAGICK #29 - alloca by hand: sub rsp, use, add rsp.
   Build: gcc -O2 -std=c11 29_alloca_rsp.c -o 29_alloca_rsp.exe
   MAGIC: dynamic stack allocation is one subtraction. No allocator, no
   free, and the memory vanishes when the function returns. */
#include <stdio.h>
#include <stdint.h>

__attribute__((noinline))
static uint64_t stack_scratch(uint64_t *addr_out)
{
    uint64_t addr, value;
    __asm__ volatile(
        "movq %%rsp, %%rax\n\t"        /* old top, only for the report */
        "subq $64, %%rsp\n\t"          /* allocate 64 bytes */
        "movq %%rsp, %0\n\t"           /* publish the new pointer */
        "movabsq $0x1122334455667788, %%rcx\n\t"
        "movq %%rcx, (%%rsp)\n\t"
        "movq (%%rsp), %1\n\t"         /* read it back */
        "addq $64, %%rsp\n\t"          /* free: restore the old top */
        : "=r"(addr), "=r"(value)
        : 
        : "rax", "rcx", "memory");
    *addr_out = addr;
    return value;
}

int main(void)
{
    uint64_t addr = 0;
    uint64_t v = stack_scratch(&addr);
    unsigned char here;
    printf("[29] scratch at %p, %lld bytes below this frame\n",
           (void *)addr, (long long)((char *)&here - (char *)addr));
    printf("[29] value read back = 0x%016llx\n", (unsigned long long)v);
    return 0;
}

/* ================= EXPLANATION =================
   What alloca does
     The stack grows down on x86. To allocate N bytes:
         sub rsp, N
         mov result, rsp
     To free, restore the old rsp (or just return; the caller's epilogue
     does it). The memory is ordinary writable RAM with automatic lifetime.
     `alloca` is a compiler builtin, not a library call: GCC emits the
     subtraction and may add alignment and stack-probe code.

   The demo's discipline
     The sub/use/add all happen inside one asm block, so the compiler never
     sees a temporarily moved rsp. That is essential: if you moved rsp in
     one asm block and used C locals in another, the compiler's rsp-relative
     addressing would point into your scratch space and corrupt the frame.
     If you need a long-lived alloca, use the builtin, not raw asm.

   Alignment
     64 is a multiple of 16, so rsp stays 16-byte aligned. If you subtract
     an arbitrary size, round down to a multiple of 16 before the sub:
         sub rsp, N
         and rsp, -16
     but then you must save the original rsp to restore it, and on Windows
     x64 you must not disturb the 32-byte shadow space of the current
     function's callees.

   Why it is magic
     It is the cheapest dynamic allocation in existence: one ALU op, perfect
     locality, and automatic reclamation. Parsers, formatting routines,
     small scratch buffers and numeric kernels use it. It is also how
     variable-length arrays (C99 VLAs) are implemented.

   Output
     [29] scratch at 0x... (address), 111 bytes below this frame
     [29] value read back = 0x1122334455667788
     (The 111 is the 64-byte allocation plus the frame's own offset; it
      changes with compiler and optimization level.)

   Gotchas
     - Allocation failure is not reported; you get a guard-page fault
       (stack overflow) instead of NULL.
     - Never return a pointer to it; the frame is gone.
     - Do not call alloca in an unbounded loop; each call consumes stack
       until the function returns.
     - Large or attacker-controlled sizes are a denial-of-service vector.
     - Windows x64 has no red zone; writing below rsp without adjusting it
       can be clobbered by an interrupt or a debugger.
     - The compiler may reorder locals relative to your asm; a "memory"
       clobber plus keeping the whole sequence in one block is the safe
       pattern.
   ============================================== */
