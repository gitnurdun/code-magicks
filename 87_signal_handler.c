/* MAGIC: a signal handler runs asynchronously, interrupting normal
   control flow; it may only touch async-signal-safe state. */
#include <signal.h>
#include <stdio.h>
static volatile sig_atomic_t caught;
static void on_signal(int sig) { caught = sig; }
int main(void)
{
    if (signal(SIGINT, on_signal) == SIG_ERR) {
        perror("signal");
        return 1;
    }
    if (raise(SIGINT) != 0) {
        perror("raise");
        return 1;
    }
    printf("caught=%d\n", (int)caught);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Installs a handler for SIGINT, sends SIGINT to the current process,
     and observes that the handler ran. The handler does one thing: store
     the signal number in a `volatile sig_atomic_t`.

   Step by step
     1. signal(SIGINT, on_signal) asks the kernel to interrupt the process
        asynchronously when SIGINT arrives. The old disposition is
        replaced.
     2. raise(SIGINT) delivers the signal to the calling thread (or the
        process, depending on the platform). Before raise returns, control
        jumps to on_signal.
     3. on_signal writes SIGINT (2) into `caught`. sig_atomic_t is the
        only integer type guaranteed to be readable/writable atomically
        with respect to signal delivery, and volatile tells the compiler
        that the value can change outside normal control flow.
     4. The handler returns; execution resumes at raise, then main prints
        caught = 2.
     5. If no handler were installed, SIGINT would terminate the process.
        SIG_IGN would ignore it, and SIG_DFL would restore the default.

   Output
     "caught=2"

   Why it matters
     Signals are the POSIX mechanism for asynchronous events: Ctrl-C,
     timers, child exits, I/O readiness, and fatal faults. The handler is
     a tiny callback in a very restricted environment, not a normal
     function. A robust design usually has the handler set a flag or write
     one byte to a self-pipe; the main loop does the real work.

   Caveats
     - Only async-signal-safe functions may be called from a handler.
       printf, malloc, stdio locks, and most library calls are forbidden;
       calling them can deadlock or corrupt state.
     - `volatile sig_atomic_t` is the portable flag type. Other types may
       be partially updated or cached in registers.
     - A signal can arrive between any two machine instructions. The
       handler must not assume the interrupted code's invariants.
     - Signals can coalesce: multiple identical signals may be delivered
       once. They are not a reliable counter or queue.
     - Multi-threaded delivery is platform-specific; use sigwait,
       signalfd, or block signals in all but one thread.
     - Windows supports only a small set of C signals and has different
       semantics for SIGINT/SIGTERM; this demo is portable C, not a
       general POSIX signal API.
     - System calls interrupted by a signal may return EINTR unless
       SA_RESTART is set; retry loops matter.
   ============================================== */
