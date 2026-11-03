#!/bin/sh
# tools/v32mkcart.sh -- build a cartridge, with the debug files v32sim reads, from a C or assembly source.
#
#   tools/v32mkcart.sh [-o OUTDIR] [--stock] [--srcmark] [--cc-opt OPT]... SRC.c|SRC.asm
#
# Default chain: v32cc -g, v32as -g, packrom.  Beside OUTDIR/NAME.v32 it leaves NAME.asm, NAME.asm.sym (data symbols with
# sizes, for `print counter`, `x/d buf`, --dump=1@counter), NAME.vbin.debug (address -> C line and function, for `bt`,
# `break FILE:LINE`, --source) and NAME.vbin.vars (locals), which v32sim finds by itself.
#
#   --stock     use the stock `compile` / `assemble` (the Vircon32 devtools) instead of v32cc / v32as. The stock tools write
#               no enhanced debug info; without --srcmark you get addresses only and `%define global_X` data symbols.
#   --srcmark   with --stock: run v32opt's scripts/v32srcmark.py on the compiler's output so the cartridge gets ;@src
#               markers, and assemble with v32as -g. ($V32SRCMARK names the script when it is not on the PATH.)
#
# Tools are taken from the PATH, or from the directory $V32TC (v32cc, v32as, compile, assemble, packrom), and
# the BIOS-independent XML is written here (no textures or sounds: a cartridge with assets needs your own .xml).
set -e
OUT=.; STOCK=0; MARK=0; OPTS=""
while [ $# -gt 1 ]; do
    case $1 in
        -o) OUT=$2; shift 2;;
        --stock) STOCK=1; shift;;
        --srcmark) MARK=1; shift;;
        --cc-opt) OPTS="$OPTS $2"; shift 2;;
        *) break;;
    esac
done
SRC=${1:?usage: v32mkcart.sh [-o OUTDIR] [--stock] [--srcmark] [--cc-opt OPT]... SRC.c|SRC.asm}
tool() { if [ -n "$V32TC" ] && [ -x "$V32TC/$1" ]; then echo "$V32TC/$1"; else command -v "$1" || { echo "v32mkcart: no $1 (PATH or \$V32TC)" >&2; exit 2; }; fi; }
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd); SRC=$(cd "$(dirname "$SRC")" && pwd)/$(basename "$SRC")
EXT=${SRC##*.}; NAME=$(basename "$SRC" ."$EXT")
cd "$(dirname "$SRC")"      # the debug files name sources relative to here
PACKROM=$(tool packrom)
case $EXT in
  c)
    if [ $STOCK = 1 ]; then
        $(tool compile) "$SRC" -o "$OUT/$NAME.asm" -g $OPTS
        if [ $MARK = 1 ]; then
            python3 -I "${V32SRCMARK:-$(command -v v32srcmark.py)}" "$OUT/$NAME.asm"
            $(tool v32as) -g --map="$OUT/$NAME.asm.debug" -o "$OUT/$NAME.vbin" "$OUT/$NAME.asm"
        else
            $(tool assemble) "$OUT/$NAME.asm" -o "$OUT/$NAME.vbin" -g program
        fi
    else
        $(tool v32cc) -g $OPTS -o "$OUT/$NAME.asm" "$SRC"
        $(tool v32as) -g -o "$OUT/$NAME.vbin" "$OUT/$NAME.asm"
    fi;;
  asm|s)
    if [ $STOCK = 1 ]; then $(tool assemble) "$SRC" -o "$OUT/$NAME.vbin" -g program
    else $(tool v32as) -g -o "$OUT/$NAME.vbin" "$SRC"; fi;;
  *) echo "v32mkcart: $SRC: not a .c or .asm file" >&2; exit 2;;
esac
cat > "$OUT/$NAME.xml" <<XML
<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<rom-definition version="1.0">
<rom type="cartridge" title="$NAME" version="1.0" />
<binary path="$NAME.vbin" />
<textures /><sounds />
</rom-definition>
XML
( cd "$OUT" && "$PACKROM" "$NAME.xml" -o "$NAME.v32" >/dev/null )
echo "$OUT/$NAME.v32"
