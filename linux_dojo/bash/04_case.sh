#!/usr/bin/env bash
# MAGIC: bash 4's case-modification operators fold a string in place, with
# all-vs-first and upper-vs-lower variants.
set -euo pipefail

v='Hello World'

printf 'original=%s\n' "$v"
printf 'upper=%s\n' "${v^^}"
printf 'lower=%s\n' "${v,,}"
printf 'first_upper=%s\n' "${v^}"
printf 'first_lower=%s\n' "${v,}"
printf 'toggle_all=%s\n' "${v~~}"
printf 'toggle_first=%s\n' "${v~}"

# ================= EXPLANATION =================
# What it does
#   Applies all six bash case operators to one mixed-case string: uppercase all,
#   lowercase all, uppercase first, lowercase first, swap all, swap first.
#
# Step by step
#   1. `^^` uppercases every character; `,,` lowercases every character.
#   2. A single `^` uppercases the first character only; a single `,` lowercases
#      the first character only.
#   3. `~~` swaps the case of every character; `~` swaps the case of the first.
#   4. These are parameter-expansion operators, not commands, so they work on
#      any variable and produce a new value without modifying the original.
#   5. The optional pattern argument narrows the operation, e.g.
#      `${v^^[aeiou]}` uppercases only vowels; the demo uses the whole string.
#
# Output
#   original=Hello World
#   upper=HELLO WORLD
#   lower=hello world
#   first_upper=Hello World
#   first_lower=hello World
#   toggle_all=hELLO wORLD
#   toggle_first=hello World
#
# Why it matters
#   Case folding is a common normalization step for config keys, command
#   dispatch, and case-insensitive comparisons. Doing it in the shell avoids a
#   fork to tr and keeps the value in the same expansion pipeline as the rest of
#   the parameter operators.
#
# Caveats
#   - Bash 4.0+ only. macOS ships bash 3.2 by default, where these are syntax
#     errors; use `tr` there.
#   - Case mapping is locale-aware. In a Turkish locale, the dotted/dotless i
#     rules differ from ASCII; do not assume `I` and `i` are a pair.
#   - Multibyte characters are handled per locale, but combining marks and
#     certain scripts may not round-trip through a simple case swap.
#   - The operators do not mutate the variable; assign the result back if you
#     want to keep it: `v=${v,,}`.
#   - Quoting is still required: the result can contain spaces or glob
#     characters.
# ==============================================
