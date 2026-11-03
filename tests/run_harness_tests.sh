#!/bin/bash
# tests/run_harness_tests.sh -- tests of the consolidated tools: the harness scripts, the oracle comparisons.
#
#   V32TC=DIR V32BIOS=StandardBios.v32 [ORACLE=DIR] tests/run_harness_tests.sh
#
#   V32TC     a directory with v32cc, v32as, packrom and (for the stock-chain checks) compile, assemble + include/
#   V32BIOS   the BIOS (StandardBios.v32)
#   ORACLE    a directory built by tools/oracle/build.sh (v32run, v32prof, v32peek, ...); without it the checks that
#             compare v32sim with the stock console logic are skipped, and said to be
#   V32SIM    the simulator (default bin/v32sim)
#   V32SRCMARK  v32opt's scripts/v32srcmark.py: enables the check of source-level debugging of stock-compiler carts
#
# What is checked, and against what:
#   selfcheck   a passing and a failing cartridge: v32sim says pass/FAIL, and so does the stock core (v32run)
#   symbols     a cartridge built by the STOCK compiler (no .asm.sym, only `%define global_X N` lines): --dump=1@NAME works
#   frame-stats --frame-stats rows equal v32prof's, frame for frame (CPU % and GPU %), on a cartridge whose load varies
#   input       a gamepad script replayed through tools/vprof2cmd.py gives the same per-frame history as v32peek
#   peek/golden/addrprof   the example cartridge, a typed-less session with a golden screen, and the function profile
set -u
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$HERE/..; T=$ROOT/tools; EX=$HERE/examples
SIM=${V32SIM:-$ROOT/bin/v32sim}; export V32SIM=$SIM
: "${V32TC:?set V32TC to the toolchain directory}"; : "${V32BIOS:?set V32BIOS}"; export V32TC V32BIOS
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
pass=0; fail=0; skip=0
ok()   { pass=$((pass+1)); echo "  ok    $1"; }
bad()  { fail=$((fail+1)); echo "  FAIL  $1"; }
skipt(){ skip=$((skip+1)); echo "  skip  $1 ($2)"; }
[ -x "$SIM" ] || { echo "no v32sim at $SIM (make, or tools/build-nogen.sh)"; exit 2; }
HAVE_ORACLE=0; [ -n "${ORACLE:-}" ] && [ -x "$ORACLE/v32prof" ] && HAVE_ORACLE=1
HAVE_STOCK=0; [ -x "$V32TC/compile" ] && [ -x "$V32TC/assemble" ] && HAVE_STOCK=1

echo "== build the example cartridges"
sh "$T/v32mkcart.sh" -o "$W/ex" "$EX/hello_text.c" >/dev/null || { echo "build failed"; exit 2; }
sh "$T/v32mkcart.sh" -o "$W/exk" --cc-opt -DKEEP_RUNNING "$EX/hello_text.c" >/dev/null || exit 2
sed 's/counter == 5050/counter == 5051/' "$EX/hello_text.c" > "$W/hello_bad.c"
sh "$T/v32mkcart.sh" -o "$W/bad" "$W/hello_bad.c" >/dev/null || exit 2

echo "== selfcheck"
sh "$T/v32sim-selfcheck.sh" "$W/ex/hello_text.v32" >/dev/null && ok "passing cartridge passes" || bad "passing cartridge"
sh "$T/v32sim-selfcheck.sh" "$W/bad/hello_bad.v32" >/dev/null && bad "failing cartridge passed" || ok "failing cartridge fails"
if [ $HAVE_ORACLE = 1 ]; then
    V32RUN=$ORACLE/v32run sh "$T/v32sim-selfcheck.sh" --oracle "$W/ex/hello_text.v32" >/dev/null && ok "stock core agrees: pass" || bad "stock core: pass"
    V32RUN=$ORACLE/v32run sh "$T/v32sim-selfcheck.sh" --oracle "$W/bad/hello_bad.v32" >/dev/null && bad "stock core: failing passed" || ok "stock core agrees: fail"
else skipt "selfcheck --oracle" "no ORACLE"; fi

echo "== symbols from a stock-compiler .asm"
if [ $HAVE_STOCK = 1 ]; then
    sh "$T/v32mkcart.sh" --stock -o "$W/stock" "$EX/hello_text.c" >/dev/null || exit 2
    rm -f "$W/stock/hello_text.asm.sym"
    v=$("$SIM" --batch -r --fast -x --max-frames=1200 -B "$V32BIOS" --dump=1@counter "$W/stock/hello_text.v32" 2>/dev/null </dev/null | tail -1)
    [ "$v" = 5050 ] && ok "--dump=1@counter via %define lines" || bad "--dump=1@counter gave '$v'"
else skipt "stock symbols" "no stock compile/assemble in V32TC"; fi

