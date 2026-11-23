#!/bin/bash
# Command-line and in-sim help checks that need no toolchain: -b (labels that look like hex, FILE:LINE),
# -L, --help against src/args.c, help topics against the parser, tbreak without an argument,
# command abbreviations, --screenshot-at counting, startup exit statuses.
# tests/kbd_probe.py writes the tiny BIOS and cartridge itself.
#
#   tests/run_cli_tests.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $1"; }
check() { if [ "$2" = "$3" ]; then ok; else bad "$1: got '$2', want '$3'"; fi; }
has()   { case "$2" in *"$3"*) ok;; *) bad "$1: '$3' not in '$2'";; esac; }
hasnt() { case "$2" in *"$3"*) bad "$1: '$3' in '$2'";; *) ok;; esac; }

python3 -I "$HERE/kbd_probe.py" --bios "$W/bios.vbin" && python3 -I "$HERE/mkcart.py" --bios "$W/bios.vbin" "$W/bios.v32" || exit 2
python3 -I "$HERE/kbd_probe.py" --hlt "$W/h.vbin" && python3 -I "$HERE/mkcart.py" "$W/h.vbin" "$W/h.v32" || exit 2
# labels that are also hex numbers: add = 0x20000000, face = 0x20000001, main = 0x20000002
printf '0x20000000,x.asm,1,add\n0x20000001,x.asm,2,face\n0x20000002,x.asm,3,main\n' > "$W/lab.dbg"

# sim ARGS...: batch run with the label file; stdin is the command list; stdout and stderr together
sim() { timeout 60 "$SIM" --batch -B "$W/bios.v32" --cart-asm-debug="$W/lab.dbg" "$@" "$W/h.v32" 2>&1; }
brk() { { echo break; echo quit; } | sim "$@" | grep '^\[[0-9]'; }

echo "== -b: a label wins over hex, else hex, FILE:LINE as 'break'"
check "-b add (label, also hex)"   "$(brk -b add)"        "[0] 0x20000000 (x.asm:1)"
check "-b face"                    "$(brk -b face)"       "[0] 0x20000001 (x.asm:2)"
check "-b main"                    "$(brk -b main)"       "[0] 0x20000002 (x.asm:3)"
check "-b 0xdead is an address"    "$(brk -b 0xdead)"     "[0] 0x0000DEAD"
check "-b dead without label"      "$(brk -b dead)"       "[0] 0x0000DEAD"
check "-b 20000002 (hex, no 0x)"   "$(brk -b 20000002)"   "[0] 0x20000002 (x.asm:3)"
check "-b 0xadd is an address"     "$(brk -b 0xadd)"      "[0] 0x00000ADD"
check "-b FILE:LINE"               "$(brk -b x.asm:2)"    "[0] 0x20000001 (x.asm:2)"
check "-b twice, same place"       "$(brk -b face -b x.asm:2)" "[0] 0x20000001 (x.asm:2)"
check "-b repeated, in order"      "$(brk -b main -b add | tr '\n' ' ')" "[0] 0x20000002 (x.asm:3) [1] 0x20000000 (x.asm:1) "
out=$({ echo quit; } | sim -b nosuchlabel); st=$?
has   "-b unknown: reported"       "$out" "[ERROR] -b: 'nosuchlabel' is not a known label"
check "-b unknown: not fatal"      "$st" "0"

echo "== -L"
out=$(timeout 20 "$SIM" -L pascal 2>&1 </dev/null); st=$?
check "-L pascal: status"          "$st" "1"
has   "-L pascal: names the modes" "$out" "unknown mode 'pascal' (use C, lua or src)"
for m in C lua src; do
    timeout 20 "$SIM" -L $m --version >/dev/null 2>&1 </dev/null; check "-L $m accepted" "$?" "0"
done

echo "== --help lists every long option of src/args.c"
H=$("$SIM" --help)
for o in $(grep -o '{ "[a-z0-9-]*"' "$HERE/../src/args.c" | tr -d '{ "'); do
    case "$H" in *"--$o"*) ok;; *) bad "--help lacks --$o";; esac
done
for o in quiet stats= stats-skip= stats-top= frame-log= ; do has "--help: --$o" "$H" "--$o"; done
has   "--help: -S is the default"  "$(echo "$H" | grep -e '--bios-start')" "(the default)"
hasnt "--help: typo"               "$H" "addres "
echo "$H" | grep -q -- '--frame-log=FILE      ' && ok || bad "--frame-log has its own line"

