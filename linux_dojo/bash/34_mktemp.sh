#!/usr/bin/env bash
# MAGIC: mktemp uses O_EXCL to create an unpredictable 0600 file or 0700 directory, closing the symlink race that $$ names invite.
set -euo pipefail

umask 077
tmpdir=$(mktemp -d)
secret=$(mktemp "$tmpdir/secret.XXXXXX")
trap 'rm -rf -- "$tmpdir"' EXIT

printf 'token=%s\n' 'correct horse battery staple' > "$secret"

printf 'dir_mode=%s\n' "$(stat -c '%a' "$tmpdir")"
printf 'file_mode=%s\n' "$(stat -c '%a' "$secret")"

# ================= EXPLANATION =================
# What it does
#   Creates a private temporary directory and a file inside it with
#   mktemp, writes a secret, and prints only the resulting permission
#   modes. The EXIT trap removes the directory and everything in it.
#
# Step by step
#   1. umask 077 clears group and other permission bits for anything
#      this shell creates from here on.
#   2. mktemp -d creates a fresh directory with a random suffix and
#      mode 0700. It retries if the name already exists; creation is
#      atomic, not a check-then-create.
#   3. mktemp "$tmpdir/secret.XXXXXX" creates the file with O_EXCL and
#      mode 0600. The X's are replaced by random characters.
#   4. The secret is written through the path just created. stat -c %a
#      prints the octal mode only, so the random names never appear in
#      the output.
#   5. The EXIT trap runs on normal shell exit and on an explicit exit,
#      removing the whole directory.
#
# Output
#   dir_mode=700
#   file_mode=600
#
# Why it matters
#   Predictable temporary names are a classic local attack surface.
#   An attacker can pre-create /tmp/myprog.$$ as a symlink to a file
#   they want overwritten; a later naive open with > follows it. With
#   O_EXCL, creation fails if anything already owns the name, and the
#   random component makes guessing impractical.
#
# Caveats
#   - mktemp requests 0600/0700, but the umask can only clear more
#     bits. A pathological umask of 0777 would leave the new object
#     with mode 000; umask 077 here keeps the intended 600/700.
#   - The mode is not a substitute for ownership and the sticky bit on
#     shared directories such as /tmp.
#   - mktemp closes the file before returning its name. In a directory
#     writable by others, an attacker could still unlink and replace
#     the path before you reopen by name. A 0700 directory or /tmp's
#     sticky bit prevents that; keeping the original fd is safer still.
#   - Do not build names from $$, $RANDOM, dates or hostnames: they are
#     predictable and can collide or be pre-created.
#   - stat -c '%a' is GNU. BSD/macOS stat uses -f '%Lp'.
#   - SIGKILL cannot be trapped, so cleanup is best-effort; a stale
#     directory can remain after a hard kill.
# ==============================================
