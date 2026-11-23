#!/bin/bash
# The built-in v32dev server (V32DEV_SERVER, V32DEV_CH0..3, serial:tcp=PORT), SPEC parsing and `dev format` types.
# Needs no toolchain and no v32io project: tests/kbd_probe.py writes the probe cartridge and a minimal BIOS,
# tests/v32dev_server_test.py plays the program's part from the debugger.
#
#   tests/run_v32dev_server_tests.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
python3 -I "$HERE/kbd_probe.py" --bios "$W/bios.vbin" && python3 -I "$HERE/mkcart.py" --bios "$W/bios.vbin" "$W/bios.v32" || exit 2
python3 -I "$HERE/kbd_probe.py" --stall 3 "$W/p.vbin" && python3 -I "$HERE/mkcart.py" "$W/p.vbin" "$W/p.v32" || exit 2
out=$(python3 -I "$HERE/v32dev_server_test.py" "$SIM" "$W/bios.v32" "$W/p.v32" "$W/server" 2>&1)
echo "$out" | grep FAIL
echo "$out" | tail -1
echo "$out" | grep -q " 0 failures" && exit 0
exit 1
