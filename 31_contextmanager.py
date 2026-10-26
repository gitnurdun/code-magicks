"""MAGIC: a generator plus a decorator becomes structured resource cleanup."""
from contextlib import contextmanager
@contextmanager
def transaction():
    print('begin')
    try: yield
    except Exception: print('rollback'); raise
    else: print('commit')
with transaction(): print('work')

# ================= EXPLANATION =================
# What it does
#   Turns a generator into a context manager: code before `yield` is the
#   setup, code after it is the teardown, and the with-statement supplies
#   the try/except/finally structure.
#
# Step by step
#   1. @contextmanager wraps `transaction` in _GeneratorContextManager.
#      Calling it creates the generator object but does not run it.
#   2. Entering `with` calls __enter__, which calls next(gen). The body
#      runs until `yield`: it prints "begin", then suspends. The value
#      yielded here is None, so the with-statement has no `as` value.
#   3. The body of the with-block runs and prints "work".
#   4. Leaving the block calls __exit__. With no exception, __exit__ calls
#      next(gen) again to resume after the yield. The `else` branch runs
#      and prints "commit". The generator then returns, raising
#      StopIteration, which the wrapper treats as normal completion.
#   5. If the block had raised, __exit__ would throw the exception into
#      the generator at the yield point. The `except` branch prints
#      "rollback", re-raises, and the exception propagates to the caller.
#
# Output
#   "begin"
#   "work"
#   "commit"
#
# Why it matters
#   Setup/teardown is the most common resource pattern: open/close,
#   lock/unlock, begin/commit/rollback, start timer/stop timer. Writing it
#   once as a generator keeps the paired actions next to each other and
#   guarantees teardown on exception, early return, or break.
#
# Caveats
#   - The generator must yield exactly once. Zero yields or two yields
#     cause a RuntimeError at runtime.
#   - Exceptions are re-raised unless __exit__ suppresses them; do not
#     swallow errors silently.
#   - A context manager instance is single-use and not thread-safe or
#     reentrant; create a new one per with-block.
#   - Code after the yield runs only when the block exits; do not rely on
#     it running before the block completes.
# ==============================================
