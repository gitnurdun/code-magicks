#!/usr/bin/env bash
# MAGIC: `${!name}` expands the variable whose name is stored in `name`, and
# `declare -n` makes that indirection a typed alias.
set -euo pipefail

target=42
name=target

printf 'direct=%s\n' "$target"
printf 'indirect=%s\n' "${!name}"

# Indirect expansion treats the value as a parameter name. Validate it first:
# arbitrary text is not a legal name and can be a code-injection vector if it
# reaches eval later.
candidate='target'
if [[ $candidate =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
    printf 'validated=%s\n' "${!candidate}"
else
    printf 'validated=rejected\n'
fi

bad='x; echo injected'
if [[ $bad =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
    printf 'bad=accepted\n'
else
    printf 'bad=rejected\n'
fi

declare -n ref=target
ref=7
printf 'nameref_target=%s ref=%s\n' "$target" "$ref"

declare -a items=(zero one two)
declare -n elem='items[2]'
printf 'nameref_array=%s\n' "$elem"

# ================= EXPLANATION =================
# What it does
#   Reads a variable through another variable holding its name, validates the
#   name before expanding it, then uses a nameref to write through an alias and
#   to address one array element.
#
# Step by step
#   1. `name=target; ${!name}` expands to the value of target, 42. The `!`
#      prefix means "indirect": use the value of name as the parameter name.
#   2. The regex guard accepts only a legal identifier. Without it, a value like
#      `x; ...` is not a valid parameter name; bash reports an error, and if the
#      string is later passed to eval the consequences are worse.
#   3. `declare -n ref=target` makes ref a nameref. Reading or writing ref reads
#      or writes target; `ref=7` changes target.
#   4. A nameref can name an array element: `declare -n elem='items[2]'` then
#      `$elem` is two. This is the clean way to pass an array element by
#      reference to a function.
#   5. The original target is printed again to prove the write went through.
#
# Output
#   direct=42
#   indirect=42
#   validated=42
#   bad=rejected
#   nameref_target=7 ref=7
#   nameref_array=two
#
# Why it matters
#   Indirection is how a shell implements pass-by-reference, dynamic dispatch
#   over configuration keys, and "set the variable named by this argument"
#   interfaces. Namerefs make that explicit and scope-aware, which is much safer
#   than the old `eval "$name=value"` pattern.
#
# Caveats
#   - Bash 4.3+ for `declare -n`. Bash 4.0-4.2 have `${!name}` but no namerefs.
#   - `${!name}` only works with a legal parameter name (or an array subscript
#     in some versions); it is not a general expression evaluator.
#   - Name collisions are real: a nameref whose name matches its target creates
#     a self-reference and bash errors out. Use distinct local names.
#   - `local -n` inside a function is the safe form; a global nameref can leak
#     across calls and surprise the next function.
#   - Validation is mandatory before any indirect expansion of untrusted input.
#     Even without eval, an attacker who controls the name can read secrets from
#     the environment.
#   - `${!prefix*}` and `${!prefix@}` are a different feature: they list
#     variable names that start with a prefix.
# ==============================================
