#!/bin/bash
# Differential test of the start-up clock: v32sim against the stock console logic when neither
# is given a fixed date. Both read time(NULL) and convert it with localtime(), so tests/ref/faketime.c
# is preloaded to make time() return a chosen instant, and TZ picks the zone: dates just before and
# after daylight-saving changes (New York, Berlin, Sao Paulo, Lord Howe, which changes by 30 min),
# midnight, year ends, leap days, 2100.
#
#   tests/run_clock.sh <tools dir>        (as run_gaps.sh: refrun, assemble, packrom)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS=${1:?tools dir (refrun, assemble, packrom)}
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
gcc -shared -fPIC -O1 -o "$W/faketime.so" "$HERE/ref/faketime.c" || exit 2
pass=0; fail=0
# TZ|UTC instant ("date -d" syntax)
while IFS='|' read -r tz when; do
    [ -z "$tz" ] && continue
    case $tz in \#*) continue;; esac
    epoch=$(date -u -d "$when" +%s) || { echo "bad date $when"; exit 2; }
    out=$(V32_FAKE_TIME=$epoch TZ=$tz LD_PRELOAD="$W/faketime.so" GAPS_DIR="$HERE/clock" WORK="$W/run" \
          bash "$HERE/run_gaps.sh" "$TOOLS" 2>&1)
    if echo "$out" | grep -q "gaps: 1 passed, 0 failed"; then pass=$((pass+1)); else
        fail=$((fail+1)); echo "FAIL $tz $when"; echo "$out" | head -8; fi
done <<'CASES'
America/New_York|2026-03-08 06:59:50
America/New_York|2026-03-08 07:00:20
America/New_York|2026-03-08 06:59:59
America/New_York|2026-11-01 05:59:50
America/New_York|2026-11-01 06:00:20
America/New_York|2026-11-01 04:59:50
America/New_York|2026-11-01 04:59:59
America/New_York|2026-11-23 12:00:00
America/New_York|2027-01-01 04:59:58
America/New_York|2028-02-29 17:00:00
America/New_York|2028-12-31 04:59:58
Europe/Berlin|2026-03-29 00:59:50
Europe/Berlin|2026-03-29 01:00:20
Europe/Berlin|2026-10-25 00:59:50
Europe/Berlin|2026-10-25 01:00:20
Europe/Berlin|2026-03-28 22:59:58
Europe/Berlin|2026-12-31 22:59:58
Europe/Berlin|2028-02-28 22:59:58
Europe/Berlin|2100-02-28 22:59:58
Europe/Berlin|2100-12-31 22:59:58
Europe/Berlin|2099-12-31 22:59:58
UTC|2000-12-31 23:59:58
UTC|2000-02-29 23:59:58
UTC|2100-03-01 12:00:00
UTC|2024-12-31 23:59:58
UTC|2025-12-31 23:59:58
UTC|1970-01-01 00:00:00
UTC|2038-01-19 03:14:00
UTC|2100-12-31 23:59:58
America/Sao_Paulo|2018-11-04 02:59:50
America/Sao_Paulo|2019-02-17 01:59:50
Australia/Lord_Howe|2026-10-03 15:29:50
Australia/Lord_Howe|2026-04-04 14:29:50
Pacific/Auckland|2026-09-26 13:59:50
Pacific/Kiritimati|2026-12-31 09:59:58
Pacific/Pago_Pago|2026-12-31 10:59:58
Asia/Kolkata|2026-12-31 18:29:58
CASES
echo "clock: $pass passed, $fail failed"
[ $fail -eq 0 ]
