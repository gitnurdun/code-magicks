#!/usr/bin/env bash
# MAGIC: wait -n is a semaphore: start a job, wait for any one to finish, and
# MAGIC: never keep more than a fixed number of children in flight.
set -euo pipefail

durations=(0.05 0.06 0.07 0.08 0.09 0.10)
active=0
maxc=0
complete=0
have_p=0
if (( BASH_VERSINFO[0] > 5 || (BASH_VERSINFO[0] == 5 && BASH_VERSINFO[1] >= 1) )); then
    have_p=1
fi

for d in "${durations[@]}"; do
    sleep "$d" &
    active=$((active + 1))
    if (( active > maxc )); then
        maxc=$active
    fi
    if (( active >= 2 )); then
        if (( have_p )); then
            wait -n -p done_pid
        else
            wait -n
        fi
        active=$((active - 1))
        complete=$((complete + 1))
    fi
done

while (( active > 0 )); do
    if (( have_p )); then
        wait -n -p done_pid
    else
        wait -n
    fi
    active=$((active - 1))
    complete=$((complete + 1))
done

printf 'completed=%s\n' "$complete"
printf 'max_concurrency=%s\n' "$maxc"

# ================= EXPLANATION =================
# What it does
#   Runs six short sleep jobs with a concurrency limit of two. Each time two
#   jobs are in flight, `wait -n` blocks until one finishes, then the next job
#   is launched. It prints only stable totals, never PIDs or timings.
#
# Step by step
#   1. durations is a fixed list, so the run is independent of scheduling and
#      the output does not depend on which sleep finishes first.
#   2. Starting a job increments active. If active reaches the limit, wait -n
#      reaps any one child, decrements active, and increments complete.
#   3. That pattern is a counting semaphore: two slots, one wait per slot. The
#      maximum is the peak value of active, which is 2 here.
#   4. After the loop one job is still running (the last launch did not hit the
#      limit), so the drain loop waits for the remainder and counts it too.
#   5. bash 5.1+ supports `wait -n -p var`. That form stores the PID of the job
#      that terminated in done_pid, which matters when different children need
#      different follow-up. The script captures it but deliberately does not
#      print it; older bash falls back to plain `wait -n` and the totals match.
#
# Output
#   completed=6
#   max_concurrency=2
#
# Why it matters
#   `wait` alone can only wait for everything or for a named PID. `wait -n`
#   turns the shell into a small work pool, enough to parallelize downloads,
#   compiles, or health checks without a process supervisor.
#
# Caveats
#   - `wait -n` requires bash 4.3 or newer; `-p` requires 5.1 or newer. On older
#     shells the option is an error, so guard on BASH_VERSINFO as above.
#   - `wait -n` returns the status of the job it reaped, not a summary. With
#     `set -e`, a single failed job aborts unless the wait is in a tested
#     context; real pools collect statuses instead of relying on errexit.
#   - A job that finishes before you call wait is still counted: it remains a
#     child until reaped. `wait -n` consumes one such status per call.
#   - The counter measures jobs this shell launched. Grandchildren, pipelines,
#     and children reparented to init are outside the count.
#   - `sleep 0.05` accepts fractions via GNU coreutils but resolution is not a
#     guarantee. The code never depends on exact timing, only on completion.
#   - Do not call `wait -n` with no children: bash returns 127 and may print an
#     error. Keep an active count and only wait while it is positive.
# ==============================================
