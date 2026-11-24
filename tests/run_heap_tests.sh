#!/bin/bash
# Tests of --heap (the heap of a v32lua cartridge, per frame) and --expect-reach (EXPECT REACHED / MISSED, exit status).
#
#   tests/run_heap_tests.sh [TOOLCHAIN_DIR]    v32lua, StandardBios.v32 and an assembler (v32as, else the stock `assemble`) in
#                                              the directory or the PATH; V32LUA names v32lua when it is elsewhere
#
# The cartridges are tests/examples/heap_{alloc,arena,oom,free}.lua: tables in a loop, --gc arena with arena.mark() / arena.release(),
# an allocation loop that fills the heap and ends in __oom_handler, and a table grown element by element (the free lists:
# `free= blocks=` need a v32lua that has them, 20261124-dev or later). With v32orun (PATH) the same cartridges are run on
# the stock console logic and the two outputs must have the same format (and, at the end, the same numbers). Without v32lua or an assembler the suite is skipped (exit 0).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
[ -n "${1:-}" ] && PATH=$(cd "$1" && pwd):$PATH
LUA=${V32LUA:-$(command -v v32lua)}
BIOS=${V32BIOS:-$(command -v StandardBios.v32)}
[ -n "$BIOS" ] || BIOS=$HERE/../tools/oracle/bin/StandardBios.v32
ORUN=${V32ORUN:-$(command -v v32orun)}
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
build heap_alloc; build heap_oom; build heap_free
# sim ARGS... CART: stdout only; stderr to $W/err; the status is the simulator's
sim() { timeout 120 "$SIM" --batch -r --fast -B "$BIOS" --fixed-clock --seed=1 "$@" 2> "$W/err" </dev/null; }

echo "== --heap: tables in a loop"
out=$(sim --max-frames=270 --heap "$W/heap_alloc.v32"); check "status" "$?" "0"
n=$(echo "$out" | grep -c '^HEAP F'); [ "$n" -ge 10 ] && ok || bad "heap lines: $n"
has   "line format"   "$out" "HEAP F"
echo "$out" | grep '^HEAP F' | head -1 | grep -qE '^HEAP F[0-9]+ ptr=0x[0-9A-F]{8} used=[0-9]+ peak=[0-9]+( free=[0-9]+ blocks=[0-9]+)?$' && ok || bad "format of the first line: $(echo "$out" | grep '^HEAP F' | head -1)"
hasnt "no arena fields without --gc arena" "$out" "depth="
# used never decreases, peak = used (nothing is ever freed) and grows
mono=$(echo "$out" | grep '^HEAP F' | sed 's/.*used=\([0-9]*\) peak=\([0-9]*\).*/\1 \2/' | awk 'BEGIN{p=-1;m=0} {if ($1<p) m=1; if ($2!=$1) m=1; p=$1} END{print m, (p>0 ? 1 : 0)}')
check "used is monotone, peak = used, heap grows" "$mono" "0 1"
last=$(echo "$out" | grep '^HEAP F' | tail -1 | sed 's/.* used=\([0-9]*\) .*/\1/')
has   "PEAK line"     "$out" "HEAP PEAK used=$last ptr=0x"
echo "$out" | grep -qE "^HEAP END ptr=0x[0-9A-F]{8} used=$last peak=$last( free=[0-9]+ blocks=[0-9]+)? frame=[0-9]+\$" && ok || bad "END line: $(echo "$out" | grep '^HEAP END')"
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
echo "$out" | grep -qE '^HEAP END ptr=0x[0-9A-F]{8}( free=[0-9]+ blocks=[0-9]+)? frame=[0-9]+$' && ok || bad "no .asm: END line has no used/peak: $(echo "$out" | grep '^HEAP END')"
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
echo "$out" | grep -qE 'depth=0 stack=-$' && ok || bad "depth back to 0, empty stack"
peak=$(echo "$out" | grep '^HEAP PEAK' | sed 's/.*used=\([0-9]*\) .*/\1/'); end=$(echo "$out" | grep '^HEAP END' | sed 's/.*used=\([0-9]*\) .*/\1/')
[ "$peak" -gt "$end" ] && ok || bad "arena.release lowers used but not the peak (peak $peak, end $end)"
has   "END line has the arena"  "$(echo "$out" | grep '^HEAP END')" "depth=0"

