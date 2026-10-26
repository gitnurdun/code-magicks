"""MAGIC: add probabilistic express lanes to a linked list. Search starts
at the top and drops down, giving expected O(log n) without rotations."""
import random

MAX_LEVEL = 4
class Node:
    __slots__ = ("key", "next")
    def __init__(self, key, level):
        self.key = key
        self.next = [None] * level

class SkipList:
    def __init__(self):
        self.head = Node(None, MAX_LEVEL)
        self.level = 1
        self.rng = random.Random(42)

    def _random_level(self):
        level = 1
        while level < MAX_LEVEL and self.rng.random() < 0.5:
            level += 1
        return level

    def insert(self, key):
        update = [None] * MAX_LEVEL
        x = self.head
        for i in reversed(range(self.level)):
            while x.next[i] and x.next[i].key < key:
                x = x.next[i]
            update[i] = x
        level = self._random_level()
        if level > self.level:
            for i in range(self.level, level):
                update[i] = self.head
            self.level = level
        node = Node(key, level)
        for i in range(level):
            node.next[i] = update[i].next[i]
            update[i].next[i] = node

    def contains(self, key):
        x = self.head
        for i in reversed(range(self.level)):
            while x.next[i] and x.next[i].key < key:
                x = x.next[i]
        x = x.next[0]
        return x is not None and x.key == key

    def items(self):
        out, x = [], self.head.next[0]
        while x:
            out.append(x.key)
            x = x.next[0]
        return out

sl = SkipList()
for key in [3, 1, 2, 5, 4]:
    sl.insert(key)
print("sorted=", sl.items())
print("find(4)=", sl.contains(4))
print("find(9)=", sl.contains(9))
print("levels=", sl.level)

# ================= EXPLANATION =================
# What it does
#   Implements a skip list: a sorted linked list with several levels of
#   forward pointers. Each inserted key gets a random height; higher levels
#   skip over many nodes, so search behaves like binary search on a list.
#
# Step by step
#   1. The head node has MAX_LEVEL forward pointers. Level 0 links every
#      key in sorted order; level 1 links a random subset, level 2 a
#      sparser subset, and so on.
#   2. insert starts at the top level and moves right while the next key
#      is smaller, recording the last node visited at each level in
#      `update`. That is the search path.
#   3. _random_level flips a coin up to MAX_LEVEL, so about half the nodes
#      reach level 1, a quarter reach level 2, and so on. The tallest level
#      is stored in self.level.
#   4. The new node is linked into each of its levels between the recorded
#      predecessors. No rotations or rebalancing are needed.
#   5. contains follows the same top-down path and then checks the level-0
#      successor. With keys 1..5, find(4) is true and find(9) is false.
#
# Output
#   "sorted= [1, 2, 3, 4, 5]"
#   "find(4)= True"
#   "find(9)= False"
#   "levels= 4"
#
# Why it matters
#   Skip lists give the expected complexity of a balanced tree with much
#   simpler insertion and deletion. They support ordered iteration and
#   range queries naturally, which is why Redis sorted sets and some
#   memory tables use them. Concurrent variants (lock-free or
#   lock-striped) are easier to build than concurrent balanced trees.
#
# Caveats
#   - The balancing is probabilistic. A pathological random sequence can
#     produce a tall, sparse structure and O(n) search, though the
#     probability is tiny.
#   - This demo seeds the RNG for reproducibility; production code uses a
#     good random source and a higher MAX_LEVEL (often 32).
#   - Duplicate keys are not handled here; decide whether to update,
#     reject, or allow them.
#   - The expected O(log n) has a constant factor; a B-tree or sorted
#     array may be faster for small or read-only data.
#   - Each node allocates an array of forward pointers, which adds memory
#     overhead and allocation cost.
#   - Concurrent skip lists need atomic pointer updates and careful memory
#     reclamation, not just a mutex around these methods.
# ==============================================
