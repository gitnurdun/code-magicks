"""MAGIC: __slots__ replaces a per-instance dictionary with fixed descriptors."""
class Point:
    __slots__ = ('x', 'y')
    def __init__(self, x, y): self.x, self.y = x, y
p = Point(2, 3); print(p.x + p.y)

# ================= EXPLANATION =================
# What it does
#   Replaces the per-instance __dict__ with a fixed set of slot
#   descriptors, so each Point stores only its two values.
#
# Step by step
#   1. `__slots__ = ('x', 'y')` tells the class machinery not to create a
#      __dict__ for instances. Instead, the class gets slot descriptors
#      named x and y, each pointing at a fixed offset in the object.
#   2. __init__ assigns self.x and self.y. The slot descriptors store the
#      values in those offsets; no dictionary is allocated.
#   3. p.x and p.y read the slots directly, and p.x + p.y prints 5.
#   4. If a third attribute were assigned (p.z = 1), it would raise
#      AttributeError, because there is no __dict__ and no z slot.
#
# Output
#   "5"
#
# Why it matters
#   A normal object carries a pointer to a dictionary (often 100+ bytes
#   plus the dict entries). For classes instantiated millions of times,
#   __slots__ cuts memory sharply, speeds up attribute access, and makes
#   the layout predictable. It is a common optimisation in parsers,
#   geometry, and data-heavy code.
#
# Caveats
#   - Instances cannot gain new attributes; that breaks duck typing and
#     some frameworks that attach metadata dynamically.
#   - Slots are inherited, but a subclass without __slots__ gets a
#     __dict__ again, negating the saving.
#   - Class-level defaults cannot share a name with a slot, and slots
#     conflict with multiple inheritance from two slotted bases.
#   - Add '__weakref__' to __slots__ if weak references are needed, and
#     implement __getstate__/__setstate__ if pickling matters.
#   - Profile first: for a few thousand objects the extra complexity is
#     rarely worth it.
# ==============================================
