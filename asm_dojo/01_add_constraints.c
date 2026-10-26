/* ASM MAGICK #01 - the constraint alphabet: "=r", "0", "+r"
   Build: gcc -O2 -std=c11 01_add_constraints.c -o 01_add_constraints.exe
   MAGIC: you write the instruction; the compiler chooses the registers,
   inserts the moves, and keeps the ABI intact. */
#include <stdio.h>

int main(void)
{
    int a = 40, b = 2;

    /* "=r" is write-only, "0" ties an input to output operand 0. The
       compiler may place a in the same register that will hold sum. */
    int sum;
    __asm__ volatile("addl %2, %0"
                     : "=r"(sum)
                     : "0"(a), "r"(b));
    printf("[01] 40 + 2          = %d\n", sum);

    /* "+r" is read-write: the same register comes in with x and leaves
       with x+b. No separate output is needed. */
    int x = a;
    __asm__ volatile("addl %1, %0" : "+r"(x) : "r"(b));
    printf("[01] in-place x += 2 = %d\n", x);

    /* AT&T syntax is src,dst. This is the same add, but written as a
       64-bit LEA: address arithmetic that does not touch EFLAGS. */
    long la = a, lb = b;
    int lea;
    __asm__ volatile("leal (%1,%2), %0" : "=r"(lea) : "r"(la), "r"(lb));
    printf("[01] lea (no flags)  = %d\n", lea);
    return 0;
}

/* ================= EXPLANATION =================
   The four operands
     GCC extended asm is a contract, not a function call:
         asm [volatile] ("template" : outputs : inputs : clobbers);
     Each %N in the template is an operand. The constraint string tells
     GCC what kind of location is allowed:
       "r"  any general-purpose register
       "m"  a memory operand
       "i"  an immediate constant
       "a"/"b"/"c"/"d"  a specific register (rax/rbx/rcx/rdx)
       "0"  must be the same location as operand 0
       "="  write-only output, "+" read-write, "&" early-clobber

   What the CPU sees
     "addl %2, %0" becomes something like
         add  ecx, eax
     AT&T order is source, destination, and the l suffix means 32-bit.
     There is no type checking: %0 is whatever register GCC chose. If a
     template says "addl" but the operands are 64-bit, you silently
     truncate - one of the most common inline-asm bugs.

   Why this is the foundation
     Every later demo is a variation on this contract: rdtsc writes edx:eax
     (fixed registers), cpuid clobbers rbx, cmpxchg needs rax, the syscall
     ABI pins six registers. Learning to state those constraints precisely
     is what makes inline asm safe enough to ship.

   Output
     [01] 40 + 2          = 42
     [01] in-place x += 2 = 42
     [01] lea (no flags)  = 42

   Gotchas
     - "volatile" stops deletion/reordering, but does NOT make the asm
       atomic or a compiler barrier unless you add "memory".
     - Missing "cc" clobber lets GCC assume flags survive; missing "memory"
       lets it cache a buffer across the asm.
     - `asm` is a GNU extension; under -std=c11 use `__asm__`.
   ============================================== */
