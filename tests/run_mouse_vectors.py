#!/usr/bin/env python3
"""v32mouse against the v32io test vectors (tests/vectors/mouse_*.vec, copied from the v32io project).

For each vector the 'mouse P ...' actions become v32sim 'mouse' commands, the probe cartridge tests/examples/pad_bits.c
records the 11 controls of gamepad 1 from the first frame that shows anything, and that is compared with the vector's
[frames] section. usage: run_mouse_vectors.py SIM BIOS CART.v32 [VEC...]   (exit 0 = all agree)"""
import os, subprocess, sys, tempfile

def parse(path):
    actions, frames, sec = [], {}, None
    for line in open(path):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if line.startswith('['):
            sec = line; continue
        if sec == '[actions]':
            actions.append(line.split())
        elif sec == '[frames]':
            f, port, bits = line.split()
            frames[int(f)] = bits
    return actions, frames

def run(sim, bios, cart, vec):
    actions, frames = parse(vec)
    cmds = ['mouse attach 1', 'mouse wait 300']
    for a in actions:
        if a[0] == 'mouse':
            cmds.append('mouse ' + ' '.join(a[2:]))
    n = len(frames)
    cmds += ['run %d' % (300 + n + 30)]
    cmds += ['x/d st %d' % n, 'quit']
    p = subprocess.run([sim, '--batch', '--fast', '-x', '--max-frames=%d' % (400 + n), '-B', bios, cart],
                       input='\n'.join(cmds) + '\n', capture_output=True, text=True)
    vals = [int(l.split()[-1]) for l in p.stdout.splitlines() if '/d:' in l]
    got = ['{:011b}'.format(v) for v in vals]
    bad = [(i, frames[i], got[i] if i < len(got) else None) for i in sorted(frames) if i >= len(got) or got[i] != frames[i]]
    return bad

def main():
    sim, bios, cart = sys.argv[1:4]
    vecs = sys.argv[4:] or sorted(os.path.join(os.path.dirname(__file__), 'vectors', f)
                                  for f in os.listdir(os.path.join(os.path.dirname(__file__), 'vectors')) if f.startswith('mouse_'))
    rc = 0
    for v in vecs:
        bad = run(sim, bios, cart, v)
        print('%s %s' % ('ok  ' if not bad else 'FAIL', os.path.basename(v)))
        for i, want, got in bad[:5]:
            print('   frame %d: want %s got %s' % (i, want, got))
        rc |= bool(bad)
    return rc

sys.exit(main())
