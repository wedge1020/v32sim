#!/bin/bash
# tests/run_harness_tests.sh -- tests of the consolidated tools: the harness scripts, the oracle comparisons.
#
#   V32TC=DIR V32BIOS=StandardBios.v32 [ORACLE=DIR] tests/run_harness_tests.sh
#
#   V32TC     a directory with v32cc, v32as, packrom and (for the stock-chain checks) compile, assemble + include/;
#             v32lua there enables the Lua locals checks
#   V32BIOS   the BIOS (StandardBios.v32)
#   ORACLE    a directory built by tools/oracle/build.sh (v32run, v32prof, v32peek, ...); without it the checks that
#             compare v32sim with the stock console logic are skipped, and said to be
#   V32SIM    the simulator (default bin/v32sim)
#   V32SRCMARK  v32opt's scripts/v32srcmark.py: enables the check of source-level debugging of stock-compiler carts
#   GBTOV32_TREE   a built GBtoV32 tree: with ORACLE/v32trace-ref, v32sim's V32TRACE files are compared with the recorder's
#
# What is checked, and against what:
#   selfcheck   a passing and a failing cartridge: v32sim says pass/FAIL, and so does the stock core (v32run)
#   symbols     a cartridge built by the STOCK compiler (no .asm.sym, only `%define global_X N` lines): --dump=1@NAME works
#   frame-stats --frame-stats rows equal v32prof's, frame for frame (CPU % and GPU %), on a cartridge whose load varies
#   input       a gamepad script replayed through tools/vprof2cmd.py gives the same per-frame history as v32peek
#   v32orun     the one oracle runner: gpu_dropped (model and real GPU), -b, -t, -g, and the refrun/v32run/vrun wrappers
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

echo "== --input (V32_INPUT scripts, cartridge frames)"
sh "$T/v32mkcart.sh" -o "$W/pin" "$EX/pad_log.c" >/dev/null || exit 2
la=$("$SIM" --batch -r --fast -x --max-frames=700 -B "$V32BIOS" --input-port=1 --input="LEFT@50-54,A+LEFT@60-61" --dump=300@la "$W/pin/pad_log.v32" 2>/dev/null </dev/null | sed -n '50,63p' | tr '\n' ' ')
ab=$("$SIM" --batch -r --fast -x --max-frames=700 -B "$V32BIOS" --input-port=1 --input="LEFT@50-54,A+LEFT@60-61" --dump=300@ab "$W/pin/pad_log.v32" 2>/dev/null </dev/null | sed -n '60,63p' | tr '\n' ' ')
[ "$la" = "-3600 1 2 3 4 5 -1 -2 -3 -4 -5 1 2 -1 " ] && ok "LEFT held in cart frames 50-54 and 60-61 reads 1.. in exactly those frames" || bad "--input LEFT: '$la'"
[ "$ab" = "-3600 1 2 -1 " ] && ok "A+LEFT: two buttons in one run; --dump of a symbol named like hex (ab)" || bad "--input A / --dump=@ab: '$ab'"

echo "== corpus runner"
out=$(python3 -I "$T/v32sim-corpus.py" -B "$V32BIOS" -F 400 --shots "$W/shots" -o "$W/corpus.csv" "$W/ex/hello_text.v32" "$W/exk/hello_text.v32" 2>&1)
grep -q "^hello_text,HALT," "$W/corpus.csv" && grep -q "^hello_text,OK," "$W/corpus.csv" && [ -f "$W/shots/hello_text.png" ] \
    && ok "HALT and OK classified, CSV and screenshot written" || bad "corpus: $(echo "$out" | head -5 | tr '\n' '|')"

echo "== stock compiler + v32opt's v32srcmark.py: source-level debugging"
if [ $HAVE_STOCK = 1 ] && [ -n "${V32SRCMARK:-}" ] && [ -f "$V32SRCMARK" ]; then
    export V32SRCMARK
    sh "$T/v32mkcart.sh" --stock --srcmark -o "$W/sm" "$EX/hello_text.c" >/dev/null 2>&1 || exit 2
    bt=$(printf 'break add_up\ncontinue\nbt\nquit\n' | "$SIM" --batch --fast -B "$V32BIOS" -L C --source "$W/sm/hello_text.v32" 2>&1 | grep '^#0')
    case $bt in *"add_up (hello_text.c:"*) ok "bt names the C function and line: $bt";; *) bad "bt gave '$bt'";; esac
else skipt "srcmark" "set V32SRCMARK to v32opt's scripts/v32srcmark.py (and have the stock chain in V32TC)"; fi

