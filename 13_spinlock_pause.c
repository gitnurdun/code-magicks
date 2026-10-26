/* MAGIC: a spin loop gives the CPU a pause hint while waiting.
   CAVEAT: short critical sections only; this is x86-oriented. */
#include <stdatomic.h>
#include <immintrin.h>
#include <stdio.h>
int main(void){atomic_flag f=ATOMIC_FLAG_INIT;while(atomic_flag_test_and_set(&f))_mm_pause();puts("locked");atomic_flag_clear(&f);}

/* ================= EXPLANATION =================
   What it does
     A minimal spin lock built from an atomic_flag, with the x86 PAUSE
     instruction executed while the lock is busy.

   Step by step
     1. atomic_flag f = ATOMIC_FLAG_INIT creates a lock in the clear
        state. atomic_flag is the one type guaranteed to be lock-free.
     2. atomic_flag_test_and_set(&f) atomically sets the flag to true and
        returns its previous value. If the previous value was false, this
        caller just acquired the lock and the loop exits immediately.
     3. If the flag was already true, the lock is held by someone else.
        The loop calls _mm_pause() and tries again. That is a spin.
     4. Once acquired, the program prints "locked" and releases the lock
        with atomic_flag_clear(&f). In real code the critical section
        would be between acquire and clear.

   Output
     "locked"

   Why it matters
     PAUSE is not a no-op. On x86 it de-pipelines the spin loop, reduces
     power and contention on the cache line, and avoids the severe
     misprediction penalty when the lock is released. It also gives
     SMT siblings a chance to run. Spin locks make sense only for very
     short critical sections where sleeping costs more than spinning.

   Caveats
     - Busy-waiting burns a core. On a uniprocessor, or when the holder
       can be preempted, this can waste the whole time slice.
     - Add a backoff (exponential pause, then sched_yield/futex) and a
       fairness plan; plain test-and-set is not fair.
     - This protects nothing in the demo; real critical sections need
       acquire/release semantics (the default seq_cst here is stronger
       than necessary).
     - _mm_pause() is x86-specific. Use a portable yield/pause wrapper
       elsewhere (Arm has YIELD, and ISB in some spin loops).
   ============================================== */
