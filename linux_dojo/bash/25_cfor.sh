#!/usr/bin/env bash
# MAGIC: the C-style for header packs init, test, and update into one
# MAGIC: arithmetic expression, and break/continue accept a nesting level.
set -euo pipefail

printf 'converge:\n'
for ((i = 0, j = 10; i < j; i++, j--)); do
    printf '  i=%d j=%d\n' "$i" "$j"
done

printf 'skip evens:\n'
for ((i = 0; i <= 6; i++)); do
    if (( i % 2 == 0 )); then
        continue
    fi
    printf '  odd=%d\n' "$i"
done

printf 'triangle:\n'
for ((row = 1; row <= 3; row++)); do
    printf '  '
    for ((col = 1; col <= row; col++)); do
        if (( col > 1 )); then
            printf ' '
        fi
        printf '%d' "$col"
    done
    printf '\n'
done

arr=(alpha beta gamma)
printf 'array by index:\n'
for ((idx = 0; idx < ${#arr[@]}; idx++)); do
    printf '  %d=%s\n' "$idx" "${arr[idx]}"
done

printf 'break 2:\n'
for ((a = 1; a <= 3; a++)); do
    for ((b = 1; b <= 3; b++)); do
        if (( a == 2 && b == 2 )); then
            printf '  stop at a=%d b=%d\n' "$a" "$b"
            break 2
        fi
        printf '  a=%d b=%d\n' "$a" "$b"
    done
done

# ================= EXPLANATION =================
# What it does
#   Prints a small set of tables built with bash's C-style for loop: two
#   counters moving toward each other, continue to skip even values, a nested
#   triangle, an array walked by index, and break 2 to leave two loops at once.
#
# Step by step
#   1. `for ((init; test; update))` evaluates three arithmetic expressions.
#      The comma in `i = 0, j = 10` and in `i++, j--` sequences them. The loop
#      stops when i < j becomes false, which happens at the midpoint.
#   2. continue skips the rest of that iteration and runs the update
#      expression, so even values print nothing and the loop still advances.
#   3. The nested triangle uses the inner loop bound as row; the conditional
#      space keeps the output column-aligned without trailing blanks.
#   4. `${#arr[@]}` is the element count, and `${arr[idx]}` evaluates idx as an
#      arithmetic subscript. No `$` is needed around idx inside the brackets.
#   5. break 2 leaves both the inner and outer loop immediately. The print
#      before it records the last pair reached; no further pairs appear.
#
# Output
#   converge:
#     i=0 j=10
#     i=1 j=9
#     i=2 j=8
#     i=3 j=7
#     i=4 j=6
#   skip evens:
#     odd=1
#     odd=3
#     odd=5
#   triangle:
#     1
#     1 2
#     1 2 3
#   array by index:
#     0=alpha
#     1=beta
#     2=gamma
#   break 2:
#     a=1 b=1
#     a=1 b=2
#     a=1 b=3
#     a=2 b=1
#     stop at a=2 b=2
#
# Why it matters
#   Counting loops with independent index and bound variables are awkward with
#   while and impossible with `for x in list`. The C-style form keeps init,
#   invariant, and update on one line, which makes the termination condition
#   auditable, and it works naturally with array indices.
#
# Caveats
#   - This is a bash/ksh extension, not POSIX sh. `for ((...))` needs bash.
#   - All three expressions are arithmetic context: `$` is optional, strings
#     are coerced, and a leading zero is octal. Bad input can silently become
#     zero or cause a division error.
#   - An omitted test is an infinite loop: `for ((;;))`. An omitted update is
#     allowed but usually a bug; use while for deliberate infinite loops.
#   - The update expressions run after continue as well as after a normal body,
#     so continue cannot create an infinite loop unless the update is missing.
#   - `break N` counts loops outward from the innermost. `break 0` is an error,
#     and break/continue outside a loop are errors.
#   - Counters are fixed-width integers; a loop near 2^63 wraps. For huge
#     ranges, use a tool that does arbitrary precision or restructure the work.
# ==============================================