echo "== locals (V32DBG section 4: NAME.vbin.vars)"
sh "$T/v32mkcart.sh" -o "$W/loc" "$EX/locals.c" >/dev/null || exit 2
if [ -f "$W/loc/locals.vbin.vars" ]; then
    out=$(printf 'break locals.c:28\ncontinue\nlocals i\nexpect i == 99\nunbreak 0\nbreak locals.c:9\ncontinue\nlocals\nup\nlocals\nquit\n' |
          "$SIM" --batch --fast -x -B "$V32BIOS" -L C "$W/loc/locals.v32" 2>"$W/loc.err")
    st=$?
    echo "$out" | grep -q "^i = 99 " && ok "the inner block's i shadows the loop's" || bad "shadowing: $(echo "$out" | head -3)"
    [ $st = 0 ] && ok "expect on a local" || bad "expect i == 99 (status $st)"
    echo "$out" | grep -q "^f = 1.5 .*float, BP+2" && echo "$out" | grep -q "^n = 130 " && echo "$out" | grep -q "^r = 195 .*float" \
        && ok "float and int parameters, a float local" || bad "scale's locals: $(echo "$out" | tr '\n' '|' | cut -c1-200)"
    echo "$out" | grep -q "^#1  main" && ! echo "$out" | sed -n '/^#1  main/,$p' | grep -q "^f = 1.5" \
        && ok "up: the caller's locals, not the callee's" || bad "up/locals"
    out=$(printf 'break locals.c:23\ncontinue\nlocals\nquit\n' | "$SIM" --batch --fast -x -B "$V32BIOS" -L C "$W/loc/locals.v32" 2>/dev/null)
    echo "$out" | grep -q "^arr = {[-0-9]*, [-0-9]*, [-0-9]*, [-0-9]*} .*int\[4\]" && echo "$out" | grep -qE "^p = (\{0x00000003, 0x00000005\} .*struct:pt:2|\{x = 3, y = 5\} .*struct:pt:2)" && echo "$out" | grep -q "^q = 0x.*ptr" \
        && ! echo "$out" | grep -q "^f = " && ok "array, struct, pointer; another function's variables stay out" || bad "work's locals: $(echo "$out" | tr '\n' '|' | cut -c1-250)"
else skipt "locals (C)" "this v32as writes no .vbin.vars"; fi
if [ -x "$V32TC/v32lua" ]; then
    mkdir -p "$W/lua"; cp "$EX/locals.lua" "$W/lua/"
    ( cd "$W/lua" && "$V32TC/v32lua" -g -o locals.asm locals.lua >/dev/null 2>&1 && "$V32TC/v32as" -g -o locals.vbin locals.asm >/dev/null 2>&1 &&
      sed 's|<binary path="[^"]*"|<binary path="locals.vbin"|' locals.xml > l.xml && "$V32TC/packrom" l.xml -o locals.v32 >/dev/null 2>&1 ) || bad "building locals.lua"
    out=$(printf 'break locals.lua:5\ncontinue\nnext\nlocals\nbreak locals.lua:19\ncontinue\nlocals\nquit\n' | "$SIM" --batch --fast -x -B "$V32BIOS" -L lua "$W/lua/locals.v32" 2>/dev/null)
    echo "$out" | grep -q '^n = 10.0000 .*boxed' && ok "a boxed Lua local is read through its box" || bad "boxed: $(echo "$out" | head -4 | tr '\n' '|')"
    echo "$out" | grep -q '^s = "hello"' && echo "$out" | grep -q '^ok = true' && echo "$out" | grep -q '^nothing = nil' && echo "$out" | grep -q '^t = table: ' \
        && ok "Lua strings, booleans, nil, tables" || bad "Lua values: $(echo "$out" | tr '\n' '|' | cut -c1-250)"
else skipt "locals (Lua)" "no v32lua in V32TC"; fi

echo "== V32TRACE report against the DesktopEmulator recorder (needs GBTOV32_TREE and ORACLE/v32trace-ref)"
if [ -n "${GBTOV32_TREE:-}" ] && [ -x "$GBTOV32_TREE/bin/GBtoV32" ] && [ $HAVE_ORACLE = 1 ] && [ -x "$ORACLE/v32trace-ref" ]; then
    # (GBtoV32 reads GBTOV32 as the path of its translator and V32SIM as its mini interpreter: both unset for it)
    # GBtoV32's gap ROM, built with a profile that never pressed A: A held from cart frame 40 runs into a trap (tools/test_trace.sh)
    mkdir -p "$W/gate" && python3 -I "$GBTOV32_TREE/tools/gaprom.py" "$W/gate/gap.gb" >/dev/null
    ( cd "$W/gate" && env -u V32SIM -u GBTOV32 PROFILE_FRAMES=60 PROFILE_INPUT=RIGHT@0-1 sh "$GBTOV32_TREE/BUILD.sh" gap.gb --no-audio --no-bezels >"$W/gate/build.log" 2>&1 )
    G=$W/gate/out/gap/gap.v32
    if [ -f "$G" ]; then
        mkdir -p "$W/gate/ref"
        printf '0 connect 0 1\n280 mark start\n290 press 0 a\n295 release 0 a\n350 press 0 right\n381 release 0 right\n480 mark later\n' > "$W/gate/ref.s"
        "$ORACLE/v32trace-ref" "$V32BIOS" "$G" "$W/gate/ref.s" 780 "$W/gate/ref" >/dev/null 2>&1
        printf 'run 280\nmark start\nrun 200\nmark later\nrun 300\nquit\n' | "$SIM" -q --fast -B "$V32BIOS" --input="A@40-44,RIGHT@100-130" \
            --v32trace="$W/gate/sim" "$G" >/dev/null 2>&1
        R=$(ls "$W"/gate/ref/gap_2*.trace.txt 2>/dev/null | head -1)
        # the same lines (equal hot-spot counts may list in another order), headers and the replay hint aside
        norm() { sed -e 1,3d -e '/Replay with/d' -e 's|^labels: .*(|labels: (|' "$1" | sort; }
        if [ -n "$R" ] && cmp -s <(norm "$R") <(norm "$W/gate/sim.trace.txt"); then ok "trace.txt: every line equal to the recorder's"; else bad "trace.txt differs from the recorder's"; fi
        cmp -s <(sed 1d "$W"/gate/ref/gap_2*.traps.txt) <(sed 1d "$W/gate/sim.traps.txt") && ok "traps.txt equal (2.1 segstart/input included)" || bad "traps.txt differs"
        cmp -s <(sort "$W"/gate/ref/gap_2*.blocks.txt) <(sort "$W/gate/sim.blocks.txt") && ok "blocks.txt equal" || bad "blocks.txt differs"
        s=$(python3 -I "$GBTOV32_TREE/tools/trace2input.py" "$W/gate/sim.trace.txt" --session 2>&1)
        [ "$s" = "RIGHT@1-31" ] && ok "trace2input --session reads v32sim's report" || bad "trace2input --session gave '$s'"
    else bad "GBtoV32 did not build the gap cartridge: $(tail -3 "$W/gate/build.log" | tr "\n" "|")"; fi
