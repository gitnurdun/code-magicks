/* MAGIC: header and variable payload share one allocation.
   WHY: fewer allocations and contiguous cache-friendly data. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct packet{size_t len;unsigned char data[];};
int main(void){size_t n=5;struct packet*p=malloc(sizeof*p+n);p->len=n;memcpy(p->data,"hello",n);printf("%.*s\n",(int)p->len,p->data);free(p);}

/* ================= EXPLANATION =================
   What it does
     Allocates a header and a variable-length payload in one block using a
     C99 flexible array member (FAM). sizeof(struct packet) covers only
     len; the data bytes trail the struct inside the same malloc block.

   Step by step
     1. struct packet ends with `unsigned char data[];`, a member with no
        stated size. It must be last and does not contribute to sizeof.
     2. malloc(sizeof *p + n) reserves room for the header (on a 64-bit
        target, 8 bytes for the size_t len) plus n payload bytes. The
        resulting pointer is aligned for the struct, and data starts right
        after len.
     3. p->len = n remembers the length, because nothing else delimits the
        payload; there is no NUL terminator here.
     4. memcpy copies 5 bytes of "hello" into the trailing space.
     5. printf("%.*s", (int)p->len, p->data) prints exactly len bytes, so
        it does not rely on a terminator. A NULL check on malloc would be
        required in real code.
     6. One free(p) releases header and payload together.

   Output
     "hello"

   Why it matters
     One allocation instead of two means less allocator overhead, one
     pointer to pass around, and header+payload contiguous in memory,
     which is friendlier to the cache and to scatter/gather I/O. This is
     the classic shape for network packets, strings, and arrays of
     variable-length records.

   Caveats
     - sizeof(struct packet) excludes the payload; never copy such a
       struct by value or put it in an array.
     - size arithmetic can overflow: check n <= SIZE_MAX - sizeof *p.
     - Portable C99/C11 only; MSVC historically used data[1]. C++ does not
       have FAMs, so use a trailing array of size 1 or std::byte storage.
     - The FAM must be the last member and cannot be initialised directly.
   ============================================== */
