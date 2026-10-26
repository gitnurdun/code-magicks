"""MAGIC: intern every immutable node by its structure, so equal
subtrees become the same object and can be compared by identity."""
class Node:
    __slots__ = ("op", "left", "right")
    def __init__(self, op, left=None, right=None):
        self.op = op
        self.left = left
        self.right = right

_table = {}
def make(op, left=None, right=None):
    key = (op, id(left), id(right))
    node = _table.get(key)
    if node is not None:
        return node
    node = Node(op, left, right)
    _table[key] = node
    return node

x = make("x")
one = make("1")
two = make("2")
add = make("+", x, one)
e1 = make("*", add, two)
e2 = make("*", make("+", x, one), make("2"))
print("same_expression=", e1 is e2)
print("shared_subtree=", e1.left is e2.left)
print("unique_nodes=", len(_table))

# ================= EXPLANATION =================
# What it does
#   Hash-conses an expression tree. `make` looks up a node by its operator
#   and the identities of its children; if an identical node already
#   exists, it returns the existing object instead of allocating a new one.
#
# Step by step
#   1. make("x"), make("1"), and make("2") each create one leaf. Their
#      keys use id(None) for the missing children, so repeated calls return
#      the same leaf.
#   2. add = make("+", x, one) creates the only addition node for those
#      children. Its key is ("+", id(x), id(one)).
#   3. e1 = make("*", add, two) creates the multiplication node.
#   4. The second expression calls make("+", x, one) again. The key is
#      identical, so it returns the existing add node. make("2") returns
#      the existing two. The outer make("*", ...) then finds the existing
#      multiplication node e1 and returns it.
#   5. Therefore e1 is e2, and e1.left is e2.left. The table holds five
#      unique nodes instead of eight for two separately built trees.
#
# Output
#   "same_expression= True"
#   "shared_subtree= True"
#   "unique_nodes= 5"
#
# Why it matters
#   Hash-consing turns a tree into a DAG with maximal sharing. It makes
#   structural equality an O(1) identity check, saves memory, and lets
#   compilers memoise analyses and rewrites. It is used in SMT solvers,
#   proof assistants, symbolic algebra, functional compilers, and some
#   interpreters.
#
# Caveats
#   - Nodes must be immutable. Mutating a shared node changes every
#     expression that references it.
#   - The key uses id(child), which is only valid while the table keeps
#     the child alive (it does). If nodes are evicted, an id can be reused
#     and the cache becomes unsound.
#   - Hashing by identity is fast but does not deduplicate structurally
#     equal nodes built with different child objects; canonical children
#     are the point of the scheme.
#   - The table grows forever and is not thread-safe. Real systems use
#     weak tables, epochs, or per-thread interning.
#   - A hash collision in a real key must be handled by equality checks,
#     not assumed away.
#   - Sharing is not free: pointer chasing increases and cache locality can
#     suffer compared with a flat representation.
# ==============================================