else skipt "V32TRACE report vs recorder" "set GBTOV32_TREE to a built GBtoV32 tree and build v32trace-ref (ONLY=v32trace-ref tools/oracle/build.sh PATCHED_CHECKOUT)"; fi

echo "== V32TRACE input2= (VS-18): gamepad 2 in the report, replayed by --input2"
mkdir -p "$W/t2" && sh "$T/v32mkcart.sh" -o "$W/t2b" "$EX/pad_bits.c" >/dev/null 2>&1 && cp "$W/t2b/pad_bits.v32" "$W/t2/" \
  && printf '0x20000000 _start\n' > "$W/t2/pad_bits.labels.txt" && printf 'format 2\narch test\ntrap _start\n' > "$W/t2/pad_bits.v32trace.meta"
"$SIM" --batch -r --fast -x --max-frames=420 -B "$V32BIOS" --input="A@5-7" --input2="B@6-9,UP+LEFT@12-12" --v32trace="$W/t2/o" "$W/t2/pad_bits.v32" >/dev/null 2>&1
grep -q '^V32_INPUT=A@5-7$' "$W/t2/o.trace.txt" && grep -q '^V32_INPUT2=B@6-9,LEFT+UP@12-12$' "$W/t2/o.trace.txt" && ok "--input2 records gamepad 2 (V32_INPUT2= in the report)" || bad "V32_INPUT2: $(grep INPUT "$W/t2/o.trace.txt")"
"$SIM" --batch -r --fast -x --max-frames=420 -B "$V32BIOS" --input="@$W/t2/o.trace.txt" --input2="@$W/t2/o.trace.txt" --v32trace="$W/t2/o2" "$W/t2/pad_bits.v32" >/dev/null 2>&1
cmp -s <(grep '^V32_INPUT' "$W/t2/o.trace.txt") <(grep '^V32_INPUT' "$W/t2/o2.trace.txt") && ok "--input=@FILE --input2=@FILE replays both pads (round trip)" || bad "input2 round trip"
grep -q 'segstart=0 input=[^ ]* input2=' "$W/t2/o.traps.txt" && ok "traps.txt lines get input2= when gamepad 2 was used" || bad "traps.txt input2"
"$SIM" --batch -r --fast -x --max-frames=420 -B "$V32BIOS" --input="A@5-7" --v32trace="$W/t2/o3" "$W/t2/pad_bits.v32" >/dev/null 2>&1
grep -q 'input2' "$W/t2/o3.trace.txt" "$W/t2/o3.traps.txt" && bad "input2 appears without a second pad" || ok "no input2 anywhere when gamepad 2 was never used (reports stay equal to the recorder's)"

echo "== symbol sources: var_NAME (v32lua) and NAME.vbin.sym (read after NAME.asm.sym; sizes survive)"
addr=$(grep '^V,counter,' "$W/ex/hello_text.asm.sym" | cut -d, -f3)
if [ -n "$addr" ]; then
    mkdir -p "$W/sy"; cp "$W/ex/hello_text.v32" "$W/sy/a.v32"
    printf '%%define var_cnt %s\n' "$addr" > "$W/sy/a.asm"
    d() { "$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --dump="1@$1" "$W/sy/a.v32" 2>&1 </dev/null | tail -1; }
    [ "$(d cnt)" = 5050 ] && ok "%define var_NAME is read (as NAME)" || bad "var_NAME: '$(d cnt)'"
    [ "$(d var_cnt)" = 5050 ] && ok "var_NAME also resolves by its full name" || bad "var_cnt: '$(d var_cnt)'"
    printf '# a comment line\nV,viasym,%s,1,x.c,1\n' "$addr" > "$W/sy/a.vbin.sym"
    [ "$(d viasym)" = 5050 ] && ok "NAME.vbin.sym is read (comment skipped)" || bad "vbin.sym: '$(d viasym)'"
    case "$(d cnt)" in *ERROR*) ok ".vbin.sym replaces .asm.sym/.asm";; *) bad ".asm still read beside .vbin.sym";; esac
else bad "no counter address in hello_text.asm.sym"; fi

