#!/bin/bash
# tests/run_v32trace_clock.sh: V32TRACE spec 2.3 in --v32trace: `inputclock ADDR` (V32_INPUT_CLOCK, clock= fields) and the
# `load:` line of [perf] and of every marker stretch. Needs an assembler (v32as, else the stock `assemble`); the BIOS is the stub.
#
#   tests/run_v32trace_clock.sh [TOOLCHAIN_DIR]
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
[ -n "${1:-}" ] && PATH=$(cd "$1" && pwd):$PATH
BIOS=$HERE/../bios/StubBios.v32
AS=$(command -v v32as); ASOPT=-g
if [ -z "$AS" ]; then AS=$(command -v assemble); ASOPT="-g program"; fi
if [ -z "$AS" ]; then echo "v32trace clock tests: skipped (need an assembler)"; exit 0; fi
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"; trap '[ -z "${WORK:-}" ] && rm -rf "$W"' EXIT
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "FAIL: $*"; }
check() { if [ "$2" = "$3" ]; then ok; else bad "$1: got '$2', want '$3'"; fi; }

cp "$HERE/examples/trace_clock.asm" "$W/clk.asm"
( cd "$W" && $AS clk.asm -o clk.vbin $ASOPT >/dev/null 2>&1 && python3 -I "$HERE/mkcart.py" clk.vbin clk.v32 ) || { echo "cannot build the test cartridge"; exit 2; }
awk -F, '/^F,/{printf "%s %s\n",$3,$2}' "$W/clk.vbin.sym" > "$W/clk.labels.txt"
run() {   # run PREFIX COMMANDS: the cartridge with A@10-20 B@36-45 RIGHT+UP@50-52, driven by the commands (run N, mark NOTE, quit)
    printf '%s' "$2" | "$SIM" -q --fast -x -B "$BIOS" --input="A@10-20,B@36-45,RIGHT+UP@50-52" --v32trace="$W/$1" "$W/clk.v32" >/dev/null 2>&1
}

# 1. no inputclock: nothing of 2.3 but the load line (the session is the 2.2 one)
printf 'format 2\narch test\ntrap _trap\n' > "$W/clk.v32trace.meta"
run a $'run 70\nquit\n'
grep -q 'clock' "$W/a.trace.txt" "$W/a.traps.txt" && bad "clock appears without inputclock" || ok
check "V32_INPUT without inputclock" "$(grep '^V32_INPUT=' "$W/a.trace.txt")" "V32_INPUT=A@10-20,B@36-45,RIGHT+UP@50-52"
grep -q '^load: cpu% min [0-9.]* avg [0-9.]* max [0-9.]*, gpu% min [0-9.]* avg [0-9.]* max [0-9.]*$' "$W/a.trace.txt" && ok || bad "no load line: $(grep -n load "$W/a.trace.txt")"

# 2. inputclock 100: V32_INPUT_CLOCK (clock = frames done by the guest), the reset comment, clock= on the trap, events and markers
printf 'format 2\narch test\ntrap _trap\ninputclock 100\n' > "$W/clk.v32trace.meta"
run b $'run 20\nmark here\nrun 30\nmark later\nrun 20\nquit\n'
check "V32_INPUT_CLOCK" "$(grep '^V32_INPUT_CLOCK=' "$W/b.trace.txt")" "V32_INPUT_CLOCK=A@8-14,B@25-26,B@0-4,RIGHT+UP@7-9"
check "V32_INPUT is as without inputclock" "$(grep '^V32_INPUT=' "$W/b.trace.txt")" "V32_INPUT=A@10-20,B@36-45,RIGHT+UP@50-52"
check "reset comment" "$(grep '^# inputclock reset' "$W/b.trace.txt")" "# inputclock reset at cart frame 39"
grep -q '^TRAP n=1 cartframe=59 clock=[0-9]* count=1 ' "$W/b.traps.txt" && ok || bad "traps.txt: $(head -2 "$W/b.traps.txt" | cut -c1-120)"
grep -q '^frame 59: TRAP #1: trap clock=[0-9]*$' "$W/b.trace.txt" && grep -q '^TRAP #1 at frame 59: trap arch=test, seen 1 time clock=[0-9]*$' "$W/b.trace.txt" && ok || bad "report TRAP lines: $(grep -n 'TRAP' "$W/b.trace.txt" | cut -c1-120)"
grep -q '^marker 1 at frame 18: here clock=13$' "$W/b.trace.txt" && grep -q '^frame 18: marker 1: here clock=13$' "$W/b.trace.txt" && ok || bad "marker clock: $(grep -n 'marker' "$W/b.trace.txt" | cut -c1-120)"
# the clock never runs ahead of the cart frame
python3 -I - "$W/b.trace.txt" <<'PY' && ok || bad "a clock value is past its cart frame"
import re, sys
t = open(sys.argv[1]).read()
for f, c in re.findall(r'^frame (\d+): .* clock=(\d+)$', t, re.M):
    assert int(c) <= int(f) + 1 or int(f) >= 39, (f, c)
PY
# 3. the load line also per marker stretch, two spaces in
n=$(grep -c '^  load: cpu% min' "$W/b.trace.txt"); check "load lines per marker stretch" "$n" "3"
# 4. without a V32_INPUT_CLOCK nothing else changes: the 2.2 report is the same apart from the clock lines
sed -e 's/ clock=[0-9]*$//' -e '/^# inputclock/d' -e '/^V32_INPUT_CLOCK=/d' -e 3d "$W/b.trace.txt" > "$W/b.norm"
printf 'format 2\narch test\ntrap _trap\n' > "$W/clk.v32trace.meta"
run c $'run 20\nmark here\nrun 30\nmark later\nrun 20\nquit\n'
sed -e 3d "$W/c.trace.txt" > "$W/c.norm"
cmp -s "$W/b.norm" "$W/c.norm" && ok || bad "the report with inputclock differs from the one without beyond the clock fields: $(diff "$W/b.norm" "$W/c.norm" | head -4)"
sed -e 's/ clock=[0-9]*//' "$W/b.traps.txt" | cmp -s - "$W/c.traps.txt" && ok || bad "traps.txt differs beyond clock="
echo "v32trace clock tests: $pass passed, $fail failed"
[ $fail -eq 0 ]
