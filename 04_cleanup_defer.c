/* MAGIC: a local variable gets a destructor-like cleanup callback.
   WHY: early returns in C can still release owned resources. */
#include <stdio.h>
#include <stdlib.h>
static void cleanup(int**p){if(*p){printf("cleanup %d\n",**p);free(*p);}}
int main(void){int*p __attribute__((cleanup(cleanup)))=malloc(sizeof*p);if(!p)return 1;*p=7;puts("work");}

/* ================= EXPLANATION =================
   What it does
     Attaches a destructor-like callback to a local variable. Whenever the
     variable goes out of scope - normal fall-through, return, goto, or
     break out of the block - GCC/Clang calls the callback automatically,
     giving C a small piece of RAII.

   Step by step
     1. int *p __attribute__((cleanup(cleanup))) declares p and registers
        `cleanup` as its cleanup function. The attribute applies to the
        variable, not to its type.
     2. malloc(sizeof *p) allocates the int. If it fails, main returns 1,
        and cleanup still runs with p == NULL; the callback checks for
        that, so nothing is freed twice or dereferenced.
     3. The body stores 7 and prints "work".
     4. At the closing brace of main, the compiler inserts a call to
        cleanup(&p). The callback receives the address of the variable,
        prints the value ("cleanup 7"), and frees it. The variable is
        still in scope when the handler runs, which is why the int** form
        is used.

   Output
     "work"
     "cleanup 7"

   Why it matters
     Long functions with many early error returns tend to leak. Cleanup
     attributes let each resource be released exactly where it is
     acquired, without a single exit label and without relying on the
     programmer to remember every path.

   Caveats
     - GCC/Clang extension; not portable to MSVC or ISO C.
     - Handlers run in reverse order of declaration (like C++ locals).
     - They do not run on longjmp, abort, or _exit.
     - The handler must not throw or longjmp; keep it small and
       non-failing. The variable's value is indeterminate after cleanup.
   ============================================== */
