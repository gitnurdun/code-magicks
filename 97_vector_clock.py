"""MAGIC: give every process a counter vector; comparing components
recovers causality and detects events that are merely concurrent."""
def tick(v, i):
    w = v.copy()
    w[i] += 1
    return w

def send(v, i):
    return tick(v, i)

def receive(v, i, message):
    w = v.copy()
    for j in range(len(w)):
        w[j] = max(w[j], message[j])
    w[i] += 1
    return w

def happens_before(a, b):
    return all(x <= y for x, y in zip(a, b)) and any(x < y for x, y in zip(a, b))

def concurrent(a, b):
    return a != b and not happens_before(a, b) and not happens_before(b, a)

a1 = tick([0, 0, 0], 0)          # A's first event
b1 = tick([0, 0, 0], 1)          # B's first event, independent
b2 = receive(b1, 1, a1)          # B receives A's event, then ticks
c1 = tick([0, 0, 0], 2)          # C is independent
print("a1", a1, "b1", b1, "b2", b2, "c1", c1)
print("a1 < b2:", happens_before(a1, b2))
print("b1 < b2:", happens_before(b1, b2))
print("a1 || c1:", concurrent(a1, c1))

# ================= EXPLANATION =================
# What it does
#   Simulates three processes with vector clocks. Each process increments
#   its own component on a local event; receiving a message merges the
#   sender's vector by taking the component-wise maximum, then increments
#   the receiver's own component.
#
# Step by step
#   1. A's first event produces a1 = [1,0,0]. B's independent first event
#      produces b1 = [0,1,0]. Neither vector dominates the other, so they
#      are concurrent.
#   2. B receives A's vector [1,0,0]. receive merges max([0,1,0],
#      [1,0,0]) = [1,1,0], then increments B's own component to
#      [1,2,0]. b2 now records both A's event and B's two events.
#   3. a1 <= b2 component-wise and a1 != b2, so a1 happens-before b2.
#      b1 also happens-before b2 for the same reason.
#   4. C's independent event c1 = [0,0,1] is concurrent with a1: neither
#      vector is <= the other.
#   5. The partial order is exactly the causality relation; there is no
#      global clock, and [1,0,0] vs [0,1,0] cannot be ordered.
#
# Output
#   "a1 [1, 0, 0] b1 [0, 1, 0] b2 [1, 2, 0] c1 [0, 0, 1]"
#   "a1 < b2: True"
#   "b1 < b2: True"
#   "a1 || c1: True"
#
# Why it matters
#   Distributed systems cannot rely on wall-clock time: clocks skew, and
#   "later" is ambiguous without communication. Vector clocks capture
#   causality, enabling conflict detection in Dynamo-style stores,
#   distributed debugging, snapshots, and causal broadcast. They are the
#   conceptual foundation of version vectors and dotted version vectors.
#
# Caveats
#   - Vector clocks grow linearly with the number of processes. Version
#     vectors prune inactive entries; dotted version vectors track
#     per-node counters more compactly.
#   - Comparing vectors is O(n) in the number of processes.
#   - A vector clock orders events only by causality, not by real time.
#     Two concurrent writes still need a conflict-resolution policy.
#   - Merging must be done at every message receive; missing one loses
#     causality.
#   - The demo assumes reliable, ordered delivery. Real networks can
#     duplicate, delay, or reorder messages; the protocol must tolerate
#     that.
#   - Vector clocks detect concurrency but do not by themselves provide
#     consensus or a total order.
# ==============================================
