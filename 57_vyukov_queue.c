/* MAGIC: a bounded MPMC queue with one sequence number per slot. The
   sequence encodes both the slot's generation and whether it is full. */
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#define CAP 8
struct cell { _Atomic size_t seq; int data; };
struct queue {
    struct cell buf[CAP];
    _Atomic size_t head, tail;
};
static void queue_init(struct queue *q)
{
    for (size_t i = 0; i < CAP; i++) atomic_init(&q->buf[i].seq, i);
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
}
static int enqueue(struct queue *q, int v)
{
    size_t pos = atomic_load_explicit(&q->tail, memory_order_relaxed);
    for (;;) {
        struct cell *c = &q->buf[pos & (CAP - 1)];
        size_t seq = atomic_load_explicit(&c->seq, memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)pos;
        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->tail, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                c->data = v;
                atomic_store_explicit(&c->seq, pos + 1, memory_order_release);
                return 1;
            }
        } else if (dif < 0) {
            return 0;
        } else {
            pos = atomic_load_explicit(&q->tail, memory_order_relaxed);
        }
    }
}
static int dequeue(struct queue *q, int *out)
{
    size_t pos = atomic_load_explicit(&q->head, memory_order_relaxed);
    for (;;) {
        struct cell *c = &q->buf[pos & (CAP - 1)];
        size_t seq = atomic_load_explicit(&c->seq, memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->head, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                *out = c->data;
                atomic_store_explicit(&c->seq, pos + CAP, memory_order_release);
                return 1;
            }
        } else if (dif < 0) {
            return 0;
        } else {
            pos = atomic_load_explicit(&q->head, memory_order_relaxed);
        }
    }
}
int main(void)
{
    struct queue q;
    queue_init(&q);
    enqueue(&q, 10); enqueue(&q, 20); enqueue(&q, 30);
    int v;
    while (dequeue(&q, &v)) printf("%d%c", v, v == 30 ? '\n' : ' ');
    puts(dequeue(&q, &v) ? "unexpected" : "empty");
}

/* ================= EXPLANATION =================
   What it does
     Implements the bounded multi-producer/multi-consumer ring queue
     popularised by Dmitry Vyukov. Producers CAS the tail, consumers CAS
     the head, and each slot carries a sequence number that says which
     generation of the ring it currently holds.

   Step by step
     1. Initialisation gives slot i the sequence number i. Both head and
        tail start at 0. A slot is available to a producer at position p
        when its seq == p; it contains a ready item for a consumer at p
        when seq == p + 1.
     2. enqueue reads tail = p, examines slot p % CAP. If seq == p, the
        slot is free; the producer CASes tail from p to p + 1 to reserve
        it. Only after winning the CAS does it write data and set seq to
        p + 1 with release.
     3. dequeue reads head = p. If seq == p + 1, an item is ready; CAS
        head to p + 1 to reserve it, copy data out, then set seq to p + CAP
        with release. That marks the slot free for the producer one full
        lap later.
     4. If a producer sees seq < p, the queue is full (the consumer has
        not advanced that slot). If a consumer sees seq < p + 1, the queue
        is empty. A larger seq means another thread moved the index, so
        reload and retry.
     5. The demo enqueues 10, 20, 30 and dequeues 10, 20, 30 in FIFO
        order; the fourth dequeue reports empty.

   Output
     "10 20 30"
     "empty"

   Why it matters
     This is a genuinely concurrent bounded queue: multiple producers and
     consumers make progress without a mutex, and sequence numbers replace
     separate full/empty flags. It is used in high-performance messaging,
     audio, networking, and trading systems. The per-slot sequence also
     solves the classic ring-buffer ambiguity between "empty" and "full"
     without wasting a slot.

   Caveats
     - The queue is fixed-size and does not grow; full producers must
       retry, block, or drop.
     - `data` is a plain int written after the CAS and read after the
       acquire on seq; the release/acquire pair publishes it. Changing the
       ordering to relaxed breaks the algorithm.
     - Adjacent cells share cache lines, so high contention causes false
       sharing; pad cells for maximum throughput.
     - The CAS reservations can starve an individual thread even though
       the queue as a whole makes progress.
     - Do not copy this with a different capacity unless CAP is a power
       of two; the modulo is a mask.
   ============================================== */
