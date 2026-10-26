"""MAGIC: concatenate strings as a binary tree of leaves, so joining two
texts is O(1) and the bytes are materialised only when needed."""
class Node:
    __slots__ = ("text", "left", "right", "length")
    def __init__(self, text=None, left=None, right=None):
        self.text = text
        self.left = left
        self.right = right
        self.length = (len(text) if text is not None
                       else left.length + right.length)

def leaf(text):
    return Node(text=text)

def concat(a, b):
    if a is None: return b
    if b is None: return a
    return Node(left=a, right=b)

def char_at(node, index):
    if node.text is not None:
        return node.text[index]
    if index < node.left.length:
        return char_at(node.left, index)
    return char_at(node.right, index - node.left.length)

def flatten(node):
    if node.text is not None:
        return node.text
    return flatten(node.left) + flatten(node.right)

rope = concat(concat(leaf("Hello, "), leaf("world")), leaf("!"))
print("text=" + flatten(rope))
print("char[7]=" + char_at(rope, 7))
print("length=" + str(rope.length))

# ================= EXPLANATION =================
# What it does
#   Represents a string as a rope: a binary tree whose leaves hold short
#   strings and whose internal nodes represent concatenation. Building the
#   tree is cheap; characters are found by walking down the tree.
#
# Step by step
#   1. leaf("Hello, ") and leaf("world") are two nodes. concat makes a
#      parent whose length is 7 + 5 = 12 and whose children are the
#      leaves. No characters are copied.
#   2. concat(..., leaf("!")) adds a root with length 13. The tree is
#      ((Hello, ) + world) + !.
#   3. char_at(rope, 7) sees that 7 < left.length (12), so it searches the
#      left subtree. That node is itself a concat; 7 >= 7, so it moves to
#      the right leaf with index 0 and returns "w".
#   4. flatten performs an in-order traversal, concatenating leaf text.
#      This is the O(n) materialisation step that the rope deferred.
#   5. The rope reports length 13 without scanning its leaves, because
#      every node stores its total length.
#
# Output
#   "text=Hello, world!"
#   "char[7]=w"
#   "length=13"
#
# Why it matters
#   Repeated string concatenation in a flat buffer copies the entire
#   prefix every time, which is O(n^2) for n appends. A rope makes the
#   append O(1) and supports cheap substring, insert, and delete by
#   splitting and rebalancing the tree. Text editors, document models, and
#   functional languages use ropes or similar persistent sequences.
#
# Caveats
#   - This tree is unbalanced. A long chain of concatenations can become a
#     linked list with O(n) char_at. Production ropes rebalance (AVL or
#     weight-balanced trees).
#   - The leaves here are Python strings; very small leaves add pointer
#     overhead, very large leaves lose the benefit.
#   - flatten creates a new flat string and is O(total length); cache it
#     if the rope is read many times.
#   - Ropes are usually immutable/persistent. This demo's nodes are not
#     protected from mutation.
#   - Indexing a rope is slower than indexing a flat string; choose based
#     on the workload.
# ==============================================
