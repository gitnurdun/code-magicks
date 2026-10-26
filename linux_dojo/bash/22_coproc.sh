#!/usr/bin/env bash
# MAGIC: a coprocess is a background child with two named pipe fds, so the
# MAGIC: shell can write requests, read replies, and reap it explicitly.
set -euo pipefail

coproc WORKER {
    while IFS= read -r line; do
        printf '%s\n' "${line^^}"
    done
}

rfd=${WORKER[0]}
wfd=${WORKER[1]}
printf 'alpha\n' >&"$wfd"
printf 'beta\n' >&"$wfd"
IFS= read -r reply1 <&"$rfd"
IFS= read -r reply2 <&"$rfd"
exec {wfd}>&-
wait "$WORKER_PID"

printf 'reply1=%s\n' "$reply1"
printf 'reply2=%s\n' "$reply2"

# ================= EXPLANATION =================
# What it does
#   Starts a long-lived worker as a coprocess, sends it two lines, reads the
#   uppercased replies in order, closes the request pipe, and waits for the
#   worker to exit at EOF.
#
# Step by step
#   1. `coproc WORKER { ...; }` forks a child and creates two pipes. The
#      parent sees WORKER[0] (read replies) and WORKER[1] (write requests);
#      the child sees the matching ends on stdin and stdout. WORKER_PID is the
#      child's PID.
#   2. The worker reads one line at a time. ${line^^} is bash 4+ case
#      conversion to uppercase; EOF makes read return non-zero and ends the
#      while loop.
#   3. The parent copies the fd numbers into rfd/wfd, writes two lines, then
#      reads two lines. Small requests fit in the pipe buffer, so there is no
#      deadlock; a large request could fill the pipe while the worker is
#      blocked writing a reply.
#   4. `exec {wfd}>&-` closes the parent's write end. EOF reaches the worker
#      only when every write end is closed. The child does not hold a stray
#      copy, so its read returns 0 and it exits.
#   5. wait reaps WORKER_PID; the read end stays open until the shell exits.
#      That is harmless once the worker is gone; the close that matters for EOF
#      is the write end above.
#   6. Replies are printed only after wait, so their order is fixed.
#
# Output
#   reply1=ALPHA
#   reply2=BETA
#
# Why it matters
#   A coprocess is the shell's bidirectional pipe: no FIFO files, no temporary
#   protocol, no external driver. It is useful for querying a long-running
#   helper (a database CLI, a REPL, an authentication agent) while keeping the
#   request and response pipes distinct.
#
# Caveats
#   - File descriptors are inherited across fork and exec. The coprocess child
#     inherits unrelated descriptors from the parent, and any external command
#     launched inside the worker inherits the coprocess fds. A daemon that keeps
#     the write end open prevents EOF and the parent's read can block forever.
#     Close unused ends explicitly in long-lived children.
#   - Closing the write end is mandatory. The parent holds both ends; without
#     the close, the worker never sees EOF and wait never returns.
#   - Reads block. There is no timeout in bash; a worker that does not answer
#     stalls the script. Use read -t for a deadline and handle the partial line.
#   - If the worker dies first, the parent's next write gets SIGPIPE (or EPIPE
#     if ignored). Check wait's status and treat a dead worker as a failure.
#   - Named coprocs require bash 4.0+; automatic {var} fd allocation and close
#     require bash 4.1+. Only one unnamed coproc exists at a time, though
#     multiple named ones can coexist.
# ==============================================
