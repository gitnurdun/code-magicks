/* MAGIC: a sparse array maps entity id to a dense slot; components live
   packed, so iteration is linear and lookup is O(1). */
#include <stdio.h>
#define MAX_ENTITIES 16
struct sparse_set {
    int sparse[MAX_ENTITIES];   /* entity -> dense index, or -1 */
    int dense[MAX_ENTITIES];    /* packed component values */
    int entity[MAX_ENTITIES];   /* dense index -> entity id */
    int count;
};
static void ss_init(struct sparse_set *s)
{
    for (int i = 0; i < MAX_ENTITIES; i++) s->sparse[i] = -1;
    s->count = 0;
}
static void ss_add(struct sparse_set *s, int e, int value)
{
    if (s->sparse[e] != -1) return;
    int i = s->count++;
    s->sparse[e] = i;
    s->dense[i] = value;
    s->entity[i] = e;
}
static void ss_remove(struct sparse_set *s, int e)
{
    int i = s->sparse[e];
    if (i == -1) return;
    int last = --s->count;
    if (i != last) {
        s->dense[i] = s->dense[last];
        s->entity[i] = s->entity[last];
        s->sparse[s->entity[i]] = i;
    }
    s->sparse[e] = -1;
}
int main(void)
{
    struct sparse_set positions;
    ss_init(&positions);
    ss_add(&positions, 3, 30);
    ss_add(&positions, 1, 10);
    ss_add(&positions, 2, 20);
    ss_remove(&positions, 1);
    for (int i = 0; i < positions.count; i++)
        printf("entity %d value %d\n", positions.entity[i], positions.dense[i]);
    printf("get(3)=%d\n", positions.dense[positions.sparse[3]]);
}

/* ================= EXPLANATION =================
   What it does
     Stores one component type (here a position value) for a sparse set of
     entity ids. A sparse array translates an entity id into an index in a
     packed dense array, and the dense array stores the actual components
     contiguously for fast iteration.

   Step by step
     1. ss_init marks every entity as absent with sparse[e] = -1.
     2. ss_add(3, 30) writes sparse[3] = 0, dense[0] = 30, entity[0] = 3.
        Adding 1 and 2 appends them at indices 1 and 2. Lookup is
        dense[sparse[e]]: two array loads, no search.
     3. ss_remove(1) finds index 1. The last element (entity 2, value 20)
        is moved into slot 1, and sparse[2] is updated to 1. Removing by
        swap keeps dense packed without shifting every later element.
     4. The dense arrays now contain entity 3 / value 30 and entity 2 /
        value 20. Iteration is a simple forward loop over count, with no
        holes and no branch per entity.
     5. get(3) follows sparse[3] = 0 to dense[0] = 30.

   Output
     "entity 3 value 30"
     "entity 2 value 20"
     "get(3)=30"

   Why it matters
     This is the data structure behind sparse-set ECS storage: membership
     tests and component access are O(1), while systems iterate only over
     entities that actually have the component. The packed layout is
     cache-friendly and makes adding/removing components cheap. It is also
     used for adjacency sets, particle systems, and any small-universe
     mapping from sparse ids to dense data.

   Caveats
     - Entity ids must be < MAX_ENTITIES; a production ECS uses dynamic
       pages or a hash map for arbitrary ids.
     - Reusing an id without a generation counter lets stale handles refer
       to a new entity. Pack (index, generation) in the handle.
     - Swap-remove changes iteration order; do not rely on insertion
       order.
     - One sparse_set per component type. An archetype-based ECS stores
       whole component groups together for even better locality.
     - `ss_remove` must update sparse[entity[i]] after the move; forgetting
       that corrupts every later lookup.
   ============================================== */
