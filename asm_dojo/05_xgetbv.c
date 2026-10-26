/* ASM MAGICK #05 - xgetbv: the OS's permission slip for wide registers.
   Build: gcc -O2 -std=c11 05_xgetbv.c -o 05_xgetbv.exe
   MAGIC: CPUID says the silicon supports AVX; XGETBV says the OS actually
   saves the state, so the instructions are safe to execute. */
#include <stdio.h>
#include <stdint.h>

static void cpuid(uint32_t leaf, uint32_t sub,
                  uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid"
                     : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(leaf), "c"(sub));
}

static uint64_t xgetbv0(void)
{
    uint32_t lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((uint64_t)hi << 32) | lo;
}

int main(void)
{
    uint32_t a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);

    int osxsave = (c >> 27) & 1;
    int avx_cpu = (c >> 28) & 1;
    int avx_usable = 0;
    uint64_t xcr0 = 0;

    if (osxsave && avx_cpu) {
        xcr0 = xgetbv0();
        avx_usable = (xcr0 & 0x6) == 0x6;   /* XMM and YMM state enabled */
    }

    printf("[05] OSXSAVE=%d AVX(cpu)=%d XCR0=0x%llx AVX(usable)=%d\n",
           osxsave, avx_cpu, (unsigned long long)xcr0, avx_usable);
    printf("[05] XCR0 bits: x87=%d SSE=%d AVX=%d opmask=%d ZMM_hi=%d hi16=%d\n",
           (int)(xcr0 & 1), (int)((xcr0 >> 1) & 1), (int)((xcr0 >> 2) & 1),
           (int)((xcr0 >> 5) & 1), (int)((xcr0 >> 6) & 1), (int)((xcr0 >> 7) & 1));
    return 0;
}

/* ================= EXPLANATION =================
   The problem
     AVX/AVX-512 add 256-bit and 512-bit registers. Those registers are not
     automatically preserved across a context switch unless the kernel
     allocates space for them. If an OS does not know about AVX, a process
     using YMM registers will silently corrupt its neighbor's state.

   The protocol
     1. CPUID.1:ECX[27] OSXSAVE - the OS has set CR4.OSXSAVE, so XGETBV is
        available.
     2. CPUID.1:ECX[28] AVX - the CPU has AVX.
     3. XGETBV with ECX=0 returns the 64-bit XCR0 mask. Bit 1 (SSE) and bit
        2 (AVX) must both be set before executing AVX instructions. For
        AVX-512 also check bits 5, 6, 7.
     Libraries that skip step 3 crash with #UD on machines where the CPU
     supports AVX but the kernel does not enable it (common in old VMs and
     in some hypervisors).

   Why "c"(0)
     XGETBV reads the control register selected by ECX. ECX is an input,
     and EAX:EDX are outputs - exactly the fixed-register constraints shown
     here. xsetbv is privileged, so user mode can read but not write XCR0.

   Output on this machine
     [05] OSXSAVE=1 AVX(cpu)=1 XCR0=0x7 AVX(usable)=1
     [05] XCR0 bits: x87=1 SSE=1 AVX=1 opmask=0 ZMM_hi=0 hi16=0
     0x7 means x87, SSE and AVX state are enabled; no AVX-512 state.

   Gotchas
     - XGETBV without OSXSAVE is #UD. Check first.
     - CPUID feature bits are per-CPU; on hybrid CPUs (P/E cores) the
       usable set can differ. Query on the core that will run the code, or
       use the kernel's HWCAP/IsProcessorFeaturePresent.
     - XCR0 does not tell you about AVX2/AVX-512; that is CPUID.7.
   ============================================== */
