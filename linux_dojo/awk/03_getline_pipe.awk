#!/usr/bin/awk -f
# MAGIC: getline is a function with three return values and three input
# sources: the current input, a file, or a command's stdout.

BEGIN {
    # 1 = a record was read, 0 = EOF, -1 = error. The pipe form runs a shell.
    cmd = "printf 'b\\na\\n'"
    while ((cmd | getline line) > 0) {
        lines[++n] = line
    }
    close(cmd)

    print "pipe_lines=" n " first=" lines[1] " second=" lines[2]

    # getline var < file reads one record from an explicit file.
    file = "/tmp/linux_dojo_getline.txt"
    print "x y z" > file
    close(file)

    status = (getline rec < file)
    print "file_status=" status " rec=" rec
    eof = (getline rec2 < file)
    print "eof_status=" eof
    close(file)

    # getline with no variable reads into $0 and updates NF.
    $0 = ""
    FS = ":"
    "printf 'one:two:three\\n'" | getline
    print "dollar0=" $0 " nf=" NF " second=" $2
    close("printf 'one:two:three\\n'")

    system("rm -f " file)
}

# ================= EXPLANATION =================
# What it does
#   Reads records from a command's stdout, from an explicit file, and into $0
#   with no variable. It also shows the three return values: 1 for data, 0 for
#   EOF, and -1 for an error.
#
# Step by step
#   1. `cmd | getline line` starts a shell running printf. Each call pulls one
#      line; the loop stops when getline returns 0 at EOF.
#   2. close(cmd) waits for the command and releases the pipe. Without it, a
#      second command with the same string may reuse the still-open stream.
#   3. `getline rec < file` opens the file, reads the first record, and returns
#      1. The next call returns 0 because the descriptor is at EOF.
#   4. `"printf ..." | getline` with no variable stores the record in $0,
#      re-splitting it with the current FS and updating NF.
#
# Output
#   pipe_lines=2 first=b second=a
#   file_status=1 rec=x y z
#   eof_status=0
#   dollar0=one:two:three nf=3 second=two
#
# Why it matters
#   getline is how awk reads more than the main input stream: configuration
#   files, lookup tables, command output. The return value is the only reliable
#   EOF test, and it is also the only way to distinguish "empty line" from
#   "no more lines". `cmd | getline` turns awk into a small process
#   orchestrator without leaving the language.
#
# Caveats
#   - `getline` without a variable clobbers $0, NF, and the current record.
#     `getline var` does not.
#   - The pipe form runs /bin/sh -c, so the command string is subject to shell
#     quoting and injection. Never paste untrusted data into it.
#   - close() is required to avoid descriptor leaks and to see a file that was
#     rewritten after the first read. Its return value differs between awks;
#     do not depend on it portably.
#   - A -1 return means an error (missing file, command failure); the demo
#     does not exercise it because the message is awk-specific.
#   - In POSIX awk, `getline < file` uses the same expression evaluation rules
#     as any other expression; parenthesise when mixing it with comparisons.
# ==============================================
