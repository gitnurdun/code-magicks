/* ASM MAGICK #02 - early-clobber "&": when the output is written before
   every input has been read.
   Build: gcc -O2 -std=c11 02_early_clobber.c -o 02_early_clobber.exe
   MAGIC: one character (&) prevents the compiler from choosing a register
   that destroys an input. */
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    /* Bug-shaped example: the mov writes %0 before %1 is read. Without
       "&", GCC may tie out and in to the same register, turning 7+5 into
       7+7. The & forces them apart. */
    int in = 5, out;
    __asm__ volatile("movl $7, %0\n\taddl %1, %0"
                     : "=&r"(out)
                     : "r"(in));
    printf("[02] 7 + 5 via early-clobber = %d\n", out);

    /* xchg swaps two read-write operands in place. */
    int a = 111, b = 222;
    __asm__ volatile("xchgl %0, %1" : "+r"(a), "+r"(b));
    printf("[02] xchg: a=%d b=%d\n", a, b);

    /* One-operand imul is hard-wired to rax * src -> rdx:rax. "+a" makes
       rax read-write; "=d" claims rdx. There is no way to ask for other
       registers: the instruction itself decides. */
    uint64_t lo = UINT64_C(0x123456789abcdef0);
    uint64_t hi;
    uint64_t y = UINT64_C(0x0fedcba987654321);
    __asm__ volatile("imulq %2" : "+a"(lo), "=d"(hi) : "r"(y));
    __int128 check = (__int128)UINT64_C(0x123456789abcdef0) * y;
    printf("[02] imul high=%016llx low=%016llx verify=%s\n",
           (unsigned long long)hi, (unsigned long long)lo,
           ((__int128)(((__int128)hi << 64) | lo) == check) ? "ok" : "BAD");
    return 0;
}

/* ================= EXPLANATION =================
   The ordering rule
     GCC's default assumption is: all inputs are consumed before any output
     is written. That lets it overlap an output register with an input
     register to save a move. It is usually right - and catastrophically
     wrong when the first instruction writes the output.

   What "&" means
     "=&r"(out) says: this output is written early; do not let it share a
     register with any input operand. GCC then allocates three distinct
     registers and emits the moves around the asm.

   The fixed-register instructions
     Some instructions do not take arbitrary operands:
       mul/imul  rax * src -> rdx:rax
       div       rdx:rax / src -> rax, remainder rdx
       cpuid     eax/ecx in, eax/ebx/ecx/edx out
       rdtsc     edx:eax out
       cmpxchg   rax is the comparand
       syscall   rax/rdi/rsi/rdx/r10/r8/r9
     For these, constraints like "a", "d", "+a" are mandatory; "&" is not
     enough. The register allocator will also insert push/pop for callee-
     saved registers it is forced to use.

   Output
     [02] 7 + 5 via early-clobber = 12
     [02] xchg: a=222 b=111
     [02] imul high=0123456789abcdef low=... verify=ok

   Gotchas
     - "&" is only about register overlap; it does not stop the compiler
       from reordering the asm with surrounding C.
     - If you clobber a register that is also an operand, GCC may not save
       it; list only registers the template touches and does not name.
     - Reading rax after "=a" is fine; reading flags after an add requires
       the "cc" clobber.
   ============================================== */
