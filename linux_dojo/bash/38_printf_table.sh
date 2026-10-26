#!/usr/bin/env bash
# MAGIC: printf's '*' width takes the column size from an argument, so column widths can be computed from the data.
set -euo pipefail

headers=(service status rss_kb)
names=(api worker cache)
states=(up degraded down)
rss=(18432 2096 512)

w0=${#headers[0]}
w1=${#headers[1]}
w2=${#headers[2]}

for i in "${!names[@]}"; do
  w0=$(( ${#names[i]} > w0 ? ${#names[i]} : w0 ))
  w1=$(( ${#states[i]} > w1 ? ${#states[i]} : w1 ))
  w2=$(( ${#rss[i]} > w2 ? ${#rss[i]} : w2 ))
done

total=$((w0 + w1 + w2 + 2))
printf -v rule '%*s' "$total" ''
rule=${rule// /-}

printf '%-*s %-*s %*s\n' "$w0" "${headers[0]}" "$w1" "${headers[1]}" "$w2" "${headers[2]}"
printf '%s\n' "$rule"
for i in "${!names[@]}"; do
  printf '%-*s %-*s %*s\n' "$w0" "${names[i]}" "$w1" "${states[i]}" "$w2" "${rss[i]}"
done
printf '%s\n' "$rule"

# ================= EXPLANATION =================
# What it does
#   Builds a three-row table whose column widths are derived from the
#   headers and the data at runtime, then prints it with a separator
#   that matches the computed total width.
#
# Step by step
#   1. w0, w1 and w2 start as the header lengths using ${#string}.
#   2. The loop widens each column to the longest value in that column.
#      The text columns are left-aligned and the numeric rss_kb column
#      is right-aligned so magnitudes line up.
#   3. printf's %-*s consumes an explicit width argument before the
#      string: '-' means left-justify, %*s without '-' means
#      right-justify. The single space between format specifiers is the
#      column separator.
#   4. printf -v rule '%*s' "$total" '' stores a string of total spaces
#      in rule instead of printing it; ${rule// /-} turns those spaces
#      into a rule of dashes.
#   5. The same format string is used for every row, so adding a row
#      needs only data, not a new set of numbers.
#
# Output
#   service status   rss_kb
#   -----------------------
#   api     up        18432
#   worker  degraded   2096
#   cache   down        512
#   -----------------------
#
# Why it matters
#   Hard-coded padding breaks as soon as a hostname, container name or
#   latency grows. Computing widths from the data keeps CLIs readable
#   without pulling in awk or a real table library, and %*s makes the
#   padding part of the format rather than a second loop.
#
# Caveats
#   - Dynamic '*' width is a bash builtin printf feature here (GNU
#     printf also supports it), but the POSIX printf utility does not
#     require it.
#   - ${#value} counts characters, while printf pads by bytes in bash;
#     non-ASCII text and double-width glyphs will misalign. There is no
#     portable terminal-cell width in printf.
#   - This table assumes ASCII and a fixed-width font. It does not wrap
#     long values or consult COLUMNS/tput.
#   - '%*s' on a number right-aligns its text; use %*d for integers and
#     %*.*f for floats if numeric formatting matters.
#   - printf -v needs bash 3.1+ and is not POSIX sh.
#   - The rule is recomputed only when the data changes; if rows are
#     appended at runtime, recompute the widths before printing again.
# ==============================================
