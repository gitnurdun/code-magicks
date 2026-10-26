/* ASM MAGICK #27 - rdrand/rdseed: the CPU's own entropy source.
   Build: gcc -O2 -std=c11 27_rdrand.c -o 27_rdrand.exe
   MAGIC: a hardware random generator behind one instruction, with a
   carry flag that reports failure instead of returning a silent zero. */
#include <stdio.h>
#include <stdint.h>

static inline uint32_t cpuid_leaf1_ecx(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(1), "c"(0));
    return c;
}

static inline uint32_t cpuid_leaf7_ebx(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(7), "c"(0));
    return b;
}

static int rdrand64(uint64_t *out)
{
    unsigned char ok;
    __asm__ volatile("rdrand %1\n\t"
                     "setc %0"
                     : "=qm"(ok), "=r"(*out)
                     :
                     : "cc");
    return ok;
}

static int rdseed64(uint64_t *out)
{
    unsigned char ok;
    __asm__ volatile("rdseed %1\n\t"
                     "setc %0"
                     : "=qm"(ok), "=r"(*out)
                     :
                     : "cc");
    return ok;
}

int main(void)
{
    uint64_t v = 0;
    int ok = 0;

    if ((cpuid_leaf1_ecx() >> 30) & 1) {
        for (int tries = 0; tries < 10 && !ok; tries++) ok = rdrand64(&v);
        printf("[27] rdrand: ok=%d value=0x%016llx\n", ok, (unsigned long long)v);
    } else {
        printf("[27] RDRAND not available\n");
    }

    if ((cpuid_leaf7_ebx() >> 18) & 1) {
        ok = 0;
        for (int tries = 0; tries < 10 && !ok; tries++) ok = rdseed64(&v);
        printf("[27] rdseed: ok=%d value=0x%016llx\n", ok, (unsigned long long)v);
    } else {
        printf("[27] RDSEED not available\n");
    }
    return 0;
}

/* ================= EXPLANATION =================
   The instructions
     RDRAND reads a 16/32/64-bit value from the CPU's on-die digital random
     number generator (DRNG), which is seeded from a hardware entropy
     source and conditioned with AES. CF=1 on success, CF=0 if the pool is
     temporarily exhausted (retry).
     RDSEED reads a raw, less-conditioned seed from the same entropy
     source; it is slower and can fail much more often. CF reports
     success.

   Why the carry flag matters
     These instructions do not return an error code; they set CF. A caller
     that ignores CF can use an all-zero or stale value as "random". The
     demo's `setc` after the instruction is the entire error-handling
     contract. In a loop, bound the retries and fall back to a different
     source rather than spinning forever.

   What they are good for
     - Seeding a CSPRNG (RDSEED is designed for this).
     - Per-process ASLR/stack canary seeds at startup.
     - Simulation and games where a fast hardware source is convenient.
     - Key generation, but only after the output is mixed/conditioned and
       the failure path is handled. Do not build a cryptographic protocol
       directly on "RDRAND returned 64 bits".

   Output
     [27] rdrand: ok=1 value=0x... (different every run)
     [27] rdseed: ok=1 value=0x... (different every run)

   Gotchas
     - RDRAND/RDSEED are not guaranteed available: CPUID.1:ECX[30] and
       CPUID.7.0:EBX[18]. They can be disabled by a hypervisor.
     - The DRNG can be under attack or unreliable in some environments;
       NIST and OS vendors recommend mixing with other entropy.
     - RDSEED failure is normal under heavy demand; retry with a bound.
     - The values are not reproducible, so never use them for a
       deterministic simulation, replay, or lockstep networking.
     - `rdrand %1` with a 64-bit register is RDRAND r64; the assembler
       picks the size from the register. The operand must be a GPR, not
       memory.
   ============================================== */
