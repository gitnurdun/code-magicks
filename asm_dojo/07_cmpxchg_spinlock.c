/* ASM MAGICK #07 - lock cmpxchg: compare-and-swap, the atom of atomics.
   Build: gcc -O2 -std=c11 07_cmpxchg_spinlock.c -o 07_cmpxchg_spinlock.exe
   MAGIC: one instruction reads, compares and conditionally writes a memory
   location without any other core observing an intermediate state. */
#include <stdio.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct { int locked; } spinlock;

static void spin_lock(spinlock *l)
{
    int expected;
    for (;;) {
        /* Read-only probe: does not invalidate the line while it is busy. */
        while (__atomic_load_n(&l->locked, __ATOMIC_RELAXED))
            __asm__ volatile("pause");

        expected = 0;
        /* If [l] == eax then [l] = 1 and ZF=1; else eax = [l], ZF=0.
           "+a" makes eax both the comparand and the failure result. */
        __asm__ volatile("lock cmpxchgl %2, %0"
                         : "+m"(l->locked), "+a"(expected)
                         : "r"(1)
                         : "cc", "memory");
        if (expected == 0)
            return;                 /* we stored 1: acquired */
    }
}

static void spin_unlock(spinlock *l)
{
    __atomic_store_n(&l->locked, 0, __ATOMIC_RELEASE);
}

int main(void)
{
    spinlock l = {0};
    int protected_counter = 0;

    spin_lock(&l);
    protected_counter++;
    printf("[07] inside critical section: counter=%d locked=%d\n",
           protected_counter, __atomic_load_n(&l.locked, __ATOMIC_RELAXED));
    spin_unlock(&l);
    printf("[07] after unlock: locked=%d\n", l.locked);

    /* The raw instruction, success and failure, without a loop. */
    int dst = 5, expected = 5;
    __asm__ volatile("lock cmpxchgl %2, %0"
                     : "+m"(dst), "+a"(expected)
                     : "r"(9)
                     : "cc", "memory");
    printf("[07] success: dst=%d expected=%d\n", dst, expected);

    dst = 5; expected = 4;
    __asm__ volatile("lock cmpxchgl %2, %0"
                     : "+m"(dst), "+a"(expected)
                     : "r"(9)
                     : "cc", "memory");
    printf("[07] failure: dst=%d expected=%d (ZF clear)\n", dst, expected);
    return 0;
}

/* ================= EXPLANATION =================
   Semantics of CMPXCHG
       if (eax == [dst]) { ZF = 1; [dst] = src; }
       else              { ZF = 0; eax = [dst]; }
     It is a single instruction, but with the LOCK prefix it is also a
     full memory barrier: all prior loads/stores are globally visible
     before, and no later access is reordered before it. That is why a
     correctly used cmpxchg does not need an extra mfence.

   Why the loop is shaped that way
     The outer loop exists because another core can win between the probe
     and the exchange. The inner `while (load) pause` is an optimization:
     a plain load does not request exclusive ownership, so a crowd of
     waiters does not continuously invalidate the line. Only when the lock
     looks free do we issue the locked operation.

   The "+a" trick
     cmpxchg hard-codes eax as the comparand and as the failure destination.
     Declaring `expected` as "+a" lets the template read it and write the
     observed value back. On success EAX is unchanged, so `expected == 0`
     still means "we acquired". On failure it now holds the old lock value.

   Why it matters
     This is the primitive under std::atomic compare_exchange, mutexes,
     lock-free stacks (#49), reference counts (#77), and DCLP (#55). The
     lock-free algorithms in this collection are all variations on "read,
     compute, cmpxchg, retry".

   Output
     [07] inside critical section: counter=1 locked=1
     [07] after unlock: locked=0
     [07] success: dst=9 expected=5
     [07] failure: dst=5 expected=5 (ZF clear)

   Gotchas
     - ABA: cmpxchg compares bits. If a value changes A->B->A, the CAS
       succeeds even though the history changed. Use a version tag or a
       hazard pointer / epoch scheme.
     - The memory must be naturally aligned; a misaligned locked operand
       can split a cache line and is dramatically slower (and on some CPUs
       raises #AC with alignment checking).
     - The LOCK prefix is implied by XCHG but not by CMPXCHG; forgetting it
       turns an atomic into a racy read-modify-write.
     - Do not spin forever on a uniprocessor without yielding; the holder
       cannot run.
     - This demo is single-threaded; it validates the instruction contract,
       not a contention policy.
   ============================================== */
