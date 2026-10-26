/* MAGIC: futex_waitv parks one thread on a vector of futexes and
   returns the index of a futex that was woken. */
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <linux/types.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifndef __NR_futex_waitv
#define __NR_futex_waitv 449
#endif
#ifndef FUTEX_32
# ifdef FUTEX2_SIZE_U32
#  define FUTEX_32 FUTEX2_SIZE_U32
# else
#  define FUTEX_32 2
# endif
#endif
#ifndef FUTEX_PRIVATE_FLAG
#define FUTEX_PRIVATE_FLAG 128
#endif
#ifndef FUTEX_WAITV_MAX
struct futex_waitv {
    __u64 val;
    __u64 uaddr;
    __u32 flags;
    __u32 __reserved;
};
#endif

static int futex_a, futex_b;
static pthread_barrier_t bar;

static void wake_all(int *f)
{
    if (syscall(SYS_futex, f, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, INT_MAX,
                NULL, NULL, 0) < 0)
        perror("futex_wake");
}

static void *waiter(void *arg)
{
    int *f = arg;
    struct timespec ts = { .tv_sec = 2 };

    (void)pthread_barrier_wait(&bar);
    for (;;) {
        if (syscall(SYS_futex, f, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0,
                    &ts, NULL, 0) == 0)
            break;
        if (errno != EINTR)
            break;
    }
    return NULL;
}

static void *waker(void *arg)
{
    (void)arg;
    (void)pthread_barrier_wait(&bar);
    usleep(10000);
    wake_all(&futex_b);
    return NULL;
}

int main(void)
{
    pthread_t t0, t1, tw;
    struct futex_waitv waiters[2] = {
        { .val = 0, .uaddr = (uintptr_t)&futex_a,
          .flags = FUTEX_32 | FUTEX_PRIVATE_FLAG },
        { .val = 0, .uaddr = (uintptr_t)&futex_b,
          .flags = FUTEX_32 | FUTEX_PRIVATE_FLAG },
    };
    long ret;
    int err = 0;

    if (pthread_barrier_init(&bar, NULL, 4) != 0) {
        perror("pthread_barrier_init");
        return 1;
    }
    if (pthread_create(&t0, NULL, waiter, &futex_a) != 0 ||
        pthread_create(&t1, NULL, waiter, &futex_b) != 0 ||
        pthread_create(&tw, NULL, waker, NULL) != 0) {
        printf("skipped: cannot create threads\n");
        return 0;
    }
    (void)pthread_barrier_wait(&bar);

    ret = syscall(__NR_futex_waitv, waiters, 2, 0, NULL, 0);
    if (ret < 0) {
        err = errno;
        /* Give the helper threads time to enter FUTEX_WAIT before
           releasing them, then wake both words. */
        usleep(20000);
        wake_all(&futex_a);
        wake_all(&futex_b);
    } else {
        wake_all(&futex_a);
    }

    if (pthread_join(t0, NULL) != 0)
        perror("pthread_join");
    if (pthread_join(t1, NULL) != 0)
        perror("pthread_join");
    if (pthread_join(tw, NULL) != 0)
        perror("pthread_join");
    pthread_barrier_destroy(&bar);

    if (ret < 0) {
        printf("skipped: futex_waitv: %s\n", strerror(err));
        return 0;
    }
    printf("woken_index=%ld\n", ret);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Two helper threads each sleep in the classic FUTEX_WAIT on their
     own int.  The main thread calls futex_waitv() on both words at once
     and is woken when another thread issues FUTEX_WAKE on the second
     word.  The syscall returns 1, the index of the woken entry, and the
     program prints woken_index=1.

   Step by step
     1. Each waiter is a struct futex_waitv: an expected value (0), a
        64-bit user address, a flags word and a reserved word that must
        be zero.  The flags select a 32-bit word (FUTEX_32, value 2) and
        a private futex (FUTEX_PRIVATE_FLAG, 128).
     2. pthread_barrier_t with four participants (two waiters, the
        waker, main) makes all threads arrive at the same point: the
        waiters are about to enter FUTEX_WAIT and main is about to call
        futex_waitv.  The barrier synchronizes the start, not the kernel
        state, so the waker still sleeps 10 ms before waking.
     3. futex_waitv(waiters, 2, 0, NULL, 0) checks both words are still
        0, enqueues main as a waiter on both, and sleeps.  The third
        argument is the syscall-wide flags (must be 0), the fourth is an
        optional timeout (NULL = forever), and the fifth is the clock id
        (ignored without a timeout).
     4. The waker calls FUTEX_WAKE on the second word.  It passes
        INT_MAX rather than 1: the plain FUTEX_WAIT thread and the
        futex_waitv caller are both queued on that word, and the kernel
        does not promise to pick the waitv caller first.  Waking all of
        them guarantees main returns; the count passed to wake is a
        maximum, not a target.
     5. futex_waitv returns 1: an index into the waiters array.  It says
        nothing about which other waiters were woken, so main still has
        to wake the first word to release the other helper thread.
     6. The program wakes futex_a, joins all three threads, destroys the
        barrier and prints the index.  The two-second FUTEX_WAIT timeout
        is only a safety net against a lost wakeup during teardown.

   Output
     woken_index=1

   Why it matters
     Go runtimes, io_uring userspace and games with many event sources
     need to sleep on several addresses without one thread per address.
     futex_waitv gives one syscall entry for up to FUTEX_WAITV_MAX (128)
     words and returns which one fired, replacing a poll/futex loop or a
     chain of eventfds.  It also pairs naturally with FUTEX_WAKE: a
     producer can wake exactly the word whose state changed.

   Caveats
     - Linux 5.16+ only; earlier kernels return ENOSYS for syscall 449
       on x86-64.  The syscall number is architecture-specific - the
       fallback here is the x86-64 value.
     - futex_waitv is a wait, not an atomic transaction: between the
       kernel's value checks and enqueueing the waits, a producer can
       change a word and wake nobody.  Real users must re-check their
       condition after EAGAIN/return, usually in a loop; the word values
       here never change, which is what makes the demo deterministic.
     - The waitv caller is one waiter among the ordinary waiters on each
       word.  A FUTEX_WAKE count of 1 may wake the wrong one, so this
       program wakes all; a per-word wake count of 1 is correct only if
       no other thread is queued on that word.
     - FUTEX_WAIT with a relative timeout is interrupted by signals
       (EINTR); the helper retries.  futex_waitv itself can also return
       EINTR, EAGAIN or ETIMEDOUT and must be handled.
     - FUTEX_PRIVATE_FLAG requires all users to share one virtual
       address space; use the shared (non-private) form across
       processes.
   ============================================== */
