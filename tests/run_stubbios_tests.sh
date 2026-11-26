#!/bin/bash
# tests/run_stubbios_tests.sh: checks of the bundled BIOS, bios/StubBios.v32 (no toolchain needed:
# the test cartridges are written here, as VBIN words, and wrapped by tests/mkcart.py).
#
#   tests/run_stubbios_tests.sh            (STOCK=StandardBios.v32 adds the hand-over comparison;
#                                            V32BIOS or the stock install path are used if STOCK is unset)
#
# - boot: the cartridge runs by frame 3 and its first instruction sees StandardBios's hand-over
#   state (registers, ports, GPU regions, SPU, RAM; only the timers and the BIOS checksum differ)
# - error handler: title and V32ERR line on the text console, the record at RAM 0x3FFFE0
# - -x still stops at the error without entering the handler (exit 3)
# - no cartridge: "NO CARTRIDGE FOUND"
# - the default BIOS lookup (-B not given) finds V32BIOS, and the bundled StubBios without it
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
SIM=$ROOT/bin/v32sim
BIOS=$ROOT/bios/StubBios.v32
STOCK=${STOCK:-${V32BIOS:-/usr/local/Vircon32/Emulator/Bios/StandardBios.v32}}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
pass=0; fail=0
ok()  { pass=$((pass + 1)); }
bad() { fail=$((fail + 1)); echo "FAIL: $*"; }
check() { if eval "$2"; then ok; else bad "$1"; fi; }

[ -x "$SIM" ] || { echo "build v32sim first"; exit 2; }
[ -f "$BIOS" ] || { echo "no $BIOS (bios/src/build.sh)"; exit 2; }

# the test cartridges, as the stock assembler writes them
python3 -I - "$W" <<'EOF'
import struct, sys
carts = {
  # mov R0,0x1234 / mov [0x100],R0 / hlt
  "hold":    [0x4e000000, 0x1234, 0x4e014000, 0x100, 0x0],
  # mov R4,0x44 / mov R13,0x1313 / mov R0,7 / mov R1,0 / idiv R0,R1 / hlt
  "divz":    [0x4e800000, 0x44, 0x4fa00000, 0x1313, 0x4e000000, 0x7, 0x4e200000, 0x0, 0xa4020000, 0x0],
  # mov R6,0x66 / mov R0,[0x500000] / hlt
  "badread": [0x4ec00000, 0x66, 0x4e008000, 0x500000, 0x0],
  # out 0x7FF,5 / hlt
  "portw":   [0x620007ff, 0x5, 0x0],
  # f: call f
  "stackov": [0x0e000000, 0x20000000],
}
for name, w in carts.items():
    open(f"{sys.argv[1]}/{name}.vbin", "wb").write(b"V32-VBIN" + struct.pack("<I", len(w)) + struct.pack(f"<{len(w)}I", *w))
EOF
for c in hold divz badread portw stackov; do python3 -I "$HERE/mkcart.py" "$W/$c.vbin" "$W/$c.v32" || exit 2; done

# ---- boot
out=$("$SIM" --batch --fast -B "$BIOS" --dump=1@0x100 "$W/hold.v32" <<< $'break 0x20000000\ncontinue\nprint TIM_FrameCounter\ncontinue' 2>/dev/null)
fc=$(echo "$out" | sed -n 's/.*: \([0-9]*\) (TIM_FrameCounter).*/\1/p')
check "boot: cartridge starts by frame 3 (got '$fc')" '[ -n "$fc" ] && [ "$fc" -le 3 ]'
check "boot: cartridge runs to its HLT (dump 4660)" '[ "$(echo "$out" | tail -1)" = 4660 ]'
"$SIM" --batch -r -x --fast --max-frames=20 -B "$BIOS" "$W/hold.v32" </dev/null >/dev/null 2>&1
check "boot: -x run of a halting cartridge exits 0" '[ $? = 0 ]'

if [ -f "$STOCK" ]; then
    for b in stub stock; do
        [ $b = stub ] && f=$BIOS || f=$STOCK
        "$SIM" --batch --fast -B "$f" "$W/hold.v32" <<< "break 0x20000000"$'\n'"continue"$'\n'"save $W/$b.state" >/dev/null 2>&1
    done
    python3 -I "$ROOT/tools/v32sim-statediff.py" --ignore header \
        --ignore-port TIM_CurrentTime,TIM_FrameCounter,TIM_CycleCounter "$W/stock.state" "$W/stub.state" > "$W/diff" 2>&1
    check "boot: hand-over state == StandardBios's ($(head -3 "$W/diff" | tr '\n' ' '))" '[ $? = 0 ]'
else
    echo "(no StandardBios.v32: hand-over comparison skipped; set STOCK=...)"
fi

# ---- error handler
err() {   # NAME TITLE CODE FAULT (globals: check's eval sees its own $1 $2)
    N=$1; T=$2; C=$3; F=$4
    O=$("$SIM" --batch --fast --max-frames=300 --console --dump=32@0x3FFFE0 -B "$BIOS" "$W/$N.v32" <<< $'continue\ncontinue' 2>/dev/null)
    REC=($(echo "$O" | tail -32))
    check "$N: title '$T' on the console" 'echo "$O" | grep -qx "  $T"'
    check "$N: V32ERR line" 'echo "$O" | grep -q "^  V32ERR $C $F "'
    check "$N: record magic/code/fault/state" '[ "${REC[0]}" = 1160917846 ] && [ "$(printf 0x%08X "${REC[1]}")" = "$C" ] && [ "$(printf 0x%08X "${REC[2]}")" = "$F" ] && [ "${REC[31]}" = 1446182914 ]'
}
err divz    "ERROR: DIVISION BY ZERO"    0x00000006 0x20000008
check "divz: R4 and R13 kept in the record" '[ "${REC[6]}" = 68 ] && [ "${REC[15]}" = 4883 ] && [ "${REC[18]}" = 1 ]'
err badread "ERROR: INVALID MEMORY READ" 0x00000000 0x20000002
check "badread: immediate 0x00500000 in the record" '[ "${REC[5]}" = 5242880 ] && [ "${REC[8]}" = 102 ]'
err portw   "ERROR: INVALID PORT WRITE"  0x00000003 0x20000000
err stackov "ERROR: STACK OVERFLOW"      0x00000004 0x20000000

"$SIM" --batch -r -x --fast --max-frames=300 -B "$BIOS" "$W/divz.v32" </dev/null >/dev/null 2>"$W/err"
check "-x: exit 3 at the error, handler not entered" '[ $? = 3 ] && grep -q "system error 6" "$W/err"'

# ---- no cartridge
o=$("$SIM" --batch -r --fast --max-frames=20 --console -B "$BIOS" - </dev/null 2>/dev/null)
check "no cartridge: NO CARTRIDGE FOUND" 'echo "$o" | grep -q "NO CARTRIDGE FOUND"'

# ---- default BIOS lookup
o=$(V32BIOS=$BIOS "$SIM" -v --batch "$W/hold.v32" </dev/null 2>&1 | grep "BIOS:")
check "lookup: V32BIOS is used without -B" 'echo "$o" | grep -qF "$BIOS"'
if [ ! -f /usr/local/Vircon32/Emulator/Bios/StandardBios.v32 ]; then
    o=$(env -u V32BIOS "$SIM" -v --batch "$W/hold.v32" </dev/null 2>&1 | grep "BIOS:")
    check "lookup: bundled StubBios without V32BIOS or the stock install ($o)" 'echo "$o" | grep -q "bios/StubBios.v32"'
fi

echo "stubbios tests: $pass passed, $fail failed"
[ $fail = 0 ]
