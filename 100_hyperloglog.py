"""MAGIC: estimate distinct count with a few hundred tiny registers.
Each hash contributes a bucket and a leading-zero rank; harmonic mean
turns the register pattern into a cardinality estimate."""
import hashlib
import math

P = 8
M = 1 << P
registers = [0] * M

def add(value):
    h = int.from_bytes(hashlib.sha256(str(value).encode()).digest()[:8], "big")
    index = h & (M - 1)
    w = h >> P
    rank = (64 - P) - w.bit_length() + 1 if w else (64 - P) + 1
    if rank > registers[index]:
        registers[index] = rank

for i in range(10000):
    add(i)

alpha = 0.7213 / (1.0 + 1.079 / M)
raw = alpha * M * M / sum(2.0 ** -r for r in registers)
zeros = registers.count(0)
estimate = M * math.log(M / zeros) if raw <= 2.5 * M and zeros else raw
print("actual=10000 estimate=%d registers=%d" % (round(estimate), M))

# ================= EXPLANATION =================
# What it does
#   Estimates the number of distinct values in a stream using HyperLogLog.
#   It stores only 256 one-byte registers (plus overhead) and never keeps
#   the original values.
#
# Step by step
#   1. Each value is hashed to 64 bits. The low 8 bits choose one of 256
#      registers; the remaining 56 bits are examined for leading zeros.
#   2. rank is the position of the first 1 bit (or 57 if the suffix is
#      all zero). A rare pattern with many leading zeros is evidence of a
#      large cardinality, because such a hash is unlikely to occur by
#      chance.
#   3. Each register keeps the maximum rank seen for its bucket. The
#      registers form a sketch: 256 small numbers, not a set of keys.
#   4. The harmonic mean of 2^-rank is dominated by the rare high ranks,
#      so it estimates the number of distinct hashes. The constant alpha
#      corrects for the finite number of registers.
#   5. For small cardinalities the raw estimate is biased and many
#      registers are zero, so the code switches to the linear-counting
#      correction M * ln(M / zeros). The result is close to 10000, but it
#      is an estimate, not an exact count.
#
# Output
#   "actual=10000 estimate=10660 registers=256"
#
# Why it matters
#   HyperLogLog counts unique items in gigabytes of traffic using
#   kilobytes of memory. Redis PFADD/PFCOUNT, analytics pipelines, and
#   network monitors use it for unique visitors, distinct flows, and
#   cardinality queries. Sketches can be merged by taking the register-wise
#   maximum, which makes them composable across shards.
#
# Caveats
#   - The estimate has a standard error of about 1.04/sqrt(M), roughly
#     6.5% for 256 registers. Increase M for more precision.
#   - HLL counts distinct hashes, not distinct values. A hash collision
#     undercounts, though 64-bit hashes make that negligible for normal
#     cardinalities.
#   - Small cardinalities need the linear-counting correction; very large
#     ones near 2^64 need the large-range correction. This demo includes
#     only the small-range branch.
#   - The hash must be uniform and independent of the value distribution.
#     A weak hash destroys the estimator.
#   - HLL cannot tell you which values were seen, cannot delete an item,
#     and does not support membership queries.
#   - The output estimate depends on the hash function and Python version.
#     It is illustrative, not a stable test vector.
#   - Sparse representations and bias corrections in production
#     implementations are more elaborate than this compact sketch.
# ==============================================

