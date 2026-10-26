/* MAGIC: test membership with k bits instead of storing every key. False
   positives are possible; false negatives are not. */
#include <stdint.h>
#include <stdio.h>
#define BITS 64
static uint64_t filter;
static uint32_t h1(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}
static uint32_t h2(const char *s)
{
    uint32_t h = 0x9E3779B9u;
    while (*s) { h ^= (unsigned char)*s++; h = (h << 5) | (h >> 27); h *= 0x85EBCA6Bu; }
    return h;
}
static void add(const char *s)
{
    uint32_t a = h1(s), b = h2(s);
    for (int i = 0; i < 3; i++) {
        unsigned bit = (a + (uint32_t)i * b + (uint32_t)i * i) % BITS;
        filter |= 1ull << bit;
    }
}
static int has(const char *s)
{
    uint32_t a = h1(s), b = h2(s);
    for (int i = 0; i < 3; i++) {
        unsigned bit = (a + (uint32_t)i * b + (uint32_t)i * i) % BITS;
        if (!(filter & (1ull << bit))) return 0;
    }
    return 1;
}
int main(void)
{
    add("apple"); add("banana"); add("cherry");
    printf("apple=%d banana=%d cherry=%d grape=%d\n",
           has("apple"), has("banana"), has("cherry"), has("grape"));
}

/* ================= EXPLANATION =================
   What it does
     Implements a 64-bit Bloom filter with three hash-derived bit
     positions per key. Inserting a key sets those bits; a query checks
     them. If any bit is zero, the key is definitely absent; if all are
     one, the key is probably present.

   Step by step
     1. h1 is FNV-1a, a fast byte hash. h2 is a second, independent mix
        (rotate and multiply). Double hashing derives three positions from
        a and b: a, a+b+1, a+2b+4, modulo 64.
     2. add("apple") sets three bits; banana and cherry set theirs. Bits
        may overlap, which is expected.
     3. has("apple") recomputes the same three positions and finds them
        all set, so it returns 1. The same holds for the other inserted
        keys.
     4. has("grape") finds at least one zero bit, so grape was never
        inserted (no false negatives).
     5. The filter is only 8 bytes. It stores no keys, so it cannot list
        them or delete one safely; clearing bits could break another key
        that shares them.

   Output
     "apple=1 banana=1 cherry=1 grape=0"

   Why it matters
     Bloom filters answer "have I seen this?" with tiny memory and O(k)
     time. They are used to avoid disk lookups in databases (LSM trees),
     block malicious URLs, deduplicate cache fills, and reduce network
     requests. The trade-off is controlled by the bit count and k: more
     bits and more hashes lower the false-positive rate, up to a point.

   Caveats
     - False positives are possible. Callers must verify the key in the
       real data store before trusting membership.
     - Standard Bloom filters cannot delete. Counting Bloom filters use
       counters; cuckoo filters support deletion and better locality.
     - Too many insertions saturate the bits and the false-positive rate
       approaches 1. Size the filter for the expected n and target rate.
     - Hash quality matters. Correlated hashes create clustering and more
       collisions than the math predicts.
     - You cannot enumerate keys or recover them; the filter is not a
       set.
     - Concurrent updates need atomics or partitioning; a torn 64-bit
       update can corrupt bits.
   ============================================== */
