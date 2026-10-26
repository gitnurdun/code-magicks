"""MAGIC: an attribute can be executable behavior rather than stored data."""
class Positive:
    def __set_name__(self, owner, name): self.name = name
    def __get__(self, obj, owner=None): return self if obj is None else obj.__dict__[self.name]
    def __set__(self, obj, value):
        if value <= 0: raise ValueError(self.name)
        obj.__dict__[self.name] = value
class User: age = Positive()
u = User(); u.age = 3; print(u.age)

# ================= EXPLANATION =================
# What it does
#   Implements an attribute as a small object that controls what happens
#   on read and write. `Positive` is a data descriptor: it validates on
#   assignment and stores the value per instance.
#
# Step by step
#   1. `age = Positive()` puts a descriptor object in the class body.
#   2. When the class is created, Python calls __set_name__ on that
#      descriptor, passing the owner class and the attribute name ("age"),
#      so the descriptor knows where it lives.
#   3. `u.age = 3` finds a data descriptor on the class, so it calls
#      __set__(u, 3) instead of writing to u.__dict__ directly. The check
#      `value <= 0` rejects bad values; 3 passes, and the value is stored
#      in the instance dictionary under the same name.
#   4. `u.age` also finds the descriptor first. __get__ returns
#      obj.__dict__["age"], i.e. 3, which is printed.
#   5. Access on the class (User.age) calls __get__ with obj=None; the
#      descriptor returns itself so introspection still works.
#
# Output
#   "3"
#
# Why it matters
#   This is how `property`, `classmethod`, `staticmethod`, slots, and ORM
#   fields are built: attribute access is a protocol, not a fixed memory
#   fetch. Validation, lazy loading, caching, and unit conversion can all
#   live behind an ordinary-looking attribute.
#
# Caveats
#   - Reading an unset attribute raises KeyError from __dict__ lookup;
#     real descriptors usually raise AttributeError or keep a default.
#   - The name is stored on the descriptor instance; sharing one descriptor
#     object between two classes would let the second __set_name__ win.
#   - Data descriptors take precedence over the instance __dict__, which is
#     why storing under the same name works.
#   - Descriptors only work on classes, not on instances, and a descriptor
#     on the class cannot be shadowed by an instance attribute.
# ==============================================
