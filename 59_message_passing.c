/* MAGIC: a release store publishes a message; an acquire load receives
   it. No mutex, just a one-way happens-before edge. */
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
static int payload;
static atomic_bool ready;
static void *producer(void *arg)
{
    (void)arg;
    payload = 42;
    atomic_store_explicit(&ready, true, memory_order_release);
    return NULL;
}
static void *consumer(void *arg)
{
    (void)arg;
    while (!atomic_load_explicit(&ready, memory_order_acquire)) { }
    printf("consumer saw %d\n", payload);
    return NULL;
}
int main(void)
{
    atomic_init(&ready, false);
    pthread_t p, c;
    pthread_create(&p, NULL, producer, NULL);
    pthread_create(&c, NULL, consumer, NULL);
    pthread_join(p, NULL);
    pthread_join(c, NULL);
}

/* ================= EXPLANATION =================
   What it does
     Passes a non-atomic integer from one thread to another using only an
     atomic boolean as the handoff. The producer writes the value, then
     release-stores true. The consumer acquire-loads true and reads the
     value, which is guaranteed to be 42.

   Step by step
     1. payload = 42 is an ordinary store. On a weakly ordered CPU it may
        sit in a store buffer and become visible later than the flag.
     2. atomic_store_explicit(&ready, true, memory_order_release)
        guarantees that all earlier writes in this thread (including
        payload) are visible before the store to ready becomes visible to
        another thread that observes it.
     3. The consumer spins until an acquire load sees true. Acquire
        prevents later loads (the read of payload) from moving before it,
        and pairs with the release store to establish happens-before.
     4. Therefore, when the consumer leaves the loop, payload == 42. The
        output is deterministic despite the spin.
     5. No mutex is involved: the flag is the entire synchronisation
        object. This is the cheapest useful message-passing primitive.

   Output
     "consumer saw 42"

   Why it matters
     Release/acquire is the foundation of lock-free publication: initialise
     an object, then publish a pointer or flag. It is how read-copy-update
     publishes new versions, how queues hand off buffers, and how
     reference counts safely destroy objects. It also explains why
     `volatile` is not enough: volatile orders compiler accesses but does
     not create inter-thread happens-before.

   Caveats
     - A relaxed store/load would make the flag update visible but give no
       guarantee about payload; the read could see 0 or a torn value.
     - The consumer busy-spins. In a general-purpose scheduler, use a
       condition variable, futex, or event after a short spin.
     - Only one producer and one consumer are shown. Multiple producers
       need a protocol for combining or serialising updates.
     - The payload is read once after the flag; if it can change later,
       it needs its own synchronisation.
     - Memory_order_seq_cst (the default) is stronger and easier to
       reason about, but release/acquire is sufficient here and cheaper
       on some architectures.
   ============================================== */
