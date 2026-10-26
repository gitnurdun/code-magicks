/* MAGIC: a ticket lock serves waiters in arrival order using two counters:
   take a ticket, then wait until serving reaches it. */
#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#define THREADS 4
#define ITERS   100000
static atomic_uint next_ticket, serving;
static unsigned long counter;
static void lock(void)
{
    unsigned ticket = atomic_fetch_add(&next_ticket, 1);
    while (atomic_load(&serving) != ticket) { }
}
static void unlock(void) { atomic_fetch_add(&serving, 1); }
static void *work(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        lock();
        counter++;
        unlock();
    }
    return NULL;
}
int main(void)
{
    pthread_t t[THREADS];
    atomic_init(&next_ticket, 0);
    atomic_init(&serving, 0);
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, work, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    printf("counter=%lu\n", counter);
}

/* ================= EXPLANATION =================
   What it does
     Implements a FIFO spinlock with two atomic counters. Each contender
     atomically takes the next ticket; the holder advances `serving` when
     it unlocks. A thread spins only while the current serving number is
     not its ticket.

   Step by step
     1. next_ticket starts at 0. Thread A calls lock first, gets ticket 0,
        and sees serving == 0, so it enters the critical section. Thread B
        gets ticket 1 and spins until serving becomes 1.
     2. unlock does atomic_fetch_add(&serving, 1), handing the lock to
        exactly the holder of the next ticket. Thread B now sees serving
        == 1, leaves the spin, and enters.
     3. Four threads each execute ITERS = 100000 protected increments.
        The lock guarantees mutual exclusion, so no increment is lost even
        though counter is a plain unsigned long.
     4. After joining, counter is exactly 400000. The FIFO order does not
        change the total, only the order of access.
     5. The ticket and serving counters wrap modulo 2^32; as long as no
        more than 2^32 threads are simultaneously waiting, unsigned wrap
        preserves the comparison.

   Output
     "counter=400000"

   Why it matters
     A test-and-set spinlock is unfair: a released lock can be grabbed by
     a newly arriving thread while an older waiter keeps spinning. A
     ticket lock guarantees first-come, first-served, which bounds
     starvation and is easy to reason about. It is used in hypervisors,
     real-time systems, and simple kernel locks. It also shows the cost
     of a shared cache line: every unlock writes `serving`, and every
     waiter reads it.

   Caveats
     - All waiters spin on the same cache line, so the lock does not
       scale to many cores. A queued lock (MCS) gives each waiter its own
       node and scales better.
     - The holder can be preempted while spinning waiters burn CPU; use
       a hybrid lock that sleeps after a bounded spin in a general-purpose
       OS.
     - No priority inheritance: a low-priority holder can block a
       high-priority waiter (priority inversion).
     - `serving` and `next_ticket` must be atomic; plain unsigned
       increments would be data races.
     - 32-bit ticket wrap is correct for unsigned arithmetic, but a
       64-bit counter avoids theoretical overflow on very long-lived
       systems.
   ============================================== */
