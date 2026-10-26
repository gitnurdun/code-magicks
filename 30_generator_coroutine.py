"""MAGIC: a generator is a suspended frame with locals and an instruction pointer."""
def accumulator():
    total = 0
    while True:
        value = yield total
        total += value
g = accumulator(); next(g); print(g.send(4)); print(g.send(5))

# ================= EXPLANATION =================
# What it does
#   Builds a running accumulator as a generator coroutine: values are sent
#   into a suspended function, and the current total is yielded back out.
#
# Step by step
#   1. `accumulator()` is a generator function because it contains
#      `yield`. Calling it creates a generator object and does not run any
#      of the body yet.
#   2. `next(g)` starts it: total = 0, the loop begins, and `yield total`
#      yields 0 and suspends. The frame (total, the instruction pointer,
#      the loop state) is kept alive on the generator object. The 0 is
#      discarded here.
#   3. `g.send(4)` resumes at the yield expression, so the yield's value
#      becomes 4 and is assigned to `value`. total becomes 4, the loop
#      continues, and it yields 4. send() returns that yielded value, so
#      "4" is printed.
#   4. `g.send(5)` resumes again: value = 5, total = 9, yields 9, printed.
#   5. The generator is still alive but is never resumed, so it is closed
#      when g is garbage-collected.
#
# Output
#   "4"
#   "9"
#
# Why it matters
#   A generator is a stack frame that can be paused and resumed. That is
#   the same idea as a coroutine: the function keeps its locals and its
#   position between calls. This is the basis of streaming pipelines,
#   asyncio tasks, and producer/consumer code that does not need threads.
#
# Caveats
#   - You cannot send a non-None value to a generator that has not started
#     yet; call next() or send(None) first (TypeError otherwise).
#   - When the function returns, send()/next() raise StopIteration; use
#     try/except or `yield from` to compose generators.
#   - The frame is retained for as long as the generator is referenced;
#     close() it (or use contextlib.closing) to release resources.
#   - Values are one-way through send/yield; use asyncio or a full
#     coroutine library for scheduling and exception propagation.
# ==============================================
