#!/bin/bash
# Memory card differential test: v32sim against the stock console logic (vrun, the console logic with a card) on
# card files of several kinds: the words the cartridge reads (including a v32dev mailbox's emulator
# words and heartbeat), the writes it makes, and the card file left behind. Also the checks of
# the card file (size, signature), which both must accept or refuse alike.
#
#   tests/run_memcard.sh <tools dir>      (tools: vrun, assemble, packrom)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS=$(cd "${1:?tools dir (vrun, assemble, packrom)}" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
WORK=$W bash "$HERE/run_gaps.sh" "$TOOLS" g01_rng >/dev/null 2>&1      # packs bios.v32
[ -f "$W/bios.v32" ] || { echo "cannot build the BIOS"; exit 2; }
"$TOOLS/assemble" "$HERE/memcard/g22_card.asm" -o "$W/c.vbin" >/dev/null || exit 2
cat > "$W/c.xml" <<XML
<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<rom-definition version="1.0"><rom type="cartridge" title="c" version="1.0" />
<binary path="c.vbin" /><textures /><sounds /></rom-definition>
XML
(cd "$W" && "$TOOLS/packrom" c.xml -o c.v32 >/dev/null) || exit 2

python3 - "$W" <<'PY'
import struct, sys, random
W = sys.argv[1]
WORDS = 256 * 1024
REG = WORDS - 16384
def card(name, header=None, host=None, cart=None, plain=None, size=None, sig=b"V32-MEMC"):
    w = [0] * WORDS
    if plain: w[0:len(plain)] = plain
    if header: w[REG:REG + len(header)] = header
    if host:  w[REG + 64:REG + 64 + len(host)] = host
    if cart:  w[REG + 64 + 8160:REG + 64 + 8160 + len(cart)] = cart
    data = sig + struct.pack("<%dI" % WORDS, *w)
    if size is not None: data = data[:size] if size < len(data) else data + bytes(size - len(data))
    open(f"{W}/card_{name}.bin", "wb").write(data)
good = [0x31564544, 1, 16384, 4, 2040, 16]
random.seed(7)
rnd = [random.getrandbits(32) for _ in range(40)]
card("plain")
card("plain_data", plain=[1, 2, 3, 0xFFFFFFFF])
card("mailbox", header=good)
card("mailbox_data", header=good + [0, 0, 0xAAAA, 0xBBBB, 0xCCCC], host=rnd, cart=rnd[::-1], plain=[9, 8, 7])
card("mailbox_flags_set", header=good + [0, 0, 0xFFFFFFFF, 0xFFFFFFFF])
card("badver", header=[0x31564544, 2, 16384, 4, 2040, 16], host=rnd)
card("badmagic", header=[0x31564545, 1, 16384, 4, 2040, 16], host=rnd)
card("bad_channels", header=[0x31564544, 1, 16384, 3, 2040, 16], host=rnd)
card("short", size=8 + WORDS * 4 - 4)
card("long", size=8 + WORDS * 4 + 4)
card("badsig", sig=b"V32-MEMX")
card("empty", size=0)
PY

pass=0; fail=0
run_case() {   # NAME EXPECT(ok|bad) [env assignment]
    local n=$1 exp=$2 envs=${3:-}
    cp "$W/card_$n.bin" "$W/ref_$n.bin"; cp "$W/card_$n.bin" "$W/sim_$n.bin"
    env $envs "$TOOLS/vrun" --bios "$W/bios.v32" --cart "$W/c.v32" --memc "$W/ref_$n.bin" -f 6 -d 448 -a 0x40 >"$W/$n.ref.raw" 2>"$W/$n.ref.err"
    local rr=$?
    yes continue | head -20 | env $envs timeout 120 "$SIM" -q -r --fast -n -B "$W/bios.v32" -M "$W/sim_$n.bin" --max-frames=6 --dump=448@40 "$W/c.v32" >"$W/$n.sim" 2>"$W/$n.sim.err"
    local sr=${PIPESTATUS[2]}
    if [ "$exp" = bad ]; then
        # refused by both: the console throws (status 2 in vrun), v32sim exits with an error status
        if [ $rr -ne 0 ] && [ $sr -ne 0 ] && grep -q "ERROR:MEMC" "$W/$n.sim.err"; then pass=$((pass+1)); else
            fail=$((fail+1)); echo "FAIL $n: console status $rr, v32sim status $sr (both should refuse the card)"; fi
        return
    fi
    grep -v "^STATUS" "$W/$n.ref.raw" >"$W/$n.ref"
    if cmp -s "$W/$n.ref" "$W/$n.sim" && cmp -s "$W/ref_$n.bin" "$W/sim_$n.bin"; then pass=$((pass+1)); else
        fail=$((fail+1)); echo "FAIL $n"
        cmp -s "$W/$n.ref" "$W/$n.sim" || paste -d' ' <(seq 0 447) "$W/$n.ref" "$W/$n.sim" | awk '$2 != $3 { printf "    [RAM %X] ref=%d sim=%d\n", 64+$1, $2, $3; if (++c >= 10) exit }'
        cmp -s "$W/ref_$n.bin" "$W/sim_$n.bin" || { echo "    card files differ at (byte offset, ref, sim):"; cmp -l "$W/ref_$n.bin" "$W/sim_$n.bin" | head -6 | sed 's/^/      /'; }
    fi
}
for n in plain plain_data mailbox mailbox_data mailbox_flags_set badver badmagic bad_channels; do run_case $n ok; done
for n in mailbox mailbox_data; do cp "$W/card_$n.bin" "$W/card_${n}_off.bin"; run_case ${n}_off ok V32DEV_DISABLE=1; done
for n in short long badsig empty; do run_case $n bad; done
echo "memcard: $pass passed, $fail failed"
[ $fail -eq 0 ]
