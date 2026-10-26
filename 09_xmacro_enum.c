/* MAGIC: replay one list under different macro definitions.
   WHY: enum names and strings cannot drift apart. */
#include <stdio.h>
#define COLORS(X) X(RED) X(GREEN) X(BLUE)
#define E(x) COLOR_##x,
enum color{COLORS(E)COLOR_COUNT};
#undef E
#define S(x)[COLOR_##x]=#x,
static const char*names[COLOR_COUNT]={COLORS(S)};
int main(void){for(int i=0;i<COLOR_COUNT;i++)puts(names[i]);}

/* ================= EXPLANATION =================
   What it does
     Defines the list of colours once as a macro, then "replays" that list
     under different definitions to generate the enum values and the
     matching string table. The two cannot drift apart because both come
     from the same tokens.

   Step by step
     1. COLORS(X) expands to X(RED) X(GREEN) X(BLUE). It is a list of
        items plus a hook macro X that decides what to do with each item.
     2. First pass: E(x) becomes COLOR_##x, so COLORS(E) becomes
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, followed by COLOR_COUNT.
        The enum therefore has values RED=0, GREEN=1, BLUE=2, and
        COLOR_COUNT == 3.
     3. E is #undef'd so it can be reused for a different expansion.
     4. Second pass: S(x) becomes [COLOR_##x] = #x, i.e. a designated
        initialiser keyed by the enum value and a stringified name. The
        array is declared with COLOR_COUNT elements, so every slot is
        filled exactly once.
     5. main() walks 0..COLOR_COUNT-1 and prints the names in enum order.

   Output
     "RED"
     "GREEN"
     "BLUE"

   Why it matters
     This "X-macro" pattern keeps a single source of truth for a set of
     variants. Add one X(YELLOW) and the enum, the name table, and any
     other replay (parser, printer, dispatch switch) all gain it at once.
     Forgetting a case in a switch becomes much less likely.

   Caveats
     - All definitions must be visible at the point of use; the list macro
       is usually kept in a header.
     - Add explicit values or a terminator if you need stable wire values.
     - Error messages from macro expansions point at odd places, and
       debuggers see the expanded code.
     - Variants with different payloads need a more elaborate X macro with
       multiple arguments.
   ============================================== */