echo "== BIOS alone (cartridge '-') and ram:0x... addresses"
"$SIM" --batch -r --fast -x --max-frames=5 -B "$V32BIOS" - >/dev/null 2>&1 </dev/null; [ $? = 2 ] && ok "'-' runs the BIOS with no cartridge (frame limit, status 2)" || bad "BIOS alone"
o=$(printf 'print [ram:0x75]\nquit\n' | "$SIM" --batch -B "$V32BIOS" - 2>&1 | head -1)
case "$o" in "[0x00000075("*) ok "[ram:0x75] is the RAM word, not IOPort 0x75";; *) bad "[ram:0x75] gave '$o'";; esac

echo "== peek, golden screen, address profile"
v=$(sh "$T/v32sim-peek.sh" -F 400 "$W/ex/hello_text.v32" counter); [ "$v" = 5050 ] && ok "v32sim-peek counter" || bad "v32sim-peek gave '$v'"
t=$(sh "$T/v32sim-peek.sh" -t -F 400 "$W/ex/hello_text.v32" message 13); [ "$t" = "hello, v32sim" ] && ok "v32sim-peek -t decodes text" || bad "v32sim-peek -t gave '$t'"
sh "$T/v32sim-golden.sh" --cart "$W/exk/hello_text.v32" --bios "$V32BIOS" --sessions "$EX/sessions" --golden "$EX/golden" --kbd-port 0 --boot-frames 260 >/dev/null \
    && ok "golden screen matches" || bad "golden screen"
"$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --profile-out="$W/h.prof" "$W/ex/hello_text.v32" >/dev/null 2>&1 </dev/null
top=$(python3 -I "$T/v32sim-addrprof.py" "$W/h.prof" --map "cart:$W/ex/hello_text.vbin.debug" -n 3 | sed -n 3p | awk '{print $3}')
[ "$top" = "cart:add_up" ] && ok "addrprof names the hottest cartridge function" || bad "addrprof top was '$top'"

echo "== v32orun: the one oracle runner"
if [ $HAVE_ORACLE = 1 ] && [ -x "$ORACLE/v32orun" ]; then
    sh "$T/v32mkcart.sh" -o "$W/od" "$EX/overdraw.c" >/dev/null || exit 2
    O="$ORACLE/v32orun"
    r=$("$O" "$W/od/overdraw.vbin" -f 10 -q | head -1)
    case "$r" in *"gpu_dropped=30 "*) ok "bare: 3 of 12 draws dropped per frame (model of the pixel budget)";; *) bad "bare gpu_dropped: $r";; esac
    r=$("$O" --bios "$V32BIOS" --cart "$W/od/overdraw.v32" -f 330 -q | head -1)
    n=$(echo "$r" | sed 's/.*gpu_dropped=\([0-9]*\).*/\1/')
    [ "${n:-0}" -ge 30 ] && [ $((n % 3)) = 0 ] && ok "console: the real GPU's own budget gives the same 3 per frame ($n)" || bad "console gpu_dropped: $r"
    "$O" "$W/od/overdraw.vbin" -f 4 -q -b | grep -c '^FC ' | grep -qx 4 && ok "-b: one FC line per frame" || bad "-b"
    ip=$("$O" "$W/od/overdraw.vbin" -f 1 -q | head -1 | sed 's/.* ip=\([0-9A-F]*\).*/\1/')
    "$O" "$W/od/overdraw.vbin" -f 5 -q -t "$ip" | grep -q '^STATUS TRAPPED' && ok "-t ADDRESS traps and prints the trail and stack" || bad "-t $ip"
    "$O" "$W/od/overdraw.vbin" -f 2 -g | grep -q 'GPU-DROPPED DrawRegion' && ok "-g tags the dropped draws" || bad "-g GPU-DROPPED"
    r=$("$ORACLE/vrun" "$W/ex/hello_text.vbin" -f 3 -d 1 | tail -1); [ "$r" = "STATUS timeout" ] || [ "$r" = "STATUS halted" ] && ok "vrun wrapper: a .vbin as BIOS" || bad "vrun: $r"
    r=$("$ORACLE/v32run" "$V32BIOS" "$W/ex/hello_text.v32" - 0 400 2>/dev/null | tail -1); case "$r" in "frames="*" halted="*" ram[0]="*) ok "v32run wrapper: BIOS + cart, one RAM word";; *) bad "v32run: $r";; esac
    r=$("$ORACLE/refrun" "$V32BIOS" "$W/ex/hello_text.v32" 400 --dump 1@0 | head -1); [ -n "$r" ] && ok "refrun wrapper: RAM words" || bad "refrun"
    [ -x "$ORACLE/v32peek" ] && [ -x "$ORACLE/v32prof" ] && ok "v32prof and v32peek are wrappers over v32orun" || bad "v32prof/v32peek wrappers missing"
    printf '0 connect 1 1\n5 segment run\n20 end\n' > "$W/pp.script"
    [ -x "$ORACLE/v32shot" ] && ok "v32shot is a wrapper over v32orun" || bad "v32shot wrapper missing"
    r=$("$ORACLE/v32shot" "$V32BIOS" "$W/ex/hello_text.v32" "$W/pp.script" "$W/vs" 20 2>&1); [ -s "$W/vs20.ppm" ] && [ "$(head -c 2 "$W/vs20.ppm")" = P6 ] && ok "v32shot wrapper writes a 640x360 PPM" || bad "v32shot: $r"
    r=$("$ORACLE/v32peek" "$V32BIOS" "$W/ex/hello_text.v32" "$W/pp.script" 20 0 2 2>&1 | head -1); case "$r" in "frames=21 "*) ok "v32peek wrapper: CPU state after 21 frames";; *) bad "v32peek: $r";; esac
    if [ $HAVE_STOCK = 1 ]; then
        sh "$T/v32mkcart.sh" --stock -o "$W/pst" "$EX/load_pattern.c" >/dev/null 2>&1 || exit 2
        r=$("$ORACLE/v32prof" "$V32BIOS" "$W/pst/load_pattern.v32" "$W/pst/load_pattern.vbin.debug" "$W/pp.script" 2>&1 | sed -n 1p)
        [ "$r" = "frames run: 20" ] && ok "v32prof wrapper: segments and per-function cycles" || bad "v32prof: $r"
    fi
