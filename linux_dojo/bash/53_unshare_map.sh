#!/usr/bin/env bash
# MAGIC: unshare -Ur puts the caller in a new user namespace where the same
# MAGIC: kernel uid is mapped to 0, granting capabilities only inside it.
set -euo pipefail

command -v unshare >/dev/null 2>&1 || { printf 'skipped: unshare not found\n'; exit 0; }
[[ -r /proc/self/uid_map ]] || { printf 'skipped: /proc/self/uid_map not available\n'; exit 0; }

outside_uid=$(id -u)

# The inner sh prints: inside uid, number of uid_map lines, first mapped id.
facts=$(unshare -Ur sh -c '
    printf "%s %s %s\n" "$(id -u)" "$(wc -l < /proc/self/uid_map)" "$(cut -d " " -f1 /proc/self/uid_map)"
' 2>/dev/null) || { printf 'skipped: user namespace creation denied (EPERM or disabled)\n'; exit 0; }

[[ -n "$facts" ]] || { printf 'skipped: unshare produced no output\n'; exit 0; }
read -r inside_uid map_lines root_target <<< "$facts"
if [[ ! "$inside_uid" =~ ^[0-9]+$ || ! "$map_lines" =~ ^[0-9]+$ || ! "$root_target" =~ ^[0-9]+$ ]]; then
    printf 'skipped: unexpected uid_map contents\n'
    exit 0
fi

printf 'inside_uid=%s\n' "$inside_uid"
printf 'uid_map_lines=%s\n' "$map_lines"
printf 'uid_map_root_target=%s\n' "$root_target"
if (( outside_uid != 0 )); then
    printf 'outside_uid_nonzero=yes\n'
else
    printf 'outside_uid_nonzero=no\n'
fi

# ================= EXPLANATION =================
# What it does
#   Asks unshare to create a user namespace and map the caller's effective uid
#   to 0 inside it. A tiny inner shell prints its id -u, the number of lines in
#   /proc/self/uid_map, and the first mapped inside id. The outer shell prints
#   the outside uid class and normalised map facts.
#
# Step by step
#   1. unshare -U calls unshare(CLONE_NEWUSER). The process gains a fresh user
#      namespace with no mappings; until uid_map is written, it cannot do much
#      in the new namespace.
#   2. The -r option makes util-linux write the mapping "0 <current-euid> 1" in
#      one go, so the caller appears as root (uid 0) inside. No host privilege
#      is involved; the mapping is one-to-one and limited to the new namespace.
#   3. Inside, id -u reads the caller's uid in that namespace and prints 0.
#      /proc/self/uid_map shows one line: "0 <outside-uid> 1". The outside uid
#      is still the same non-zero number, and the host sees it unchanged.
#   4. The inner shell prints only numbers, so the outer script can parse them
#      without embedding a variable uid_map line in the output. The actual
#      outside uid varies, so outside_uid_nonzero is a boolean.
#   5. A failure from unshare (missing CONFIG_USER_NS, an EPERM from the
#      sysctl/AppArmor/LSM, or an unsupported option) becomes one skipped line
#      and exit 0 instead of a traceback.
#
# Output
#   inside_uid=0
#   uid_map_lines=1
#   uid_map_root_target=0
#   outside_uid_nonzero=yes
#
# Why it matters
#   User namespaces are what make rootless containers work. They let an
#   unprivileged process own a namespace where it has full capabilities, mount
#   a private /proc or overlay, and bind low ports, without any of that power
#   leaking to the host. uid_map and gid_map are the translation tables: a
#   kernel uid inside maps to a different uid outside, and unmapped ids are
#   treated as nobody.
#
# Caveats
#   - The kernel must be built with CONFIG_USER_NS. Debian and Ubuntu carried a
#     kernel.unprivileged_userns_clone sysctl that can disable creation; Ubuntu
#     23.10+ also restricts unprivileged user namespaces through AppArmor.
#   - uid_map may be written only once; each line is an inside-id, outside-id,
#     count triple. The writer needs CAP_SETUID in the parent namespace (or the
#     creator's single-line exception). gid_map also needs setgroups "deny".
#   - unshare -r maps the effective uid and egid to 0; other ids stay unmapped,
#     and supplementary groups need separate handling.
#   - Capabilities gained inside are scoped to the new user namespace and its
#     children. They do not grant access to files owned by the host root or to
#     resources outside the namespace.
#   - Nested user namespaces are limited by /proc/sys/user/max_user_namespaces
#     and by the depth limit MAX_USER_NS_DEPTH; a container runtime may already
#     have used one level.
#   - Inside a user namespace, some /proc and sysfs files are filtered or show
#     the namespace's view; do not assume the host's values.
# ==============================================
