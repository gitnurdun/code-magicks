"""MAGIC: sort live intervals by start, expire old ones, and hand out a
register. If none is free, spill the value to memory."""
intervals = [("a", 0, 3), ("b", 1, 4), ("c", 2, 5), ("d", 6, 8)]
registers = ["r0", "r1"]
active = []          # (end, register, name)
assignment = {}

for name, start, end in intervals:
    active = [item for item in active if item[0] > start]
    used = {reg for _, reg, _ in active}
    free = next((reg for reg in registers if reg not in used), None)
    if free is None:
        assignment[name] = "spill"
        continue
    assignment[name] = free
    active.append((end, free, name))

for name, _, _ in intervals:
    print(f"{name} -> {assignment[name]}")

# ================= EXPLANATION =================
# What it does
#   Performs linear-scan register allocation for four values with known
#   live intervals. It walks the intervals in start order, frees registers
#   whose intervals have ended, and spills when every register is busy.
#
# Step by step
#   1. a is live from 0 to 3. No register is active, so it gets r0.
#   2. b starts at 1 while a is still live (end 3 > 1), so b gets r1.
#   3. c starts at 2. Both a and b are still live, so no register is free
#      and c is spilled to memory.
#   4. d starts at 6. All earlier intervals have ended (their end <= 6),
#      so the active list is empty and d gets r0 again.
#   5. The result maps a to r0, b to r1, c to spill, d to r0. The actual
#      spill code would store c after its definition and reload it before
#      each use.
#
# Output
#   "a -> r0"
#   "b -> r1"
#   "c -> spill"
#   "d -> r0"
#
# Why it matters
#   Register allocation is the bridge between an unbounded SSA graph and a
#   fixed register file. Linear scan is simple, fast, and good enough for
#   JITs and baseline compilers; graph-colouring allocators produce better
#   code but cost more compile time. The "expire old intervals" step is the
#   key insight: at any program point, only live values compete for
#   registers.
#
# Caveats
#   - Linear scan is approximate. It assumes intervals are contiguous and
#     does not model holes, so it can spill when a graph-colouring
#     allocator would not.
#   - This demo ignores register classes (integer, float, vector), calling
#     conventions, and precoloured registers.
#   - Spill cost matters: spilling a value used in a hot loop is much
#     worse than spilling a cold temporary. Production allocators weigh
#     uses and loop depth.
#   - Live intervals must be computed correctly from the control-flow
#     graph, including phi nodes and loops.
#   - The algorithm processes intervals sorted by start; unsorted input
#     breaks the active-expiry logic.
#   - Coalescing moves and handling fixed constraints (e.g. return value
#     in rax) are additional passes.
# ==============================================