else skipt "v32orun" "no ORACLE with v32orun"; fi

echo "== v32mouse (v32io mouse on a gamepad port)"
sh "$T/v32mkcart.sh" -o "$W/pbits" "$EX/pad_bits.c" >/dev/null || exit 2
r=$(python3 -I "$HERE/run_mouse_vectors.py" "$SIM" "$V32BIOS" "$W/pbits/pad_bits.v32") && ok "mouse commands reproduce the v32io vectors (tests/vectors/mouse_*.vec): $(echo $r | tr -s ' \n' ' ')" || bad "mouse vectors: $r"
printf 'run 300\nrun 5\nquit\n' > "$W/m.cmd"
r=$("$SIM" --batch --fast -x --max-frames=700 -B "$V32BIOS" --v32mouse=2 "$W/pbits/pad_bits.v32" < "$W/m.cmd" 2>&1; printf 'mouse\nquit\n' | "$SIM" --batch --fast -x --max-frames=2 -B "$V32BIOS" --v32mouse=2 "$W/pbits/pad_bits.v32" 2>&1)
case "$r" in *"v32mouse: on gamepad 2"*) ok "--v32mouse=PORT attaches the mouse";; *) bad "--v32mouse: $r";; esac
r=$(printf 'mouse attach 1\nkbd attach 1\nquit\n' | "$SIM" --batch -x --max-frames=2 -B "$V32BIOS" "$W/pbits/pad_bits.v32" 2>&1)
case "$r" in *"has the mouse on it"*|*"has the keyboard on it"*) ok "mouse and keyboard refuse the same port";; *) bad "same port: $r";; esac

echo "== GPU / SPU counters (VS-13)"
sh "$T/v32mkcart.sh" -o "$W/pod" "$EX/overdraw.c" >/dev/null || exit 2
r=$("$SIM" --batch -r --fast -x --max-frames=300 -B "$V32BIOS" --frame-stats="$W/pod.csv" --frame-stats-skip=250 --frame-stats-top=3 "$W/pod/overdraw.v32" 2>&1)
[ "$(sed -n 1p "$W/pod.csv")" = "frame,cycles,cpu_pct,gpu_pixels,gpu_pct,end,draws,dropped,clears,tex_switches,region_switches,regions,spu_cmds,spu_plays,scaled_draws,rotated_draws,region_defs,color_changes,blend_changes,spu_assigns,spu_params,movs_words,sets_words,gamepad_reads,instructions,memcard_accesses,spu_playing" ] && ok "--frame-stats CSV: the six original columns, then the counters" || bad "CSV header"
[ "$(sed -n 280p "$W/pod.csv" | cut -d, -f7,8)" = "12,3" ] && ok "overdraw.c: 12 draws, 3 dropped per frame (9 screens of budget)" || bad "per-frame counters: $(sed -n 280p "$W/pod.csv")"
case "$r" in *"gpu.draws "*"12.00"*"12      12      12      12"*"gpu.dropped"*"3.00"*"-1:0 "*) ok "the summary has the counters (skip applied) and the top regions";; *) bad "summary: $r";; esac
printf 'run 300\nstats expect dropped = 3\nstats expect total_draws >= 600\nquit\n' | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/pod/overdraw.v32" >/dev/null 2>&1 && ok "stats expect passes" || bad "stats expect"
printf 'run 300\nstats expect dropped = 4\nquit\n' | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/pod/overdraw.v32" >/dev/null 2>&1; [ $? = 1 ] && ok "a failing stats expect gives exit status 1" || bad "stats expect failure status"

