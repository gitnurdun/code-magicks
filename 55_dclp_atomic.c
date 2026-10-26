/* MAGIC: double-checked locking initialises once, without locking the
   common already-initialised path. The atomic pointer is the safety net. */
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
struct config { int value; };
static _Atomic(struct config *) global_cfg;
static pthread_mutex_t init_mu = PTHREAD_MUTEX_INITIALIZER;
static struct config *get_config(void)
{
    struct config *p = atomic_load_explicit(&global_cfg, memory_order_acquire);
    if (p) return p;
    pthread_mutex_lock(&init_mu);
    p = atomic_load_explicit(&global_cfg, memory_order_relaxed);
    if (!p) {
        p = malloc(sizeof *p);
        p->value = 42;
        atomic_store_explicit(&global_cfg, p, memory_order_release);
    }
    pthread_mutex_unlock(&init_mu);
    return p;
}
int main(void)
{
    struct config *a = get_config();
    struct config *b = get_config();
    printf("first=%d second=%d same=%d\n", a->value, b->value, a == b);
}

/* ================= EXPLANATION =================
   What it does
     Lazily creates a singleton configuration object. The fast path reads
     an atomic pointer without taking a lock; if it is NULL, the slow path
     locks, checks again, and publishes a fully initialised object.

   Step by step
     1. The first call loads global_cfg with acquire and sees NULL.
     2. It takes init_mu and loads again with relaxed ordering. The second
        check matters: another thread may have initialised the object
        while this thread waited for the mutex. If so, it just unlocks and
        returns the existing object.
     3. Otherwise it allocates, writes value = 42, and stores the pointer
        with release. Release ensures that the write to value is visible
        to any thread that observes the pointer with acquire.
     4. The second call takes the fast path: the acquire load sees the
        published pointer and returns it without locking.
     5. Both calls return the same address, so the output is 42, 42, 1.

   Output
     "first=42 second=42 same=1"

   Why it matters
     Double-checked locking is a classic attempt to avoid a lock on a hot
     read path. It is correct only when the pointer is atomic (or
     otherwise synchronised) and publication uses release/acquire. The
     naive version without atomics is famously broken: a compiler or CPU
     can make the pointer visible before the object's fields are written,
     so another thread sees non-NULL but uninitialised memory. This is one
     of the best examples of why the C/C++ memory model exists.

   Caveats
     - The object is never freed here; process-lifetime singletons often
       accept that. If it must be destroyed, add reference counting and a
       shutdown protocol.
     - The initialisation is thread-safe, but mutation after publication
       is not. Freeze the object or protect later writes.
     - In C++11 and later, a function-local static is guaranteed to be
       initialised exactly once, even under concurrency; prefer it to
       hand-written DCLP.
     - Do not use `volatile` as a substitute for atomics; it does not
       provide inter-thread ordering in the C memory model.
     - malloc failure is not handled here; a production version must
       propagate an error or abort.
   ============================================== */
