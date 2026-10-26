/* MAGIC: RCU readers never lock; a writer removes first, waits for
   pre-existing readers to drain (a grace period), then frees. */
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
struct node { int value; };
static _Atomic(struct node *) head;
static atomic_int readers;
static atomic_bool reader_has_ptr;
static void *reader(void *arg)
{
    (void)arg;
    atomic_fetch_add(&readers, 1);
    struct node *p = atomic_load(&head);
    atomic_store(&reader_has_ptr, true);
    printf("reader saw %d\n", p->value);
    atomic_fetch_sub(&readers, 1);
    return NULL;
}
int main(void)
{
    struct node *old = malloc(sizeof *old);
    old->value = 1;
    atomic_init(&head, old);
    atomic_init(&readers, 0);
    atomic_init(&reader_has_ptr, false);
    pthread_t t;
    pthread_create(&t, NULL, reader, NULL);
    while (!atomic_load(&reader_has_ptr)) { }
    struct node *fresh = malloc(sizeof *fresh);
    fresh->value = 2;
    atomic_exchange(&head, fresh);
    while (atomic_load(&readers) != 0) { }
    printf("grace period over\n");
    free(old);
    pthread_join(t, NULL);
    printf("head now %d\n", atomic_load(&head)->value);
}

/* ================= EXPLANATION =================
   What it does
     Shows the core of read-copy-update (RCU): a reader enters a read-side
     critical section without acquiring a lock, takes a pointer to the
     current object, and uses it. A writer publishes a new object, waits
     until all readers that could have seen the old one have left, and
     only then frees the old object.

   Step by step
     1. The initial head points to node 1. A reader thread increments the
        global reader count, loads head, and sets reader_has_ptr. The
        increment-before-load order is what lets the writer know the
        reader may hold the old pointer.
     2. The writer (main) waits for reader_has_ptr, so the reader has
        definitely loaded the old head. It allocates node 2 and publishes
        it with atomic_exchange. New readers now see 2.
     3. The reader prints the old node's value while still counted. The
        writer spins until readers drops to 0. That interval - from the
        publication of the new pointer until every pre-existing reader
        has left - is the grace period.
     4. Only after the grace period does the writer free old. No reader
        can still dereference it, so the free is safe even though readers
        were never blocked or locked.
     5. The writer joins the reader and prints the new head, 2.

   Output
     "reader saw 1"
     "grace period over"
     "head now 2"

   Why it matters
     RCU is one of the most important kernel synchronisation techniques.
     Read-mostly data (routing tables, mount tables, configuration) can be
     traversed with zero atomic operations in the read path: the reader
     just loads a pointer. Writers pay the cost of copying, publishing,
     and waiting. It trades memory and writer latency for extremely cheap
     reads.

   Caveats
     - This is a teaching sketch, not a full RCU implementation. Real RCU
       handles preemption, CPU hotplug, nesting, and quiescent states,
       and uses per-CPU counters or expedited grace periods.
     - The global reader counter is a contention point and assumes
       readers are short; a sleeping reader can stall the writer forever.
     - A reader must not use a pointer after it exits the read-side
       critical section. In preemptible kernels, disabling preemption
       around the read is essential.
     - Publish with release and read with acquire (or stronger), or the
       new object's fields may not be visible before the pointer.
     - RCU protects against freeing, not against concurrent modification
       of the object; writers must copy/update immutably.
   ============================================== */
