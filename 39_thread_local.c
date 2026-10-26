/* MAGIC: `_Thread_local` gives every thread its own copy of a variable,
   as if the variable lived in the thread's stack frame. */
#include <stdio.h>
#include <pthread.h>
static _Thread_local int tls = 7;
static void *worker(void *arg)
{
    (void)arg;
    tls = 11;
    printf("worker tls=%d\n", tls);
    return NULL;
}
int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);
    pthread_join(t, NULL);
    printf("main tls=%d\n", tls);
}

/* ================= EXPLANATION =================
   What it does
     Declares one variable name with thread-local storage duration. Each
     thread sees and modifies its own instance; the main thread's copy is
     unaffected by the worker's assignment.

   Step by step
     1. `_Thread_local int tls = 7` gives the variable a per-thread
        instance. The initializer runs once per thread (conceptually at
        thread start), so every thread begins with 7.
     2. main creates a worker thread. The worker writes 11 into its own
        copy and prints it. This is not a race: the two threads never
        touch the same int.
     3. pthread_join waits for the worker, so its output is complete
        before main prints.
     4. main reads tls in its own thread and still sees 7. The worker's
        copy is destroyed when that thread exits.

   Output
     "worker tls=11"
     "main tls=7"

   Why it matters
     TLS is how `errno`, per-thread allocator caches, random seeds, and
     profiling state are implemented without passing a context pointer
     through every call. The compiler emits accesses relative to a thread
     pointer (fs/gs on x86-64), so the cost is close to an ordinary global
     load. It is the storage-duration counterpart of a thread argument.

   Caveats
     - `_Thread_local` needs C11 (`thread_local` with <threads.h> is not
       available everywhere; GCC accepts __thread too).
     - The initializer must be a constant expression for static TLS.
     - Dynamic libraries and dlopen can allocate TLS lazily; access may
       go through __tls_get_addr and can fail if the module is unloaded.
     - Do not store pointers to another thread's TLS in shared memory;
       the storage dies with its thread.
     - This demo uses POSIX threads; on Windows use TLS or
       thread_local with the MSVC runtime.
   ============================================== */
