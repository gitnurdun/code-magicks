/* MAGIC: layout separates counters so cores do not false-share a cache line.
   CAVEAT: line size is target-dependent; measure in production. */
#include <stdatomic.h>
#include <stdio.h>
struct counter{_Alignas(64) atomic_long value;char pad[64-sizeof(atomic_long)];};
int main(void){struct counter c={0};atomic_fetch_add(&c.value,1);printf("%ld\n",c.value);}

/* ================= EXPLANATION =================
   What it does
     Forces each counter onto its own cache line by aligning the member to
     64 bytes and padding the struct out to a full line, so two counters
     never share a line.

   Step by step
     1. _Alignas(64) on atomic_long value raises the alignment requirement
        of the struct to 64, and places value at a 64-byte boundary.
     2. char pad[64 - sizeof(atomic_long)] fills the rest of the line. On
        a typical LP64 target atomic_long is 8 bytes, so pad is 56 bytes
        and the struct is exactly 64 bytes (alignment 64 also rounds the
        size up).
     3. Because each object starts on a distinct 64-byte boundary, an
        array of counter has one counter per cache line. Two threads
        updating different elements do not invalidate each other's line.
     4. main() atomically increments the counter and prints 1.

   Output
     "1"

   Why it matters
     False sharing: two independent variables happen to sit on the same
     cache line, so cores writing them bounce the line between caches even
     though the variables are logically unrelated. Padding removes that
     ping-pong, which can be a large win in per-core statistics.

   Caveats
     - 64 bytes is a common x86 line size; Arm and some CPUs use 128 or
       64+prefetch. Measure, or use
       std::hardware_destructive_interference_size in C++17.
     - Padding costs memory and cache footprint; only separate counters
       that are actually written by different threads.
     - The `64 - sizeof(...)` expression breaks if the type ever grows
       beyond 64 bytes.
     - Atomicity is separate from padding: plain long would still need
       atomics for concurrent updates.
   ============================================== */
