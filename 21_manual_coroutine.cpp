/* MAGIC: a coroutine can be a switch over a saved program counter. */
#include <cstdio>
struct Counter{int state=0,value=0;bool next(int&out){switch(state){case 0:state=1;out=value++;return true;case 1:state=2;out=value++;return true;case 2:state=3;out=value++;return true;default:return false;}}};
int main(){Counter c;int x;while(c.next(x))std::printf("%d\n",x);}

/* ================= EXPLANATION =================
   What it does
     Implements a resumable function (a coroutine) as an explicit state
     machine: the struct stores the state and the current value, and a
     switch jumps back to the point where the previous call returned.

   Step by step
     1. state starts at 0 and value at 0.
     2. The first next() enters case 0: it sets state = 1, writes
        out = value++ (yielding 0), and returns true. The return exits the
        function, but state survives in the object.
     3. The next call's switch sees state == 1 and jumps to case 1, which
        yields 1 and sets state = 2. The third call yields 2 and sets
        state = 3.
     4. When state is 3, the switch falls to default and returns false, so
        the while loop in main stops.

   Output
     "0"
     "1"
     "2"

   Why it matters
     A coroutine is just a saved instruction pointer plus saved locals.
     Here both live in the struct, so the function can be suspended
     between calls without a thread or a stack. This is how protothreads,
     event-loop state machines, and generators work under the hood; C++20
     coroutines automate the same transformation.

   Caveats
     - Every local that must survive a suspension has to be a member; a
       normal local is destroyed on return.
     - The states are hand-numbered, so adding a yield means renumbering
       carefully; the compiler cannot check the transitions.
     - This runs on the caller's stack and cannot suspend across a call
       chain (no stackful context switch).
     - The switch is O(1) thanks to a jump table, but a long chain of
       states can hurt readability.
   ============================================== */
