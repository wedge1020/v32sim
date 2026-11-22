#!/bin/bash
# --turbo, the deferred GPU painting and the hot-path changes of 20261122: results must not change.
#
#   tests/run_turbo_tests.sh <toolchain dir>      (compile, assemble, packrom and StandardBios.v32 in it: the stock chain,
#                                                  as built by tools/oracle/build.sh; no v32cc needed)
#
# What is checked, with carts from tests/turbo/:
#   * spin_pure: a poll loop with no stores. A --turbo run gives the same RAM, the same per-frame statistics (including
#     cpu.instructions and bus.gamepad_reads) and the same exit status as a normal run, and --turbo=report says cycles were skipped.
#   * spin_store: a poll loop that stores every trip. Nothing may be skipped, results equal.
#   * gfxmix: every draw kind and blending mode. Screenshots of the normal (deferred) run, of --gpu-eager (painted at once)
#     and of --turbo are byte-identical.
#   * overdraw: no clear, so the pending queue fills: a normal run and --gpu-eager give the same picture; --turbo drops the
#     queue, warns when a screenshot is taken, and --turbo=report counts the dropped draws.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
TC=$(cd "${1:?toolchain dir (compile assemble packrom StandardBios.v32)}" && pwd)
BIOS=$TC/StandardBios.v32
for t in compile assemble packrom; do [ -x "$TC/$t" ] || { echo "need $t in $TC"; exit 2; }; done
[ -f "$BIOS" ] || { echo "need StandardBios.v32 in $TC"; exit 2; }
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $1"; }
same() { if cmp -s "$2" "$3"; then ok; else bad "$1 ($2 != $3)"; fi; }
for n in spin_pure spin_store gfxmix overdraw; do
    V32TC=$TC "$HERE/../tools/v32mkcart.sh" --stock -o "$W" "$HERE/turbo/$n.c" >/dev/null 2>&1 || { echo "cannot build $n"; exit 2; }
done
run() { # tag cart frames args... : stdout, stderr and the status go to $W/TAG.out/.err/.rc
    local tag=$1 cart=$2 frames=$3; shift 3
    ( cd "$W" && timeout 300 "$SIM" -B "$BIOS" --fast --batch -r -x --fixed-clock --seed=1 --max-frames=$frames "$@" "$cart.v32" </dev/null >"$tag.out" 2>"$tag.err"; echo $? >"$tag.rc" )
}

echo "== spin loops"
run sp_a spin_pure 700 --dump=4@frames --frame-stats=sp_a.csv
run sp_b spin_pure 700 --dump=4@frames --frame-stats=sp_b.csv --turbo=report
same "spin_pure: RAM" "$W/sp_a.out" "$W/sp_b.out"
same "spin_pure: per-frame statistics" "$W/sp_a.csv" "$W/sp_b.csv"
same "spin_pure: exit status" "$W/sp_a.rc" "$W/sp_b.rc"
n=$(sed -n 's/.*fast-forwarded \([0-9]*\) spin-loop.*/\1/p' "$W/sp_b.err")
[ "${n:-0}" -gt 1000000 ] && ok || bad "spin_pure: --turbo=report should show skipped cycles (got '${n:-none}')"
run ss_a spin_store 500 --dump=4@frames --frame-stats=ss_a.csv
run ss_b spin_store 500 --dump=4@frames --frame-stats=ss_b.csv --turbo=report
same "spin_store: RAM" "$W/ss_a.out" "$W/ss_b.out"
same "spin_store: per-frame statistics" "$W/ss_a.csv" "$W/ss_b.csv"
n=$(sed -n 's/.*fast-forwarded \([0-9]*\) spin-loop.*/\1/p' "$W/ss_b.err")
[ "${n:-1}" = "0" ] && ok || bad "spin_store: nothing may be skipped (got '${n:-none}')"
run sp_c spin_pure 700 --dump=4@frames --frame-stats=sp_c.csv --turbo=report --profile-out=sp_c.prof
n=$(sed -n 's/.*fast-forwarded \([0-9]*\) spin-loop.*/\1/p' "$W/sp_c.err")
[ "${n:-1}" = "0" ] && ok || bad "a profile run must not skip (got '$n')"

echo "== deferred painting"
SH="--screenshot-at=1-90/6,150"
run g_a gfxmix 400 $SH --screenshot-file=g_a-%d.png
run g_b gfxmix 400 $SH --screenshot-file=g_b-%d.png --gpu-eager
run g_c gfxmix 400 $SH --screenshot-file=g_c-%d.png --turbo
cnt=$(ls "$W"/g_a-*.png 2>/dev/null | wc -l)
[ "$cnt" -ge 15 ] && ok || bad "gfxmix: screenshots were written ($cnt)"
for f in "$W"/g_a-*.png; do b=$(basename "$f"); same "gfxmix $b: deferred = eager" "$f" "$W/g_b-${b#g_a-}"; same "gfxmix $b: deferred = --turbo" "$f" "$W/g_c-${b#g_a-}"; done
run o_a overdraw 2300 --screenshot-at=1900 --screenshot-file=o_a-%d.png
run o_b overdraw 2300 --screenshot-at=1900 --screenshot-file=o_b-%d.png --gpu-eager
run o_c overdraw 2300 --screenshot-at=1900 --screenshot-file=o_c-%d.png --turbo=report
same "overdraw: deferred = eager after a full queue" "$W/o_a-1900.png" "$W/o_b-1900.png"
grep -q "warning: --turbo skipped" "$W/o_c.err" && ok || bad "overdraw: --turbo screenshot warning"
grep -q "dropped [1-9][0-9]* queued GPU draws" "$W/o_c.err" && ok || bad "overdraw: --turbo=report counts dropped draws"
! grep -q "warning" "$W/o_a.err" && ok || bad "overdraw: no warning without --turbo"
run o_d overdraw 450 --screenshot-at=100 --screenshot-file=o_d-%d.png
run o_e overdraw 450 --screenshot-at=100 --screenshot-file=o_e-%d.png --turbo
[ -s "$W/o_d-100.png" ] && ok || bad "overdraw: short run screenshot written"
same "overdraw: a short --turbo run is exact" "$W/o_d-100.png" "$W/o_e-100.png"

echo "---- $pass passed, $fail failed"
[ $fail = 0 ]
