#!/bin/bash
# tests/run_present_tests.sh: the screen v32sim shows for a frame is the framebuffer at the frame's end, WAIT or
# not, as the DesktopEmulator presents it after RunNextFrame (the 250,000-cycle tick ends a frame that never
# reached WAIT, and that frame is presented half drawn). No toolchain: the cartridge is written here.
#
# The cartridge, per loop: clear red, burn ~300,000 cycles (past the tick), clear blue, WAIT. On the console the
# frames alternate: red (ended by the tick, the blue clear not reached yet), blue (ended by WAIT).
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
SIM=$ROOT/bin/v32sim
BIOS=$ROOT/bios/StubBios.v32
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
pass=0; fail=0
ok()  { pass=$((pass + 1)); }
bad() { fail=$((fail + 1)); echo "FAIL: $*"; }

python3 -I - "$W/ov.vbin" <<'EOF'
import struct, sys
w = [0x62000202, 0xff0000ff, 0x62000200, 0x10, 0x4e000000, 150000, 0x9e000000, 0x1, 0x16000000, 0x20000006,
     0x62000202, 0xffff0000, 0x62000200, 0x10, 0x04000000, 0x0a000000, 0x20000000]
open(sys.argv[1], "wb").write(b"V32-VBIN" + struct.pack("<I", len(w)) + struct.pack(f"<{len(w)}I", *w))
EOF
python3 -I "$HERE/mkcart.py" "$W/ov.vbin" "$W/ov.v32" || exit 2

# the top-left pixel of a v32sim screenshot as R or B (row 0's first pixel is unfiltered under every PNG filter)
pix() { python3 -I - "$@" <<'EOF'
import struct, sys, zlib
out = []
for p in sys.argv[1:]:
    d = open(p, "rb").read(); i = 8; idat = b""
    while i < len(d):
        n, t = struct.unpack(">I4s", d[i:i + 8]); i += 8
        if t == b"IDAT": idat += d[i:i + n]
        i += n + 4
    r, g, b = zlib.decompress(idat)[1:4]
    out.append("R" if (r, g, b) == (255, 0, 0) else "B" if (r, g, b) == (0, 0, 255) else "?")
print("".join(out))
EOF
}

"$SIM" --batch -r --fast -x --max-frames=20 -B "$BIOS" --screenshot-at=0-5 --screenshot-file="$W/s%d.png" \
    --frame-stats="$W/fs.csv" "$W/ov.v32" </dev/null >/dev/null 2>&1
got=$(pix "$W"/s{0..5}.png)
[ "$got" = RBRBRB ] && ok || bad "--screenshot-at frames 0-5: $got (want RBRBRB)"

(echo "break 0x20000000"; echo continue; echo "unbreak 0"; for i in 1 2 3 4; do echo "run 1"; echo "screenshot $W/r$i.png"; done) \
    | "$SIM" --batch --fast -B "$BIOS" "$W/ov.v32" >/dev/null 2>&1
got=$(pix "$W"/r{1..4}.png)
[ "$got" = RBRB ] && ok || bad "run 1 + screenshot: $got (want RBRB)"

# --frame-stats `end`: b = ended by the cycle budget, w = by WAIT (the cartridge frames alternate)
got=$(awk -F, 'NR > 1 { printf "%s", $6 }' "$W/fs.csv" | grep -o 'bwbwbw' | head -1)
[ -n "$got" ] && ok || bad "--frame-stats end column does not alternate b/w: $(awk -F, 'NR > 1 { printf "%s", $6 }' "$W/fs.csv")"

echo "present tests: $pass passed, $fail failed"
[ $fail = 0 ]
