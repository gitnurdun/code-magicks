"""MAGIC: rename every definition, then a variable is just the latest
value flowing in. Constants can be folded as soon as they are defined."""
code = [
    ("x0", "const", 2, None),
    ("y0", "const", 3, None),
    ("t0", "mul", "x0", "y0"),
    ("z0", "add", "t0", 4),
    ("x1", "add", "x0", 1),
    ("w0", "mul", "x1", 1),
    ("p0", "param", None, None),
    ("q0", "add", "p0", 1),
]
constants = {}
for dest, op, a, b in code:
    if op == "const":
        constants[dest] = a
        print(f"{dest} = {a}  ; const")
    elif op == "param":
        print(f"{dest} = param  ; unknown")
    else:
        av = constants.get(a, a)
        bv = constants.get(b, b)
        if isinstance(av, int) and isinstance(bv, int):
            value = av * bv if op == "mul" else av + bv
            constants[dest] = value
            print(f"{dest} = {op}({av}, {bv}) -> {value}  ; folded")
        else:
            print(f"{dest} = {op}({av}, {bv})  ; not foldable")

# ================= EXPLANATION =================
# What it does
#   Converts a straight-line program into static single assignment (SSA)
#   form and performs constant folding. Each assignment gets a fresh name:
#   the second assignment to x becomes x1, so every name is defined once.
#
# Step by step
#   1. x0 and y0 are compile-time constants 2 and 3. They go into the
#      constants table.
#   2. t0 = mul(x0, y0) sees both operands as 2 and 3, so it computes 6
#      and records t0 = 6. z0 = add(t0, 4) folds to 10 in the same way.
#   3. x1 = add(x0, 1) is a new definition of x, not an overwrite. It
#      folds to 3 while x0 remains 2. Downstream code that still refers to
#      x0 keeps seeing 2; SSA removes the ambiguity of a mutable variable.
#   4. w0 = mul(x1, 1) folds to 3. p0 is a parameter with no known value,
#      so q0 = add(p0, 1) cannot fold and is printed symbolically.
#   5. In a real SSA graph, a control-flow join uses a phi node such as
#      x2 = phi(x0 from then, x1 from else), merging the definitions that
#      reach the join. This straight-line demo has no branches, so no phi
#      is needed.
#
# Output
#   "x0 = 2  ; const"
#   "y0 = 3  ; const"
#   "t0 = mul(2, 3) -> 6  ; folded"
#   "z0 = add(6, 4) -> 10  ; folded"
#   "x1 = add(2, 1) -> 3  ; folded"
#   "w0 = mul(3, 1) -> 3  ; folded"
#   "p0 = param  ; unknown"
#   "q0 = add(p0, 1)  ; not foldable"
#
# Why it matters
#   SSA is the intermediate representation behind LLVM, GCC's GIMPLE,
#   V8's TurboFan, and most modern optimisers. Because each value has one
#   definition, def-use chains are explicit and optimisations such as
#   constant propagation, dead code elimination, and register allocation
#   become graph algorithms. Renaming is the key move: it turns "which
#   assignment reached here?" into a direct edge.
#
# Caveats
#   - Real SSA handles branches, loops, and phi nodes; constructing it
#     correctly requires dominance frontiers or a Cytron-style algorithm.
#   - Folding must respect integer overflow, floating-point rounding, and
#     trapping operations; this demo assumes unbounded Python ints.
#   - A parameter is not a constant, even if a particular call passes a
#     constant; interprocedural analysis is needed for that.
#   - SSA names are not machine registers. A later pass maps many short-
#     lived names onto a small register set.
#   - Side effects, exceptions, and volatile loads cannot be reordered or
#     folded as freely as pure arithmetic.
# ==============================================
