#!/usr/bin/env bash
# MAGIC: readlink prints one link hop; readlink -f / realpath resolve the whole
# MAGIC: chain, and the -e variant is the one that rejects a dangling target.
set -euo pipefail

for t in readlink realpath ln basename; do
    command -v "$t" >/dev/null 2>&1 || { printf 'skipped: %s not available\n' "$t"; exit 0; }
done

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

mkdir -p "$work/dir"
printf 'payload\n' > "$work/dir/target"
ln -s b "$work/a"
ln -s dir/target "$work/b"

printf 'readlink_one_hop=%s\n' "$(readlink "$work/a")"

resolved=$(readlink -f "$work/a")
printf 'readlink_f_relative=%s\n' "${resolved#"$work"/}"
printf 'resolved_basename=%s\n' "$(basename "$resolved")"

if [[ "$resolved" == "$(realpath "$work/a")" ]]; then
    printf 'realpath_agrees=yes\n'
else
    printf 'realpath_agrees=no\n'
fi

printf 'realpath_relative_to_work=%s\n' "$(realpath --relative-to="$work" "$work/a")"

ln -s missing "$work/dangle"
ln -s nodir/leaf "$work/deep"

# -e requires every component, including the final target, to exist.
if readlink -e "$work/dangle" >/dev/null 2>&1; then
    printf 'dangling_e_status=0\n'
else
    printf 'dangling_e_status=%s\n' "$?"
fi

# -f resolves all but the last component; a missing middle component fails.
if readlink -f "$work/deep" >/dev/null 2>&1; then
    printf 'dangling_mid_f_status=0\n'
else
    printf 'dangling_mid_f_status=%s\n' "$?"
fi

# A dangling final target is allowed by -f, so basename still returns a name.
printf 'dangling_final_f_basename=%s\n' "$(basename "$(readlink -f "$work/dangle")")"

# ================= EXPLANATION =================
# What it does
#   Builds a two-hop symlink chain (a -> b -> dir/target) and shows the
#   difference between reading one link and canonicalising the chain. It also
#   shows relative canonicalisation and the two dangling-link cases: a missing
#   final target, and a missing intermediate directory.
#
# Step by step
#   1. readlink <path> prints exactly the stored target of that one symlink and
#      stops. For a -> b it prints "b"; it does not follow b.
#   2. readlink -f and realpath follow every symlink component, collapse ".", ".."
#      and duplicate slashes, and print an absolute path. Both reach
#      <work>/dir/target here, so their strings match.
#   3. realpath --relative-to=<work> removes the <work> prefix after resolving,
#      yielding "dir/target". This is GNU coreutils; it is not the same as a
#      textual prefix strip when ".." or alternate mounts are involved.
#   4. dangle -> missing has a missing final component. readlink -f resolves all
#      but the last component, so it succeeds and prints <work>/missing.
#      readlink -e requires every component to exist, so it fails with status 1.
#   5. deep -> nodir/leaf has a missing non-final component. readlink -f must
#      traverse nodir to continue, so it fails with status 1 as well.
#   6. basename of the -f result for dangle is "missing": -f returned a path,
#      not proof that the file exists.
#
# Output
#   readlink_one_hop=b
#   readlink_f_relative=dir/target
#   resolved_basename=target
#   realpath_agrees=yes
#   realpath_relative_to_work=dir/target
#   dangling_e_status=1
#   dangling_mid_f_status=1
#   dangling_final_f_basename=missing
#
# Why it matters
#   Symlink resolution is a security boundary. A canonical path is what you
#   check against an allow-list, and a one-hop readlink is how you inspect what
#   an attacker-controlled link actually stores. Mixing the two up is the root
#   of many path-traversal and symlink-race bugs.
#
# Caveats
#   - readlink without -f/-e/-m only reads a link; on a non-symlink it prints
#     nothing and exits 1. Do not use it to get an absolute path.
#   - readlink -f requires all but the last component to exist; readlink -e
#     requires all of them; readlink -m requires none. The GNU manual is explicit
#     about this, and scripts routinely assume -f means "must exist".
#   - realpath follows symlinks and requires all but the last component by
#     default; realpath -e is the strict form. --relative-to is GNU-specific.
#   - A symlink can change between resolution and use (TOCTOU). For privileged
#     operations prefer openat2(RESOLVE_*) or dirfd-relative calls.
#   - Symlink loops fail with ELOOP; permission-denied directories fail even if
#     the final target exists.
#   - Hard links are not visible to any of this: they are multiple names for the
#     same inode, not pointers.
# ==============================================