printf 'run 300\nexpect stats gpu.draws.max <= 12\nexpect stats gpu.draws.p50 >= 0\nexpect stats gpu.dropped.last == 3\nstats expect gpu.draws.p99 > 11\nquit\n' | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/pod/overdraw.v32" >/dev/null 2>&1 && ok "expect stats NAME.AGG (last max p50 p99) with dotted names" || bad "expect stats aggregates"
printf 'run 300\nexpect stats gpu.draws.p99 < 12\nquit\n' | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/pod/overdraw.v32" >/dev/null 2>&1; [ $? = 1 ] && ok "a failing expect stats aggregate gives exit status 1" || bad "expect stats aggregate failure"
printf 'run 300\nstats save %s/ss.csv\nstats save %s/ss.jsonl json\nquit\n' "$W" "$W" | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/pod/overdraw.v32" >/dev/null 2>&1
[ "$(wc -l < "$W/ss.csv")" -gt 250 ] && head -1 "$W/ss.csv" | grep -q "^frame,cycles,gpu_pixels,draws," && ok "stats save FILE writes every frame's counters as CSV" || bad "stats save csv"
tail -1 "$W/ss.jsonl" | grep -q '"type":"summary"' && sed -n 280p "$W/ss.jsonl" | grep -q '"gpu.draws":12,' && ok "stats save FILE json writes JSON Lines with a summary line" || bad "stats save json"
"$SIM" --batch -r --fast -x --max-frames=300 -B "$V32BIOS" --stats="$W/pj.jsonl" --stats-format=json --stats-skip=250 "$W/pod/overdraw.v32" >/dev/null 2>&1
sed -n 280p "$W/pj.jsonl" | grep -q '"gpu.draws":12,"gpu.dropped":3,' && tail -1 "$W/pj.jsonl" | grep -q '"gpu.draws":{"mean":12.0000,"min":12,"p50":12,"p95":12,"p99":12,"max":12,' && ok "--stats-format=json: JSON Lines rows and a summary with mean/min/p50/p95/p99/max" || bad "--stats json: $(tail -c 300 "$W/pj.jsonl")"
python3 -I "$T/v32sim-statdiff.py" "$W/pod.csv" "$W/pj.jsonl" --skip 250 --fail-over 1 --fail-columns gpu.draws >/dev/null 2>&1 && ok "statdiff reads CSV and JSON Lines, stable names" || bad "statdiff json"
printf 'stats off\nrun 300\nscreenshot %s/soff.png\nquit\n' "$W" | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/pod/overdraw.v32" >/dev/null 2>&1
printf 'run 300\nscreenshot %s/son.png\nquit\n' "$W" | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/pod/overdraw.v32" >/dev/null 2>&1
cmp -s "$W/soff.png" "$W/son.png" && ok "stats on / off: the machine's output is identical" || bad "stats changes the machine"
sh "$T/v32mkcart.sh" -o "$W/pbits" "$EX/pad_bits.c" >/dev/null 2>&1
printf 'run 400\nstats expect bus.gamepad_reads.total > 0\nstats expect cpu.instructions.max > 0\nquit\n' | "$SIM" --batch --fast -x --max-frames=500 -B "$V32BIOS" "$W/pbits/pad_bits.v32" >/dev/null 2>&1 && ok "bus.gamepad_reads and cpu.instructions count" || bad "gamepad_reads"
python3 -I "$T/v32sim-statdiff.py" "$W/pod.csv" "$W/pod.csv" --fail-over 1 >/dev/null 2>&1 && ok "statdiff: a run against itself passes the gate" || bad "statdiff self"
sed 's/,12,3,/,24,3,/' "$W/pod.csv" > "$W/pod2.csv"
python3 -I "$T/v32sim-statdiff.py" "$W/pod.csv" "$W/pod2.csv" --skip 250 --fail-over 10 >/dev/null 2>&1; [ $? = 1 ] && ok "statdiff: doubled draws fail --fail-over 10" || bad "statdiff gate"
python3 -I "$T/v32sim-statdiff.py" "$W/pod.csv" "$W/pod2.csv" --skip 250 --json | python3 -c 'import json,sys; d=json.load(sys.stdin); sys.exit(0 if any(c["column"]=="draws" and c["mean_change_pct"]>90 for c in d["columns"]) else 1)' && ok "statdiff --json" || bad "statdiff json"

echo "== VS-13 4.2 / 4.3 / 4.4: --profile-event, traces and replay, time breakdown"
r=$("$SIM" --batch -r --fast -x --max-frames=300 -B "$V32BIOS" --profile-event=gpu-draw --profile-by=function --profile-top=5 "$W/pod/overdraw.v32" 2>&1)
case "$r" in *"600 "*"draw_region_at"*|*"gpu-draw; hottest functions"*"draw_region_at"*) ok "--profile-event=gpu-draw --profile-by=function names the function that draws";; *) bad "profile-event: $r";; esac
r=$("$SIM" --batch -r --fast -x --max-frames=300 -B "$V32BIOS" --profile-event=bogus "$W/pod/overdraw.v32" 2>&1); [ $? = 1 ] && ok "--profile-event refuses an unknown kind" || bad "profile-event bogus"
"$SIM" --batch -r --fast -x --max-frames=300 -B "$V32BIOS" --profile-out="$W/pi.txt" "$W/pod/overdraw.v32" >/dev/null 2>&1
"$SIM" --batch -r --fast -x --max-frames=300 -B "$V32BIOS" --profile-out="$W/pc.txt" --profile-event=cycles "$W/pod/overdraw.v32" >/dev/null 2>&1
[ "$(grep -v '^#' "$W/pi.txt")" = "$(grep -v '^#' "$W/pc.txt")" ] && ok "--profile-event=cycles equals instr (a cycle per instruction)" || bad "profile-event cycles"
"$SIM" --batch -r --fast -x --max-frames=300 -B "$V32BIOS" --gpu-trace="$W/g.trc" --spu-trace="$W/s.trc" --stats="$W/gt.csv" --stats-skip=0 "$W/pod/overdraw.v32" >/dev/null 2>&1
"$SIM" --trace-dump="$W/g.trc" > "$W/g.dump" 2>&1; head -1 "$W/g.dump" | grep -q "^V32TRACE version 1 gpu" && grep -q "^W 0x200 GPU_Command = 0x00000011" "$W/g.dump" && tail -1 "$W/g.dump" | grep -q "^E 300" && ok "--gpu-trace and --trace-dump: header, named port writes, end record" || bad "trace dump: $(head -3 "$W/g.dump")"
"$SIM" --trace-dump="$W/s.trc" 2>&1 | head -1 | grep -q "gpu" && bad "spu trace is a gpu trace" || ok "--spu-trace is a SPU trace"
if [ $HAVE_ORACLE = 1 ] && [ -x "$ORACLE/v32gpureplay" ]; then
    rep=$("$ORACLE/v32gpureplay" "$W/g.trc" --spu "$W/s.trc" 2>&1)
    case "$rep" in *"gpu draws 920 dropped 150 clears 141 pixels 105967622"*) ok "v32gpureplay: the stock GPU logic on the trace gives the draws, drops, clears and pixels of the live run";; *) bad "gpureplay: $rep";; esac
    echo "$rep" | grep -q "replay time per frame" && ok "v32gpureplay reports the replay time per frame" || bad "gpureplay timing"
