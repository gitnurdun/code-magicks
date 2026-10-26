/* MAGIC: group boxes in a tree; a query rejects a whole subtree with one
   box test, so it visits only the leaves that can contain the point. */
#include <stdio.h>
#include <stdlib.h>
#define N 4
typedef struct { float minx, miny, maxx, maxy; } box;
typedef struct {
    box b;
    int left, right;
    int start, count;
} node;
static box boxes[N];
static node nodes[2 * N];
static int used;
static float center_x(const box *b) { return (b->minx + b->maxx) * 0.5f; }
static int by_x(const void *pa, const void *pb)
{
    float a = center_x((const box *)pa), b = center_x((const box *)pb);
    return (a > b) - (a < b);
}
static box merge(box a, box b)
{
    box r;
    r.minx = a.minx < b.minx ? a.minx : b.minx;
    r.miny = a.miny < b.miny ? a.miny : b.miny;
    r.maxx = a.maxx > b.maxx ? a.maxx : b.maxx;
    r.maxy = a.maxy > b.maxy ? a.maxy : b.maxy;
    return r;
}
static int build(int start, int count)
{
    int idx = used++;
    nodes[idx].start = start;
    nodes[idx].count = count;
    nodes[idx].left = nodes[idx].right = -1;
    nodes[idx].b = boxes[start];
    for (int i = 1; i < count; i++) nodes[idx].b = merge(nodes[idx].b, boxes[start + i]);
    if (count <= 2) return idx;
    qsort(boxes + start, count, sizeof(box), by_x);
    int half = count / 2;
    nodes[idx].left = build(start, half);
    nodes[idx].right = build(start + half, count - half);
    return idx;
}
static int contains(box b, float x, float y)
{
    return x >= b.minx && x <= b.maxx && y >= b.miny && y <= b.maxy;
}
static int query(int idx, float x, float y)
{
    node *n = &nodes[idx];
    if (!contains(n->b, x, y)) return 0;
    if (n->left < 0) {
        int hits = 0;
        for (int i = 0; i < n->count; i++)
            if (contains(boxes[n->start + i], x, y)) hits++;
        return hits;
    }
    return query(n->left, x, y) + query(n->right, x, y);
}
int main(void)
{
    box input[N] = {
        {0, 0, 2, 2}, {1, 1, 3, 3}, {5, 5, 6, 6}, {1.5f, 1.5f, 1.8f, 1.8f}
    };
    for (int i = 0; i < N; i++) boxes[i] = input[i];
    int root = build(0, N);
    printf("nodes=%d point hits=%d\n", used, query(root, 1.6f, 1.6f));
}

/* ================= EXPLANATION =================
   What it does
     Builds a bounding volume hierarchy (BVH) over four 2D boxes and
     answers a point query. Each internal node stores the union of its
     children's boxes; if the query point is outside that union, the whole
     subtree is skipped.

   Step by step
     1. build computes the bounding box of the current range, stores it,
        and marks the node as a leaf if the range has at most two boxes.
     2. Otherwise it sorts the range by box center on x and splits it in
        half. The two recursive calls build the left and right subtrees.
        For four boxes the root splits into two leaves of two boxes each,
        so used == 3: one internal node and two leaves.
     3. query starts at the root. If (1.6,1.6) is outside the root's
        union, it returns immediately. Otherwise it descends into children
        whose boxes contain the point.
     4. At a leaf, it tests the actual boxes. (1.6,1.6) is inside [0,0,2,2],
        [1,1,3,3], and [1.5,1.5,1.8,1.8], but outside [5,5,6,6], so the
        answer is 3.
     5. The tree does not change the answer; it changes how many boxes are
        examined. A query that misses the root costs one box test instead
        of four.

   Output
     "nodes=7 point hits=3"

   Why it matters
     BVHs are the workhorse acceleration structure for ray tracing,
     collision detection, and frustum culling. A well-built tree turns a
     linear scan into O(log n) expected traversal. Games build a BVH for
     static geometry and refit or rebuild it for dynamic objects; renderers
     use SAH (surface area heuristic) to choose better splits.

   Caveats
     - Splitting by median x is simple but not optimal. SAH minimises the
       expected traversal cost and produces much better trees for rays.
     - This is a point query; a ray query needs slab intersection and an
       ordered traversal (near child first) to find the closest hit.
     - qsort reorders the boxes in place; the leaf's start/count indexes
       depend on that order and must remain valid if boxes are edited.
     - Recursion depth is O(log n) only for balanced splits; degenerate
       distributions can produce deep trees. Build iteratively or cap
       depth for untrusted data.
     - Boxes are axis-aligned and not padded. A tiny epsilon expansion
       avoids missing hits due to floating-point roundoff.
     - Dynamic objects require refitting ancestor boxes or a different
       structure (BVH with refit, spatial hash, or loose octree).
   ============================================== */
