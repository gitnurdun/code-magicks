#!/usr/bin/env bash
# MAGIC: PIPESTATUS is the per-stage status array; read it before the next
# MAGIC: command, because every command replaces it.
set -u

false | true | false
codes=("${PIPESTATUS[@]}")
printf 'default: false|true|false -> PIPESTATUS=%s last=%s\n' "${codes[*]}" "${codes[2]}"

printf 'an intervening command\n' >/dev/null
printf 'after one command: PIPESTATUS=%s\n' "${PIPESTATUS[*]}"

set -o pipefail
true | false | true
codes=("${PIPESTATUS[@]}")
printf 'pipefail: true|false|true -> PIPESTATUS=%s\n' "${codes[*]}"
true | false | true
printf 'pipefail: pipeline status=%s\n' "$?"

# ================= EXPLANATION =================
# What it does
#   Runs a three-stage pipeline and prints every stage's status before anything
#   can overwrite it. It then shows that the next command resets PIPESTATUS,
#   and that pipefail changes the status of the pipeline as a whole. Errexit is
#   deliberately left off: these failures are the data being observed.
#
# Step by step
#   1. false | true | false leaves PIPESTATUS=(1 0 1). Without pipefail the
#      pipeline status is only the last stage, 1.
#   2. codes=(...) copies the array in the same command that reads it. A bare
#      assignment is itself a foreground pipeline of length one, so PIPESTATUS
#      becomes (0) immediately afterwards; the copy must happen first.
#   3. The next printf is another command. When the following printf expands
#      PIPESTATUS it sees (0), not the old three-element pipeline.
#   4. `set -o pipefail` makes a pipeline return the rightmost non-zero stage
#      status. true | false | true still has PIPESTATUS=(0 1 0), but its $? is
#      now 1 instead of 0.
#   5. The final pipeline is rerun and $? is read directly, without an
#      intervening assignment, so the effect of pipefail is visible.
#
# Output
#   default: false|true|false -> PIPESTATUS=1 0 1 last=1
#   after one command: PIPESTATUS=0
#   pipefail: true|false|true -> PIPESTATUS=0 1 0
#   pipefail: pipeline status=1
#
# Why it matters
#   A pipeline is a process group, and its overall status is not all of its
#   result. Without PIPESTATUS, `producer | consumer` reports a successful
#   consumer even when the producer died. Build systems, backup scripts, and
#   data pipelines need to know which stage failed.
#
# Caveats
#   - PIPESTATUS is bash (and ksh) specific; POSIX sh has no equivalent. zsh
#     spells it pipestatus, lowercase.
#   - Any command resets it: echo, printf, a test, or an assignment. Capture the
#     array, or the single element you need, in the immediately next command.
#   - `set -o pipefail` changes only $?; the full array is still available.
#   - With `set -e` and pipefail, a failure in any stage can abort the script,
#     including a producer that legitimately exits non-zero early.
#   - `time pipeline` and subshell wrappers report the wrapper's status;
#     PIPESTATUS outside the wrapper refers to the wrapper, not the stages.
# ==============================================
