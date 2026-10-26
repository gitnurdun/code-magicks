"""MAGIC: __getattr__ runs only after ordinary lookup fails."""
class Env:
    def __init__(self): self.host = 'localhost'
    def __getattr__(self, name):
        if name.startswith('is_'): return False
        raise AttributeError(name)
e = Env(); print(e.host, e.is_secure)

# ================= EXPLANATION =================
# What it does
#   Uses __getattr__ as a last-resort fallback: normal attribute lookup
#   runs first, and only when it fails does Python call __getattr__, which
#   synthesises a value for names starting with "is_".
#
# Step by step
#   1. Env.__init__ sets self.host = "localhost" in the instance __dict__.
#   2. e.host is found by ordinary lookup, so __getattr__ is never called;
#      it prints "localhost".
#   3. e.is_secure does not exist in the instance or the class. After
#      normal lookup fails, Python calls __getattr__(e, "is_secure").
#   4. The method sees the "is_" prefix and returns False, a made-up
#      answer. For any other missing name it raises AttributeError(name),
#      preserving normal failure semantics (and making hasattr work).
#   5. print prints both values.
#
# Output
#   "localhost False"
#
# Why it matters
#   __getattr__ lets an object answer attributes that are computed,
#   proxied, or optional. It is the basis of dynamic proxies, lazy
#   imports, configuration objects, and APIs that turn arbitrary names
#   into queries. Raising AttributeError for genuinely unknown names keeps
#   the object honest.
#
# Caveats
#   - __getattr__ is only the fallback. To intercept every access use
#     __getattribute__, which is far more invasive and easy to break.
#   - Do not raise anything other than AttributeError for "not found";
#     hasattr, getattr with a default, and duck typing rely on it.
#   - Accessing a missing attribute inside __getattr__ can recurse
#     infinitely; use object.__getattribute__ or a guard.
#   - AttributeError can hide real bugs (typos) by silently returning a
#     default; keep the synthesised namespace narrow.
#   - Class-level access (Env.is_secure) does not go through the instance
#     __getattr__ path in the same way; use a metaclass if needed.
# ==============================================
