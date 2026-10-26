/* MAGIC: a lock-free stack is one atomic head pointer and a CAS loop.
   WHY: push/pop need no lock, only retry when another thread wins. */
#include <stdatomic.h>
#include <stdio.h>
struct node { int value; struct node *next; };
static _Atomic(struct node *) top;
static void push(struct node *n)
{
    struct node *old = atomic_load(&top);
    do { n->next = old; }
    while (!atomic_compare_exchange_weak(&top, &old, n));
}
static struct node *pop(void)
{
    struct node *old = atomic_load(&top);
    while (old) {
        struct node *next = old->next;
        if (atomic_compare_exchange_weak(&top, &old, next)) return old;
    }
    return NULL;
}
int main(void)
{
    struct node a = {1, NULL}, b = {2, NULL}, c = {3, NULL};
    atomic_init(&top, NULL);
    push(&a); push(&b); push(&c);
    struct node *n;
    while ((n = pop()) != NULL) printf("%d%c", n->value, n->next ? ' ' : '\n');
}

/* ================= EXPLANATION =================
   What it does
     Implements a LIFO stack shared by any number of threads without a
     mutex. The only shared state is an atomic pointer to the head node;
     every modification is a compare-and-swap that retries if another
     thread changed the head first.

   Step by step
     1. push(n) loads the current head into old, points n->next at it,
        and attempts `compare_exchange_weak(&top, &old, n)`. The operation
        succeeds only if top still equals old; otherwise it stores the new
        head in old and the loop retries with the fresh value.
     2. Pushing 1, 2, 3 builds 3 -> 2 -> 1 -> NULL. The stack owns no
        wrapper nodes: each node is embedded in the caller's object.
     3. pop() loads the head, remembers head->next, and CASes the head to
        next. If another thread popped first, the CAS fails, old is
        refreshed, and the loop tries again.
     4. The loop prints 3, 2, 1. A weak CAS may fail spuriously, which is
        why it lives in a loop; a strong CAS would still need the loop for
        contention.
     5. All operations are seq_cst here for simplicity; release/acquire
        would suffice for publication.

   Output
     "3 2 1"

   Why it matters
     The Treiber stack is the "hello world" of lock-free data structures.
     It shows the essential pattern: read a shared pointer, build a new
     state privately, publish it with CAS, retry on conflict. The same
     pattern underlies lock-free queues, reference counts, and many kernel
     fast paths. Progress is guaranteed system-wide: some thread always
     completes, even if a particular thread starves.

   Caveats
     - The ABA problem: if a node is popped, freed, and a new node reuses
       the same address, a delayed CAS can succeed against the wrong
       state. Hazard pointers, epochs, or tagged pointers are needed for
       safe reclamation.
     - This demo uses stack nodes, so no node is freed while another
       thread might still read it. A real lock-free stack must solve
       memory reclamation, not just the pointer update.
     - Reading old->next before the CAS is unsafe if another thread can
       free old; a production implementation pins the node first.
     - Contention causes CAS retries; a spinlock can outperform this under
       heavy load, and NUMA systems need more care.
     - `compare_exchange_weak` may fail spuriously; never call it once
       outside a retry loop.
   ============================================== */
