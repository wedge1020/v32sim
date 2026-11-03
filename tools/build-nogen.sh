#!/bin/sh
# tools/build-nogen.sh [OUT_DIR] -- build v32sim with cc and GNU readline only: no make, no flex, no bison.
# Uses the pre-generated scanners/parsers in gen/ (refresh them with `make gen` after editing a .l or .y file); without
# gen/ it runs flex and bison. Downstream projects (v32nix, V32DOS) build v32sim this way from their bootstrap archive.
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/.." && pwd)
OUT=${1:-$SRC/bin}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd); SRC=$(cd "$SRC" && pwd)
echo '#include <readline/readline.h>' | cc -E - >/dev/null 2>&1 || { echo "v32sim: GNU readline headers not found (install libreadline-dev)" >&2; exit 1; }
W=${TMPDIR:-/tmp}/v32sim-build.$$; mkdir -p "$W/gen"
if [ -f "$SRC/gen/parser.c" ] && [ -f "$SRC/gen/asm_lexer.c" ]; then
    cp "$SRC"/gen/* "$W/gen/"
else
    command -v flex >/dev/null && command -v bison >/dev/null || { echo "v32sim: no gen/ in the sources and no flex/bison to make it" >&2; exit 1; }
    ( cd "$W/gen" && bison --defines=parser.h -o parser.c "$SRC/src/parser.y" && flex -o lexer.c "$SRC/src/lexer.l" &&
      bison -p asm --defines=asm_parser.h -o asm_parser.c "$SRC/src/asm_parser.y" && flex -P asm -o asm_lexer.c "$SRC/src/asm_lexer.l" ) || exit 1
fi
LH=; [ "$(uname -s)" = Darwin ] || LH=-lhistory
( cd "$W" && cc -O2 -w --std=gnu18 -funsigned-char -I"$SRC/inc" -Igen -o "$OUT/v32sim" "$SRC"/src/*.c gen/*.c -lreadline -lm $LH ) || { echo "v32sim build failed" >&2; exit 1; }
rm -rf "$W"; echo "built $OUT/v32sim"
