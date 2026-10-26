"""MAGIC: a stack machine executes one small integer opcode at a time;
the program counter is just an index into a bytecode list."""
PUSH, ADD, MUL, PRINT, HALT = range(5)
code = [PUSH, 6, PUSH, 7, MUL, PRINT, HALT]
stack = []
pc = 0
while True:
    op = code[pc]
    pc += 1
    if op == PUSH:
        stack.append(code[pc])
        pc += 1
    elif op == ADD:
        b = stack.pop()
        a = stack.pop()
        stack.append(a + b)
    elif op == MUL:
        b = stack.pop()
        a = stack.pop()
        stack.append(a * b)
    elif op == PRINT:
        print(stack.pop())
    elif op == HALT:
        break

# ================= EXPLANATION =================
# What it does
#   Runs a tiny stack-based bytecode program: push 6, push 7, multiply,
#   print, halt. Operands live on an explicit stack, and the interpreter
#   loop fetches one opcode at a time.
#
# Step by step
#   1. PUSH 6 appends 6 to the stack; PUSH 7 appends 7. The stack is
#      [6, 7].
#   2. MUL pops b = 7 and a = 6, multiplies, and pushes 42. Order matters
#      for subtraction and division, but multiplication is commutative.
#   3. PRINT pops 42 and prints it, leaving the stack empty.
#   4. HALT breaks the dispatch loop. A real VM would also check pc bounds,
#      validate opcodes, and handle stack underflow before executing.
#   5. The program counter pc is the entire control state. There are no
#      Python functions for the opcodes; the interpreter is one loop with
#      an if/elif chain (a switch or computed goto in C).
#
# Output
#   "42"
#
# Why it matters
#   Stack machines are simple to compile to and easy to interpret:
#   expressions become postfix code, and temporaries need no register
#   allocation. CPython, the JVM, and many scripting languages use stack
#   bytecode; register machines (Lua, Dalvik) use fewer instructions but
#   more complex decoding. A JIT can compile this bytecode to native code
#   and keep the same semantics.
#
# Caveats
#   - This interpreter trusts the bytecode. Real VMs must validate
#     opcodes, operand counts, jump targets, and stack depth to avoid
#     crashes or sandbox escapes.
#   - The if/elif chain is a linear dispatch. A table of function pointers
#     or a computed goto is faster; CPython uses specialised instructions
#     and an adaptive interpreter.
#   - Python integers are arbitrary precision; a real bytecode VM must
#     define fixed-width arithmetic and overflow behaviour.
#   - There are no locals, calls, branches, or exceptions here. Those add
#     frames, a call stack, and unwinding.
#   - Bytecode is not a security boundary by itself; validate and isolate
#     untrusted code.
# ==============================================
