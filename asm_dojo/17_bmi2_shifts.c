/* ASM MAGICK #17 - shlx/shrx/sarx: variable shifts without CL.
   Build: gcc -O2 -std=c11 17_bmi2_shifts.c -o 17_bmi2_shifts.exe
   MAGIC: BMI2 frees the shift count from CL and removes the false
   dependency on the flags register. */
#include <stdio.h>
#include <stdint.h>

static inline uint32_t cpuid_leaf7_ebx(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(7), "c"(0));
    return b;
}

int main(void)
{
    uint64_t v = UINT64_C(0x0123456789ABCDEF);
    unsigned n = 40;
    uint64_t count = n;   /* SHLX's count operand is a 64-bit register */

    if ((cpuid_leaf7_ebx() >> 8) & 1) {
        uint64_t shl, shr, sar;
        /* Intel: shlx dest, src, count. AT&T: count, src, dest. */
        __asm__ volatile("shlxq %2, %1, %0" : "=r"(shl) : "r"(v), "r"(count));
        __asm__ volatile("shrxq %2, %1, %0" : "=r"(shr) : "r"(v), "r"(count));
        __asm__ volatile("sarxq %2, %1, %0" : "=r"(sar) : "r"(v), "r"(count));
        printf("[17] shlx 0x%016llx << %u = 0x%016llx\n",
               (unsigned long long)v, n, (unsigned long long)shl);
        printf("[17] shrx 0x%016llx >> %u = 0x%016llx\n",
               (unsigned long long)v, n, (unsigned long long)shr);
        printf("[17] sarx 0x%016llx >> %u = 0x%016llx (arithmetic)\n",
               (unsigned long long)v, n, (unsigned long long)sar);
    } else {
        printf("[17] BMI2 not available; shlx/shrx/sarx would #UD\n");
    }
    return 0;
}

/* ================= EXPLANATION =================
   The problem with CL
     The classic variable shift is `shl rax, cl`: the count must live in CL
     (the low byte of RCX). That forces the register allocator to move the
     count into RCX, and CL is part of RCX, which may hold a live value.
     Worse, shifts write the flags, creating a false dependency chain
     through EFLAGS that limits out-of-order execution.

   The BMI2 fix
     SHLX/SHRX/SARX take the count in any general-purpose register, leave
     flags untouched, and write the destination independently:
         shlx dst, src, count
     That removes both the RCX constraint and the flags dependency. It
     also means the compiler can schedule the shift without a MOV to CL.

   Arithmetic vs logical
     SHRX is a logical shift: zeros fill from the left. SARX is
     arithmetic: the sign bit is replicated. For signed division by a power
     of two, SARX rounds toward negative infinity, which is not C's
     truncation toward zero - a classic off-by-one for negative values.

   Where it appears
     - Hash mixing and PRNGs (xorshift, splitmix) with runtime shift
       amounts.
     - Bitstream readers that shift by a variable number of bits.
     - Compilers generating code for `x >> n` when BMI2 is enabled.

   Output
     [17] shlx 0x0123456789abcdef << 40 = 0xabcdef0000000000
     [17] shrx 0x0123456789abcdef >> 40 = 0x0000000000012345
     [17] sarx 0x0123456789abcdef >> 40 = 0x0000000000012345

   Gotchas
     - The count is masked to 6 bits for 64-bit operands (0..63), so a
       count of 64 becomes 0, not zero-fill. C says shifting by >= width is
       undefined; BMI2 gives a defined-but-surprising result.
     - BMI2 only; #UD elsewhere. Check CPUID.7.0:EBX[8].
     - AT&T operand order differs from Intel: count, src, dest.
     - The flags are not modified, so a following Jcc needs its own CMP.
   ============================================== */
