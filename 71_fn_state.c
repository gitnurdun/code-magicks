/* MAGIC: put function pointers in a table indexed by state; transitions
   are data, not a growing switch statement. */
#include <stdio.h>
typedef enum { IDLE, CHASE, ATTACK, STATE_COUNT } state;
typedef state (*step_fn)(void);
static state idle(void)   { puts("idle -> chase");   return CHASE; }
static state chase(void)  { puts("chase -> attack"); return ATTACK; }
static state attack(void) { puts("attack -> idle");  return IDLE; }
static step_fn table[STATE_COUNT] = {idle, chase, attack};
int main(void)
{
    state s = IDLE;
    for (int frame = 0; frame < 4; frame++) s = table[s]();
}

/* ================= EXPLANATION =================
   What it does
     Implements an AI/state machine as a table of function pointers. The
     current state is an enum used as an index; calling the handler for
     that state returns the next state.

   Step by step
     1. Each state handler is a function with the same signature: no
        arguments, returns the next state. idle prints a transition and
        returns CHASE; chase returns ATTACK; attack returns IDLE.
     2. table maps the enum values to those functions. The array order
        must match the enum; a static assertion or designated initializers
        can enforce that in larger code.
     3. The loop starts at IDLE. `table[s]()` calls idle, prints
        "idle -> chase", and returns CHASE. The next iteration calls
        chase, then attack, then idle again.
     4. The control flow is a cycle: IDLE -> CHASE -> ATTACK -> IDLE. Four
        frames produce four transitions.
     5. Adding a state means adding an enum value, a handler, and a table
        entry - no edits to a central switch. In a real game the handler
        also receives an entity/context pointer and can inspect the world.

   Output
     "idle -> chase"
     "chase -> attack"
     "attack -> idle"
     "idle -> chase"

   Why it matters
     Table-driven state machines keep transition logic local to each
     state and make the state set explicit. They are easy to serialise,
     visualise, and extend, and they avoid the branch misprediction and
     maintenance cost of a large switch. The same shape drives menus,
     network protocol states, animation graphs, and compiler passes.

   Caveats
     - All handlers must share the same signature; per-state data belongs
       in a context struct passed as an argument.
     - The enum must stay in sync with the table. A wrong index calls the
       wrong function or reads out of bounds.
     - Function pointer calls usually cannot be inlined, which matters in
       a very hot inner loop.
     - This is a flat finite-state machine. Hierarchical states,
       transitions with guards, and entry/exit actions need more
       structure (statecharts).
     - The demo ignores frame time and events; a real FSM transitions on
       conditions, not once per frame.
   ============================================== */
