/* ASM MAGICK #11 - setcc and the sign-mask abs.
   Build: gcc -O2 -std=c11 11_setcc_abs.c -o 11_setcc_abs.exe
   MAGIC: a comparison becomes a byte (0/1), and a sign bit becomes a mask
   that negates without a branch. */
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

static inline int less_than(int a, int b)
{
    int r;
    /* setcc writes one byte; movzbl clears the upper 24 bits. */
    __asm__ volatile("cmpl %2, %1\n\t"
                     "setl %b0\n\t"
                     "movzbl %b0, %0"
                     : "=r"(r)
                     : "r"(a), "r"(b)
                     : "cc");
    return r;
}

static inline int iabs_asm(int x)
{
    int r;
    /* cdq/cltd sign-extends eax into edx: edx = x < 0 ? -1 : 0.
       xor with the mask flips all bits when negative; sub then adds 1,
       i.e. two's-complement negation. */
    __asm__ volatile("movl %1, %%eax\n\t"
                     "cltd\n\t"
                     "xorl %%edx, %%eax\n\t"
                     "subl %%edx, %%eax\n\t"
                     "movl %%eax, %0"
                     : "=r"(r)
                     : "r"(x)
                     : "eax", "edx", "cc");
    return r;
}

int main(void)
{
    printf("[11] 3 < 9  = %d\n", less_than(3, 9));
    printf("[11] 9 < 3  = %d\n", less_than(9, 3));
    printf("[11] abs(42)  = %d\n", iabs_asm(42));
    printf("[11] abs(-42) = %d\n", iabs_asm(-42));
    printf("[11] abs(INT_MIN) = %d (still negative: no positive counterpart)\n",
           iabs_asm(INT_MIN));
    return 0;
}

/* ================= EXPLANATION =================
   SETcc
     SETcc r/m8 writes 1 or 0 according to the flags, without a branch.
     It writes only one byte, so the destination's upper bits are
     untouched. Two safe ways to materialize a full int:
         xor eax, eax ; cmp ... ; setl al
         cmp ... ; setl al ; movzx eax, al
     The second is what the demo does; `%b0` asks GCC for the byte name of
     operand 0 (al, cl, ...).

   The mask trick
     For a two's-complement int x:
         mask = x >> 31        (arithmetic shift: 0 or -1)
         abs  = (x ^ mask) - mask
     If x >= 0, mask = 0, so nothing changes. If x < 0, mask = 0xFFFFFFFF,
     x ^ mask = ~x, and ~x - (-1) = ~x + 1 = -x. CDQ/QWORD is the
     one-instruction way to produce that mask from EAX: it copies the sign
     of EAX into every bit of EDX.

   Why not just `neg` + `cmovs`?
     Both are branchless. The mask form uses only ALU ops and no flags
     dependency for the select, which can matter in a vectorized or
     deeply pipelined loop. Compilers usually choose `neg`/`cmov` or
     `sar`/`xor`/`sub` depending on context; all are valid.

   The INT_MIN trap
     -INT_MIN overflows: the mathematical result 2147483648 is not
     representable, and two's complement wraps back to INT_MIN. The mask
     trick has the same behavior. If INT_MIN is possible, widen to
     int64_t or handle it explicitly; `abs()` in C also has undefined
     behavior for INT_MIN.

   Output
     [11] 3 < 9  = 1
     [11] 9 < 3  = 0
     [11] abs(42)  = 42
     [11] abs(-42) = 42
     [11] abs(INT_MIN) = -2147483648

   Gotchas
     - setcc reads flags; any instruction between CMP and SETcc that
       writes flags breaks it. Keep them adjacent or in one asm block.
     - Using the wrong condition code (l vs b) is the classic signedness
       bug.
     - `movzbl` after `setl` is required; writing only AL leaves garbage in
       the rest of EAX, and returning that int is undefined-looking.
     - The abs mask works for 32/64-bit integers, not for floats.
   ============================================== */
