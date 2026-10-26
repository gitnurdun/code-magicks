/* MAGIC: many objects can be freed by resetting one bump pointer.
   WHY: parsers and request handlers have natural phase lifetimes. */
#include <stddef.h>
#include <stdio.h>
struct arena{unsigned char*buf;size_t cap,used;};
static void*alloc_a(struct arena*a,size_t n){n=(n+7)&~7u;if(a->used+n>a->cap)return NULL;void*p=a->buf+a->used;a->used+=n;return p;}
int main(void){unsigned char s[128];struct arena a={s,sizeof s,0};int*x=alloc_a(&a,sizeof*x);*x=5;printf("%d used=%zu\n",*x,a.used);}

/* ================= EXPLANATION =================
   What it does
     A bump-pointer arena: allocation only moves a cursor forward, and the
     whole arena is released at once by resetting that cursor. There is no
     per-object free list and no malloc call per object.

   Step by step
     1. struct arena holds a buffer pointer, its capacity, and how much is
        already used. main() supplies a 128-byte stack buffer, so the demo
        needs no heap at all.
     2. alloc_a(a, n) rounds n up to a multiple of 8 with
        (n + 7) & ~7u, so every returned pointer is 8-byte aligned.
     3. It checks a->used + n > a->cap before handing out memory. On
        overflow it returns NULL; the caller must handle that.
     4. Otherwise p = a->buf + a->used is the current bump pointer, and
        used is advanced by n. The allocation itself is one addition.
     5. main() asks for sizeof(int) == 4 bytes, which rounds to 8, stores
        5, and prints the value and the new used count.

   Output
     "5 used=8"

   Why it matters
     Compilers, parsers, request handlers, and frame allocators have a
     natural phase lifetime: everything allocated while processing one
     input can be thrown away together. An arena turns N frees into one
     pointer reset, gives excellent locality, and makes allocation nearly
     free. Memory is reclaimed when the arena is reset or destroyed.

   Caveats
     - Individual objects cannot be freed, and destructors are not run.
     - 8-byte alignment is not enough for types needing 16 (long double,
       SIMD); align the cursor to alignof(max_align_t) if required.
     - `~7u` is a 32-bit mask; for sizes above 4 GiB use ~(size_t)7.
     - Check a->used + n for overflow, or the bound test itself can wrap.
     - The arena does not own the backing buffer here; lifetime is the
       caller's responsibility.
   ============================================== */
