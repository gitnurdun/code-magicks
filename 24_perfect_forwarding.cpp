/* MAGIC: a wrapper preserves whether its argument is an lvalue or rvalue. */
#include <cstdio>
#include <utility>
void take(const int&){std::puts("lvalue path");}void take(int&&){std::puts("rvalue path");}
template<class T>void relay(T&&x){take(std::forward<T>(x));}
int main(){int n=1;relay(n);relay(2);}

/* ================= EXPLANATION =================
   What it does
     A wrapper that passes its argument through while preserving whether
     it was an lvalue or an rvalue, so the correct overload of take() is
     selected on the other side.

   Step by step
     1. relay's parameter is T&& where T is deduced - a forwarding
        reference, not an rvalue reference. Copy the argument in:
          - relay(n) with an lvalue int: T deduces to int&, and
            reference collapsing makes int& && become int&.
          - relay(2) with an rvalue int: T deduces to int, so the
            parameter is int&&.
     2. std::forward<T>(x) casts x back to exactly that deduced category:
        an lvalue for T = int&, an rvalue for T = int. A plain
        static_cast would need the type spelled out; forward encodes the
        rule in one call.
     3. take(const int&) is chosen for the lvalue, take(int&&) for the
        rvalue, so the two lines print different paths.

   Output
     "lvalue path"
     "rvalue path"

   Why it matters
     Perfect forwarding is what lets make_unique, emplace_back, and
     factory functions pass constructor arguments through layers without
     extra copies or accidental moves. Without it, a wrapper collapses
     rvalues to lvalues and silently copies, or moves things that should
     stay put.

   Caveats
     - T&& is a forwarding reference only when T is deduced (a template
       parameter of that function); in a class template or with an
       explicit type it is a plain rvalue reference.
     - Forward an argument at most once; forwarding it twice can move
       from it twice.
     - Use std::move only to promise "I am done with this"; use
       std::forward only to pass a forwarding reference along.
     - The deduction fails for braced initialisers and overloaded
       functions; be explicit there.
   ============================================== */
