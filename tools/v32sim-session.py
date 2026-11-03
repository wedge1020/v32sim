#!/usr/bin/env python3
r"""v32sim-session.py [options] --file OUT.cmd SESSION.txt  -- write a v32sim script that types a session on a v32kbd.

Generalized from V32DOS's tests/mkcmd.py (same session syntax, byte-identical output with
`--prompt-symbol dos_print_prompt`). Use it for any program with a prompt: a shell, a DOS, a monitor, a menu.

SESSION.txt, one line each:
    #...                   a comment
    (empty)                types just Enter
    [N:][{TEXT}]command    types the command on a v32kbd, waits until it is done, then dumps the screen
    @key CHORD             presses a key chord (ctrl+c) and waits for the prompt (a program that Ctrl-C stops)
    @hold KEY FRAMES       holds KEY down for FRAMES frames (key repeat), then releases it and dumps the screen
    [N:][{TEXT}]~keys      the same, but the keys are typed without the Enter (for a program that reads single keys)
    @run N                 runs N frames (no input)
    @console               dumps the screen now
    =TEXT                  checks that the screen shows TEXT (v32sim `expect console`: ^ and $ anchor to a row)

options:
    --kbd-port N           gamepad port of the v32kbd (default 2: the third pad, as v32nix and V32DOS use)
    --prompt-symbol SYM    "done" is the next time the program reaches SYM (a label or C function the program runs each
                           time it shows its prompt; needs the symbol file): no guess about screen text or time
    --prompt-text TEXT     "done" is the next time the screen shows TEXT (`until`, so `^` and `$` anchor a row), e.g.
                           --prompt-text '^MEM:\>$'. With neither option every command is followed by a fixed wait
                           of --wait-frames (default 120)
    --wait-frames N        frames to run after a command when there is no prompt to wait for (default 120)
    --boot-frames N        frames to run before the first command (default 30; with a prompt symbol: until it is reached)

Done is the next time the prompt appears. With {TEXT} the command is done when the screen shows TEXT instead: for a command
that asks something (FORMAT's Y/N) or does not come back to the prompt, and the next line is typed SETTLE_TEXT frames later.
N: gives {TEXT} at most N frames (default 600; the wait ends the run with exit status 1 and v32sim names the text).
TEXT is taken as it is (no escapes). Run the file with `v32sim --batch -q --max-frames=N -x`: a failed `=` check or an
{TEXT} that does not come exits with status 1 and a diagnostic on stderr; the screen dumps (the golden) are the only stdout.
"""
import re
import sys

SETTLE_TYPE = 10       # frames between the prompt and the first key (the shell has to be reading keys)
SETTLE_DRAW = 6        # frames between the prompt and the screen dump (the terminal paints what the command printed)
SETTLE_TEXT = 60        # frames between the TEXT and the screen dump (the terminal may still be scrolling)
DEFAULT_TIMEOUT = 600

args = sys.argv[1:]
KBD, PSYM, PTEXT, WAITF, BOOTF, out, sess = 2, None, None, 120, 30, None, None
while args:
    a = args.pop(0)
    if a == "--file":
        out = args.pop(0)
    elif a == "--kbd-port":
        KBD = int(args.pop(0))
    elif a == "--prompt-symbol":
        PSYM = args.pop(0)
    elif a == "--prompt-text":
        PTEXT = args.pop(0)
    elif a == "--wait-frames":
        WAITF = int(args.pop(0))
    elif a == "--boot-frames":
        BOOTF = int(args.pop(0))
    elif a in ("-h", "--help") or a.startswith("-"):
        sys.exit(__doc__)
    else:
        sess = a
if out is None or sess is None:
    sys.exit(__doc__)
cmds = []
for line in open(sess):
    line = line.rstrip("\n")
    if line.startswith("#"):
        continue
    cmds.append(line)
while cmds and cmds[-1] == "":
    cmds.pop()

def brk():                    # (re)arm the prompt breakpoint
    return ["break %s" % PSYM] if PSYM else []


def unbrk():                  # a breakpoint stops `run` too: disarm it while waiting for something else
    return ["unbreak 0"] if PSYM else []


def prompt_wait():            # run until the program is ready for the next line
    if PSYM:
        return ["continue"]
    if PTEXT:
        return ['until "%s" timeout %d' % (PTEXT, DEFAULT_TIMEOUT)]
    return ["run %d" % WAITF]


if PSYM:
    o = brk() + ["kbd attach %d" % KBD, "continue", "run %d" % (SETTLE_TYPE + 20)]
elif PTEXT:
    o = ["kbd attach %d" % KBD, 'until "%s" timeout 3600' % PTEXT, "run %d" % SETTLE_TYPE]
else:
    o = ["kbd attach %d" % KBD, "run %d" % BOOTF]
for c in cmds:
    if c.startswith("="):
        o.append('expect console "%s"' % c[1:])
        continue
    m = re.match(r"@run (\d+)$", c)
    if m:
        o.append("run %d" % int(m.group(1)))
        continue
    if c == "@console":
        o.append("console")
        continue
    m = re.match(r"@hold (\S+) (\d+)$", c)
    if m:
        o += unbrk() + ["kbd key %s down" % m.group(1), "run %d" % int(m.group(2)), "kbd key %s up" % m.group(1),
                        "run %d" % SETTLE_DRAW, "console"] + brk() + ["run %d" % SETTLE_TYPE]
        continue
    m = re.match(r"@key (\S+)$", c)
    if m:                               # a key chord (ctrl+c), then run until the prompt is back
        o += ["kbd key %s" % m.group(1)] + prompt_wait() + ["run %d" % SETTLE_DRAW, "console", "run %d" % SETTLE_TYPE]
        continue
    timeout, wait = DEFAULT_TIMEOUT, None
    m = re.match(r"(\d+):", c)
    if m:
        timeout, c = int(m.group(1)), c[m.end():]
    m = re.match(r"\{([^}]*)\}", c)
    if m:
        wait, c = m.group(1), c[m.end():]
    if wait is not None:
        o += unbrk()
    nl = "\\n"
    if c.startswith("~"):
        c, nl = c[1:], ""
    o.append('kbd type "%s%s"' % (c.replace("\\", "\\\\"), nl))
    if wait is None:
        o += prompt_wait() + ["run %d" % SETTLE_DRAW]
    else:
        o += ['until "%s" timeout %d' % (wait, timeout), "run %d" % SETTLE_TEXT] + brk()
    o.append("console")
    o.append("run %d" % SETTLE_TYPE)
o.append("quit")
open(out, "w").write("\n".join(o) + "\n")
