"""MAGIC: CPython can call a Python function on execution events."""
import sys
def trace(frame, event, arg):
    if event == 'call': print('call', frame.f_code.co_name)
    return trace
def work(x): return x * 2
sys.settrace(trace); work(3); sys.settrace(None)

# ================= EXPLANATION =================
# What it does
#   Installs a global trace function that CPython calls on execution
#   events, and prints the name of every function that is called.
#
# Step by step
#   1. sys.settrace(trace) registers `trace` for the current thread. It
#      takes effect for frames created after the call, so the module frame
#      already running is not traced.
#   2. When work(3) is called, CPython fires a "call" event and invokes
#      trace(frame, "call", None). frame is the executing frame;
#      frame.f_code.co_name is "work".
#   3. Returning the trace function itself means "keep tracing this
#      frame". If it returned None, tracing would stop for that frame.
#      Line and return events would then also be reported, but this trace
#      only prints for "call".
#   4. work computes 3 * 2 and returns 6; the result is discarded.
#   5. sys.settrace(None) removes the hook. Without it, overhead would
#      persist and the trace could fire in later code (and recurse if the
#      tracer called traced functions).
#
# Output
#   "call work"
#
# Why it matters
#   sys.settrace is the hook behind pdb, coverage.py, profilers, and
#   tracing tools. It exposes the interpreter's execution events (call,
#   line, return, exception, opcode) and the frame objects, so debuggers
#   can step, breakpoints can pause, and call graphs can be recorded.
#
# Caveats
#   - It is per-thread; new threads need threading.settrace.
#   - The overhead is large - often 10x or more - because every event
#     calls back into Python. Never leave it enabled in production.
#   - An exception in the trace function disables tracing; it is also
#     easy to cause infinite recursion by tracing the tracer.
#   - Only frames created after settrace are traced; the current one is
#     not.
#   - Python 3.12's sys.monitoring offers a lower-overhead, more granular
#     alternative for new tools.
# ==============================================
