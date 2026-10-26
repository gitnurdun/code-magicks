/* MAGIC: cleanup is an object whose destructor runs during unwinding. */
#include <cstdio>
#include <utility>
template<class F>struct Guard{F f;~Guard(){f();}};
template<class F>Guard<F> defer(F f){return{std::move(f)};}
int main(){bool committed=false;auto g=defer([&]{if(!committed)std::puts("rollback");});}

/* ================= EXPLANATION =================
   What it does
     Turns a lambda into a scope guard: the lambda runs automatically when
     the Guard object is destroyed, including during stack unwinding after
     an exception.

   Step by step
     1. Guard<F> stores the callable and calls f() in its destructor. The
        destructor is the only place cleanup is invoked, so there is no
        path that forgets it.
     2. defer(F f) returns a Guard<F> by value. In C++17 the returned
        prvalue is materialised directly in `g`, so no copy or move of
        the lambda is required.
     3. main() creates g with a lambda that captures `committed` by
        reference. `committed` stays false, so the lambda will print
        "rollback".
     4. `g` is the last local, so at the closing brace of main its
        destructor runs and the lambda executes. Nothing else needs to be
        written.

   Output
     "rollback"

   Why it matters
     This is RAII applied to arbitrary cleanup: a transaction rollback, a
     mutex unlock, a file close, a profiling timer stop. It removes the
     duplicated cleanup code at every early-return path and is exception
     safe by construction.

   Caveats
     - The Guard must be non-copyable. The implicit copy constructor would
       let two objects run the same callable twice; use
       Guard(const Guard&) = delete and a move constructor that disarms
       the source.
     - The destructor must not throw. Mark it noexcept and keep the
       callable's exceptions contained.
     - This demo's lambda captures by reference, so g must not outlive
       `committed`; in general watch dangling captures.
     - Compiler warnings about unused variables are why a real defer()
       returns [[nodiscard]] Guard.
     - Order matters: guards run in reverse order of construction.
   ============================================== */