else skipt "v32gpureplay" "no ORACLE/v32gpureplay"; fi
r=$("$SIM" --batch -r --fast -x --max-frames=300 -B "$V32BIOS" --time-breakdown --frame-log="$W/fl.csv" "$W/pod/overdraw.v32" 2>&1)
case "$r" in *"[time]"*"cpu_us"*"gpu_us"*"work_us"*) ok "--time-breakdown prints the per-frame wall-clock split";; *) bad "time-breakdown: $r";; esac
[ "$(head -1 "$W/fl.csv")" = "frame,cpu_us,gpu_us,spu_us,output_us,sleep_us,work_us,late_us" ] && [ "$(wc -l < "$W/fl.csv")" -gt 250 ] && ok "--frame-log writes a row per frame" || bad "frame-log"
printf 'run 300\nscreenshot %s/tbon.png\nquit\n' "$W" | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" --time-breakdown --gpu-trace="$W/g2.trc" --profile-event=gpu-draw "$W/pod/overdraw.v32" >/dev/null 2>&1
cmp -s "$W/tbon.png" "$W/son.png" && ok "traces, --time-breakdown and --profile-event leave the machine's output identical" || bad "observability changed the machine"

echo "== counters against a cartridge with known counts (tests/examples/counts.c)"
sh "$T/v32mkcart.sh" -o "$W/cnt" "$EX/counts.c" >/dev/null 2>&1
printf 'run 300\nexpect stats gpu.draws.p50 == 6\nexpect stats gpu.scaled_draws.p50 == 4\nexpect stats gpu.rotated_draws.p50 == 3\nexpect stats gpu.region_defs.p50 == 12\nexpect stats gpu.color_changes.p50 == 2\nexpect stats gpu.blend_changes.p50 == 3\nexpect stats spu.assignments.p50 == 1\nexpect stats spu.param_writes.p50 == 3\nexpect stats bus.gamepad_reads.p50 == 4\nexpect stats spu.channels_playing.max == 0\nquit\n' | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" --stats-skip=250 "$W/cnt/counts.v32" >/dev/null 2>&1 && ok "known counts: draws, scaled, rotated, region defs, colour / blend writes, SPU assignments / parameters, gamepad reads" || bad "known counts"

echo "== --v32dev-doorbell (VS-17, v32nix P-E2): tests/examples/bell.c with tests/doorbell_host.py"
sh "$T/v32mkcart.sh" -o "$W/bell" "$EX/bell.c" >/dev/null 2>&1
bell() {   # $1 = host seconds ("0" = no host), rest = v32sim options; prints "flags spin delta done"
    local secs=$1; shift
    python3 -I "$V32IO/tools/v32dev.py" format "$W/bell.memc" --ch0 serial --force >/dev/null 2>&1
    local hp=""
    if [ "$secs" != "0" ]; then python3 -I "$HERE/doorbell_host.py" "$W/bell.memc" "$secs" & hp=$!; fi
    printf 'run 330\nprint [0x00200000]\nprint [0x00200001]\nprint [0x00200002]\nprint [0x00200003]\nquit\n' \
      | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" -M "$W/bell.memc" "$@" "$W/bell/bell.v32" 2>&1 \
      | sed -n 's/^\[0x0020000[0-3](0x\([0-9A-F]*\))\].*/\1/p' | tr '\n' ' '
    [ -n "$hp" ] && { kill $hp 2>/dev/null; wait $hp 2>/dev/null; }
}
if [ -f "${V32IO:-/nonexistent}/tools/v32dev.py" ]; then
    r=$(bell 20 --v32dev-doorbell=50); [ "$r" = "00000003 00000000 00000001 00000001 " ] && ok "doorbell on: flags bit 1, the reply counter moves at once (spin count 0)" || bad "doorbell on: '$r'"
    r=$(bell 20); [ "$r" = "00000001 000186A0 00000000 00000001 " ] && ok "doorbell off: flags 1, no counter, the reply waits for the frame boundary (as before)" || bad "doorbell off: '$r'"
    r=$(bell 0 --v32dev-doorbell=50); case "$r" in "00000003 000186A0 00000000 00000001 ") ok "doorbell on, no host: no wait, no counter change";; *) bad "doorbell, no host: '$r'";; esac
