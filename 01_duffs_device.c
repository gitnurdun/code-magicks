/* MAGIC: a switch can be woven into a loop to unroll a byte copy.
   WHY: the loop body is entered at an arbitrary byte offset. */
#include <stdio.h>
#include <stddef.h>
static void duff_copy(char *to, const char *from, size_t count)
{
    size_t n = (count + 7) / 8;
    if (count == 0) return;
    switch (count % 8) {
    case 0: do { *to++ = *from++; /* fall through */
    case 7:      *to++ = *from++; /* fall through */
    case 6:      *to++ = *from++; /* fall through */
    case 5:      *to++ = *from++; /* fall through */
    case 4:      *to++ = *from++; /* fall through */
    case 3:      *to++ = *from++; /* fall through */
    case 2:      *to++ = *from++; /* fall through */
    case 1:      *to++ = *from++;
            } while (--n > 0);
    }
}
int main(void)
{
    const char src[] = "Duff's device works!";
    char dst[sizeof src] = {0};
    duff_copy(dst, src, sizeof src - 1);
    printf("%s\n", dst);
}

/* ================= EXPLANATION =================
   What it does
     Copies an arbitrary number of bytes eight at a time using Duff's
     device: a switch whose cases are interleaved with the body of a
     do/while loop. It is the canonical demonstration that a switch is
     just a computed jump into code, not a separate "block" construct.

   Step by step
     1. n = (count + 7) / 8 is the number of eight-byte rounds needed.
        For count = 20, n = 3, and count % 8 = 4.
     2. The switch jumps directly to case 4. Execution copies four bytes,
        falls through case 3, 2, 1, and reaches the while condition.
     3. `--n > 0` decrements n to 2 and loops. The loop re-enters at
        case 0 (the top of the body), copying eight bytes, then n becomes
        1 and it copies the final eight bytes. Total: 4 + 8 + 8 = 20.
     4. When n reaches 0 the loop exits. The destination is printed; it is
        NUL-terminated because dst was zero-initialised.

   Output
     "Duff's device works!"

   Why it matters
     It is a compact loop-unrolling idiom from an era when branch
     overhead mattered, and it is a perfect illustration of fallthrough
     and label placement. The same idea - jumping into the middle of a
     loop - appears in coroutine implementations, bytecode interpreters,
     and hand-written parsers. It also explains why switch cases are
     labels, not scopes.

   Caveats
     - The guard for count == 0 is essential: otherwise case 0 is entered
       with n == 0 and `--n` wraps to SIZE_MAX.
     - It assumes `to` and `from` do not overlap; use memmove for overlap.
     - Modern compilers already unroll small loops, and this form can be
       harder to read or vectorise. Use memcpy unless you are studying the
       trick.
     - The number of copies is fixed at eight per round; a wider unroll
       needs a matching prologue.
   ============================================== */
