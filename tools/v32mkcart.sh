#!/bin/sh
# tools/v32mkcart.sh -- build a cartridge, with the debug files v32sim reads, from a C or assembly source.
#
#   tools/v32mkcart.sh [-o OUTDIR] [-O LEVEL] [--stock] [--srcmark] [--cc-opt OPT]... SRC.c|SRC.cpp|SRC.lua|SRC.asm
#
# Default chain: v32cc -g, v32as -g, packrom.  By source language (the one place that knows every front end's -g chain):
#   .c      v32cc -g  ->  v32as -g
#   .cpp .cc .cxx   v32c++ -g --compiler=v32cc (C with #line markers: the C++ lines reach the debugger)  ->  v32cc -g
#           --target-compat=v32cxx  ->  v32as -g          (v32c++'s headers: $V32CXX_INCLUDE, or TOOLCHAIN/include/v32c++)
#   .lua    v32lua -g  ->  v32as -g   (the cartridge XML v32lua wrote is used: textures and sounds come along)
#   .asm    v32as -g
#   -O LEVEL   run v32opt -O LEVEL (1, 2, 3, s) on the compiler's assembly before v32as: the optimized build keeps its source
#              lines and variable homes (V32DBG); the unoptimized assembly stays beside it as NAME.O0.asm.  Beside OUTDIR/NAME.v32 it leaves NAME.asm, NAME.asm.sym (data symbols with
# sizes, for `print counter`, `x/d buf`, --dump=1@counter), NAME.vbin.debug (address -> C line and function, for `bt`,
# `break FILE:LINE`, --source) and NAME.vbin.vars (locals), which v32sim finds by itself.
#
#   --stock     use the stock `compile` / `assemble` (the Vircon32 devtools) instead of v32cc / v32as. The stock tools write
#               no enhanced debug info; without --srcmark you get addresses only and `%define global_X` data symbols.
#   --srcmark   with --stock: run v32opt's scripts/v32srcmark.py on the compiler's output so the cartridge gets ;@src
#               markers, and assemble with v32as -g. ($V32SRCMARK names the script when it is not on the PATH.)
#
# Tools are taken from the PATH, or from the directory $V32TC (which is also put first on the PATH, so v32c++ finds the stock SDK headers) (v32cc, v32as, compile, assemble, packrom), and
# the BIOS-independent XML is written here (no textures or sounds: a cartridge with assets needs your own .xml).
set -e
[ -n "${V32TC:-}" ] && PATH="$V32TC:$PATH" && export PATH
OUT=.; STOCK=0; MARK=0; OPTS=""; OPTLVL=""
while [ $# -gt 1 ]; do
    case $1 in
        -o) OUT=$2; shift 2;;
        -O) OPTLVL=$2; shift 2;;
        --stock) STOCK=1; shift;;
        --srcmark) MARK=1; shift;;
        --cc-opt) OPTS="$OPTS $2"; shift 2;;
        *) break;;
    esac
done
SRC=${1:?usage: v32mkcart.sh [-o OUTDIR] [-O LEVEL] [--stock] [--srcmark] [--cc-opt OPT]... SRC.c|SRC.cpp|SRC.lua|SRC.asm}
tool() { if [ -n "$V32TC" ] && [ -x "$V32TC/$1" ]; then echo "$V32TC/$1"; else command -v "$1" || { echo "v32mkcart: no $1 (PATH or \$V32TC)" >&2; exit 2; }; fi; }
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd); SRC=$(cd "$(dirname "$SRC")" && pwd)/$(basename "$SRC")
EXT=${SRC##*.}; NAME=$(basename "$SRC" ."$EXT")
cd "$(dirname "$SRC")"      # the debug files name sources relative to here
PACKROM=$(tool packrom)
OWNXML=1
# optimize($asm): v32opt -O$OPTLVL in place, the plain assembly kept as NAME.O0.asm (the side files are named after NAME.asm)
optimize() {
    [ -n "$OPTLVL" ] || return 0
    cp "$1" "${1%.asm}.O0.asm"
    $(tool v32opt) -O"$OPTLVL" "${1%.asm}.O0.asm" -o "$1"
}
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
        optimize "$OUT/$NAME.asm"
        $(tool v32as) -g -o "$OUT/$NAME.vbin" "$OUT/$NAME.asm"
    fi;;
  cpp|cc|cxx)
    # v32c++ with v32cc as the compiler: -g then writes the #line markers by itself (v32cxx 20261109-dev)
    $(tool v32c++) -g --compiler=v32cc --no-build-scripts $OPTS -o "$OUT/$NAME.c" "$(basename "$SRC")"
    $(tool v32cc) -g --target-compat=v32cxx -o "$OUT/$NAME.asm" "$OUT/$NAME.c"
    optimize "$OUT/$NAME.asm"
    $(tool v32as) -g -o "$OUT/$NAME.vbin" "$OUT/$NAME.asm";;
  lua)
    $(tool v32lua) -g --no-build-scripts $OPTS -o "$OUT/$NAME.asm" "$(basename "$SRC")" >/dev/null
    optimize "$OUT/$NAME.asm"
    $(tool v32as) -g -o "$OUT/$NAME.vbin" "$OUT/$NAME.asm"
    sed -i "s|<binary path=\"[^\"]*\"|<binary path=\"$NAME.vbin\"|" "$OUT/$NAME.xml"
    OWNXML=0;;
  asm|s)
    if [ $STOCK = 1 ]; then $(tool assemble) "$SRC" -o "$OUT/$NAME.vbin" -g program
    else $(tool v32as) -g -o "$OUT/$NAME.vbin" "$SRC"; fi;;
  *) echo "v32mkcart: $SRC: not a .c, .cpp, .lua or .asm file" >&2; exit 2;;
esac
[ $OWNXML = 0 ] || cat > "$OUT/$NAME.xml" <<XML
<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<rom-definition version="1.0">
<rom type="cartridge" title="$NAME" version="1.0" />
<binary path="$NAME.vbin" />
<textures /><sounds />
</rom-definition>
XML
( cd "$OUT" && "$PACKROM" "$NAME.xml" -o "$NAME.v32" >/dev/null )
echo "$OUT/$NAME.v32"
