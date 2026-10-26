#!/usr/bin/awk -f
# MAGIC: awk compares "10" and "9" as strings until one side is forced numeric;
# a strnum from input is the exception, and every number is an IEEE double.

function csv_split(s, out,    i, c, field, n, q) {
    n = 0
    field = ""
    q = 0
    for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (q) {
            if (c == "\"") {
                if (substr(s, i + 1, 1) == "\"") {
                    field = field "\""
                    i++
                } else {
                    q = 0
                }
            } else {
                field = field c
            }
        } else if (c == "\"") {
            q = 1
        } else if (c == ",") {
            out[++n] = field
            field = ""
        } else {
            field = field c
        }
    }
    out[++n] = field
    return n
}

BEGIN {
    x = "10"
    y = "9"
    print "string_cmp=" (x < y)          # lexical: "1" < "9"
    print "numeric_cmp=" ((x + 0) < (y + 0))

    n = csv_split("alice,\"Smith, Alice\",42", f)
    print "csv_fields=" n
    for (i = 1; i <= n; i++) {
        print "csv[" i "]=" f[i]
    }

    # All awk numbers are doubles (POSIX); 2^53+1 is not representable.
    big = 9007199254740993
    printf "big=%.0f\n", big
    printf "pi=%.2f\n", 3.14159

    # CONVFMT controls number -> string conversion in concatenation.
    CONVFMT = "%.2f"
    print "convfmt=" (1 / 3)
}

# ================= EXPLANATION =================
# What it does
#   Shows awk's two-type value model: a value can be a string, a number, or a
#   "strnum" that remembers it came from input and looks numeric. It also
#   parses a quoted CSV row and demonstrates double-precision rounding.
#
# Step by step
#   1. x and y are assigned from string literals, so `<` is a string compare.
#      "10" < "9" is true because the first character '1' is less than '9'.
#   2. Adding 0 forces both operands numeric, so 10 < 9 is false.
#   3. csv_split scans characters. A quote toggles quoted mode; "" inside a
#      quoted field becomes a literal quote; a comma outside quotes ends the
#      field. The third field "42" stays a string in the array.
#   4. 9007199254740993 is rounded to the nearest double, 9007199254740992.
#      The %.2f CONVFMT conversion applies to the 1/3 concatenation.
#
# Output
#   string_cmp=1
#   numeric_cmp=0
#   csv_fields=3
#   csv[1]=alice
#   csv[2]=Smith, Alice
#   csv[3]=42
#   big=9007199254740992
#   pi=3.14
#   convfmt=0.33
#
# Why it matters
#   Most awk surprises are type surprises: a number read from a file is a
#   strnum and compares numerically, while the same digits in a string literal
#   compare lexically. Knowing the rule prevents "why is 100 less than 20?"
#   bugs in log processing. The double limit matters for IDs, nanosecond
#   timestamps, and money; use strings or scaled integers for those.
#
# Caveats
#   - POSIX awk numbers are doubles. gawk can use MPFR with -M, but that is a
#     non-standard extension and still not automatic.
#   - The CSV parser is a teaching subset: it does not handle embedded
#     newlines inside quoted fields, and it treats a quote in the middle of an
#     unquoted field as an error rather than preserving it.
#   - CONVFMT affects conversion in concatenation and array subscripts, not
#     printf; OFMT affects print of numbers. Both can silently round.
#   - Numeric strings with leading zeros ("08") are still numbers in numeric
#     context; do not use them as opaque IDs.
#   - Comparing a strnum to a string may force a string comparison depending
#     on the other operand's type; convert explicitly when it matters.
# ==============================================
