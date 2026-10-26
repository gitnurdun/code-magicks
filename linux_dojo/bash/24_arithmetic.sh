#!/usr/bin/env bash
# MAGIC: bash arithmetic is fixed-width intmax_t: C operators, base prefixes,
# MAGIC: and two's-complement wraparound instead of arbitrary precision.
set -euo pipefail

(( b = 2#1010 ))
printf '2#1010 = %d\n' "$b"
(( h = 16#ff ))
printf '16#ff = %d\n' "$h"
(( o = 10#08 ))
printf '10#08 = %d\n' "$o"
(( t = 5 > 3 ? 100 : 200 ))
printf 'ternary 5>3 = %d\n' "$t"
(( c = (1, 2, 3) ))
printf 'comma (1,2,3) = %d\n' "$c"
(( p = 2**10 ))
printf '2**10 = %d\n' "$p"
(( q = -7 / 2 ))
(( r = -7 % 2 ))
printf '%s\n' "-7/2 = $q (truncated toward zero), -7%2 = $r"
(( x = 2**63 - 1 ))
printf 'x = 2**63-1 = %d\n' "$x"
(( x++ ))
printf 'x++ = %d\n' "$x"

# ================= EXPLANATION =================
# What it does
#   Evaluates the arithmetic operators that matter in shell scripts: explicit
#   bases, the ternary and comma operators, exponentiation, signed division and
#   remainder, and the 64-bit limit. The last two lines overflow INT64_MAX and
#   show the result wrapping negative.
#
# Step by step
#   1. In arithmetic context, 2#1010, 16#ff, and 10#08 select base 2, 16, and
#      10. The last one is necessary because a leading 0 alone means octal, and
#      08 is not a valid octal digit.
#   2. ?: and the comma operator follow C precedence. (1, 2, 3) evaluates left
#      to right and yields 3, its last expression.
#   3. ** is exponentiation, right-associative and higher precedence than
#      multiplication. 2**10 is 1024.
#   4. Integer division truncates toward zero: -7/2 is -3, not -4. The
#      remainder takes the sign of the dividend, so -7%2 is -1.
#   5. (( x = 2**63 - 1 )) lands exactly on INT64_MAX. `(( x++ ))` adds one and
#      wraps to INT64_MIN, -9223372036854775808. This is fixed-width
#      two's-complement behavior, not bignum arithmetic.
#
# Output
#   2#1010 = 10
#   16#ff = 255
#   10#08 = 8
#   ternary 5>3 = 100
#   comma (1,2,3) = 3
#   2**10 = 1024
#   -7/2 = -3 (truncated toward zero), -7%2 = -1
#   x = 2**63-1 = 9223372036854775807
#   x++ = -9223372036854775808
#
# Why it matters
#   Loops, sizes, timestamps, and bit masks all live in arithmetic context.
#   Knowing that it is C-like integer math with a hard 64-bit ceiling prevents
#   silent corruption when computing byte counts, hash values, or deadlines
#   near the 2^63 boundary.
#
# Caveats
#   - bash uses intmax_t from the C implementation, typically 64-bit signed on
#     Linux. It is not arbitrary precision; bc, awk, or a real language is
#     needed beyond that range. Overflow is not diagnosed.
#   - `(( expr ))` returns status 0 when expr is non-zero and 1 when it is zero.
#     Under set -e, a legitimate result of zero aborts the script unless the
#     expression is in a tested context.
#   - A leading zero means octal, so 08 and 09 are invalid. Use 10# for
#     decimal, 16# for hex, 2# for binary, and bases up to 64 with base#.
#   - Division or remainder by zero is an arithmetic error and can terminate a
#     non-interactive shell. Validate divisors.
#   - Variables are strings until arithmetic touches them; a malformed value is
#     treated as a variable name (usually 0) rather than rejected. Do not feed
#     untrusted text to (( )).
# ==============================================
