"""MAGIC: a weak reference observes an object without owning it, and
reference counting alone cannot free a cycle - the tracing GC must."""
import gc
import weakref

class Node:
    def __init__(self, name):
        self.name = name
        self.peer = None

a = Node("a")
b = Node("b")
a.peer = b
b.peer = a          # a reference cycle
watch = weakref.ref(a)
print("alive before gc:", watch() is not None)
del a, b            # the cycle still holds both objects
collected = gc.collect()
print("alive after gc: ", watch() is not None)
print("cycle collected:", collected > 0)

# ================= EXPLANATION =================
# What it does
#   Builds a two-object reference cycle, drops the external names, and
#   uses a weak reference to observe whether the objects were reclaimed.
#   Python's reference counting leaves the cycle alive until its cyclic
#   garbage collector runs.
#
# Step by step
#   1. a.peer = b and b.peer = a give each object one strong reference
#      from the other. The local names a and b add two more.
#   2. weakref.ref(a) creates a reference that does not increment a's
#      count. It can be called later to get a strong reference, or None
#      if the object is gone.
#   3. del a, b removes the local names. Each object still has one strong
#      reference from its peer, so their counts are 1, not 0. Reference
#      counting cannot free them; this is the classic cycle leak.
#   4. gc.collect() runs the tracing collector: it finds the cycle is not
#      reachable from any root, breaks it, and frees both objects. The
#      weak reference is cleared, so watch() returns None.
#   5. collected is nonzero, confirming that at least the cycle was
#      reclaimed.
#
# Output
#   "alive before gc: True"
#   "alive after gc:  False"
#   "cycle collected: True"
#
# Why it matters
#   Reference counting gives immediate, predictable reclamation but cannot
#   handle cycles. Tracing GC finds unreachable cycles but needs roots,
#   barriers, and pauses. Many runtimes combine both. Weak references are
#   essential for caches, observer lists, and parent/child links that must
#   not keep an object alive.
#
# Caveats
#   - Do not rely on CPython's refcount timing in portable code; other
#     implementations may collect differently.
#   - `gc.collect()` is a full collection and can be expensive; production
#     code rarely calls it in a hot path.
#   - Objects with __del__ can complicate collection; modern Python handles
#     most cases, but finalizer resurrection is still subtle.
#   - A weak reference is not a cache by itself: the object can disappear
#     between the `watch()` check and the next use. Keep a strong local
#     reference for the duration of the operation.
#   - Weak dictionaries (WeakKeyDictionary/WeakValueDictionary) are the
#     usual tools for caches and registries.
# ==============================================
