#!/usr/bin/env bash
# MAGIC: `"${a[@]}"` is one word per element, `"${a[*]}"` is one word joined by
# IFS, and `@Q` renders a value as a reusable shell word.
set -euo pipefail

a=(alpha beta gamma)
a+=(delta)

printf 'count=%d\n' "${#a[@]}"
printf 'indexes=%s\n' "${!a[@]}"
printf 'slice=%s\n' "$(IFS=' '; printf '%s' "${a[*]:1:2}")"
printf 'quoted_words='; printf '<%s>' "${a[@]}"; printf '\n'

IFS=:
star="${a[*]}"
printf 'star=%s\n' "$star"
unset IFS

printf 'first_q=%s\n' "${a[0]@Q}"
printf 'all_q='; printf '%s ' "${a[@]@Q}"; printf '\n'

# ================= EXPLANATION =================
# What it does
#   Builds an indexed array, appends to it, prints its length, indexes, a slice,
#   the per-element quoted form, the IFS-joined form, and the `@Q` shell-quoted
#   rendering.
#
# Step by step
#   1. `a=(alpha beta gamma)` creates a dense array with indexes 0-2;
#      `a+=(delta)` appends at index 3.
#   2. `${#a[@]}` is the element count. `${!a[@]}` expands to the index list,
#      which is 0 1 2 3 for a dense array.
#   3. `"${a[@]:1:2}"` slices two elements starting at index 1. Quoting the `@`
#      form preserves one word per element; the subshell joins them for display.
#   4. `"${a[@]}"` passed to printf produces one argument per element, so the
#      format repeats: <alpha><beta><gamma><delta>.
#   5. `"${a[*]}"` joins all elements into one word using the first character
#      of IFS. With IFS=: the result is alpha:beta:gamma:delta. Unquoted, the
#      elements would be split again on the current IFS.
#   6. `${a[0]@Q}` and `"${a[@]@Q}"` quote values so the output could be fed
#      back to the shell; simple words become 'alpha', etc.
#
# Output
#   count=4
#   indexes=0 1 2 3
#   slice=beta gamma
#   quoted_words=<alpha><beta><gamma><delta>
#   star=alpha:beta:gamma:delta
#   first_q='alpha'
#   all_q='alpha' 'beta' 'gamma' 'delta'
#
# Why it matters
#   The `@`/`*` distinction is the difference between "an array of arguments"
#   and "one string containing separators". Most array bugs in shell are a
#   missing quote or the wrong sigil. `@Q` is the safe way to print a value that
#   a human or another script can re-enter, and it is invaluable in `set -x`
#   style diagnostics.
#
# Caveats
#   - `"${a[@]}"` is not the same as `${a[@]}`. Unquoted, each element is
#     subject to word splitting and pathname expansion.
#   - `"${a[*]}"` uses only the first character of IFS; a multibyte IFS
#     character is not supported as a separator.
#   - Indexed arrays can be sparse. `${!a[@]}` then lists the present indexes,
#     not 0..n-1, and `${#a[@]}` is the count of present elements.
#   - Negative indexes (a[-1]) are bash 4.3+ and have their own pitfalls; the
#     `@` slice with a negative offset needs the space, as in `${a[@]: -2}`.
#   - `@Q` output is bash syntax, not POSIX sh syntax. It quotes for bash
#     re-entry, not for every shell.
#   - Associative arrays use the same `@`/`*` syntax but have unspecified key
#     order; sort the keys when order matters.
# ==============================================
