"""MAGIC: importing is a protocol with pluggable finders and loaders."""
import sys, importlib.abc, importlib.util
class Loader(importlib.abc.Loader):
    def create_module(self, spec): return None
    def exec_module(self, module): module.answer = 42
class Finder(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path, target=None):
        return importlib.util.spec_from_loader(fullname, Loader()) if fullname == 'magic_module' else None
sys.meta_path.insert(0, Finder())
import magic_module
print(magic_module.answer)

# ================= EXPLANATION =================
# What it does
#   Installs a custom finder at the front of sys.meta_path that resolves a
#   module named "magic_module" out of thin air, with a loader that
#   populates it at import time.
#
# Step by step
#   1. Loader implements the importlib.abc.Loader protocol. create_module
#      returns None, meaning "let the default machinery create a plain
#      ModuleType"; exec_module receives that module and sets
#      module.answer = 42.
#   2. Finder implements MetaPathFinder. find_spec is called with the
#      dotted module name. It returns a ModuleSpec built by
#      spec_from_loader only when fullname == "magic_module"; for every
#      other name it returns None, telling Python to try the next finder.
#   3. sys.meta_path.insert(0, Finder()) puts this finder before the
#      standard PathFinder, so it gets first refusal on every import.
#   4. `import magic_module` first consults sys.meta_path. Our finder
#      returns a spec, the loader's create_module/exec_module run, and the
#      module is cached in sys.modules under "magic_module".
#   5. print(magic_module.answer) reads the attribute the loader attached.
#
# Output
#   "42"
#
# Why it matters
#   Import is a protocol: finders locate a spec, loaders create and
#   execute a module. This is how frozen modules, zip imports, namespace
#   packages, vendored dependencies, plugins, and test doubles work.
#   Returning None is the key contract that lets finders cooperate.
#
# Caveats
#   - Meta path finders run for every import in the process; a slow or
#     buggy one slows everything down.
#   - Returning a spec for a name you do not own can shadow real modules
#     and break imports unpredictably. Prefer a distinctive prefix.
#   - Once imported, the module is cached in sys.modules; later imports
#     accept the cached object. Use importlib.reload or remove the cache
#     entry when experimenting.
#   - This toy loader ignores packaging, submodules, and __path__, and it
#     never invalidates caches; production hooks need more of the protocol.
#   - sys.meta_path is global and process-wide; clean up test hooks.
# ==============================================
