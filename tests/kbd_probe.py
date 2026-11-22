#!/usr/bin/env python3
"""kbd_probe.py [--stall N] OUT.vbin     |     kbd_probe.py --bios OUT.vbin     |     kbd_probe.py --hlt OUT.vbin

Writes a VBIN (no toolchain needed: the few instructions are encoded here) of a v32kbd reader that
makes the problem of "a key lost after a long operation" reproducible:

  * selects gamepad 1 (where tests attach the v32kbd),
  * does not look at the gamepad at all for N frames (the program is "busy": a v32dev round trip,
    a long computation),
  * then, every frame, reads the gamepad; when the strobe (Left > 0) differs from the
    last one it saw, it logs one event word at RAM[0x100 + n]:  key code | (pressed << 7).
    RAM[0xFF] counts frames after the stall.

An event the program never sees is one that the next event replaced before it looked.
"""
import struct, sys

OP = dict(HLT=0x00, WAIT=0x01, JMP=0x02, JT=0x05, JF=0x06, IGT=0x09, INE=0x08, MOV=0x13,
          IN=0x17, OUT=0x18, OR=0x22, SHL=0x25, IADD=0x26, ISUB=0x27)
PORT = dict(Sel=0x400, Left=0x402, Up=0x404, Start=0x406, A=0x407, B=0x408, X=0x409, Y=0x40A, L=0x40B, R=0x40C)
BASE = 0x20000000
prog = []            # entries: ("label", name) or (words, fixups)


def ins(op, dst=0, src=0, mode=0, port=0, imm=None, target=None):
    w = (OP[op] << 26) | (dst << 21) | (src << 17) | (mode << 14) | port
    if imm is not None or target is not None:
        w |= 0x02000000
    prog.append((w, imm, target))


def label(n):
    prog.append(("label", n))


def bios(out):
    """the minimal BIOS of tests/gaps/bios.asm: word 0 = error vector (halts), word 4 jumps to the cartridge"""
    w = [(OP["JMP"] << 26) | 0x02000000, 0x10000008, 0, 0, (OP["JMP"] << 26) | 0x02000000, BASE, 0]
    open(out, "wb").write(b"V32-VBIN" + struct.pack("<I", len(w)) + b"".join(struct.pack("<I", x) for x in w))


def main():
    a = sys.argv[1:]
    if a and a[0] == "--bios":
        return bios(a[1])
    if a and a[0] == "--hlt":                        # a program that executes HLT after two frames
        w = [OP["WAIT"] << 26, OP["WAIT"] << 26, OP["HLT"] << 26]
        open(a[1], "wb").write(b"V32-VBIN" + struct.pack("<I", len(w)) + b"".join(struct.pack("<I", x) for x in w))
        return
    stall = 0
    while a and a[0].startswith("--"):
        if a[0] == "--stall": stall = int(a[1])
        else: sys.exit("bad option " + a[0])
        a = a[2:]
    out = a[0]

    ins("OUT", port=PORT["Sel"], imm=1)
    ins("MOV", dst=5, mode=0, imm=0x100)             # R5: log pointer
    ins("MOV", dst=6, mode=0, imm=0)                 # R6: last strobe
    ins("MOV", dst=8, mode=0, imm=0xFF)              # R8: address of the frame counter
    ins("MOV", dst=9, mode=0, imm=0)                 # R9: frames after the stall
    if stall > 0:
        ins("MOV", dst=0, mode=0, imm=stall)
        label("stall")
        ins("WAIT")
        ins("ISUB", dst=0, imm=1)
        ins("MOV", dst=1, mode=1, src=0)
        ins("IGT", dst=1, imm=0)
        ins("JT", dst=1, target="stall")
    label("poll")
    ins("WAIT")
    ins("IADD", dst=9, imm=1)
    ins("MOV", dst=8, mode=6, src=9)                 # RAM[0xFF] = frames after the stall
    ins("IN", dst=2, port=PORT["Left"])
    ins("IGT", dst=2, imm=0)
    ins("MOV", dst=7, mode=1, src=2)
    ins("INE", dst=7, src=6)
    ins("JF", dst=7, target="poll")
    ins("MOV", dst=6, mode=1, src=2)
    ins("MOV", dst=3, mode=0, imm=0)
    for bit, p in enumerate(["Start", "A", "B", "X", "Y", "L", "R"]):
        ins("IN", dst=4, port=PORT[p])
        ins("IGT", dst=4, imm=0)
        ins("SHL", dst=4, imm=bit)
        ins("OR", dst=3, src=4)
    ins("IN", dst=4, port=PORT["Up"])
    ins("IGT", dst=4, imm=0)
    ins("SHL", dst=4, imm=7)
    ins("OR", dst=3, src=4)
    ins("MOV", dst=5, mode=6, src=3)
    ins("IADD", dst=5, imm=1)
    ins("JMP", target="poll")

    # layout: first pass addresses, second pass encode
    labels, pos = {}, 0
    for e in prog:
        if e[0] == "label": labels[e[1]] = BASE + pos
        else: pos += 2 if (e[1] is not None or e[2] is not None) else 1
    words = []
    for e in prog:
        if e[0] == "label": continue
        w, imm, tgt = e
        words.append(w)
        if tgt is not None: words.append(labels[tgt])
        elif imm is not None: words.append(imm & 0xFFFFFFFF)
    open(out, "wb").write(b"V32-VBIN" + struct.pack("<I", len(words)) + b"".join(struct.pack("<I", x) for x in words))


main()
