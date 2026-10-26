/* MAGIC: a reader publishes the address it may touch; the reclaimer frees
   only nodes no published hazard pointer names. */
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
struct node { int value; struct node *next; };
static _Atomic(struct node *) hazard[2];
static struct node *retired[8];
static int nretired;
static int is_protected(struct node *p)
{
    for (int i = 0; i < 2; i++)
        if (atomic_load_explicit(&hazard[i], memory_order_acquire) == p)
            return 1;
    return 0;
}
static void retire(struct node *p) { retired[nretired++] = p; }
static void scan(void)
{
    int keep = 0;
    for (int i = 0; i < nretired; i++) {
        if (is_protected(retired[i])) retired[keep++] = retired[i];
        else free(retired[i]);
    }
    nretired = keep;
}
int main(void)
{
    struct node *a = malloc(sizeof *a), *b = malloc(sizeof *b);
    a->value = 1; b->value = 2;
    for (int i = 0; i < 2; i++) atomic_init(&hazard[i], NULL);
    atomic_store(&hazard[0], a);
    retire(a); retire(b);
    scan();
    printf("kept=%d after protected scan\n", nretired);
    atomic_store(&hazard[0], NULL);
    scan();
    printf("kept=%d after hazard cleared\n", nretired);
}

/* ================= EXPLANATION =================
   What it does
     Demonstrates hazard pointers: before dereferencing a shared node, a
     reader publishes its address in a per-thread hazard slot. A reclaimer
     scans those slots and frees retired nodes only when no slot names
     them.

   Step by step
     1. Two nodes are allocated and two hazard slots are initialised to
        NULL. Slot 0 is made to point at node a: this is the reader's
        promise that a is still in use.
     2. Both nodes are put on the retired list. They are no longer
        reachable from the public structure, but they may still be in a
        reader's local pointer.
     3. scan() checks every retired node against both hazard slots.
        Node a is protected, so it is compacted back into retired[]. Node
        b is not, so it is freed immediately. nretired becomes 1.
     4. The reader clears slot 0, ending its critical section. A second
        scan finds no protection for a, frees it, and leaves the retired
        list empty.
     5. The key ordering is acquire on the hazard load and release on the
        hazard store, so the reclaimer sees a published pointer before it
        can conclude the node is unprotected.

   Output
     "kept=1 after protected scan"
     "kept=0 after hazard cleared"

   Why it matters
     Hazard pointers solve the memory-reclamation problem of lock-free
     structures without stopping all threads. Each reader pays a store;
     each reclaimer scans a bounded number of slots. They are used in
     concurrent hash maps and queues, and are simpler to reason about
     than epoch reclamation when reads are long.

   Caveats
     - A real implementation needs per-thread hazard records, a
       thread-safe retire list, and a scan threshold to amortise cost.
       This single-thread demo only shows the protocol.
     - The reader must re-check that the pointer is still the head after
       publishing it; otherwise the node may already be retired.
     - Hazard slots must be cleared promptly, or reclamation stalls.
     - Protect every pointer derived from the node, and use release/
       acquire ordering; a relaxed hazard store can be reordered after
       the read.
     - Hazard pointers do not solve ABA for lock-free stacks by
       themselves; they ensure a node is not freed while observed.
   ============================================== */
