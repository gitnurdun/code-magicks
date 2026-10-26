/* MAGIC: erase a callable's type while keeping small callables inline. */
#include <cstdio>
#include <new>
#include <utility>
struct AnyFn{using Call=void(*)(void*);using Drop=void(*)(void*);alignas(16) unsigned char buf[32];Call call{};Drop drop{};template<class F>AnyFn(F f){static_assert(sizeof(F)<=32);new(buf)F(std::move(f));call=[](void*p){(*static_cast<F*>(p))();};drop=[](void*p){static_cast<F*>(p)->~F();};}~AnyFn(){if(drop)drop(buf);}void operator()(){call(buf);}};
int main(){AnyFn f=[](){std::puts("small and erased");};f();}

/* ================= EXPLANATION =================
   What it does
     Type erasure with small-buffer optimisation: AnyFn can hold any
     callable that fits in 32 bytes and only stores two function pointers
     plus the object's bytes - no heap allocation.

   Step by step
     1. `buf` is 32 bytes aligned to 16. `call` knows how to invoke the
        stored object; `drop` knows how to destroy it. Both are ordinary
        function pointers, so they cost one word each.
     2. The templated constructor deduces the concrete type F. The
        static_assert rejects callables that do not fit.
     3. `new (buf) F(std::move(f))` is placement new: it constructs the F
        inside the buffer without allocating. The buffer's lifetime is
        the AnyFn's lifetime, but the F object must be constructed and
        destroyed explicitly.
     4. Two captureless lambdas are converted to function pointers. One
        casts buf back to F* and calls the stored callable; the other
        calls its destructor. They are monomorphised per F, which is what
        recovers the type at the call site.
     5. operator() invokes call(buf) without knowing F. The destructor
        runs drop(buf) if an object was constructed.
     6. main() stores a small lambda, then calls it.

   Output
     "small and erased"

   Why it matters
     std::function and similar type-erased wrappers usually allocate when
     the callable is larger than their internal buffer. Keeping small
     callables inline removes an allocation from hot paths such as event
     callbacks, task queues, and signal handlers. The vtable-like pair of
     function pointers is the classic hand-rolled type erasure.

   Caveats
     - No copy or move constructor is defined. The implicit copy would
       memcpy buf and the pointers, so both copies would destroy the same
       object (double destruction). A real implementation must handle or
       delete copy/move.
     - Only sizeof(F) is checked, not alignof(F); an over-aligned callable
       would break the 16-byte buffer alignment.
     - If F's constructor throws, the object is not created and drop stays
       null; the destructor must guard on that.
     - std::function already performs this optimisation for small
       callables; prefer it unless you need a fixed footprint.
   ============================================== */
