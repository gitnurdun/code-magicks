/* MAGIC: record operations in a buffer now, replay them later. The
   producer and the consumer are decoupled in time. */
#include <stdio.h>
enum op { OP_SET, OP_ADD, OP_MUL };
struct command { int op, arg; };
struct cmd_buffer { struct command items[8]; int count; };
static void record(struct cmd_buffer *b, int op, int arg)
{
    b->items[b->count++] = (struct command){op, arg};
}
static int replay(const struct cmd_buffer *b, int value)
{
    for (int i = 0; i < b->count; i++) {
        switch (b->items[i].op) {
        case OP_SET: value = b->items[i].arg; break;
        case OP_ADD: value += b->items[i].arg; break;
        case OP_MUL: value *= b->items[i].arg; break;
        }
    }
    return value;
}
int main(void)
{
    struct cmd_buffer b = {0};
    record(&b, OP_SET, 5);
    record(&b, OP_ADD, 3);
    record(&b, OP_MUL, 4);
    printf("recorded=%d result=%d\n", b.count, replay(&b, 0));
}

/* ================= EXPLANATION =================
   What it does
     Stores a sequence of operations in a command buffer, then executes
     them later in one pass. SET 5, ADD 3, MUL 4 evaluates to (5 + 3) * 4
     = 32.

   Step by step
     1. Each command is a small tagged record: an opcode and one integer
        argument. record appends it to a fixed array; no operation is
        performed at record time.
     2. The three commands capture intent, not immediate side effects.
        The value being modified is supplied only when replay runs.
     3. replay walks the buffer in order and dispatches on the opcode.
        SET replaces the value, ADD adds, MUL multiplies. Order matters:
        multiplying before adding would give a different result.
     4. The final value is 32, and count is 3.
     5. Because commands are data, the same buffer could be replayed
        against another object, serialised to disk, sent over the network,
        or inspected before execution.

   Output
     "recorded=3 result=32"

   Why it matters
     Command buffers are how GPUs, databases, and distributed systems
     separate "what to do" from "when to do it". Graphics APIs record draw
     calls and replay them on the GPU; editors record undoable commands;
     databases write a write-ahead log and replay it after a crash. The
     pattern enables batching, validation, deterministic replay, and
     parallel execution.

   Caveats
     - The buffer is fixed-size and unchecked; a real implementation must
       grow or reject when full.
     - Commands often reference resources by handle, not raw pointer;
       lifetime and ownership must outlive the replay.
     - Validation is essential if the buffer crosses a trust boundary. A
       malformed opcode or argument can corrupt state.
     - Replay must be deterministic: avoid hidden reads of wall-clock
       time, random numbers, or mutable global state.
     - Undo requires inverse commands or snapshots, not just replay from
       zero. Some operations are not invertible.
   ============================================== */