echo "== in-sim help against the parser"
run() { { printf '%s\n' "$@"; echo quit; } | timeout 20 "$SIM" --batch -B "$W/bios.v32" 2>&1; }
out=$(run 'help set');             has   "help set: src"        "$out" "asm|src|C|lua"
out=$(run 'set nosuch=1');         has   "set error: source"    "$out" "(color deref errorchk debug profile verbose source mode)"
out=$(run 'help print');           has   "help print: /s"       "$out" "/s    display as string"
out=$(run 'help display');         has   "help display: /s"     "$out" "/s    display as string"
out=$(run 'help break');           has   "help break: FILE:LINE" "$out" "break FILE:LINE | LINE"
out=$(run 'help until');           has   "help until: source"   "$out" "until [LINE | FILE:LINE]"

echo "== tbreak with no argument behaves like break with none"
check "break, none set"            "$(run break | tr '\n' ' ')"  ""
check "tbreak, none set"           "$(run tbreak | tr '\n' ' ')" ""
check "tbreak lists"               "$({ echo 'tbreak 0x20000000'; echo tbreak; echo break; echo quit; } | timeout 20 "$SIM" --batch -B "$W/bios.v32" "$W/h.v32" 2>/dev/null | tr '\n' '|')" "[0] 0x20000000|      (one-shot)|[0] 0x20000000|      (one-shot)|"
out=$(run tbreak); check "tbreak: no error"  "$(echo "$out" | grep -c ERROR)" "0"

echo "== abbreviations: the first command of the table wins"
out=$(run 'help r');   has "r = replace"  "$out" "replace IP:0xADDR"
out=$(run 'help ru');  has "ru = run"     "$out" "run N"
out=$(run 'help s');   has "s = step"     "$out" "Single-step execution of current instruction"
out=$(run 'help w');   has "w = watch"    "$out" "watch[/d] LOCATION OP VALUE"
out=$(run 'help');     has "help lists the letters" "$out" "r replace (run needs ru)"

echo "== --screenshot-at counts cartridge frames (the tiny BIOS hands over at once, the cartridge runs 2 frames)"
rm -f "$W"/s*.png
out=$(timeout 60 "$SIM" --batch -r --fast -B "$W/bios.v32" --screenshot-at=0,1,2 --screenshot-file="$W/s%d.png" "$W/h.v32" 2>&1 </dev/null)
check "frames 0 and 1 written"  "$(ls "$W" | grep -c '^s[0-9]*\.png$')" "2"
has   "frame 2 never reached: warning" "$out" "[WARNING] --screenshot-at: 2 of 3 screenshots taken"
out=$(timeout 60 "$SIM" --batch -r --fast -B "$W/bios.v32" --screenshot-at=0-1 --screenshot-file="$W/t%d.png" "$W/h.v32" 2>&1 </dev/null)
hasnt "all reached: no warning" "$out" "WARNING"
out=$(timeout 60 "$SIM" --batch -r --fast -B "$W/bios.v32" --screenshot-at=0 --screenshot-file="$W/u%d.png" - 2>&1 </dev/null)

echo "== startup exit statuses"
echo hello > "$W/junk.v32"
timeout 20 "$SIM" --batch -B "$W/nosuchfile" "$W/h.v32" >/dev/null 2>&1 </dev/null; check "missing BIOS: 11"    "$?" "11"
timeout 20 "$SIM" --batch -B "$W/junk.v32"   "$W/h.v32" >/dev/null 2>&1 </dev/null; check "not a V32 file: 21" "$?" "21"
timeout 20 "$SIM" --bogus >/dev/null 2>&1 </dev/null;                                  check "bad option: 1"      "$?" "1"
timeout 20 "$SIM" --halt-status=0 >/dev/null 2>&1 </dev/null;                          check "bad value: 1"       "$?" "1"
timeout 20 "$SIM" --batch -C "$W/nosuchcmds" -B "$W/bios.v32" "$W/h.v32" >/dev/null 2>&1 </dev/null; check "-C unreadable: 1" "$?" "1"
H=$("$SIM" --help)
has "--help lists 11 / 12 / 21"   "$H" "11 a file cannot be opened, 12 not read, 21 not a V32 file"

echo "cli tests: $pass passed, $fail failed"
[ $fail -eq 0 ]
