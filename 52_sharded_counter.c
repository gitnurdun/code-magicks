/* MAGIC: give each thread its own cache line, then sum the shards.
   WHY: one global counter makes every core fight over one cache line. */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#define SHARDS 4
#define ITERS  100000
struct shard {
    _Alignas(64) unsigned long long value;
    char pad[64 - sizeof(unsigned long long)];
};
static struct shard shards[SHARDS];
static void *work(void *arg)
{
    size_t i = (size_t)(uintptr_t)arg;
    for (int n = 0; n < ITERS; n++) shards[i].value++;
    return NULL;
}
int main(void)
{
    pthread_t t[SHARDS];
    for (size_t i = 0; i < SHARDS; i++)
        pthread_create(&t[i], NULL, work, (void *)(uintptr_t)i);
    unsigned long long total = 0;
    for (size_t i = 0; i < SHARDS; i++) {
        pthread_join(t[i], NULL);
        printf("shard %zu: %llu\n", i, shards[i].value);
        total += shards[i].value;
    }
    printf("total=%llu\n", total);
}

/* ================= EXPLANATION =================
   What it does
     Splits one logical counter into four independently written shards,
     each padded to a 64-byte cache line. Four threads increment their own
     shard; the main thread sums them after joining.

   Step by step
     1. Each struct shard begins with a 64-byte-aligned value and is
        padded to 64 bytes, so two shards never share a cache line on a
        typical x86-64 or ARM64 machine. (_Alignas(64) raises the
        alignment; pad makes the size a full line too.)
     2. Thread i receives its index and increments only shards[i].value.
        No atomic operation is needed: there is exactly one writer per
        object, and the join provides the happens-before edge for the
        final read.
     3. The false-sharing counterexample is one global `long counter`
        incremented by all four threads. The cache line bounces between
        cores, and each increment serialises on the line's exclusive
        ownership even though the C code has no data race (with atomics).
        Padding removes that contention.
     4. After joining, each shard is exactly ITERS = 100000, and the sum
        is 400000.

   Output
     "shard 0: 100000"
     "shard 1: 100000"
     "shard 2: 100000"
     "shard 3: 100000"
     "total=400000"

   Why it matters
     Per-CPU and per-thread counters are standard in kernels, allocators,
     network stacks, and statistics systems. They turn a contended
     read-modify-write into a private increment, then aggregate rarely.
     The same layout rule explains why adjacent hot fields in a struct can
     be slower than fields separated by padding.

   Caveats
     - The total is only a snapshot. In a live system, readers must sum
       shards while writers continue, so exact counts require care
       (atomics, seqlocks, or per-CPU reads with a stable epoch).
     - 64 bytes is the common cache-line size, but not universal (some
       CPUs use 128-byte lines or adjacent-line prefetch).
     - Padding wastes memory; do it only for hot, frequently written
       fields.
     - Sharding increases read cost and can make debugging harder.
     - This demo relies on one writer per shard; sharing a shard still
       needs atomic increments.
   ============================================== */
