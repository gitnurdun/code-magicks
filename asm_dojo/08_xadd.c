/* ASM MAGICK #08 - xadd: fetch-and-add returns the old value.
   Build: gcc -O2 -std=c11 08_xadd.c -o 08_xadd.exe
   MAGIC: the destination gets the sum, the source register gets the old
   value - one locked instruction, two results. */
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>

static uint64_t fetch_add(uint64_t *p, uint64_t v)
{
    /* xadd src, dst: dst = dst + src; src = old dst. The source starts
       as the value to add and is overwritten with the old counter, so a
       single read-write operand carries both directions. */
    uint64_t old = v;
    __asm__ volatile("lock xaddq %0, %1"
                     : "+r"(old), "+m"(*p)
                     :
                     : "cc", "memory");
    return old;
}

int main(void)
{
    uint64_t counter = 0;

    for (int i = 0; i < 3; i++) {
        uint64_t before = fetch_add(&counter, 10);
        printf("[08] add 10: returned %2llu, counter now %2llu\n",
               (unsigned long long)before, (unsigned long long)counter);
    }

    /* The same thing spelled as a C11 atomic, for comparison. */
    uint64_t c2 = 0;
    uint64_t old = atomic_fetch_add_explicit(
        (_Atomic uint64_t *)&c2, 7, memory_order_relaxed);
    printf("[08] atomic_fetch_add(7): returned %llu, counter=%llu\n",
           (unsigned long long)old, (unsigned long long)c2);
    return 0;
}

/* ================= EXPLANATION =================
   The instruction
     XADD dst, src (Intel order) does two things atomically:
         tmp = dst; dst = dst + src; src = tmp
     With LOCK it is a full barrier and a single indivisible
     read-modify-write. Without LOCK it is just a weird add that also
     reports the old value.

   Why a single "+r" operand
     `"+r"(old)` is the source register: it must start with the value to
     add (v, copied into old) and ends with the old counter. One read-write
     operand expresses both directions without a separate matching input.
     That is the cleanest way to describe XADD's dual semantics.

   Where it is used
     - Reference counts: `if (fetch_add(&rc, 1) == 0) ...`
     - Statistics and sharded counters (#52): each shard is a separate
       counter, so the locked operation rarely contends.
     - Ticket locks (#54): `ticket = fetch_add(&next, 1)` gives every
       waiter a unique position.
     - Work-stealing deques and queues often use fetch_add for the tail.

   Cost
     A locked XADD on an uncontended cache line is roughly 15-25 cycles on
     modern x86; contended, it is a coherence storm measured in hundreds.
     The instruction is not the problem - the cache line is.

   Output
     [08] add 10: returned  0, counter now 10
     [08] add 10: returned 10, counter now 20
     [08] add 10: returned 20, counter now 30
     [08] atomic_fetch_add(7): returned 0, counter=7

   Gotchas
     - XADD requires the destination in memory for the atomic form; a
       register-register xadd is not locked and is rarely useful.
     - Overflow wraps modulo 2^64; check before adding if that matters.
     - `memory_order_relaxed` is correct for a pure counter, but not for a
       counter that publishes other data (use release/acquire).
     - The cast to `_Atomic uint64_t *` in the demo is legal only because
       the object is suitably aligned and has no other concurrent access;
       in real code declare it atomic from the start.
   ============================================== */
