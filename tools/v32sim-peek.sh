#!/bin/sh
# tools/v32sim-peek.sh -- run a cartridge headless for N frames and print words of a C global (or a text buffer).
#
#   tools/v32sim-peek.sh [-B BIOS] [-F FRAMES] [-t] [-s INPUT.script] [-M CARD.memc] CART.v32 SYMBOL [COUNT]
#
# What v32nix's tools/peek.sh and klog.sh did on the stock emulator core (v32peek), done in v32sim: the address of SYMBOL
# comes from the debug files beside the cartridge (CART.asm.sym, or the `%define global_NAME` lines of CART.asm), so
# the cartridge must have been built with them (tools/v32mkcart.sh does). COUNT words (default 1) print as signed integers,
# one per line. With -t the words are decoded as characters: a kernel log ring, a console buffer, a string result.
# -s plays a v32prof-style input script (tools/vprof2cmd.py). -M puts a memory card in the slot (a blank one is made when the file
# does not exist), so a second run sees what the first wrote. The run stops at FRAMES (default 600) or at HLT; either way
# the words are printed (a frame limit is not an error here).
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}; [ -x "$SIM" ] || SIM=$(command -v v32sim)
BIOS=${V32BIOS:-StandardBios.v32}; FRAMES=600; TEXT=0; INPUT=; CARD=
while [ $# -gt 0 ]; do
    case $1 in -B) BIOS=$2; shift 2;; -F) FRAMES=$2; shift 2;; -t) TEXT=1; shift;; -s) INPUT=$2; shift 2;; -M) CARD=$2; shift 2;; *) break;; esac
done
CART=${1:?usage: v32sim-peek.sh [-B BIOS] [-F FRAMES] [-t] [-s SCRIPT] CART.v32 SYMBOL [COUNT]}; SYM=${2:?symbol}; COUNT=${3:-1}
[ -f "$BIOS" ] || BIOS=$(command -v "$BIOS") || { echo "v32sim-peek: no BIOS (-B or \$V32BIOS)" >&2; exit 2; }
MARG=
if [ -n "$CARD" ]; then
    [ -f "$CARD" ] || { printf 'V32-MEMC'; head -c 1048576 /dev/zero; } > "$CARD"
    MARG="-M $CARD"
fi
if [ -n "$INPUT" ]; then
    CMD=$(mktemp); python3 -I "$HERE/vprof2cmd.py" "$INPUT" --frames "$FRAMES" > "$CMD" || exit 2
    out=$("$SIM" --batch --fast -x --max-frames="$FRAMES" -B "$BIOS" $MARG --dump="$COUNT@$SYM" "$CART" < "$CMD" 2>/dev/null); rm -f "$CMD"
else
    out=$("$SIM" --batch -r --fast -x --max-frames="$FRAMES" -B "$BIOS" $MARG --dump="$COUNT@$SYM" "$CART" < /dev/null 2>/dev/null)
fi
if [ $TEXT = 1 ]; then
    echo "$out" | python3 -I -c '
import sys
out = []
for line in sys.stdin:
    try: out.append(chr(int(line.split()[0])))
    except (ValueError, IndexError): pass
sys.stdout.write("".join(out))'
else
    echo "$out"
fi
