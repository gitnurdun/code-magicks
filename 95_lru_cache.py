"""MAGIC: a hash map plus a doubly linked list gives O(1) lookup and O(1)
recency updates; evict the least recently used tail."""
from collections import OrderedDict

class LRU:
    def __init__(self, capacity):
        self.capacity = capacity
        self.data = OrderedDict()

    def get(self, key):
        if key not in self.data:
            return None
        self.data.move_to_end(key)   # mark as most recently used
        return self.data[key]

    def put(self, key, value):
        if key in self.data:
            self.data.move_to_end(key)
        elif len(self.data) >= self.capacity:
            self.data.popitem(last=False)  # evict least recently used
        self.data[key] = value

cache = LRU(2)
for key in ["a", "b", "a", "c"]:
    if cache.get(key) is None:
        cache.put(key, key.upper())
        print("miss", key)
    else:
        print("hit ", key)
print("keys", list(cache.data))

# ================= EXPLANATION =================
# What it does
#   Implements a least-recently-used cache with capacity 2 using
#   OrderedDict. A hit moves the key to the end (most recent); a miss
#   inserts it, evicting the front (least recent) when full.
#
# Step by step
#   1. get("a") misses because the cache is empty. put inserts a; order is
#      [a].
#   2. get("b") misses. put inserts b; order is [a, b], still within
#      capacity.
#   3. get("a") hits. move_to_end moves a to the back; order is [b, a].
#      This recency update is the whole point of LRU.
#   4. get("c") misses. The cache is full, so popitem(last=False) removes
#      b, the least recently used entry. c is inserted, leaving [a, c].
#   5. The final keys are a and c. b was evicted even though it was used
#      more recently than nothing - it simply lost to a and c.
#
# Output
#   "miss a"
#   "miss b"
#   "hit  a"
#   "miss c"
#   "keys ['a', 'c']"
#
# Why it matters
#   LRU is the default eviction policy for page caches, CPU caches, DNS
#   resolvers, and many application caches. OrderedDict combines a hash
#   table with a linked list so lookup, insert, delete, and recency update
#   are all O(1). Real systems often approximate LRU (CLOCK, segmented
#   LRU) to avoid locking and pointer overhead on every access.
#
# Caveats
#   - This cache is not thread-safe. Concurrent get/put need a lock or a
#     sharded/atomic design.
#   - Capacity 2 is tiny; the policy's behaviour is easier to see, but
#     real hit ratios depend on the workload.
#   - LRU can be scanned and thrashed: a one-pass stream of unique keys
#     evicts everything useful. LRU-K, LFU, ARC, and TinyLFU address this.
#   - Storing large values in memory is different from caching disk pages;
#     eviction may need writeback and dirty tracking.
#   - A cache miss here is `None`, so the cache cannot store None as a
#     legitimate value. Use a sentinel or a contains check.
#   - Python's OrderedDict is implemented in C but still has per-entry
#     object overhead; a specialized cache may use arrays or a compact
#     hash map.
# ==============================================
