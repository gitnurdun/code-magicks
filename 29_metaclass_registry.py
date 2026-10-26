"""MAGIC: defining a class can run registration code immediately."""
REGISTRY = {}
class Registered(type):
    def __new__(mcls, name, bases, ns):
        cls = super().__new__(mcls, name, bases, ns)
        if name != 'Plugin': REGISTRY[name] = cls
        return cls
class Plugin(metaclass=Registered): pass
class Json(Plugin): pass
class Text(Plugin): pass
print(sorted(REGISTRY))

# ================= EXPLANATION =================
# What it does
#   Uses a metaclass to run registration code at class-definition time:
#   every plugin subclass is added to REGISTRY as soon as its class body
#   is executed, with no decorator and no explicit register() call.
#
# Step by step
#   1. `class Registered(type)` subclasses the metaclass `type`. Its
#      __new__ runs whenever a class using it is created.
#   2. __new__ calls super().__new__(mcls, name, bases, ns) to build the
#      class normally, then records `REGISTRY[name] = cls` - unless the
#      class is the base Plugin itself.
#   3. `class Plugin(metaclass=Registered)` creates Plugin and skips it
#      because of the name check.
#   4. Json and Text inherit from Plugin, so they inherit its metaclass
#      too. Defining each one calls Registered.__new__ and registers it.
#   5. sorted(REGISTRY) returns the registered names in alphabetical order,
#      which is printed.
#
# Output
#   "['Json', 'Text']"
#
# Why it matters
#   Plugins, serializers, command handlers, and schema classes often need
#   a global name -> class map. Building it in the metaclass guarantees
#   that every subclass is registered at import time, including ones
#   defined later in other modules. There is no way to define a subclass
#   and forget to register it.
#
# Caveats
#   - Registration is a global side effect at import time; import order
#     decides the dictionary contents, and test isolation can suffer.
#   - The base class is excluded by a name string, which is fragile;
#     compare `cls.__bases__` or use an explicit marker attribute instead.
#   - Name collisions overwrite silently.
#   - Metaclasses compose poorly: combining this with ABCMeta or another
#     custom metaclass requires a joint metaclass.
#   - For simple cases, __init_subclass__ is the lighter, more modern hook.
# ==============================================
