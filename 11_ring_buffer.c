/* MAGIC: modulo becomes a mask when capacity is a power of two.
   WHY: unsigned counters wrap naturally; no division is needed. */
#include <stdio.h>
#define CAP 8u
struct ring{unsigned a[CAP],head,tail;};
static int push(struct ring*r,unsigned x){if(r->head-r->tail==CAP)return 0;r->a[r->head&(CAP-1)]=x;r->head++;return 1;}
static int pop(struct ring*r,unsigned*x){if(r->head==r->tail)return 0;*x=r->a[r->tail&(CAP-1)];r->tail++;return 1;}
int main(void){struct ring r={0};unsigned x;push(&r,42);pop(&r,&x);printf("%u\n",x);}

/* ================= EXPLANATION =================
   What it does
     A fixed-capacity circular queue that uses monotonically increasing
     unsigned counters and a bit mask instead of modulo arithmetic.

   Step by step
     1. CAP is 8, a power of two, so index % CAP is just index & (CAP - 1).
        That replaces a division with an AND.
     2. head counts total pushes, tail counts total pops; both only ever
        increase. The number of queued items is head - tail, and the queue
        is full when that difference equals CAP.
     3. push() rejects when full, otherwise stores at
        head & (CAP - 1) and increments head.
     4. pop() rejects when head == tail (empty), reads at
        tail & (CAP - 1), and increments tail.
     5. Because the counters are unsigned, wrapping at 2^32 is defined
        behaviour and the difference remains correct across a wrap. The
        mask keeps the physical index inside the 8-slot array.
     6. main() pushes 42, pops it, and prints it.

   Output
     "42"

   Why it matters
     This is the core of lock-free queues and stream buffers: producers
     and consumers only touch their own counter, and the mask makes the
     index cheap. All CAP slots are usable (unlike the "keep one slot
     empty" variant).

   Caveats
     - As written this is single-threaded. A real SPSC ring needs atomics
       (release on head/tail stores, acquire on loads) and often cached
       copies of the other side's index to avoid contention.
     - head - tail must never exceed CAP; the full check assumes the
       producer is the only writer of head.
     - CAP must stay a power of two; otherwise use a modulo or a
       conditional subtract.
     - The element type here is unsigned; for non-trivial types you must
       handle construction/destruction and copy semantics.
   ============================================== */
