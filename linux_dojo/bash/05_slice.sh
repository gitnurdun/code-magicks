#!/usr/bin/env bash
# MAGIC: `${var:offset:length}` slices a string with arithmetic offsets, and a
# leading space keeps a negative offset from looking like a default value.
set -euo pipefail

s=abcdefghij

printf 'string=%s len=%d\n' "$s" "${#s}"
printf 'mid=%s\n' "${s:2:4}"       # cdef
printf 'tail=%s\n' "${s: -3}"      # hij
printf 'neg_len=%s\n' "${s:1:-2}"  # bcdefgh
printf 'neg_both=%s\n' "${s: -4:2}" # gh
printf 'beyond=%s\n' "${s:7:99}"   # hij: clamped, no error

# `${s:-3}` is not a slice: it is the default-value operator with word "3".
printf 'not_a_slice=%s\n' "${s:-3}"

# The length operator counts characters, not bytes, in a UTF-8 locale.
utf8_locale=$(locale -a 2>/dev/null | grep -iE '^C\.utf-?8$' | head -n 1)
if [[ -n $utf8_locale ]] && LC_ALL=$utf8_locale true 2>/dev/null; then
    export LC_ALL=$utf8_locale
    u='héllo'
    printf 'utf8_chars=%d utf8_first=%s\n' "${#u}" "${u:1:1}"
else
    printf 'utf8_chars=unavailable utf8_first=unavailable\n'
fi

# ================= EXPLANATION =================
# What it does
#   Slices a ten-character string from the middle, the end, with a negative
#   length, and past the end, then shows the syntax trap and the multibyte
#   length rule.
#
# Step by step
#   1. `${s:2:4}` starts at byte/character offset 2 (zero-based) and takes four
#      characters: cdef.
#   2. `${s: -3}` takes the last three. The space is required: without it bash
#      parses `:-3` as the default-value operator.
#   3. `${s:1:-2}` starts at 1 and treats the negative length as "stop two
#      characters before the end": bcdefgh.
#   4. `${s: -4:2}` combines a negative offset with a length: gh.
#   5. `${s:7:99}` clamps the length to what remains; slicing past the end is
#      not an error and does not pad.
#   6. `${#s}` counts characters in a multibyte locale; the slice offsets are
#      also character-based. A UTF-8 é is one character but two bytes.
#
# Output
#   string=abcdefghij len=10
#   mid=cdef
#   tail=hij
#   neg_len=bcdefgh
#   neg_both=gh
#   beyond=hij
#   not_a_slice=abcdefghij
#   utf8_chars=5 utf8_first=é
#
# Why it matters
#   Substring extraction is the shell's equivalent of slicing a byte array or a
#   string view: no allocation, no fork, and it composes with the trim
#   operators. It is the right tool for parsing fixed-width records, prefixes,
#   and protocol fields in pure bash.
#
# Caveats
#   - The syntax is bash-specific. POSIX sh has no `${var:offset:length}`.
#   - Negative offsets need the space (or parentheses); `:-` is the default
#     operator. This is the single most common bug with this feature.
#   - Offsets and lengths are evaluated as arithmetic expressions, so variables
#     and expressions are allowed, but an empty or non-numeric value can be an
#     error.
#   - Under `set -u`, slicing an unset variable is an error; guard with
#     `${var:-}`.
#   - Character vs byte depends on the locale. Under `LC_ALL=C`, `héllo` is six
#     bytes and `${u:1:1}` yields the first byte of é, not the character.
#   - The result is a value, not a view: assigning it copies in bash's model.
# ==============================================
