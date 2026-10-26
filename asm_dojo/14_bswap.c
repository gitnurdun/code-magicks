/* ASM MAGICK #14 - bswap and movbe: endianness is a byte order, not a
   conversion you should write by hand.
   Build: gcc -O2 -std=c11 14_bswap.c -o 14_bswap.exe
   MAGIC: one instruction reverses the byte order of a register; movbe
   does it while loading from memory. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static inline uint32_t cpuid_leaf1_ecx(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(1), "c"(0));
    return c;
}

int main(void)
{
    uint32_t v32 = 0x11223344u;
    uint64_t v64 = UINT64_C(0x1122334455667788);

    __asm__ volatile("bswapl %0" : "+r"(v32));
    __asm__ volatile("bswapq %0" : "+r"(v64));
    printf("[14] bswap32: 0x11223344 -> 0x%08x\n", v32);
    printf("[14] bswap64: 0x1122334455667788 -> 0x%016llx\n",
           (unsigned long long)v64);

    /* MOVBE loads a big-endian value from memory and byte-swaps it in one
       instruction. This is exactly what a network header parser wants. */
    unsigned char wire[4] = {0x11, 0x22, 0x33, 0x44};
    uint32_t be = 0;
    if ((cpuid_leaf1_ecx() >> 22) & 1) {
        __asm__ volatile("movbel %1, %0" : "=r"(be) : "m"(wire));
        printf("[14] movbe load: bytes 11 22 33 44 -> 0x%08x\n", be);
    } else {
        uint32_t tmp;
        memcpy(&tmp, wire, 4);
        __asm__ volatile("bswapl %0" : "+r"(tmp));
        be = tmp;
        printf("[14] movbe unavailable; bswap fallback -> 0x%08x\n", be);
    }

    printf("[14] this machine is %s-endian\n",
           (union { uint16_t u; unsigned char b[2]; }){1}.b[0] ? "little" : "big");
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     BSWAP r32/r64 reverses the bytes of a register:
         0x11223344 -> 0x44332211
     It has no effect on 16-bit values (use rol ax,8) and no flags.

   MOVBE
     MOVBE dst, [mem] loads 2/4/8 bytes and byte-swaps them as part of the
     move. It is a load-store-unit operation, so a struct field can be read
     in wire order without a separate register swap:
         movbe eax, [rdi]      ; Intel
         movbel (%rdi), %eax   ; AT&T
     CPUID.1:ECX[22] advertises it. Some virtual machines mask it off.

   Why endianness matters
     x86 is little-endian: the least-significant byte is at the lowest
     address. Network protocols (TCP/IP, DNS) and many file formats are
     big-endian. Reading a 32-bit length field with a plain load gives
     0x44332211 instead of 0x11223344. The fix is a swap, but doing it
     with shifts and masks is slower and easy to get wrong.

   The compiler builtins
     __builtin_bswap16/32/64 are portable and usually compile to BSWAP (or
     MOVBE when the value comes from memory). Use them unless you are
     writing the instruction for teaching or for a specific code path.

   Output
     [14] bswap32: 0x11223344 -> 0x44332211
     [14] bswap64: 0x1122334455667788 -> 0x8877665544332211
     [14] movbe load: bytes 11 22 33 44 -> 0x11223344
     [14] this machine is little-endian

   Gotchas
     - Byte order is a property of the data, not the CPU. Declare the wire
       format explicitly; do not "fix" it by swapping twice.
     - Bitfields and unions for wire parsing are implementation-defined;
       memcpy + bswap is the portable pattern.
     - MOVBE on unaligned addresses is allowed on modern x86 but can be
       slower; on some old Atoms it faults. Prefer aligned fields.
     - Don't confuse bswap (full reverse) with ror/rol (rotation) or with
       bit-reversal (#42).
   ============================================== */
