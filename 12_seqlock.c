/* MAGIC: readers retry if a writer changed a version counter.
   WHY: tiny snapshots can have wait-free-ish reads without a mutex. */
#include <stdatomic.h>
#include <stdio.h>
struct point{atomic_uint seq;int x,y;};
static void write_p(struct point*p,int x,int y){atomic_fetch_add(&p->seq,1);p->x=x;p->y=y;atomic_fetch_add(&p->seq,1);}
int main(void){struct point p={0};write_p(&p,3,4);for(;;){unsigned a=atomic_load(&p.seq);if(a&1)continue;int x=p.x,y=p.y;if(a==atomic_load(&p.seq)){printf("%d,%d\n",x,y);break;}}}

/* ================= EXPLANATION =================
   What it does
     A seqlock: readers take an optimistic snapshot and retry if a writer
     was active or if the version changed during the read. Readers never
     block the writer and never take a lock.

   Step by step
     1. seq is an atomic counter. A writer increments it to an odd value,
        meaning "write in progress", stores the fields, then increments
        again to an even value, meaning "stable".
     2. write_p() performs exactly that: odd, write x and y, even.
     3. The reader loads seq. If it is odd, a write is in progress, so it
        retries (the demo spins).
     4. It copies x and y into locals.
     5. It reloads seq. If the value matches the earlier even value, no
        writer ran in between, so the snapshot is consistent; print it and
        stop. Otherwise retry.
     6. In this single-threaded demo there is no concurrent writer, so the
        first pass succeeds and prints "3,4".

   Why it matters
     For small, frequently read data (timestamps, statistics, clock
     snapshots) a seqlock gives readers a wait-free fast path with no
     cache-line ownership ping-pong, while writers stay cheap. The Linux
     kernel uses this for timekeeping and counters.

   Caveats
     - This version only synchronises seq. x and y are plain ints read
       while a writer may be storing them, which is a C11 data race (UB)
       unless they are _Atomic with relaxed accesses or the accesses are
       fenced/volatile in the kernel style.
     - Real implementations need atomic_thread_fence or acquire/release
       ordering to stop the compiler and CPU from reordering the field
       reads across the seq loads.
     - Readers can starve under a heavy writer stream, and the writer must
       not block inside the critical section.
     - The reader must not cause side effects while validating.
   ============================================== */
