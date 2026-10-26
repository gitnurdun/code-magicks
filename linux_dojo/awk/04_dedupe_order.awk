#!/usr/bin/awk -f
# MAGIC: `!seen[$0]++` deduplicates while preserving first-seen order, because
# the test happens before the increment; a separate array keeps the order.

BEGIN {
    n = split("pear apple pear banana apple cherry", w, " ")

    for (i = 1; i <= n; i++) {
        if (!(w[i] in seen)) {
            order[++u] = w[i]
        }
        seen[w[i]]++
    }

    print "unique=" u
    for (i = 1; i <= u; i++) {
        printf "first %s %d\n", order[i], seen[order[i]]
    }

    # Hash iteration order is unspecified, so collect and sort by count
    # descending, then by name ascending. Insertion sort is fine for small n.
    for (k in seen) {
        keys[++m] = k
    }
    for (i = 2; i <= m; i++) {
        key = keys[i]
        j = i - 1
        while (j > 0 && (seen[keys[j]] < seen[key] ||
                         (seen[keys[j]] == seen[key] && keys[j] > key))) {
            keys[j + 1] = keys[j]
            j--
        }
        keys[j + 1] = key
    }
    for (i = 1; i <= m; i++) {
        printf "rank %s %d\n", keys[i], seen[keys[i]]
    }
}

# ================= EXPLANATION =================
# What it does
#   Deduplicates a word list, prints the words in first-seen order, then prints
#   a ranking by frequency (count descending, name ascending).
#
# Step by step
#   1. split() turns the embedded sentence into n words.
#   2. `!(w[i] in seen)` is true only the first time a word appears. The
#      increment still runs every time, so seen[word] ends up as its count.
#   3. The first-seen order is recorded separately, because iterating `seen`
#      would give hash order, not input order.
#   4. A rank array is filled from the hash keys and insertion-sorted with a
#      two-key comparator: higher count first, then lexicographically smaller
#      name.
#
# Real-world form
#   awk '!seen[$0]++' file                 # dedupe, preserve order
#   awk '{c[$0]++} END {for (k in c) print c[k], k}' file | sort -rn
#
# Output
#   unique=4
#   first pear 2
#   first apple 2
#   first banana 1
#   first cherry 1
#   rank apple 2
#   rank pear 2
#   rank banana 1
#   rank cherry 1
#
# Why it matters
#   `!seen[$0]++` is the smallest useful awk idiom: one hash, one post-
#   increment, no external sort. It shows how awk's arrays are associative and
#   why the position of the increment matters. The two-array pattern (seen +
#   order) is the standard way to combine O(1) membership with stable output.
#
# Caveats
#   - The whole set must fit in memory; this is not an external-memory dedupe.
#   - Hash iteration order is unspecified in POSIX awk. Sort keys if the output
#     must be reproducible; gawk's PROCINFO["sorted_in"] is a gawk extension.
#   - `seen[$0]` uses the string value of the record. With a numeric-looking
#     key, "01" and "1" are different strings, and a numeric value can be
#     converted with CONVFMT, so normalise keys deliberately.
#   - Insertion sort is O(m^2). For large cardinalities pipe through sort(1).
#   - The input sentence is split on a single space, not general whitespace.
#     Use split(s, w, /[ \t]+/) or set FS for real text.
# ==============================================
