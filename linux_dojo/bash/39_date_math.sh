#!/usr/bin/env bash
# MAGIC: GNU date parses '@epoch' and free-form offsets, so calendar math reduces to integer seconds plus formatting.
set -euo pipefail
export TZ=UTC0
export LC_ALL=C

printf 'epoch0=%s\n' "$(date -u -d '@0' '+%F %T %Z')"
printf 'plus36=%s\n' "$(date -u -d '2024-01-01 00:00:00 + 36 hours' '+%F %T')"

start=$(date -u -d '2024-01-01 00:00:00' +%s)
target=$(date -u -d '2024-01-01 00:00:00 + 36 hours' +%s)
printf 'start_epoch=%s\n' "$start"
printf 'target_epoch=%s\n' "$target"
printf 'delta_seconds=%d\n' "$((target - start))"

later=$((start + 90 * 60))
printf 'plus90min=%s\n' "$(date -u -d "@$later" '+%F %T')"

secs=3661
printf 'duration=%02d:%02d:%02d\n' "$((secs / 3600))" "$((secs % 3600 / 60))" "$((secs % 60))"

# ================= EXPLANATION =================
# What it does
#   Formats the Unix epoch, parses a fixed date with a relative offset,
#   converts both to seconds with %s, does arithmetic on those seconds,
#   and formats a duration with integer division and modulo.
#
# Step by step
#   1. date -u -d '@0' reads the epoch as UTC. '%F %T %Z' prints an
#      ISO date, a 24-hour clock and the zone, avoiding locale names.
#   2. GNU date accepts a date followed by relative items. '-u' makes
#      both the base date and the calculation UTC, so no DST rule can
#      change the result.
#   3. %s prints seconds since 1970-01-01T00:00:00Z. 36 hours is
#      129600 seconds, which is exactly the subtraction result.
#   4. Arithmetic on the epoch is simple addition. Formatting the
#      result with date -d "@$later" converts back to calendar fields.
#   5. A duration is not a date. 3661 seconds is 1 hour, 1 minute and
#      1 second: hours = 3661/3600, minutes = (3661%3600)/60, seconds =
#      3661%60. %02d adds leading zeroes.
#
# Output
#   epoch0=1970-01-01 00:00:00 UTC
#   plus36=2024-01-02 12:00:00
#   start_epoch=1704067200
#   target_epoch=1704196800
#   delta_seconds=129600
#   plus90min=2024-01-01 01:30:00
#   duration=01:01:01
#
# Why it matters
#   Epoch seconds are a single integer that sorts, subtracts and stores
#   cleanly. Converting at the edges - parse on input, format on output
#   - avoids most timezone bugs, and %s gives scripts a portable-enough
#   numeric timestamp on Linux.
#
# Caveats
#   - date -d, '@epoch', '+%s' and the free-form relative parser are
#     GNU extensions. BSD/macOS use date -j -f; busybox date is smaller;
#     POSIX date has no -d and no %s.
#   - Do not print "now" in a deterministic demo. Here every input is
#     fixed; the script never asks the clock.
#   - Relative months and years are calendar operations and do not have
#     a fixed number of seconds. Hours and minutes do. Dates near a DST
#     transition can be ambiguous or nonexistent in a local zone; -u
#     avoids that question.
#   - Unix time ignores leap seconds: a leap second is not represented,
#     so a UTC minute containing one lasts 61 real seconds but advances
#     Unix time by only 60.
#   - date parsing accepts many ambiguous formats. Prefer an explicit
#     ISO 8601 string plus a zone, and validate the exit status.
# ==============================================
