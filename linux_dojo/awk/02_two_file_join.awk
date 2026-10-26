#!/usr/bin/awk -f
# MAGIC: the NR==FNR idiom joins two files by loading the first into an array
# before the second one starts; empty first files break it.

BEGIN {
    users  = "/tmp/linux_dojo_join_users.txt"
    shells = "/tmp/linux_dojo_join_shells.txt"

    print "alice:Alice" > users
    print "bob:Bob"     > users
    print "carol:Carol" > users
    close(users)

    print "alice:/bin/bash" > shells
    print "bob:/bin/zsh"    > shells
    print "dave:/bin/sh"    > shells
    close(shells)

    # Load the "left" table first. This is what NR==FNR does implicitly when
    # awk is given two file operands.
    while ((getline line < users) > 0) {
        split(line, u, ":")
        name[u[1]] = u[2]
    }
    close(users)

    # Stream the "right" table and probe the hash.
    while ((getline line < shells) > 0) {
        split(line, s, ":")
        label = (s[1] in name) ? name[s[1]] : "?"
        printf "%s %s %s\n", s[1], label, s[2]
    }
    close(shells)

    system("rm -f " users " " shells)
}

# ================= EXPLANATION =================
# What it does
#   Joins a user table (login -> display name) with a shell table
#   (login -> shell), printing "?" for a shell row with no matching user.
#   The real-world form is a two-operand awk program, shown below.
#
# Step by step
#   1. The demo writes both tables to /tmp so the file-reading code is real,
#      then closes them. close() flushes the output and resets the file.
#   2. The first getline loop loads login -> name into an associative array.
#      This is the explicit version of `NR==FNR { ...; next }`.
#   3. The second loop reads the shell table and does an O(1) hash probe with
#      `s[1] in name`. The ternary supplies "?" for a missing key.
#   4. close() on each input releases the descriptor. The temporary files are
#      removed at the end.
#
# Real-world form
#   awk -F: 'NR==FNR { name[$1]=$2; next } { print $1, name[$1] }' users shells
#
# Output
#   alice Alice /bin/bash
#   bob Bob /bin/zsh
#   dave ? /bin/sh
#
# Why it matters
#   Two-file joins are the bread and butter of log and configuration
#   reconciliation. The hash probe is O(1) per row, so the join is O(n+m)
#   instead of the O(n*m) nested loop most shell scripts accidentally write.
#
# Caveats
#   - NR==FNR fails when the first file is empty: NR and FNR stay equal into
#     the second file. Guard with `FILENAME==ARGV[1]` or test `ARGV[1]` first.
#   - NR==FNR also misbehaves with stdin operands and with more than two files
#     unless the program resets state per file.
#   - The left table must fit in memory. For huge inputs, sort both sides and
#     use the join(1) command instead.
#   - Keys are compared as strings by default; a numeric key and its string
#     spelling can still collide because awk array subscripts are strings.
#   - This demo uses fixed /tmp names. In production use mktemp and quote paths
#     before passing them to system(); a space or shell metacharacter in the
#     path would otherwise split the command.
# ==============================================
