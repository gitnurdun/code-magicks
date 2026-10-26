#!/usr/bin/env bash
# MAGIC: printf %q emits shell-safe syntax, and one printf call can format hex, bytes, time and padded columns.
set -euo pipefail
export TZ=UTC0
export LC_ALL=C

# 1. %q quotes spaces, quotes and newlines as reusable shell input.
for s in 'hello world' "it's \"quoted\"" $'line1\nline2'; do
  printf 'q=%q\n' "$s"
done

# 2. Controlled round trip: only the generated %q text reaches eval.
original=$'space\ttab\nquote \' end'
safe=$(printf '%q' "$original")
printf 'safe=%s\n' "$safe"
eval "restored=$safe"
if [[ $restored == "$original" ]]; then printf 'roundtrip=ok\n'; else printf 'roundtrip=bad\n'; fi

# 3. printf -v stores instead of printing; %b expands escapes.
printf -v h '%x' 48879
printf -v o '%o' 48879
printf -v b '%b' 'A\tB\x43\0101'
printf 'hex=%s oct=%s\n' "$h" "$o"
printf 'b=%q\n' "$b"

# 4. %(...)T formats epoch seconds; TZ=UTC0 makes it stable.
epoch=1700000000
printf 'time=%(%F %T)T\n' "$epoch"

# 5. Width, precision, sign and zero padding.
printf 'str=[%10s][%-10s][%.3s]\n' left left abcdef
printf 'num=[%08d][%+.2f]\n' 42 3.14159

# ================= EXPLANATION =================
# What it does
#   Uses bash's printf as both a formatter and a quoting engine: %q
#   renders a string as shell input, -v stores a result, %x/%o/%b
#   convert numbers and escapes, and %(fmt)T formats an epoch.
#
# Step by step
#   1. %q escapes spaces, single and double quotes, and switches to
#      $'...' ANSI-C quoting when the value contains control bytes.
#   2. original is built in the script, converted with %q, and eval'd
#      as one assignment. Only that generated text is evaluated.
#   3. printf -v writes into a variable instead of standard output.
#   4. 48879 is 0xbeef and 0o137357. %b expands \t, \x43 and \0101,
#      unlike %s, which would print the backslashes literally.
#   5. TZ=UTC0 pins the calendar used by %(...)T. The argument is
#      seconds since the Unix epoch; -1 means now and -2 means shell
#      start, but fixed inputs keep this demo reproducible.
#   6. The last two lines show right/left padding, truncation with
#      %.3s, zero padding, an explicit sign and %.2f rounding.
#
# Output
#   q=hello\ world
#   q=it\'s\ \"quoted\"
#   q=$'line1\nline2'
#   safe=$'space\ttab\nquote \' end'
#   roundtrip=ok
#   hex=beef oct=137357
#   b=$'A\tBCA'
#   time=2023-11-14 22:13:20
#   str=[      left][left      ][abc]
#   num=[00000042][+3.14]
#
# Why it matters
#   Generating a command line that survives spaces, quotes and
#   newlines is a recurring shell problem. %q produces one word that
#   the shell parses back to the same bytes, and -v avoids a fork or
#   an extra command substitution when building strings.
#
# Caveats
#   - %q is a bash builtin extension, not POSIX printf; ksh, zsh and
#     /usr/bin/printf may not support it or may quote differently. The
#     $'...' form it emits is not valid POSIX sh either.
#   - eval is still code execution. %q protects syntax for a single
#     word; never eval text from an untrusted source just because it
#     looks quoted.
#   - %(...)T requires bash 4.2+. TZ and locale affect calendar and
#     day/month names; pin both for reproducible output.
#   - %b performs backslash expansion on the argument; do not use
#     untrusted data as the format string.
#   - Bash arithmetic is integer-only, and printf numeric arguments
#     can overflow or be interpreted with base prefixes; check ranges.
#   - Width counts bytes as bash implements it, so multibyte text can
#     be padded to the wrong display width.
# ==============================================