echo "== frame statistics against v32prof"
if [ $HAVE_ORACLE = 1 ] && [ $HAVE_STOCK = 1 ]; then
    sh "$T/v32mkcart.sh" --stock -o "$W/ld" "$EX/load_pattern.c" >/dev/null || exit 2
    echo "400 end" > "$W/end.script"
    "$ORACLE/v32prof" "$V32BIOS" "$W/ld/load_pattern.v32" "$W/ld/load_pattern.vbin.debug" "$W/end.script" 85 "$W/prof.csv" >/dev/null 2>&1
    "$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --frame-stats="$W/sim.csv" "$W/ld/load_pattern.v32" >/dev/null 2>"$W/fs.err" </dev/null
    r=$(python3 -I - "$W/prof.csv" "$W/sim.csv" <<'PY'
import csv, sys
a = list(csv.reader(open(sys.argv[1]))); b = list(csv.reader(open(sys.argv[2])))[1:]
bad = [i for i, (x, y) in enumerate(zip(a, b)) if x[2] != y[2] or x[3] != y[4]]
print("%d %d %d" % (len(a), len(b), len(bad)))
PY
)
    [ "$r" = "400 400 0" ] && ok "400 frames, CPU % and GPU % identical row for row" || bad "frame stats differ: rows/rows/mismatches = $r"
    grep -q "frames at 100%" "$W/fs.err" && ok "summary printed" || bad "no summary"
else skipt "frame-stats vs v32prof" "needs ORACLE and the stock chain"; fi

echo "== gamepad input replay against v32peek"
if [ $HAVE_ORACLE = 1 ]; then
    sh "$T/v32mkcart.sh" -o "$W/pad" "$EX/pad_log.c" >/dev/null || exit 2
    AL=$(sed -n 's/^V,la,\([0-9]*\),.*/\1/p' "$W/pad/pad_log.asm.sym"); AB=$(sed -n 's/^V,ab,\([0-9]*\),.*/\1/p' "$W/pad/pad_log.asm.sym")
    words() { python3 -I -c '
import re, sys
w = []
for l in sys.stdin:
    m = re.match(r"\s*\S*?(?:\(0x[0-9A-Fa-f]+\))?:\s*((?:-?\d+\s*)+)$", l)
    if m: w += m.group(1).split()
print(" ".join(w))'; }
    "$ORACLE/v32peek" "$V32BIOS" "$W/pad/pad_log.v32" "$EX/pad_log.script" 640 "$AL" 300 "$AB" 300 2>/dev/null | words > "$W/peek.w"
    python3 -I "$T/vprof2cmd.py" "$EX/pad_log.script" --frames 640 --cmd "x/d la 300" --cmd "x/d ab 300" > "$W/in.cmd"
    "$SIM" --batch --fast -x --max-frames=700 -B "$V32BIOS" "$W/pad/pad_log.v32" < "$W/in.cmd" 2>/dev/null | words > "$W/sim.w"
    n=$(wc -w < "$W/peek.w")
    if [ "$n" = 600 ] && cmp -s "$W/peek.w" "$W/sim.w"; then ok "600 words of button history identical"; else bad "button history differs ($n words)"; fi
else skipt "input replay vs v32peek" "no ORACLE"; fi

echo "== stock compiler + v32opt's v32srcmark.py: source-level debugging"
if [ $HAVE_STOCK = 1 ] && [ -n "${V32SRCMARK:-}" ] && [ -f "$V32SRCMARK" ]; then
    export V32SRCMARK
    sh "$T/v32mkcart.sh" --stock --srcmark -o "$W/sm" "$EX/hello_text.c" >/dev/null 2>&1 || exit 2
    bt=$(printf 'break add_up\ncontinue\nbt\nquit\n' | "$SIM" --batch --fast -B "$V32BIOS" -L C --source "$W/sm/hello_text.v32" 2>&1 | grep '^#0')
    case $bt in *"add_up (hello_text.c:"*) ok "bt names the C function and line: $bt";; *) bad "bt gave '$bt'";; esac
else skipt "srcmark" "set V32SRCMARK to v32opt's scripts/v32srcmark.py (and have the stock chain in V32TC)"; fi

echo "== peek, golden screen, address profile"
v=$(sh "$T/v32sim-peek.sh" -F 400 "$W/ex/hello_text.v32" counter); [ "$v" = 5050 ] && ok "v32sim-peek counter" || bad "v32sim-peek gave '$v'"
t=$(sh "$T/v32sim-peek.sh" -t -F 400 "$W/ex/hello_text.v32" message 13); [ "$t" = "hello, v32sim" ] && ok "v32sim-peek -t decodes text" || bad "v32sim-peek -t gave '$t'"
sh "$T/v32sim-golden.sh" --cart "$W/exk/hello_text.v32" --bios "$V32BIOS" --sessions "$EX/sessions" --golden "$EX/golden" --kbd-port 0 --boot-frames 260 >/dev/null \
    && ok "golden screen matches" || bad "golden screen"
"$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --profile-out="$W/h.prof" "$W/ex/hello_text.v32" >/dev/null 2>&1 </dev/null
top=$(python3 -I "$T/v32sim-addrprof.py" "$W/h.prof" --map "cart:$W/ex/hello_text.vbin.debug" -n 3 | sed -n 3p | awk '{print $3}')
[ "$top" = "cart:add_up" ] && ok "addrprof names the hottest cartridge function" || bad "addrprof top was '$top'"

echo "== $pass passed, $fail failed, $skip skipped"
[ $fail -eq 0 ]
