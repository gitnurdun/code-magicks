"""MAGIC: reachability, not reference counts, decides what is garbage.
Mark everything reachable from the roots, then sweep the rest."""
class Obj:
    def __init__(self, name):
        self.name = name
        self.refs = []

def mark(obj, seen):
    if id(obj) in seen:
        return
    seen.add(id(obj))
    for child in obj.refs:
        mark(child, seen)

def sweep(heap, seen):
    return [obj for obj in heap if id(obj) not in seen]

a, b = Obj("a"), Obj("b")
c, d = Obj("c"), Obj("d")
a.refs = [b]
b.refs = [a]       # a cycle
c.refs = [d]
d.refs = [c]       # another cycle, but unreachable
heap = [a, b, c, d]
roots = [a]
marked = set()
mark(roots[0], marked)
freed = sweep(heap, marked)
print("marked:", " ".join(o.name for o in heap if id(o) in marked))
print("freed: ", " ".join(o.name for o in freed))

# ================= EXPLANATION =================
# What it does
#   Simulates a tracing mark-and-sweep garbage collector on a small object
#   graph. It finds every object reachable from the roots, then frees the
#   objects that were not reached.
#
# Step by step
#   1. Four objects form two cycles: a <-> b and c <-> d. Reference
#      counting alone would never free either cycle, because each object
#      has a nonzero count.
#   2. The only root is a. mark(a) adds a to the seen set, then recurses
#      into b; b recurses into a, which is already seen, so the cycle
#      terminates.
#   3. c and d are not reachable from a. Their mutual references do not
#      matter; reachability is what defines liveness.
#   4. sweep scans the heap and returns objects whose id is not in seen:
#      c and d. A real collector would return them to a free list or move
#      the survivors.
#   5. The output lists a, b as marked and c, d as freed. A reference-
#      counting collector would leak both cycles without a cycle detector.
#
# Output
#   "marked: a b"
#   "freed:  c d"
#
# Why it matters
#   Mark-and-sweep collects cycles and needs no per-object count updates
#   on every pointer write. It is the conceptual basis of tracing GCs in
#   Java, Go, JavaScript, and many language runtimes. Real collectors add
#   generations, concurrency, compaction, and write barriers to avoid
#   stopping the world.
#
# Caveats
#   - This recursive mark can overflow Python's stack on a deep graph;
#     production collectors use an explicit worklist and often a tri-color
#     invariant.
#   - Stop-the-world marking pauses the program. Incremental/concurrent
#     collectors interleave marking with mutation and need barriers.
#   - Sweep here does not reclaim memory in any real sense; Python still
#     owns the objects. It only demonstrates the algorithm.
#   - Non-moving sweep leaves fragmentation. A copying or compacting
#     collector moves survivors and updates pointers.
#   - Finalizers and weak references complicate liveness; an object can
#     be reachable only through a finalizer queue or temporarily
#     resurrected.
#   - Roots must include stacks, registers, globals, and thread-local
#     storage; missing a root frees live data, which is a security bug.
# ==============================================
