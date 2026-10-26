/* MAGIC: a fixed set of workers sleeps on one condition variable and
   wakes only when the task queue changes. */
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#define WORKERS 3
#define TASKS   4
struct task { int arg; struct task *next; };
struct pool {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    struct task *head, *tail;
    int shutdown;
    int done;
};
static struct pool p;
static atomic_int total;
static void run_task(int arg) { atomic_fetch_add(&total, arg); }
static void submit(int arg)
{
    struct task *t = malloc(sizeof *t);
    t->arg = arg; t->next = NULL;
    pthread_mutex_lock(&p.mu);
    if (p.tail) p.tail->next = t; else p.head = t;
    p.tail = t;
    pthread_cond_signal(&p.cv);
    pthread_mutex_unlock(&p.mu);
}
static void *worker(void *arg)
{
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&p.mu);
        while (!p.head && !p.shutdown) pthread_cond_wait(&p.cv, &p.mu);
        if (!p.head && p.shutdown) { pthread_mutex_unlock(&p.mu); break; }
        struct task *t = p.head;
        p.head = t->next;
        if (!p.head) p.tail = NULL;
        pthread_mutex_unlock(&p.mu);
        run_task(t->arg);
        free(t);
        pthread_mutex_lock(&p.mu);
        p.done++;
        pthread_cond_broadcast(&p.cv);
        pthread_mutex_unlock(&p.mu);
    }
    return NULL;
}
int main(void)
{
    pthread_mutex_init(&p.mu, NULL);
    pthread_cond_init(&p.cv, NULL);
    atomic_init(&total, 0);
    pthread_t t[WORKERS];
    for (int i = 0; i < WORKERS; i++) pthread_create(&t[i], NULL, worker, NULL);
    for (int i = 1; i <= TASKS; i++) submit(i);
    pthread_mutex_lock(&p.mu);
    while (p.done < TASKS) pthread_cond_wait(&p.cv, &p.mu);
    int done = p.done;
    pthread_mutex_unlock(&p.mu);
    pthread_mutex_lock(&p.mu);
    p.shutdown = 1;
    pthread_cond_broadcast(&p.cv);
    pthread_mutex_unlock(&p.mu);
    for (int i = 0; i < WORKERS; i++) pthread_join(t[i], NULL);
    printf("completed=%d sum=%d\n", done, atomic_load(&total));
}

/* ================= EXPLANATION =================
   What it does
     Creates a small thread pool with a shared FIFO task queue. The main
     thread submits four tasks; three workers wake, pop tasks, execute
     them, and go back to sleep when the queue is empty.

   Step by step
     1. A worker locks the pool mutex and waits on cv while the queue is
        empty and shutdown is false. pthread_cond_wait atomically releases
        the mutex while sleeping and reacquires it on wakeup, closing the
        race between checking the queue and going to sleep.
     2. submit() links a new task at the tail under the mutex and signals
        one waiter. The lock protects head, tail, and the linked list;
        signal is allowed outside or inside the lock, but doing it inside
        keeps the predicate change and wakeup together.
     3. A woken worker re-checks the predicate in a while loop (spurious
        wakeups are allowed), removes the head task, unlocks, runs it, then
        locks again to increment done and broadcast.
     4. main waits until done == 4, records the count, then sets shutdown
        and broadcasts. Workers that see an empty queue plus shutdown exit;
        join reclaims their stacks.
     5. The four task arguments are 1, 2, 3, 4 and total is 10. The task
        order is not deterministic because three workers race for the
        queue, but the final count and sum are.

   Output
     "completed=4 sum=10"

   Why it matters
     Creating a thread per request is expensive and can exhaust memory.
     A pool amortises thread creation, bounds concurrency, and gives a
     natural place for backpressure and scheduling policy. This
     producer/consumer shape is the basis of web servers, game job
     systems, and parallel build tools.

   Caveats
     - The queue is unbounded; a fast producer can exhaust memory. Add a
       capacity and make submit block or reject.
     - The shutdown check only runs when a worker wakes; broadcast is
       required, and a worker that is mid-task cannot be cancelled here.
     - The task function must not throw or longjmp across the worker
       loop, and must not access the pool after freeing its task.
     - Condition-variable predicates must always be rechecked in a while
       loop; a single if is a classic lost-wakeup bug.
     - Work stealing would reduce tail latency when tasks have very
       different costs; a single queue serialises task acquisition.
   ============================================== */
