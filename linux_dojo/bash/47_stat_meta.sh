#!/usr/bin/env bash
# MAGIC: an inode is the file; names are links to it. stat %h counts those
# MAGIC: links, and %a/%A show the same mode in octal and symbolic form.
set -euo pipefail
export LC_ALL=C

for t in stat find touch date ln; do
    command -v "$t" >/dev/null 2>&1 || { printf 'skipped: %s not available\n' "$t"; exit 0; }
done

umask 022
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
data=$work/data
mkdir "$data"

file=$data/file.txt
hard=$data/hard.txt
printf 'hello\n' > "$file"
ln "$file" "$hard"

# %n name, %i inode, %h link count, %s size, %a octal mode, %F type.
read -r name ino links size mode ftype <<< "$(cd "$data" && stat -c '%n %i %h %s %a %F' file.txt)"
ino2=$(stat -c %i "$hard")
if [[ "$ino" == "$ino2" ]]; then
    printf 'hardlink_same_inode=yes\n'
else
    printf 'hardlink_same_inode=no\n'
fi
printf 'stat_link_count=%s\n' "$links"
printf 'stat_size=%s\n' "$size"
printf 'perm_octal=%s\n' "$mode"
printf 'perm_symbolic=%s\n' "$(stat -c %A "$file")"
printf 'file_type=%s\n' "$ftype"

# find's %n is the link count too; both names must point at the same inode.
find_out=$(find "$data" -maxdepth 1 -type f -printf '%i %n %s %p\n')
printf 'find_entries=%s\n' "$(printf '%s\n' "$find_out" | wc -l)"
find_all_links_2=$(printf '%s\n' "$find_out" | awk '$2 != 2 { bad = 1 } END { print bad ? "no" : "yes" }')
printf 'find_all_link_counts_2=%s\n' "$find_all_links_2"

# Pin mtime so the epoch-to-text round trip is reproducible.
TZ=UTC touch -d '2024-01-02 03:04:05' "$file"
epoch=$(stat -c %Y "$file")
printf 'mtime_utc=%s\n' "$(date -u -d "@$epoch" '+%Y-%m-%d %H:%M:%S')"

# ================= EXPLANATION =================
# What it does
#   Creates a file and a hard link to it, then reads the inode metadata both
#   with stat's custom format and with find -printf. It confirms that the two
#   names share one inode and one link count of two, shows octal and symbolic
#   modes, and round-trips the modification time through date.
#
# Step by step
#   1. A regular file is an inode: an identity plus data blocks. A directory
#      entry is a name mapped to an inode number. Creating a file adds a name
#      and a link count of one.
#   2. ln file hard.txt adds a second directory entry for the same inode. There
#      is no new data and no new inode; %h becomes 2 and both stat calls return
#      the same %i. The numbers themselves are not printed because they vary.
#   3. stat -c '%n %i %h %s %a %F' prints the name, inode, link count, size,
#      permission bits in octal, and the file type. %a is the numeric mode
#      (644); %A is the ls-style string (-rw-r--r--). They encode the same bits,
#      but %A also shows setuid/setgid/sticky letters and a trailing "+" for an
#      ACL.
#   4. find -printf '%i %n %s %p\n' prints the same metadata per entry. Both
#      names appear, each with link count 2, which proves find sees two names
#      for one inode rather than a copy.
#   5. touch pins the mtime to 2024-01-02 03:04:05 UTC. %Y is an integer count
#      of seconds since the Unix epoch; date -u -d @<epoch> turns it back into
#      that UTC timestamp. %y is the human-readable form, %X is atime, %Z is
#      ctime (inode change time), and %W is birth time where supported.
#   6. The temp directory and both names are removed by the exit trap.
#
# Output
#   hardlink_same_inode=yes
#   stat_link_count=2
#   stat_size=6
#   perm_octal=644
#   perm_symbolic=-rw-r--r--
#   file_type=regular file
#   find_entries=2
#   find_all_link_counts_2=yes
#   mtime_utc=2024-01-02 03:04:05
#
# Why it matters
#   Inodes explain hard links, rename atomicity, open-unlinked files, and why
#   `rm` does not free data while a process still holds a descriptor. Link
#   counts are how the kernel decides when to reclaim an inode. Timestamp
#   fields are the basis of build systems and backup tools, which is also why
#   atime behaviour (strictatime, relatime, noatime) matters.
#
# Caveats
#   - Inode numbers are unique only within one filesystem. Comparing %i across
#     mount points is meaningless, and an inode number can be reused after the
#     last link is gone.
#   - Hard links cannot cross filesystems and cannot name a directory (only root
#     could, and even then the kernel forbids it).
#   - %h counts directory entries, including links from other directories. An
#     open-unlinked file has %h 0 but is still alive.
#   - %a is the permission bits only; ACLs, capabilities, and SELinux labels are
#     separate metadata. %A's "+" is only an indicator.
#   - %Y has whole-second resolution. %y shows nanoseconds where the filesystem
#     stores them; %W birth time is often unavailable.
#   - atime may be updated by reads under strictatime, but relatime (the common
#     default) suppresses most updates.
#   - find -printf is GNU-specific; BSD find uses -print0 plus stat.
# ==============================================