else skipt "doorbell" "no v32dev.py (set V32IO to the v32io project)"; fi

echo "== screenshot highlight"
sh "$T/v32mkcart.sh" -o "$W/hlk" --cc-opt -DKEEP_RUNNING "$EX/hello_text.c" >/dev/null || exit 2
shot() { printf "$1run 300\nscreenshot $W/$2.png\nquit\n" | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/hlk/hello_text.v32" >/dev/null 2>&1; }
shot "" plain; shot "highlight bios:104\n" red; shot "highlight bios:104 00ff00\n" green; shot "highlight bios:104\nhighlight off\n" off; shot "highlight bios:104\nhighlight clear\n" cleared
cmp -s "$W/plain.png" "$W/red.png" && bad "highlight: the screenshot did not change" || ok "highlight bios:104 outlines the 'h' glyphs in the screenshot"
cmp -s "$W/red.png" "$W/green.png" && bad "highlight: color ignored" || ok "highlight COLOR changes the outline color"
cmp -s "$W/plain.png" "$W/off.png" && cmp -s "$W/plain.png" "$W/cleared.png" && ok "highlight off / clear give the plain screenshot" || bad "highlight off/clear"
python3 -I - "$W/plain.png" "$W/red.png" <<'PY' && ok "only outline pixels differ, all red, outside the glyph quads" || bad "highlight pixels"
import sys, zlib, struct
def load(p):
    d = open(p, 'rb').read(); i = 8; idat = b''
    while i < len(d):
        n, t = struct.unpack('>I4s', d[i:i+8]); 
        if t == b'IDAT': idat += d[i+8:i+8+n]
        i += 12 + n
    raw = zlib.decompress(idat); w = 640
    return [raw[y*(1+w*3)+1:(y+1)*(1+w*3)] for y in range(360)]
a, b = load(sys.argv[1]), load(sys.argv[2]); diff = 0
for y in range(360):
    for x in range(640):
        if a[y][x*3:x*3+3] != b[y][x*3:x*3+3]:
            diff += 1
            if b[y][x*3:x*3+3] != b'\xff\x00\x00': sys.exit(1)
sys.exit(0 if diff > 20 else 1)
PY

shot "screengrid on\n" grid; shot "screengrid 32 30\n" grid2; shot "screengrid on\nnoextra\n" gridoff
cmp -s "$W/plain.png" "$W/grid.png" && bad "screengrid: no change" || ok "screengrid lays a grid over the screenshot"
cmp -s "$W/grid.png" "$W/grid2.png" && bad "screengrid spacing ignored" || ok "screengrid DX DY changes the spacing"
cmp -s "$W/plain.png" "$W/gridoff.png" && ok "noextra gives the plain screenshot" || bad "noextra"
printf "screengrid on\nrun 300\nscreenshot --plain $W/plain2.png\nquit\n" | "$SIM" --batch --fast -x --max-frames=400 -B "$V32BIOS" "$W/hlk/hello_text.v32" >/dev/null 2>&1
cmp -s "$W/plain.png" "$W/plain2.png" && ok "screenshot --plain leaves the grid out" || bad "screenshot --plain"

rm -f "$W"/cli-*.png
"$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --screenshot-at=3,5-9/2 --screenshot-file="$W/cli-%03d.png" --highlight=bios:104 "$W/hlk/hello_text.v32" >/dev/null 2>&1
n=$(ls "$W"/cli-*.png 2>/dev/null | wc -l)
[ "$n" = 4 ] && [ -f "$W/cli-003.png" ] && [ -f "$W/cli-009.png" ] && ok "--screenshot-at=3,5-9/2 writes cart frames 3 5 7 9" || bad "--screenshot-at wrote $n files"
"$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --screenshot-at=5 --screenshot-file="$W/cli-g.png" --screengrid "$W/hlk/hello_text.v32" >/dev/null 2>&1
"$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --screenshot-at=5 --screenshot-file="$W/cli-n.png" --screengrid=nolabels "$W/hlk/hello_text.v32" >/dev/null 2>&1
"$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --screenshot-at=5 --screenshot-file="$W/cli-x.png" --screengrid --noextra "$W/hlk/hello_text.v32" >/dev/null 2>&1
cmp -s "$W/cli-005.png" "$W/cli-g.png" && bad "--screengrid: no grid" || ok "--screengrid puts the grid (with coordinates) on a --screenshot-at shot"
cmp -s "$W/cli-g.png" "$W/cli-n.png" && bad "--screengrid=nolabels: same as labels" || ok "--screengrid=nolabels leaves the coordinates out"
"$SIM" --batch -r --fast -x --max-frames=400 -B "$V32BIOS" --screenshot-at=5 --screenshot-file="$W/cli-p.png" "$W/hlk/hello_text.v32" >/dev/null 2>&1
cmp -s "$W/cli-x.png" "$W/cli-p.png" && ok "--noextra drops grid and outlines" || bad "--noextra"
"$SIM" --batch --screenshot-at=x -B "$V32BIOS" "$W/hlk/hello_text.v32" </dev/null >/dev/null 2>&1; [ $? = 1 ] && ok "a bad --screenshot-at is refused (exit 1)" || bad "bad --screenshot-at accepted"

echo "== $pass passed, $fail failed, $skip skipped"
[ $fail -eq 0 ]
