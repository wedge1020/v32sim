#!/bin/bash
# Tests of --heap (the heap of a v32lua cartridge, per frame) and --expect-reach (EXPECT REACHED / MISSED, exit status).
#
#   tests/run_heap_tests.sh [TOOLCHAIN_DIR]    v32lua, StandardBios.v32 and an assembler (v32as, else the stock `assemble`) in
#                                              the directory or the PATH; V32LUA names v32lua when it is elsewhere
#
# The cartridges are tests/examples/heap_{alloc,arena,oom}.lua: tables in a loop, --gc arena with arena.mark() / arena.release(),
# and an allocation loop that fills the heap and ends in __oom_handler. Without v32lua or an assembler the suite is skipped (exit 0).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
[ -n "${1:-}" ] && PATH=$(cd "$1" && pwd):$PATH
LUA=${V32LUA:-$(command -v v32lua)}
BIOS=${V32BIOS:-$(command -v StandardBios.v32)}
[ -n "$BIOS" ] || BIOS=$HERE/../tools/oracle/bin/StandardBios.v32
AS=$(command -v v32as); ASOPT=-g
if [ -z "$AS" ]; then AS=$(command -v assemble); ASOPT="-g program"; fi
[ -z "$AS" ] && [ -x "$HERE/../tools/oracle/bin/assemble" ] && { AS=$HERE/../tools/oracle/bin/assemble; ASOPT="-g program"; }
if [ -z "$LUA" ] || [ -z "$AS" ] || [ ! -f "$BIOS" ]; then echo "heap tests: skipped (need v32lua, an assembler and StandardBios.v32)"; exit 0; fi
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $1"; }
check() { if [ "$2" = "$3" ]; then ok; else bad "$1: got '$2', want '$3'"; fi; }
has()   { case "$2" in *"$3"*) ok;; *) bad "$1: '$3' not in '$2'";; esac; }
hasnt() { case "$2" in *"$3"*) bad "$1: '$3' in '$2'";; *) ok;; esac; }

build() {   # build NAME [v32lua options]: $W/NAME.{lua,asm,vbin,vbin.debug,v32}
    local n=$1; shift
    cp "$HERE/examples/$n.lua" "$W/$n.lua"
    ( cd "$W" && "$LUA" -g --no-build-scripts "$@" -o "$n.asm" "$n.lua" >/dev/null && $AS "$n.asm" -o "$n.vbin" $ASOPT >/dev/null 2>&1 &&
      python3 -I "$HERE/mkcart.py" "$n.vbin" "$n.v32" ) || { echo "cannot build $n"; exit 2; }
}
build heap_alloc; build heap_oom
# sim ARGS... CART: stdout only; stderr to $W/err; the status is the simulator's
sim() { timeout 120 "$SIM" --batch -r --fast -B "$BIOS" --fixed-clock --seed=1 "$@" 2> "$W/err" </dev/null; }

echo "== --heap: tables in a loop"
out=$(sim --max-frames=270 --heap "$W/heap_alloc.v32"); check "status" "$?" "0"
n=$(echo "$out" | grep -c '^HEAP F'); [ "$n" -ge 10 ] && ok || bad "heap lines: $n"
has   "line format"   "$out" "HEAP F"
echo "$out" | grep '^HEAP F' | head -1 | grep -qE '^HEAP F[0-9]+ ptr=0x[0-9A-F]{8} used=[0-9]+ peak=[0-9]+$' && ok || bad "format of the first line: $(echo "$out" | grep '^HEAP F' | head -1)"
hasnt "no arena fields without --gc arena" "$out" "depth="
# used never decreases, peak = used (nothing is ever freed) and grows
mono=$(echo "$out" | grep '^HEAP F' | sed 's/.*used=\([0-9]*\) peak=\([0-9]*\)/\1 \2/' | awk 'BEGIN{p=-1;m=0} {if ($1<p) m=1; if ($2!=$1) m=1; p=$1} END{print m, (p>0 ? 1 : 0)}')
check "used is monotone, peak = used, heap grows" "$mono" "0 1"
last=$(echo "$out" | grep '^HEAP F' | tail -1 | sed 's/.* used=\([0-9]*\) .*/\1/')
has   "PEAK line"     "$out" "HEAP PEAK used=$last words ptr=0x"
has   "END line"      "$out" "HEAP END used=$last words ptr=0x"
check "stderr is quiet" "$(cat "$W/err")" ""
out0=$(sim --max-frames=270 "$W/heap_alloc.v32"); hasnt "no --heap, no HEAP lines" "$out0" "HEAP"
d1=$(sim --max-frames=270 --dump=400@0 "$W/heap_alloc.v32"); d2=$(sim --max-frames=270 --heap --dump=400@0 "$W/heap_alloc.v32" | grep -v '^HEAP')
check "--heap does not change the machine (RAM dump)" "$d2" "$d1"
d3=$(sim --max-frames=270 --dump=400@0 --expect-reach __oom_handler "$W/heap_alloc.v32" | grep -v '^EXPECT'); check "--expect-reach does not change the machine" "$d3" "$d1"

