#!/bin/bash
# v32kbd delivery, --kbd-sync / kbd sync, --warn-halt / --halt-status, "regex:" patterns.
# Needs no toolchain: tests/kbd_probe.py writes the few cartridges (and a minimal BIOS) itself.
#
#   tests/run_kbd_sync_tests.sh
#
# Background (v32nix report, 2026-11-22): the v32kbd delivers one key event per frame, whether the
# program looks at the gamepad or not, and the controls hold only the last one. A program that is
# busy for a few frames (a v32dev round trip) loses the events delivered meanwhile: typing "cat"
# right after it gives "at" (the press and the release of c both went by: the strobe flipped twice,
# so it looks unchanged). The probe cartridge makes that exact situation.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $1"; }
check() { if [ "$2" = "$3" ]; then ok; else bad "$1: got '$2', want '$3'"; fi; }
has()   { case "$2" in *"$3"*) ok;; *) bad "$1: '$3' not in '$2'";; esac; }

python3 -I "$HERE/kbd_probe.py" --bios "$W/bios.vbin" && python3 -I "$HERE/mkcart.py" --bios "$W/bios.vbin" "$W/bios.v32" || exit 2
python3 -I "$HERE/kbd_probe.py" --hlt "$W/h.vbin" && python3 -I "$HERE/mkcart.py" "$W/h.vbin" "$W/h.v32" || exit 2

# probe STALL [sim options...]: the program does not look at the gamepad for STALL frames; "cat" is typed at once.
# Prints the events it saw (RAM[0x100..0x107]) and the 'replaced' / 'sync' line of the kbd status.
probe() {
    local stall=$1; shift
    python3 -I "$HERE/kbd_probe.py" --stall "$stall" "$W/p.vbin" && python3 -I "$HERE/mkcart.py" "$W/p.vbin" "$W/p.v32" || exit 2
    {
        echo 'kbd attach 1'; echo 'kbd type "cat"'; echo 'run 40'
        for i in 0 1 2 3 4 5 6 7; do printf 'print [0x%08x]\n' $((0x100+i)); done
        echo kbd; echo quit
    } | timeout 60 "$SIM" --batch --fast "$@" -B "$W/bios.v32" "$W/p.v32" 2>&1 \
      | sed -n 's/^\[0x000001..(0x0*\([0-9A-F][0-9A-F]*\))\].*/\1/p;s/^ *replaced before the program read them: \([0-9]*\)   sync: \([a-z]*\) (\([0-9]*\) frames held)/R=\1 sync=\2 held=\3/p' | tr '\n' ' ' | sed 's/ $//'
}
ALL="E3 63 E1 61 F4 74 0 0"       # c a t: press (+0x80) and release of each
echo "== v32kbd: the program reads every frame"
check "no stall"                 "$(probe 0)"            "$ALL R=0 sync=off held=0"
echo "== v32kbd: the program is busy for a few frames (default: events are replaced, as on the real keyboard)"
check "stall 1 loses c"          "$(probe 1)"            "E1 61 F4 74 0 0 0 0 R=1 sync=off held=0"
check "stall 2 loses c"          "$(probe 2)"            "E1 61 F4 74 0 0 0 0 R=2 sync=off held=0"
check "stall 3 loses c a"        "$(probe 3)"            "F4 74 0 0 0 0 0 0 R=3 sync=off held=0"
check "stall 20 loses all"       "$(probe 20)"           "0 0 0 0 0 0 0 0 R=5 sync=off held=0"
echo "== v32kbd: --kbd-sync waits for the program"
check "sync, no stall"           "$(probe 0 --kbd-sync)" "$ALL R=0 sync=on held=0"
check "sync, stall 3"            "$(probe 3 --kbd-sync)" "$ALL R=0 sync=on held=3"
check "sync, stall 20"           "$(probe 20 --kbd-sync)" "$ALL R=0 sync=on held=20"
echo "== kbd sync command"
python3 -I "$HERE/kbd_probe.py" --stall 3 "$W/p.vbin" && python3 -I "$HERE/mkcart.py" "$W/p.vbin" "$W/p.v32"
out=$( { echo 'kbd attach 1'; echo 'kbd sync on'; echo 'kbd type "cat"'; echo 'run 40'; echo 'print [0x00000100]'; echo kbd; echo 'kbd sync off'; echo 'kbd sync maybe'; echo quit; } \
       | timeout 60 "$SIM" --batch --fast -B "$W/bios.v32" "$W/p.v32" 2>&1 )
has "sync on by command" "$out" "[0x00000100(0x000000E3)]"
has "sync status"        "$out" "sync: on (3 frames held)"
has "bad argument"       "$out" "kbd sync: on or off"

echo "== --warn-halt / --halt-status"
run_h() { timeout 60 "$SIM" --batch -r --fast -x "$@" -B "$W/bios.v32" "$W/h.v32" < /dev/null 2> "$W/err" > "$W/out"; echo $?; }
check "HLT: status 0"            "$(run_h)"                         "0"
check "HLT: silent by default"   "$(cat "$W/err")"                  ""
check "--warn-halt: status 0"    "$(run_h --warn-halt)"             "0"
has   "--warn-halt: message"     "$(cat "$W/err")"                  "[HLT] the program executed HLT at 0x20000002 after 2 frames"
check "--halt-status=7"          "$(run_h --halt-status=7)"         "7"
check "--halt-status: silent"    "$(cat "$W/err")"                  ""
check "both"                     "$(run_h --warn-halt --halt-status=9)" "9"
check "--halt-status=0 refused"  "$(timeout 20 "$SIM" --halt-status=0 2>/dev/null; echo $?)" "1"
echo "== regex: patterns"
out=$( { echo 'run 3'; echo 'expect console "regex:(" '; echo 'expect console "regex:(" '; echo quit; } | timeout 60 "$SIM" --batch --fast -B "$W/bios.v32" "$W/p.v32" 2>&1 ); st=$?
has   "bad regex reported"       "$out" 'bad regular expression "("'
check "bad regex: expect failed" "$st" "1"
check "reported once"            "$(echo "$out" | grep -c 'bad regular expression')" "1"

echo "kbd sync tests: $pass passed, $fail failed"
[ $fail -eq 0 ]
