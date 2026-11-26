#!/bin/sh
# bios/src/build.sh: rebuild bios/StubBios.v32 (and bios/StubBios.vbin.debug) from StubBios.asm.
#
#   bios/src/build.sh [TOOLCHAIN_DIR]
#
# Assembler: v32as (preferred: it also writes the debug file, so v32sim shows BIOS labels and
# source lines) or the stock `assemble`, both with -b; they give the same VBIN. Packer: the
# stock `packrom` with StubBios.xml, or, without it, python3 (the V32 header is 128 bytes).
# BiosTexture.vtex and BiosSound.vsnd are StandardBios's own texture and sound, byte for byte
# as they are in StandardBios.v32 (v1.2).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
[ -n "$1" ] && PATH=$(cd "$1" && pwd):$PATH
cd "$HERE"
mkdir -p obj

if command -v v32as >/dev/null 2>&1; then
    v32as -b -g -o obj/StubBios.vbin StubBios.asm
    cp obj/StubBios.vbin.debug ../StubBios.vbin.debug
elif command -v assemble >/dev/null 2>&1; then
    assemble -b StubBios.asm -o obj/StubBios.vbin
    echo "note: stock assembler: bios/StubBios.vbin.debug left as it was (v32as writes it)"
else
    echo "need v32as or assemble in PATH (or give the toolchain directory)" >&2; exit 2
fi

if command -v packrom >/dev/null 2>&1; then
    packrom StubBios.xml -o ../StubBios.v32
else
    python3 -I - obj/StubBios.vbin BiosTexture.vtex BiosSound.vsnd ../StubBios.v32 <<'EOF'
import struct, sys
vbin, vtex, vsnd = (open(p, "rb").read() for p in sys.argv[1:4])
title = b"v32sim StubBios".ljust(64, b"\0")
prog = 128; video = prog + len(vbin); audio = video + len(vtex)
hdr = b"V32-BIOS" + struct.pack("<II", 1, 0) + title + struct.pack("<IIII", 1, 0, 1, 1)
hdr += struct.pack("<IIIIII", prog, len(vbin), video, len(vtex), audio, len(vsnd))
open(sys.argv[4], "wb").write(hdr.ljust(128, b"\0") + vbin + vtex + vsnd)
EOF
fi
echo "built $(cd .. && pwd)/StubBios.v32"
