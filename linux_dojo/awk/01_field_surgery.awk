#!/usr/bin/awk -f
# MAGIC: assigning any field rebuilds $0 from the fields with OFS, so field
# surgery is really record reconstruction.

BEGIN {
    FS = ":"
    OFS = ":"
    $0 = "root:x:0:0:root:/root:/bin/bash"

    print "original=" $0
    print "nf=" NF " first=" $1 " last=" $NF

    # Assigning $0 re-splits it with the current FS. It does not "refresh" NF
    # by magic; it parses the record again.
    $0 = $0
    print "after_touch=" $0

    # Appending a field sets NF and rebuilds $0 with OFS.
    $(NF + 1) = "extra"
    print "appended=" $0 " nf=" NF

    # There is no portable `delete $2`. Rebuild the record by hand.
    out = ""
    for (i = 1; i <= NF; i++) {
        if (i == 2) continue
        out = out (out == "" ? "" : OFS) $i
    }
    $0 = out
    print "deleted=" $0 " nf=" NF
}

# ================= EXPLANATION =================
# What it does
#   Treats an awk record as a mutable vector of fields: prints it, appends a
#   field, then deletes a field by rebuilding the record. The output shows the
#   record and NF changing together.
#
# Step by step
#   1. FS=":" makes ":" the field separator. $0 is set to a passwd-style line,
#      which splits it into seven fields.
#   2. $0=$0 re-parses the existing text with FS. It is a no-op here except for
#      the rebuild, but it shows that $0 and the fields are two views of one
#      record.
#   3. $(NF+1)="extra" extends NF to 8 and rebuilds $0 by joining every field
#      with OFS. The new field appears at the end, not at the old text offset.
#   4. To delete field 2, the loop copies every other field into `out` and
#      assigns the result back to $0. NF drops to 6 and the record is re-split.
#
# Output
#   original=root:x:0:0:root:/root:/bin/bash
#   nf=7 first=root last=/bin/bash
#   after_touch=root:x:0:0:root:/root:/bin/bash
#   appended=root:x:0:0:root:/root:/bin/bash:extra nf=8
#   deleted=root:0:0:root:/root:/bin/bash nf=6
#
# Why it matters
#   awk's fields are not slices of the input string; they are the parsed form.
#   Any field assignment rewrites $0 using OFS. That is why
#       awk -F: -v OFS=, '{$1=$1; print}'
#   is the canonical "convert FS to OFS" idiom, and why touching a field can
#   change spacing in the rest of the record.
#
# Caveats
#   - Assigning a field rebuilds $0, but it does not preserve the original
#     separators. Runs of whitespace under the default FS collapse.
#   - `delete $2` is not portable awk (and is not valid in gawk either); the
#     rebuild loop is the portable way.
#   - Changing FS after $0 was split does not re-split the current record until
#     you assign $0 again.
#   - Very large NF values make the rebuild O(n) per assignment; batch field
#     edits when the record is wide.
#   - Numeric-looking fields still behave as strings until compared or used
#     arithmetically (see 05_strnum_csv.awk).
# ==============================================
