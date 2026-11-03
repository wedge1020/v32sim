#!/bin/sh
# tools/v32sim-golden.sh -- run typed sessions against a cartridge in v32sim and compare the screen with golden files.
#
#   tools/v32sim-golden.sh [-u] --cart CART.v32 --bios BIOS.v32 --sessions DIR --golden DIR [options] [-- V32SIM_ARGS...]
#
# Generalized from V32DOS's tests/run.sh. Every DIR/session*.txt is turned into a v32sim script by tools/v32sim-session.py
# (syntax there), run with `v32sim --batch --fast -x`, and its stdout (the screen dumps) is compared with GOLDEN/NAME.txt.
# -u writes the golden files instead of comparing. Exit status 0 all passed, 1 a session failed or differed, 2 setup problem.
#
#   --prompt-symbol SYM | --prompt-text TEXT | --kbd-port N | --wait-frames N | --boot-frames N   passed to v32sim-session.py
#   --prepare PROG       run as `PROG CARDFILE` before each session to put a memory card together (a card is then in the
#                        slot as -M CARDFILE); without it no card is given
#   --mask FILE          a sed script applied to the screen text before comparing: mask what changes with every build
#                        (sizes, dates, versions) and nothing else
#   --max-frames N       frame limit per session (default 40000; hitting it fails the session: exit status 2 of v32sim)
#   --timeout SECONDS    wall-clock limit per session (default 600)
#   everything after -- goes to v32sim (e.g. --symbol-file=F.asm.sym --cart-c-debug=F.vbin.debug --fixed-clock --seed=1)
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}; [ -x "$SIM" ] || SIM=$(command -v v32sim)
UPDATE=0; CART=; BIOS=; SESS=; GOLD=; PREP=; MASK=; MAXF=40000; TMO=600; SARGS=""
while [ $# -gt 0 ]; do
    case $1 in
        -u) UPDATE=1; shift;; --cart) CART=$2; shift 2;; --bios) BIOS=$2; shift 2;; --sessions) SESS=$2; shift 2;;
        --golden) GOLD=$2; shift 2;; --prepare) PREP=$2; shift 2;; --mask) MASK=$2; shift 2;;
        --max-frames) MAXF=$2; shift 2;; --timeout) TMO=$2; shift 2;;
        --prompt-symbol|--prompt-text|--kbd-port|--wait-frames|--boot-frames) SARGS="$SARGS $1 '$2'"; shift 2;;
        --) shift; break;;
        *) echo "v32sim-golden: unknown option $1 (see the header of this file)" >&2; exit 2;;
    esac
done
[ -f "$CART" ] && [ -f "$BIOS" ] && [ -d "$SESS" ] && [ -n "$GOLD" ] || { echo "v32sim-golden: need --cart, --bios, --sessions DIR and --golden DIR" >&2; exit 2; }
[ -x "$SIM" ] || { echo "v32sim-golden: no v32sim (set V32SIM)" >&2; exit 2; }
W=${TMPDIR:-/tmp}/v32sim-golden.$$; mkdir -p "$W"; trap 'rm -rf "$W"' EXIT
fail=0; ran=0
for sess in "$SESS"/session*.txt; do
    [ -f "$sess" ] || continue
    n=$(basename "$sess" .txt); ran=$((ran+1))
    eval python3 -I "$HERE/v32sim-session.py" $SARGS --file "$W/$n.cmd" "$sess" || { echo "FAIL $n: cannot make the script"; fail=1; continue; }
    MARG=; if [ -n "$PREP" ]; then "$PREP" "$W/card.memc" >/dev/null || { echo "FAIL $n: --prepare failed"; fail=1; continue; }; MARG="-M $W/card.memc"; fi
    timeout "$TMO" "$SIM" -q --batch -x --max-frames="$MAXF" -B "$BIOS" --fast $MARG "$@" "$CART" < "$W/$n.cmd" > "$W/$n.raw" 2> "$W/$n.err"
    st=$?
    if [ -n "$MASK" ]; then sed -E -f "$MASK" "$W/$n.raw" > "$W/$n.out"; else cp "$W/$n.raw" "$W/$n.out"; fi
    if [ $st -ne 0 ]; then echo "FAIL $n: v32sim exit status $st (1 check or wait failed, 2 frame limit, 3 system error)"; cat "$W/$n.err"; tail -5 "$W/$n.out"; fail=1; continue; fi
    if [ $UPDATE = 1 ]; then mkdir -p "$GOLD"; cp "$W/$n.out" "$GOLD/$n.txt"; echo "wrote $GOLD/$n.txt"
    elif [ ! -f "$GOLD/$n.txt" ]; then echo "FAIL $n: no golden file $GOLD/$n.txt (run with -u)"; fail=1
    elif diff -u "$GOLD/$n.txt" "$W/$n.out" > "$W/$n.diff"; then echo "pass $n"
    else echo "FAIL $n"; head -40 "$W/$n.diff"; fail=1; fi
done
[ $ran -gt 0 ] || { echo "v32sim-golden: no $SESS/session*.txt" >&2; exit 2; }
exit $fail
