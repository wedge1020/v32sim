#!/usr/bin/env python3
"""vprof2cmd.py -- turn a v32prof / v32peek / v32shot input script into a v32sim command file.

    vprof2cmd.py SCRIPT [--frames N] [--cmd 'TEXT' ...] > session.cmd
    v32sim --batch --fast -x -B BIOS cart.v32 < session.cmd

The oracle tools (tools/oracle: v32prof, v32peek, v32shot) are driven by a frame-stamped script:

    <frame> connect <port> <0|1>        plug / unplug a gamepad
    <frame> press|release|tap <port> <control>     left right up down start a b x y l r
    <frame> segment <name>              (ignored here: v32sim has no segments)
    <frame> end                         stop the run

v32sim is driven by commands, and `gamepad N CONTROL` toggles a button, so this script tracks which buttons are held,
turns the frame stamps into `run N` (frames since the previous event) and writes the toggles. Events at the same frame
are applied together, before that frame runs, as the oracle tools do. A `tap` presses now and releases at the next
frame. The result runs the same input in v32sim, so a script made for v32cxx's check.sh (tests/NNNsample.input, made
by tools/v32io-script.py from keyboard and mouse actions) works against v32sim too.

--frames N    stop after frame N when the script has no `end` line (default: no stop; add --max-frames to the run)
--cmd TEXT    append a command at the end (repeatable): --cmd 'print counter' --cmd 'screenshot out.png'
The file ends with `quit` unless the last --cmd is a quit.
"""
import sys

CONTROLS = {"left": "left", "right": "right", "up": "up", "down": "down", "start": "start",
            "a": "A", "b": "B", "x": "X", "y": "Y", "l": "L", "r": "R"}


def main(argv):
    if len(argv) < 2 or argv[1] in ("-h", "--help"):
        sys.stdout.write(__doc__)
        return 0 if len(argv) >= 2 else 2
    path, frames, extra = argv[1], None, []
    i = 2
    while i < len(argv):
        if argv[i] == "--frames":
            frames = int(argv[i + 1]); i += 2
        elif argv[i] == "--cmd":
            extra.append(argv[i + 1]); i += 2
        else:
            sys.stderr.write("vprof2cmd: unknown option %s\n" % argv[i]); return 2
    events = []                                  # (frame, order, words)
    for n, line in enumerate(open(path), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        w = line.split()
        try:
            f = int(w[0])
        except ValueError:
            sys.stderr.write("vprof2cmd: %s:%d: expected a frame number: %s\n" % (path, n, line)); return 2
        if len(w) < 2:
            sys.stderr.write("vprof2cmd: %s:%d: no command\n" % (path, n)); return 2
        events.append((f, n, w[1:]))
    events.sort(key=lambda e: (e[0], e[1]))
    out, now = [], 0
    held = {}                                    # (port, control) -> True
    pending = []                                 # releases of taps, as events
    end = frames
    k = 0
    while k < len(events) or pending:
        pending.sort(key=lambda e: e[0])
        if pending and (k >= len(events) or pending[0][0] <= events[k][0]):
            ev = pending.pop(0)
        else:
            ev = events[k]; k += 1
        f, _, w = ev
        if w[0] == "end":
            end = f
            break
        if f > now:
            out.append("run %d" % (f - now)); now = f
        if w[0] == "segment":
            continue
        if w[0] == "connect" and len(w) == 3:
            out.append("gamepad %s %s" % (w[1], "connect" if w[2] != "0" else "disconnect"))
            if w[2] == "0":
                for key in [k2 for k2 in held if k2[0] == w[1]]:
                    del held[key]
        elif w[0] in ("press", "release", "tap") and len(w) == 3 and w[2] in CONTROLS:
            key = (w[1], w[2])
            if w[0] in ("press", "tap") and key not in held:
                out.append("gamepad %s %s" % (w[1], CONTROLS[w[2]])); held[key] = True
            elif w[0] == "release" and key in held:
                out.append("gamepad %s %s" % (w[1], CONTROLS[w[2]])); del held[key]
            if w[0] == "tap":
                pending.append((f + 1, 0, ["release", w[1], w[2]]))
        else:
            sys.stderr.write("vprof2cmd: cannot convert: %s\n" % " ".join(w)); return 2
    if end is not None and end > now:
        out.append("run %d" % (end - now))
    out.extend(extra)
    if not out or not out[-1].split()[0].lower().startswith("quit"):
        out.append("quit")
    sys.stdout.write("\n".join(out) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
