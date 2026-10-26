"""MAGIC: remember the method looked up for a type, so the next call at
the same site skips the full attribute search."""
class A:
    def method(self): return 10
class B:
    def method(self): return 20

cache = {}
def dispatch(obj):
    cls = type(obj)
    entry = cache.get(cls)
    if entry is None:
        print("IC miss for", cls.__name__)
        entry = cls.method
        cache[cls] = entry
    return entry(obj)

print(dispatch(A()))
print(dispatch(A()))
print(dispatch(B()))

# ================= EXPLANATION =================
# What it does
#   Implements a tiny inline cache (IC) for method dispatch. The first
#   call with a given receiver type performs the lookup and records the
#   resolved function; later calls with the same type reuse it.
#
# Step by step
#   1. dispatch(A()) computes type(obj) == A. The cache has no entry for
#      A, so it prints "IC miss", looks up A.method, and stores the
#      unbound function in cache[A].
#   2. The second A call finds the cached function and calls it directly
#      with obj as self. It prints 10 without another lookup or miss.
#   3. dispatch(B()) sees a new type, prints "IC miss for B", resolves
#      B.method, and returns 20.
#   4. The dictionary is a global cache here. A real VM attaches the cache
#      entry to the individual bytecode instruction (the call site), not
#      to the function or class.
#
# Output
#   "IC miss for A"
#   "10"
#   "10"
#   "IC miss for B"
#   "20"
#
# Why it matters
#   Dynamic languages spend a large fraction of time on attribute lookup.
#   Inline caches make the common case - the same receiver type at a call
#   site - almost as cheap as a static call. They are the heart of
#   CPython's adaptive interpreter, V8's ICs, and Java's invokevirtual
#   optimisations. A monomorphic site remembers one type; a polymorphic
#   site remembers a few; a megamorphic site falls back to a generic
#   lookup.
#
# Caveats
#   - A real IC must guard the assumption: if the receiver type changes,
#     the cached function is invalid and the lookup must be redone.
#   - This cache never invalidates. Monkey-patching A.method would leave
#     the old function in place; production ICs invalidate on class
#     mutation.
#   - A global dict is not per call site, so unrelated call sites share
#     entries and hide the true hit rate.
#   - Attribute lookup can involve descriptors, __getattr__, inheritance,
#     and metaclasses; caching only type(obj) is a simplification.
#   - Thread safety and cache growth matter in a long-running runtime.
# ==============================================
