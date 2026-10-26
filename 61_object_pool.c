/* MAGIC: a free list threaded through the free objects themselves turns
   allocate and release into two pointer operations. */
#include <stdio.h>
#define POOL_SIZE 4
struct object { int id; struct object *next; };
static struct object pool[POOL_SIZE];
static struct object *free_list;
static int free_count;
static void pool_init(void)
{
    for (int i = 0; i < POOL_SIZE; i++) {
        pool[i].id = 0;
        pool[i].next = (i + 1 < POOL_SIZE) ? &pool[i + 1] : NULL;
    }
    free_list = &pool[0];
    free_count = POOL_SIZE;
}
static struct object *pool_alloc(void)
{
    struct object *p = free_list;
    if (!p) return NULL;
    free_list = p->next;
    p->next = NULL;
    free_count--;
    return p;
}
static void pool_release(struct object *p)
{
    p->next = free_list;
    free_list = p;
    free_count++;
}
int main(void)
{
    pool_init();
    struct object *a = pool_alloc(); a->id = 1;
    struct object *b = pool_alloc(); b->id = 2;
    pool_release(a);
    struct object *c = pool_alloc(); c->id = 3;
    printf("a=%td b=%td reuse=%td id=%d free=%d\n",
           a - pool, b - pool, c - pool, c->id, free_count);
}

/* ================= EXPLANATION =================
   What it does
     Preallocates a fixed array of objects and links the unused slots into
     a singly linked free list. Allocation pops the head; release pushes
     the object back. No malloc or free is called per object.

   Step by step
     1. pool_init links slot 0 -> 1 -> 2 -> 3 -> NULL and points free_list
        at slot 0. Every free slot's `next` pointer is stored inside the
        object itself: while free, the object is its own list node.
     2. pool_alloc returns slot 0, advances free_list to slot 1, and clears
        the returned object's next. It decrements free_count to 3. A
        second allocation returns slot 1 and leaves 2 free.
     3. pool_release(a) pushes slot 0 back at the head, so it is the next
        object allocated. This is LIFO reuse: the most recently freed
        object is the hottest in cache.
     4. The third allocation returns slot 0 again (reuse = 0) and id 3 is
        written into it. Slot 1 remains allocated. free_count is 2: slots
        2 and 3 are still free.
     5. `a - pool` and friends are pointer differences, printed with %td,
        to show which pool slot each pointer refers to.

   Output
     "a=0 b=1 reuse=0 id=3 free=2"

   Why it matters
     Object pools give predictable allocation cost, stable addresses, and
     excellent locality. Games use them for bullets, particles, entities,
     and network messages; kernels use slab-like caches for frequently
     allocated fixed-size objects. Because the free list lives inside the
     objects, there is no separate metadata allocation and no fragmentation
     from variable-size blocks.

   Caveats
     - The pool is fixed-size; pool_alloc returns NULL when exhausted.
       Decide whether to grow, block, or fail.
     - Released objects are not destructed automatically. If they own
       resources, call a cleanup function before pool_release.
     - Use-after-release corrupts the free list; debug builds can poison
       the object or add a magic value.
     - The pool is not thread-safe. A per-thread pool or a lock-free free
       list is needed for concurrent use.
     - Object addresses are stable, but the `next` field is only valid
       while the object is free; do not read it from live objects.
   ============================================== */