echo "== --heap: the symbols"
mkdir -p "$W/bare"; cp "$W/heap_alloc.v32" "$W/bare/"
out=$(sim --max-frames=260 --heap "$W/bare/heap_alloc.v32")
has   "no .asm: warning once" "$(cat "$W/err")" "HEAP_START is unknown"
check "no .asm: one warning" "$(grep -c WARNING "$W/err")" "1"
has   "no .asm: ptr only"     "$(echo "$out" | grep '^HEAP F' | head -1)" "ptr=0x"
hasnt "no .asm: no used"      "$(echo "$out" | grep '^HEAP F' | head -1)" "used="
out=$(sim --max-frames=260 --heap="$W/heap_alloc.asm" "$W/bare/heap_alloc.v32")
has   "--heap=PROG.asm"       "$out" " used="
check "--heap=PROG.asm: no warning" "$(cat "$W/err")" ""
sim --max-frames=260 --heap="$W/nosuch.asm" "$W/bare/heap_alloc.v32" >/dev/null; check "--heap=missing file: exit 1" "$?" "1"

echo "== --heap: --gc arena"
build heap_arena --gc arena
out=$(sim --max-frames=275 --heap "$W/heap_arena.v32")
has   "arena fields"   "$out" " floor=0x"
has   "depth 1"        "$out" "depth=1 stack=0x"
echo "$out" | grep -qE 'depth=2 stack=0x[0-9A-F]{8},0x[0-9A-F]{8}$' && ok || bad "depth 2 lists two marks"
echo "$out" | grep -qE 'depth=0 stack=$' && ok || bad "depth back to 0, empty stack"
peak=$(echo "$out" | grep '^HEAP PEAK' | sed 's/.*used=\([0-9]*\) .*/\1/'); end=$(echo "$out" | grep '^HEAP END' | sed 's/.*used=\([0-9]*\) .*/\1/')
[ "$peak" -gt "$end" ] && ok || bad "arena.release lowers used but not the peak (peak $peak, end $end)"
has   "END line has the arena"  "$(echo "$out" | grep '^HEAP END')" "depth=0"

echo "== --expect-reach"
out=$(sim -x --max-frames=1800 --expect-reach __oom_handler "$W/heap_oom.v32"); st=$?
check "OOM reaches __oom_handler: exit 0 (also with -x)" "$st" "0"
echo "$out" | grep -qE '^EXPECT REACHED __oom_handler frame=[0-9]+ ip=0x[0-9A-F]{8}$' && ok || bad "REACHED line: $out"
check "stops at once (no more output)" "$(echo "$out" | wc -l)" "1"
out=$(sim -x --max-frames=1800 --expect-reach __game_loop_return,__oom_handler "$W/heap_oom.v32"); has "second label of a list" "$out" "EXPECT REACHED __oom_handler"
out=$(sim -x --max-frames=1800 --expect-reach __oom_handler --heap "$W/heap_oom.v32"); has "with --heap: END line still printed" "$out" "HEAP END"
out=$(sim --max-frames=300 --expect-reach __oom_handler "$W/heap_alloc.v32"); st=$?
check "normal cart: EXPECT MISSED" "$out" "EXPECT MISSED __oom_handler"; check "MISSED: exit 1" "$st" "1"
out=$(sim -x --max-frames=300 --expect-reach __oom_handler,__nolabel "$W/heap_alloc.v32"); st=$?
check "unknown label: exit 1" "$st" "1"; has "unknown label: message" "$(cat "$W/err")" "'__nolabel' is not a known label"; check "unknown label: nothing on stdout" "$out" ""
out=$(sim -x --max-frames=300 --expect-reach __oom_handler,__game_loop_skip_missing "$W/heap_alloc.v32"); check "unknown label in a list: exit 1" "$?" "1"
out=$(sim -x --max-frames=300 --expect-reach __oom_handler "$W/heap_alloc.v32"); st=$?
check "-x: MISSED outranks the frame limit (2)" "$st" "1"
out=$(printf 'continue\n' | timeout 120 "$SIM" --batch --fast -B "$BIOS" --max-frames=300 --expect-reach __oom_handler "$W/heap_alloc.v32" 2>/dev/null); st=$?
check "--batch script: MISSED, exit 1" "$st$out" "1EXPECT MISSED __oom_handler"
out=$(printf 'continue\n' | timeout 120 "$SIM" --batch --fast -B "$BIOS" --expect-reach __function_game_loop "$W/heap_alloc.v32" 2>/dev/null); st=$?
check "--batch script: REACHED, exit 0" "$st" "0"; has "--batch script: label" "$out" "EXPECT REACHED __function_game_loop"
out=$(sim --max-frames=300 -n --expect-reach __oom_handler "$W/heap_alloc.v32"); check "-n (no debug files): label unknown, exit 1" "$?" "1"

echo "heap tests: $pass passed, $fail failed"
[ $fail -eq 0 ]
