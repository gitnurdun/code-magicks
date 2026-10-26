/* ASM MAGICK #04 - cpuid: asking the CPU who it is, without clobbering rbx.
   Build: gcc -O2 -std=c11 04_cpuid.c -o 04_cpuid.exe
   MAGIC: one instruction returns four 32-bit answers, including a 12-byte
   vendor string packed into three registers. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void cpuid(uint32_t leaf, uint32_t sub,
                  uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    /* "=b" is allowed on x86-64: GCC saves/restores rbx around the asm
       because rbx is callee-saved in both the SysV and Windows ABIs. */
    __asm__ volatile("cpuid"
                     : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(leaf), "c"(sub));
}

int main(void)
{
    uint32_t a, b, c, d;
    char vendor[13] = {0};

    cpuid(0, 0, &a, &b, &c, &d);
    memcpy(vendor + 0, &b, 4);   /* EBX = "Genu" */
    memcpy(vendor + 4, &d, 4);   /* EDX = "ineI" */
    memcpy(vendor + 8, &c, 4);   /* ECX = "ntel" */
    printf("[04] vendor=%s max_leaf=%u\n", vendor, a);

    cpuid(1, 0, &a, &b, &c, &d);
    printf("[04] family=%u model=%u stepping=%u\n",
           ((a >> 8) & 0xf) + ((a >> 20) & 0xff),
           ((a >> 4) & 0xf) | (((a >> 16) & 0xf) << 4),
           a & 0xf);
    printf("[04] sse2=%u popcnt=%u sse4.2=%u osxsave=%u avx=%u rdrand=%u\n",
           (d >> 26) & 1, (c >> 23) & 1, (c >> 20) & 1,
           (c >> 27) & 1, (c >> 28) & 1, (c >> 30) & 1);

    cpuid(7, 0, &a, &b, &c, &d);
    printf("[04] bmi1=%u avx2=%u bmi2=%u rdseed=%u\n",
           (b >> 3) & 1, (b >> 5) & 1, (b >> 8) & 1, (b >> 18) & 1);
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     CPUID takes a leaf in EAX and a subleaf in ECX, and returns four
     results in EAX, EBX, ECX, EDX. Leaf 0 is special: EAX is the highest
     basic leaf, and EBX/EDX/ECX hold the 12 ASCII bytes of the vendor ID
     in that (non-obvious) order.

   Why "=b" is interesting
     RBX is callee-saved. A naive `asm("cpuid" : ... : "a"(leaf))` that
     writes rbx without telling GCC corrupts the caller's register. Using
     "=b" makes GCC treat rbx as an output operand; it will save and restore
     it if necessary. The alternative - the one used in hand-written
     assembly - is `xchg rbx, save` around the instruction.

   Decoding family/model
     The raw fields are not simply "family = bits 11:8". If family == 0xF,
     add the extended family (bits 27:20); if model == 0xF (or family 6),
     combine the extended model (bits 19:16) with the base model. The code
     above deliberately shows the short form; production detection code uses
     the full algorithm.

   Feature bits used later in this dojo
     CPUID.1:ECX[20] SSE4.2  -> crc32 instruction (#15)
     CPUID.1:ECX[23] POPCNT  -> popcnt (#13)
     CPUID.1:ECX[27] OSXSAVE -> xgetbv (#05)
     CPUID.7.0:EBX[3] BMI1   -> tzcnt/lzcnt (#12)
     CPUID.7.0:EBX[8] BMI2   -> pdep/pext/shlx/mulx (#16-#18)
     CPUID.7.0:EBX[18] RDSEED-> rdseed (#27)

   Why it matters
     Every optimized library does this once at startup and selects a code
     path. Doing it wrong - assuming AVX2, using an instruction the OS has
     not enabled, or forgetting to check OSXSAVE before XGETBV - is the
     classic "works on my machine" crash.

   Gotchas
     - CPUID is serializing and slow (hundreds of cycles): cache the result.
     - The leaf range is sparse; check max_leaf before querying.
     - Hypervisors lie or mask bits; treat it as a hint, not a guarantee.
     - CPUID clobbers all four registers; list them all as outputs.
   ============================================== */
