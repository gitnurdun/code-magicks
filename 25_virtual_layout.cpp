/* MAGIC: virtual dispatch is data: an object usually begins with a vptr. */
#include <cstdio>
struct Base{virtual void speak(){std::puts("base");}virtual~Base()=default;};struct Dog:Base{void speak()override{std::puts("dog");}};
int main(){Dog d;Base*b=&d;b->speak();std::printf("object=%zu\n",sizeof d);}

/* ================= EXPLANATION =================
   What it does
     Demonstrates that virtual dispatch is ordinary data: each polymorphic
     object carries a pointer (the vptr) to its class's virtual table, and
     the call loads the function address from that table.

   Step by step
     1. Base declares virtual speak() and a virtual destructor. That makes
        Base polymorphic, so the compiler adds a hidden vptr as a member
        (usually at offset 0). It also creates a vtable per polymorphic
        class: an array of function pointers for speak and the destructor.
     2. Dog derives from Base and overrides speak(). Dog has its own
        vtable whose speak slot points at Dog::speak.
     3. `Dog d;` constructs the object with Dog's vptr at offset 0.
        `Base* b = &d;` stores the same address, because the Base
        subobject is at offset 0 in this single-inheritance case.
     4. b->speak() looks up the vptr, indexes the speak slot, and calls
        Dog::speak - dynamic dispatch, resolved at run time even though
        the static type of b is Base*.
     5. sizeof d is the size of the vptr (typically 8 on x86-64), because
        Dog adds no data members. The printf prints that size.

   Output
     "dog"
     "object=8"   (typical 64-bit ABI; may differ by compiler/ABI)

   Why it matters
     Virtual calls cost one or two indirections and inhibit inlining;
     knowing the layout explains devirtualisation, why adding the first
     virtual function changes an object's size, and why a virtual
     destructor is required when deleting through a base pointer.

   Caveats
     - The vptr/vtable layout is not specified by the C++ standard; it is
       an ABI detail (Itanium ABI vs MSVC). Do not depend on the exact
       size or offset in portable code.
     - With multiple or virtual inheritance, converting Dog* to Base* may
       adjust the pointer; the base subobject is not always at offset 0.
     - A vtable is per class (plus RTTI), a vptr is per object. An empty
       polymorphic class is therefore not empty.
     - Mark overrides `override` and base destructors `virtual` to avoid
       silent slicing and undefined deletion.
   ============================================== */
