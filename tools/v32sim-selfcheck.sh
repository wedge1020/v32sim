#!/bin/sh
# tools/v32sim-selfcheck.sh -- run self-checking cartridges headless in v32sim and say which pass.
#
#   tools/v32sim-selfcheck.sh [-B BIOS] [-F FRAMES] [-g GLOBAL] [-M] [--oracle] CART.v32...
#
# The convention (v32cxx, v32nix, V32DOS all use it): a test cartridge declares `int test_errors = -1;`, sets it to 0 when
# every check passed, and halts. Starting at -1 means a crash, a hang or a cartridge that never reaches the
# end fails too. A cartridge passes when it halts within FRAMES frames (default 1200) and the global reads 0.
#
#   -g GLOBAL   the global to read (default test_errors)
#   -M          give the cartridge a fresh blank memory card (CART.memc, recreated blank first)
#   --oracle    run on the stock console logic instead (tools/oracle: v32run) as a second opinion; the address of
#               GLOBAL comes from CART's .asm.sym or its `%define global_NAME` line
#
# A CART.input beside the cartridge (a v32prof-style script, as v32cxx's tests/NNNsample.input) is played: it is converted
# by tools/vprof2cmd.py and fed to v32sim. The data symbol comes from CART.asm.sym / CART.asm next to the cartridge
# (tools/v32mkcart.sh leaves them there). Exit status: 0 all passed, 1 one failed, 2 usage / tool missing.
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}; [ -x "$SIM" ] || SIM=$(command -v v32sim)
BIOS=${V32BIOS:-StandardBios.v32}; FRAMES=1200; G=test_errors; CARD=0; ORACLE=0
while [ $# -gt 0 ]; do
    case $1 in
        -B) BIOS=$2; shift 2;; -F) FRAMES=$2; shift 2;; -g) G=$2; shift 2;; -M) CARD=1; shift;; --oracle) ORACLE=1; shift;;
        -*) echo "usage: v32sim-selfcheck.sh [-B BIOS] [-F FRAMES] [-g GLOBAL] [-M] [--oracle] CART.v32..." >&2; exit 2;;
        *) break;;
    esac
done
[ $# -gt 0 ] || { echo "v32sim-selfcheck: no cartridge given" >&2; exit 2; }
[ -x "$SIM" ] || { echo "v32sim-selfcheck: no v32sim (build it, or set V32SIM)" >&2; exit 2; }
[ -f "$BIOS" ] || BIOS=$(command -v "$BIOS") || { echo "v32sim-selfcheck: no BIOS (-B or \$V32BIOS)" >&2; exit 2; }
fail=0; n=0
for cart in "$@"; do
    n=$((n+1)); base=${cart%.v32}; name=$(basename "$base")
    [ -f "$cart" ] || { echo "FAIL $name: no such cartridge"; fail=1; continue; }
    card=; [ $CARD = 1 ] && { card="$base.memc"; rm -f "$card"; { printf "V32-MEMC"; head -c 1048576 /dev/zero; } > "$card"; }   # a blank card: header + 256K zero words
    if [ $ORACLE = 1 ]; then
        RUN=${V32RUN:-$HERE/oracle/bin/v32run}; [ -x "$RUN" ] || RUN=$(command -v v32run)
        [ -x "$RUN" ] || { echo "v32sim-selfcheck: no v32run (tools/oracle/build.sh)" >&2; exit 2; }
        addr=$(sed -n "s/^V,$G,\([0-9]*\),.*/\1/p" "$base.asm.sym" 2>/dev/null | head -1)
        [ -n "$addr" ] || addr=$(sed -n "s/^ *%define global_$G \([0-9xA-Fa-f]*\).*/\1/p" "$base.asm" 2>/dev/null | head -1)
        [ -n "$addr" ] || { echo "FAIL $name: no address for $G ($base.asm.sym / $base.asm)"; fail=1; continue; }
        res=$("$RUN" "$BIOS" "$cart" "${card:--}" "$addr" "$FRAMES" 2>/dev/null)
        halted=$(echo "$res" | sed -n 's/.*halted=\([01]\).*/\1/p'); value=$(echo "$res" | sed -n 's/.*ram\[[0-9]*\]=\(-\{0,1\}[0-9]*\).*/\1/p')
        st=$([ "$halted" = 1 ] && echo 0 || echo 2)
    else
        set -- -x --fast --max-frames="$FRAMES" -B "$BIOS" --dump="1@$G"; [ -n "$card" ] && set -- "$@" -M "$card"
        if [ -f "$base.input" ]; then
            cmd=$(mktemp); python3 -I "$HERE/vprof2cmd.py" "$base.input" > "$cmd" || { echo "FAIL $name: cannot convert $name.input"; fail=1; continue; }
            sed -i '$d' "$cmd"; echo "continue" >> "$cmd"        # play the script, then let it run on to the end
            out=$("$SIM" --batch "$@" "$cart" < "$cmd" 2>/dev/null); st=$?; rm -f "$cmd"
        else
            out=$("$SIM" --batch -r "$@" "$cart" < /dev/null 2>/dev/null); st=$?
        fi
        value=$(echo "$out" | tail -1)
    fi
    if [ "$st" = 0 ] && [ "$value" = 0 ]; then echo "pass $name"
    else echo "FAIL $name: v32sim status $st (0 halted, 2 frame limit, 3 system error), $G = ${value:-?}"; fail=1; fi
done
echo "$n cartridge(s), $([ $fail = 0 ] && echo all passed || echo FAILED)"
exit $fail