echo "== --heap: free lists (free= blocks=), v32lua with __mfree"
if grep -q '^%define  *FREE_MASK' "$W/heap_free.asm"; then
    out=$(sim --max-frames=290 --heap "$W/heap_free.v32"); check "status" "$?" "0"
    endl=$(echo "$out" | grep '^HEAP END')
    echo "$endl" | grep -qE '^HEAP END ptr=0x[0-9A-F]{8} used=[0-9]+ peak=[0-9]+ free=[0-9]+ blocks=[0-9]+ frame=[0-9]+$' && ok || bad "END format: $endl"
    free=$(echo "$endl" | sed 's/.* free=\([0-9]*\) .*/\1/'); blocks=$(echo "$endl" | sed 's/.* blocks=\([0-9]*\) .*/\1/'); used=$(echo "$endl" | sed 's/.* used=\([0-9]*\) .*/\1/')
    [ "$free" -gt 0 ] && ok || bad "free > 0 after a table outgrew its arrays: $endl"
    [ "$blocks" -gt 0 ] && ok || bad "blocks > 0: $endl"
    [ "$free" -lt "$used" ] && ok || bad "free < used: $endl"
    echo "$out" | grep '^HEAP F' | head -1 | grep -qE '^HEAP F[0-9]+ ptr=0x[0-9A-F]{8} used=[0-9]+ peak=[0-9]+ free=0 blocks=0$' && ok || bad "first line: free=0 blocks=0 before anything is freed"
    check "stderr is quiet" "$(cat "$W/err")" ""
    d1=$(sim --max-frames=290 --dump=400@0 "$W/heap_free.v32"); d2=$(sim --max-frames=290 --heap --dump=400@0 "$W/heap_free.v32" | grep -v '^HEAP')
    check "free-list walk does not change the machine (RAM dump)" "$d2" "$d1"
    grep -v '^%define  *FREE_' "$W/heap_free.asm" > "$W/nofree.asm"
    out=$(sim --max-frames=290 --heap="$W/nofree.asm" "$W/heap_free.v32")
    hasnt "no FREE_ symbols (older v32lua): no free= field" "$out" "free="
    has   "no FREE_ symbols: used still shown" "$(echo "$out" | grep '^HEAP END')" " used="
    sed -e 's/^%define  *FREE_MASK .*/%define FREE_MASK 0x00000000/' -e 's/^%define  *FREE_BINS .*/%define FREE_BINS 0x00000000/' "$W/heap_free.asm" > "$W/badfree.asm"
    out=$(sim --max-frames=290 --heap="$W/badfree.asm" "$W/heap_free.v32"); has "corrupt free list: free=? blocks=?" "$(echo "$out" | grep '^HEAP END')" " free=? blocks=?"
    if [ -n "$ORUN" ]; then
        o=$("$ORUN" "$W/heap_free.vbin" -a "$W/heap_free.asm" --heap -f 60)
        oe=$(echo "$o" | grep '^HEAP END')
        echo "$oe" | grep -qE '^HEAP END ptr=0x[0-9A-F]{8} used=[0-9]+ peak=[0-9]+ free=[0-9]+ blocks=[0-9]+ frame=[0-9]+$' && ok || bad "v32orun END format: $oe"
        check "v32orun and v32sim agree at the end (ptr used peak free blocks)" "$(echo "$oe" | sed 's/ frame=.*//')" "$(echo "$endl" | sed 's/ frame=.*//')"
        check "v32orun PEAK has the same format" "$(echo "$o" | grep '^HEAP PEAK' | sed 's/ frame=.*//')" "$(echo "$out" | grep -c xxx >/dev/null; sim --max-frames=290 --heap "$W/heap_free.v32" | grep '^HEAP PEAK' | sed 's/ frame=.*//')"
        check "v32orun -q keeps the HEAP F lines" "$("$ORUN" "$W/heap_free.vbin" -a "$W/heap_free.asm" --heap -q -f 60 | grep -c '^HEAP F')" "60"
        echo "$o" | grep '^HEAP F' | sed -n 10p | grep -qE '^HEAP F[0-9]+ ptr=0x[0-9A-F]{8} used=[0-9]+ peak=[0-9]+ free=[0-9]+ blocks=[0-9]+$' && ok || bad "v32orun frame line format"
    fi
else
    echo "  (this v32lua has no free lists: skipped)"
fi
if [ -n "$ORUN" ]; then
    echo "== v32orun: the same options on the same cartridges (also with v32as debug files)"
    build heap_arena --gc arena
    o=$("$ORUN" "$W/heap_arena.vbin" -a "$W/heap_arena.asm" --heap -f 20)
    echo "$o" | grep -qE 'depth=2 stack=0x[0-9A-F]{8},0x[0-9A-F]{8}$' && ok || bad "v32orun: depth 2 stack format"
    echo "$o" | grep -qE '^HEAP F[0-9]+ .*depth=0 stack=-$' && ok || bad "v32orun: empty stack is -"
    has "v32orun: END with arena fields last" "$(echo "$o" | grep '^HEAP END')" " frame="
    echo "$o" | grep '^HEAP END' | grep -qE 'frame=[0-9]+ floor=0x[0-9A-F]{8} pin=0x[0-9A-F]{8} depth=0 stack=-$' && ok || bad "v32orun: END line order"
    grep -v '^%define  *HEAP_START' "$W/heap_alloc.asm" > "$W/nostart.asm"
    o=$("$ORUN" "$W/heap_alloc.vbin" -a "$W/nostart.asm" --heap -f 3 2>"$W/err"); st=$?
    check "v32orun without HEAP_START: runs (exit 0)" "$st" "0"; has "v32orun without HEAP_START: warning" "$(cat "$W/err")" "HEAP_START is unknown"
    echo "$o" | grep '^HEAP F' | head -1 | grep -qE '^HEAP F0 ptr=0x[0-9A-F]{8}( free=[0-9]+ blocks=[0-9]+)?$' && ok || bad "v32orun without HEAP_START: ptr only"
    o=$("$ORUN" "$W/heap_oom.vbin" --expect-reach __oom_handler -f 3000); st=$?
    check "v32orun --expect-reach __oom_handler (v32as or stock debug file): exit 0" "$st" "0"; has "v32orun REACHED" "$o" "EXPECT REACHED __oom_handler"
    o=$("$ORUN" "$W/heap_oom.vbin" --expect-reach __function_game_loop -f 3); has "v32orun __function_game_loop" "$o" "EXPECT REACHED __function_game_loop"
    "$ORUN" "$W/heap_oom.vbin" --expect-reach __nolabel -f 3 >/dev/null 2>&1; check "v32orun unknown label: exit 2" "$?" "2"
fi

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
