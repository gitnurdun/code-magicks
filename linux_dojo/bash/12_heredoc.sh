#!/usr/bin/env bash
# MAGIC: a quoted heredoc delimiter disables expansion, `<<-` strips leading
# tabs, and `<<<` adds exactly one newline.
set -euo pipefail

name=world

printf 'expanded:\n'
cat <<EOF
hello $name
sum=$(( 2 + 3 ))
EOF

printf 'literal:\n'
cat <<'EOF'
hello $name
sum=$(( 2 + 3 ))
EOF

printf 'tab_stripped:\n'
cat <<-EOF
	indented line
	EOF

read -r a b <<< "x y"
printf 'herestring=<%s><%s>\n' "$a" "$b"

# ================= EXPLANATION =================
# What it does
#   Prints the same heredoc twice, once with an unquoted delimiter (expanded)
#   and once with a quoted delimiter (literal). It then shows tab stripping and
#   a here-string feeding read.
#
# Step by step
#   1. `<<EOF` means "read until a line containing only EOF". Because the
#      delimiter is unquoted, parameter expansion, command substitution, and
#      arithmetic expansion all run on the body.
#   2. `<<'EOF'` quotes the delimiter, which turns off all expansion. The body
#      is passed byte for byte, which is why shell code in a heredoc is usually
#      written this way.
#   3. `<<-EOF` strips leading tab characters from every line of the body and
#      from the delimiter. Spaces are not stripped; the indentation must be
#      tabs.
#   4. `<<< "x y"` is a here-string. It supplies the text plus one newline on
#      stdin to read, which splits it into a and b. The newline is not part of
#      either variable.
#   5. The output labels make the expanded/literal difference visible without
#      relying on quoting subtleties in the terminal.
#
# Output
#   expanded:
#   hello world
#   sum=5
#   literal:
#   hello $name
#   sum=$(( 2 + 3 ))
#   tab_stripped:
#   indented line
#   herestring=<x><y>
#
# Why it matters
#   Heredocs are how shell scripts embed multi-line configuration, SQL, Python,
#   and remote command bodies without escaping every quote. The quoted
#   delimiter is the safe default when the content is code or data; the
#   unquoted form is a small template engine. `<<-` makes indented scripts
#   readable without changing the payload.
#
# Caveats
#   - The delimiter must appear alone on its line (leading tabs are allowed
#     with `<<-`), with no trailing spaces.
#   - Unquoted heredocs still perform word splitting? No: expansion happens,
#     but the result is not split or globbed in the heredoc body. Command
#     substitution can still produce multiple lines.
#   - A quoted delimiter cannot contain the quote characters as part of the
#     terminator; use a distinctive word.
#   - `<<-` strips tabs only. Mixing spaces and tabs is the usual reason it
#     "does not work".
#   - Here-strings are bash/ksh/zsh; POSIX sh uses a pipe or a heredoc.
#   - A heredoc inside `$(...)` is parsed by the outer shell in some contexts;
#     parenthesise or use a function when the nesting gets deep.
# ==============================================
