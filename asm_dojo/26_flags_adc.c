/* ASM MAGICK #26 - pushfq/popq and adc: read the flags, then use the carry.
   Build: gcc -O2 -std=c11 26_flags_adc.c -o 26_flags_adc.exe
   MAGIC: EFLAGS is a register you can save, inspect and feed to the next
   operation - the hidden operand of every arithmetic instruction. */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

static uint64_t flags_after_sub(uint64_t a, uint64_t b)
{
    uint64_t f;
    __asm__ volatile("cmpq %2, %1\n\t"
                     "pushfq\n\t"
                     "popq %0"
                     : "=r"(f)
                     : "r"(a), "r"(b)
                     : "cc", "memory");
    return f;
}

int main(void)
{
    uint64_t f = flags_after_sub(5, 3);
    printf("[26] 5-3: CF=%llu ZF=%llu SF=%llu OF=%llu\n",
           (unsigned long long)(f & 1), (unsigned long long)((f >> 6) & 1),
           (unsigned long long)((f >> 7) & 1), (unsigned long long)((f >> 11) & 1));

    f = flags_after_sub(3, 5);
    printf("[26] 3-5: CF=%llu ZF=%llu SF=%llu OF=%llu\n",
           (unsigned long long)(f & 1), (unsigned long long)((f >> 6) & 1),
           (unsigned long long)((f >> 7) & 1), (unsigned long long)((f >> 11) & 1));

    /* 128-bit add: add low halves, then ADC the high halves with the carry
       out of the low add. This is the manual version of __int128 addition. */
    uint64_t lo = UINT64_MAX, hi = 0;
    uint64_t alo = 1, ahi = 0;
    __asm__ volatile("addq %2, %0\n\t"
                     "adcq %3, %1"
                     : "+r"(lo), "+r"(hi)
                     : "r"(alo), "r"(ahi)
                     : "cc");
    printf("[26] (2^64-1) + 1 = hi=0x%016" PRIx64 " lo=0x%016" PRIx64 "\n", hi, lo);

    __int128 sum = (__int128)UINT64_MAX + 1;
    printf("[26] __int128 agrees: %s\n",
           (((__int128)hi << 64) | lo) == sum ? "yes" : "NO");
    return 0;
}

/* ================= EXPLANATION =================
   EFLAGS, the hidden operand
     Most arithmetic instructions write flags as a side effect:
         bit 0  CF  carry / borrow (unsigned overflow)
         bit 2  PF  parity of the low byte
         bit 6  ZF  result is zero
         bit 7  SF  result's sign bit
         bit 11 OF  signed overflow
         bit 10 DF  direction (string ops)
     Conditional jumps, SETcc, CMOVcc (#10, #11) and ADC/SBB all read them.
     PUSHFQ pushes RFLAGS onto the stack; POPQ reads it back. LAHF/SAHF do
     the same for the low byte via AH (CF, PF, AF, ZF, SF).

   The unsigned/signed split
     3 - 5 = -2. As unsigned, 3 < 5, so CF=1. As signed, -2 is
     representable, so OF=0. SF=1 because the result's top bit is 1. This
     is why the same CMP feeds both `jb` (unsigned below) and `jl` (signed
     less): the condition code chooses which flags to test.

   ADC: carry propagation
     ADD sets CF when the unsigned sum exceeds 64 bits. ADC adds
     src + CF. To add two 128-bit numbers:
         add lo1, lo2     ; CF = carry out of bit 63
         adc hi1, hi2     ; hi1 += hi2 + CF
     This is the core of bignum addition, and the basis of the MULX/ADCX/
     ADOX chains in #18. ADCX (BMI2) uses only CF; ADOX uses only OF, so
     two independent carry chains can run in parallel.

   Output
     [26] 5-3: CF=0 ZF=0 SF=0 OF=0
     [26] 3-5: CF=1 ZF=0 SF=1 OF=0
     [26] (2^64-1) + 1 = hi=0x0000000000000001 lo=0x0000000000000000
     [26] __int128 agrees: yes

   Gotchas
     - PUSHFQ/POPQ in inline asm changes RSP; GCC is not told, so keep it
       inside a single asm block with a "memory" clobber, as here. It is
       not safe to pop into an arbitrary operand if the compiler is using
       the red zone (Windows x64 has no red zone, but SysV does).
     - Do not assume flags survive a function call or a compiler barrier;
       they are caller-clobbered.
     - `pushfq` pushes 8 bytes; `popfq` pops 8. Mixing with 32-bit pushes
       corrupts the stack.
     - In user mode, POPFQ cannot change IOPL/IF; it silently ignores
       privileged bits.
   ============================================== */
