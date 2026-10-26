/* MAGIC: storage and construction are separate operations. */
#include <cstdio>
#include <new>
struct Widget{int x;Widget(int n):x(n){}};
int main(){alignas(Widget)unsigned char raw[sizeof(Widget)];Widget*w=new(raw)Widget(7);std::printf("%d\n",w->x);w->~Widget();}

/* ================= EXPLANATION =================
   What it does
     Separates storage from construction: `raw` provides aligned bytes,
     and placement new builds a Widget inside them. The destructor is
     called explicitly and no deallocation happens, because none was done.

   Step by step
     1. alignas(Widget) unsigned char raw[sizeof(Widget)] reserves exactly
        enough correctly aligned storage on the stack. It is just bytes,
        not a Widget; no constructor has run.
     2. new (raw) Widget(7) is placement new: it uses the `operator new`
        overload that takes a void* and returns it unchanged, then runs the
        Widget constructor at that address. No heap allocation occurs.
     3. w->x reads the constructed member, printing 7.
     4. w->~Widget() runs the destructor explicitly. This is required for
        non-trivial types; the storage itself goes away with `raw` at the
        end of main.
     5. There is no delete: delete would call the destructor again and
        pass a stack address to the deallocator.

   Output
     "7"

   Why it matters
     Containers, allocators, and fixed-capacity pools reserve raw storage
     up front and construct objects in place as needed. Placement new is
     also the tool for memory-mapped I/O, shared memory, and avoiding
     repeated allocation in hot paths. It makes explicit what normal new
     does implicitly: allocate, then construct.

   Caveats
     - Every placement-new'd object needs an explicit destructor call (or
       std::destroy_at), or its resources leak.
     - The storage must be correctly aligned and at least sizeof(T) bytes;
       otherwise the behaviour is undefined. Use std::aligned_storage or
       alignas(T) std::byte[sizeof(T)].
     - If the constructor throws, no object exists and nothing must be
       destroyed; the storage is still yours.
     - After reusing storage for a new object, older pointers may need
       std::launder to refer to the new object.
     - C++20's std::construct_at is the cleaner, constexpr-friendly
       spelling.
   ============================================== */
