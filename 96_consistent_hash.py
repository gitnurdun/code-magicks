"""MAGIC: place nodes and keys on a hash ring; a key belongs to the first
node clockwise. Adding a node moves only its share of the keys."""
import bisect
import hashlib
from collections import Counter

class Ring:
    def __init__(self):
        self.positions = []
        self.owner = {}

    @staticmethod
    def _hash(text):
        return int.from_bytes(hashlib.md5(text.encode()).digest()[:8], "big")

    def add(self, node, vnodes=500):
        for i in range(vnodes):
            pos = self._hash(f"{node}#{i}")
            self.positions.append(pos)
            self.owner[pos] = node
        self.positions.sort()

    def get(self, key):
        pos = self._hash(key)
        i = bisect.bisect_left(self.positions, pos)
        return self.owner[self.positions[i % len(self.positions)]]

ring = Ring()
for node in ["A", "B", "C"]:
    ring.add(node)
keys = [f"key{i}" for i in range(30)]
before = {k: ring.get(k) for k in keys}
ring.add("D")
after = {k: ring.get(k) for k in keys}
moved = sum(before[k] != after[k] for k in keys)
print("before", dict(sorted(Counter(before.values()).items())))
print("after ", dict(sorted(Counter(after.values()).items())))
print("moved", moved, "of", len(keys))

# ================= EXPLANATION =================
# What it does
#   Builds a consistent-hashing ring with 500 virtual positions per node,
#   assigns 30 keys to nodes, adds a fourth node, and counts how many keys
#   moved.
#
# Step by step
#   1. Each node is hashed at 500 virtual points (node#0 ... node#499). The
#      points are sorted on a 64-bit circle; the owner map records which
#      node owns each point.
#   2. A key is hashed to a position. bisect_left finds the first node
#      point at or after that position; the key belongs to that node. If
#      the key lies past the last point, modulo wraps to the first.
#   3. With A, B, and C, the 30 keys are distributed roughly evenly. The
#      counts depend on the hash and virtual-node layout, but every key
#      has exactly one owner.
#   4. Adding D inserts its 500 points. Only keys whose clockwise successor
#      changed move: they now fall between a D point and the next old
#      node. Keys owned by A, B, or C for the same interval do not move.
#   5. moved is the number of changed assignments. With 4 nodes, the
#      expected fraction is about 1/4; virtual nodes reduce variance.
#
# Output
#   "before {'A': 11, 'B': 9, 'C': 10}"
#   "after  {'A': 9, 'B': 9, 'C': 6, 'D': 6}"
#   "moved 6 of 30"
#
# Why it matters
#   Consistent hashing lets a distributed cache or storage cluster add or
#   remove a node without reshuffling every key. Only the departing or
#   arriving node's share moves, which keeps rebalancing cheap. Virtual
#   nodes smooth the distribution and let heterogeneous machines take
#   proportional load. Dynamo, Cassandra, and many CDNs use this idea
#   (often with rendezvous hashing instead of a ring).
#
# Caveats
#   - The exact moved count depends on the hash function, key names, and
#     virtual-node count. The printed numbers are for this demo, not a
#     guarantee.
#   - Few virtual nodes produce uneven rings and hotspots. More vnodes
#     improve balance but increase memory and lookup cost.
#   - MD5 is used here as a fast, deterministic hash, not for security.
#     Production systems use a well-distributed hash and may salt it.
#   - Node weights (capacity) require proportional vnode counts or a
#     bounded-load variant.
#   - Adding a node still requires migrating the keys it now owns. The ring
#     minimises movement; it does not perform the transfer.
#   - Replication needs the next R distinct nodes clockwise, and failure
#     handling must skip dead nodes.
#   - Rendezvous (highest-random-weight) hashing avoids storing a ring and
#     gives similar minimal disruption; compare before choosing.
# ==============================================

