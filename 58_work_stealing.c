/* MAGIC: the owner works at the bottom (LIFO), thieves take from the top
   (FIFO). Hot local work stays local; idle cores steal old work. */
#include <stdatomic.h>
#include <stdio.h>
#define CAP 8
#define EMPTY (-1)
struct deque {
    _Atomic int top, bottom;
    int buf[CAP];
};
static void deque_init(struct deque *d)
{
    atomic_init(&d->top, 0);
    atomic_init(&d->bottom, 0);
}
static void push(struct deque *d, int v)
{
    int b = atomic_load(&d->bottom);
    d->buf[b & (CAP - 1)] = v;
    atomic_store(&d->bottom, b + 1);
}
static int pop(struct deque *d)
{
    int b = atomic_load(&d->bottom) - 1;
    atomic_store(&d->bottom, b);
    int t = atomic_load(&d->top);
    if (b > t) {
        return d->buf[b & (CAP - 1)];
    }
    if (b == t) {
        int v = d->buf[b & (CAP - 1)];
        if (!atomic_compare_exchange_strong(&d->top, &t, t + 1)) {
            atomic_store(&d->bottom, t + 1);
            return EMPTY;
        }
        atomic_store(&d->bottom, t + 1);
        return v;
    }
    atomic_store(&d->bottom, t);
    return EMPTY;
}
static int steal(struct deque *d)
{
    int t = atomic_load(&d->top);
    int b = atomic_load(&d->bottom);
    if (t < b) {
        int v = d->buf[t & (CAP - 1)];
        if (atomic_compare_exchange_strong(&d->top, &t, t + 1)) return v;
    }
    return EMPTY;
}
int main(void)
{
    struct deque d;
    deque_init(&d);
    push(&d, 1); push(&d, 2); push(&d, 3);
    int first = pop(&d);
    int stolen = steal(&d);
    int second = pop(&d);
    printf("pop=%d steal=%d pop=%d\n", first, stolen, second);
    printf("next=%s\n", pop(&d) == EMPTY ? "empty" : "present");
}

/* ================= EXPLANATION =================
   What it does
     Implements the work-stealing deque shape used by Cilk, Java's
     ForkJoinPool, and many game job systems. The owner pushes and pops at
     the bottom (LIFO), so recently created work stays hot in cache.
     Idle thieves steal from the top (FIFO), so they take older work and
     leave the owner's local stack alone.

   Step by step
     1. push writes at bottom and increments bottom. Only the owner does
        this on the fast path, so no CAS is needed for the store itself.
     2. pop moves bottom down by one. If there is more than one item
        (b > top), the owner simply takes the bottom item. If it is the
        last item (b == top), the owner and a thief may race; a CAS on top
        decides who gets it.
     3. steal reads top and bottom. If top < bottom, it copies the top
        item and CASes top upward. If the CAS fails, another thief won;
        this thief retries or gives up.
     4. Starting with 1, 2, 3, the owner pops 3. A thief steals the oldest
        item, 1. The owner then pops 2. The deque is empty.
     5. The CAS on the last item is the subtle part: without it, owner and
        thief could both return the same element.

   Output
     "pop=3 steal=1 pop=2"
     "next=empty"

   Why it matters
     Work stealing gives both locality and load balance. A thread's own
     LIFO stack is cheap and cache-friendly; when it runs dry, it steals
     from the FIFO end of another thread's deque, which is the least
     recently used work and therefore least likely to conflict. This is
     the scheduling backbone of parallel runtimes and frame-based game
     engines.

   Caveats
     - This is a fixed-capacity teaching version. Chase-Lev's real
       algorithm grows and shrinks the array with careful memory ordering;
       resizing while thieves read is the hard part.
     - `top` and `bottom` must be atomic and the last-item CAS must be
       correct; a plain load/store version can duplicate or lose items.
     - `EMPTY` is -1, so the demo cannot store -1. Use a separate status
       out-parameter in real code.
     - False sharing between top and bottom is possible; pad them on
       separate cache lines when throughput matters.
     - The owner may starve thieves if it always keeps its deque non-empty;
       stealing is opportunistic, not fair scheduling.
   ============================================== */
