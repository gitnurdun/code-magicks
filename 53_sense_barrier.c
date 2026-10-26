/* MAGIC: a reusable barrier flips a generation bit, so threads from the
   previous round cannot mistake the next round for their release. */
#include <stdio.h>
#include <pthread.h>
#define THREADS 4
struct barrier {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    unsigned count;
    unsigned sense;
};
static void barrier_init(struct barrier *b)
{
    pthread_mutex_init(&b->mu, NULL);
    pthread_cond_init(&b->cv, NULL);
    b->count = 0;
    b->sense = 0;
}
static void barrier_wait(struct barrier *b)
{
    pthread_mutex_lock(&b->mu);
    unsigned mine = b->sense;
    if (++b->count == THREADS) {
        b->count = 0;
        b->sense = !b->sense;
        pthread_cond_broadcast(&b->cv);
    } else {
        while (mine == b->sense) pthread_cond_wait(&b->cv, &b->mu);
    }
    pthread_mutex_unlock(&b->mu);
}
static struct barrier bar;
static void *worker(void *arg)
{
    (void)arg;
    barrier_wait(&bar);
    barrier_wait(&bar);
    return NULL;
}
int main(void)
{
    barrier_init(&bar);
    pthread_t t[THREADS - 1];
    for (int i = 0; i < THREADS - 1; i++) pthread_create(&t[i], NULL, worker, NULL);
    barrier_wait(&bar);
    barrier_wait(&bar);
    for (int i = 0; i < THREADS - 1; i++) pthread_join(t[i], NULL);
    puts("4 threads crossed 2 barriers");
}

/* ================= EXPLANATION =================
   What it does
     Implements a reusable sense-reversing barrier: every thread calls
     barrier_wait, and none proceeds until all four have arrived. The
     barrier can then be used again immediately, for any number of rounds,
     without a separate "reset" step.

   Step by step
     1. count tracks how many threads have arrived in the current round.
        sense is the current generation, initially 0.
     2. A thread locks the mutex and saves the current sense in `mine`.
        If it is the last to arrive (count reaches 4), it resets count to
        0, flips sense to 1, and broadcasts to wake the others.
     3. A non-last thread waits while `mine == b->sense`. When the last
        thread flips sense, the predicate becomes false and the waiter
        leaves the loop. The while loop handles spurious wakeups.
     4. In the next round, a fast thread re-enters and saves the new sense
        (1). Slow threads from the previous round still have mine == 0, so
        they cannot mistake the new round's state for their release.
        That is the point of reversing the sense instead of just resetting
        a flag.
     5. main is one of the four participants; three workers plus main
        cross two barriers, then main prints after joining.

   Output
     "4 threads crossed 2 barriers"

   Why it matters
     Barriers are how parallel simulations, iterative numerical kernels,
     and phased game systems keep every worker on the same timestep. A
     reusable barrier avoids allocating or reinitialising synchronisation
     state every frame. Sense reversal is the standard fix for the
     generation race that a naive counter-and-flag barrier has.

   Caveats
     - One mutex and one condition variable make this a centralised
       barrier; large machines use tree or dissemination barriers to
       reduce contention.
     - All participants must call barrier_wait the same number of times,
       or the barrier deadlocks.
     - pthread_barrier_t is the portable POSIX version; Windows has
       SYNCHRONIZATION_BARRIER.
     - Cancellation or a thread crash while waiting can strand the rest;
       production code needs timeout or failure handling.
     - The count and sense are protected by the mutex; reading them
       outside it would be a data race.
   ============================================== */
