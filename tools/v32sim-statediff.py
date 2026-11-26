#!/usr/bin/env python3
"""Compare two v32sim machine-state files (the `save FILE` command) and report the differences section by section.

  v32sim-statediff.py [--ignore SECTION,...] [--ignore-port NAME,...] [--ram-range LO-HI] [--max-lines N] [--quiet] A.state B.state

Both files are parsed exactly as src/savestate.c writes them (magic V32SIMSV, version 1, little-endian words) and must be
consumed to the last byte, so a truncated or padded file is an error. One line per difference, prefixed by its section:
REG, PORT (by name), RAM and CARD (adjacent differing words collapsed into runs), GPU (region tables), FRAMEBUFFER (one
summary line), SPU, GAMEPAD, HEADER (BIOS/cartridge checksums, which differ between runs with different BIOSes) and DEVHOST
(the optional v32dev host block after the SPU part, compared as raw words). Sections for --ignore: header (alias
checksums), reg, port, ram, card, gpu-regions, framebuffer, spu, gamepad, devhost. --ram-range compares only RAM
addresses LO..HI (hex, inclusive). --max-lines caps the lines printed per section (default 200; the rest is counted).
Exit status: 0 no differences (after ignoring), 1 differences, 2 parse error or bad usage. --quiet prints nothing.
"""
import argparse, struct, sys
from array import array

SECTIONS = ('header', 'reg', 'port', 'ram', 'card', 'gpu-regions', 'framebuffer', 'spu', 'gamepad', 'devhost')
NUM_REGS, RAM_WORDS, CARD_WORDS, REGIONS, PADS, SPU_CHANNELS, FB_W, FB_H = 19, 4 * 1024 * 1024, 262144, 4096, 4, 16, 640, 360
REG_NAMES = ['R%d' % i for i in range(16)]
REG_NAMES[11], REG_NAMES[12], REG_NAMES[13], REG_NAMES[14], REG_NAMES[15] = 'R11/CR', 'R12/SR', 'R13/DR', 'R14/BP', 'R15/SP'
REG_NAMES += ['IP', 'IR', 'IV']
PORT_QTY = (4, 1, 18, 14, 13, 4, 1)                          # TIM RNG GPU SPU INP CAR MEM
PORT_NAMES = """TIM_CurrentDate TIM_CurrentTime TIM_FrameCounter TIM_CycleCounter RNG_CurrentValue GPU_Command
    GPU_RemainingPixels GPU_ClearColor GPU_MultiplyColor GPU_ActiveBlending GPU_SelectedTexture GPU_SelectedRegion
    GPU_DrawingPointX GPU_DrawingPointY GPU_DrawingScaleX GPU_DrawingScaleY GPU_DrawingAngle GPU_RegionMinX GPU_RegionMinY
    GPU_RegionMaxX GPU_RegionMaxY GPU_RegionHotspotX GPU_RegionHotspotY SPU_Command SPU_GlobalVolume SPU_SelectedSound
    SPU_SelectedChannel SPU_SoundLength SPU_SoundPlayWithLoop SPU_SoundLoopStart SPU_SoundLoopEnd SPU_ChannelState
    SPU_ChannelAssignedSound SPU_ChannelVolume SPU_ChannelSpeed SPU_ChannelLoopEnabled SPU_ChannelPosition
    INP_SelectedGamepad INP_GamepadConnected INP_GamepadLeft INP_GamepadRight INP_GamepadUp INP_GamepadDown
    INP_GamepadButtonStart INP_GamepadButtonA INP_GamepadButtonB INP_GamepadButtonX INP_GamepadButtonY INP_GamepadButtonL
    INP_GamepadButtonR CAR_Connected CAR_ProgramROMSize CAR_NumberOfTextures CAR_NumberOfSounds MEM_Connected""".split()
BUTTONS = 'left right up down start A B X Y L R'.split()
CHAN_STATE = {0x40: 'stopped', 0x41: 'paused', 0x42: 'playing'}
DH_TAG = 0x53544844


class Bad(Exception):
    pass


class Reader:
    """Cursor over the little-endian words that follow the 8-byte magic."""
    def __init__(self, path):
        try:
            data = open(path, 'rb').read()
        except OSError as e:
            raise Bad('%s: %s' % (path, e.strerror))
        if data[:8] != b'V32SIMSV':
            raise Bad('%s: not a v32sim state file' % path)
        if len(data) % 4:
            raise Bad('%s: size is not a multiple of 4 (truncated or damaged)' % path)
        self.w, self.i, self.path = array('I'), 0, path
        self.w.frombytes(data[8:])
        if sys.byteorder == 'big':
            self.w.byteswap()

    def take(self, n):
        if n < 0 or self.i + n > len(self.w):
            raise Bad('%s: truncated or damaged (wanted %d words at word %d)' % (self.path, n, self.i))
        a = self.w[self.i:self.i + n]
        self.i += n
        return a

    def u32(self):
        return self.take(1)[0]

    def rng(self, words):
        """A zero-run compressed range: (zeros, literals, literal words...) groups until `words` are done."""
        out, done = array('I'), 0
        while done < words:
            z, l = self.u32(), self.u32()
            if z > words - done or l > words - done - z or z + l == 0:
                raise Bad('%s: bad range group at word %d' % (self.path, self.i))
            out.extend(array('I', [0]) * z)
            out.extend(self.take(l))
            done += z + l
        return out


def parse(path):
    r = Reader(path)
    s = {'version': r.u32()}
    if s['version'] != 1:
        raise Bad('%s: unsupported state file version %d' % (path, s['version']))
    s['bsum'], s['csum'] = r.u32(), r.u32()
    s['reg'] = list(r.take(NUM_REGS + 1))                    # R0..R15, IP, IR, IV, then rom_offset
    s['ports'] = [list(r.take(n)) for n in PORT_QTY]
    s['ram'] = r.rng(RAM_WORDS)
    s['card'] = r.rng(CARD_WORDS) if r.u32() else None

    def regions():
        if not r.u32():
            return None
        return [struct.unpack('<6h', r.take(3).tobytes()) for _ in range(REGIONS)]   # (minX,minY) (maxX,maxY) (hotX,hotY)
    s['tex'] = {-1: regions()}
    for t in range(r.u32()):
        s['tex'][t] = regions()
    s['pads'] = [list(r.take(12)) for _ in range(PADS)]      # connected, then 11 buttons
    s['fb'] = r.take(FB_W * FB_H)
    loaded, selsound, selchan = r.u32(), r.u32(), r.u32()
    s['spu'] = {'loaded': loaded, 'selsound': selsound, 'selchannel': selchan}
    s['chan'] = []
    for _ in range(SPU_CHANNELS):
        st, snd, vol, spd, loop, lo, hi = r.take(7)
        s['chan'].append((st, snd, struct.unpack('<f', struct.pack('<I', vol))[0], struct.unpack('<f', struct.pack('<I', spd))[0],
                          loop, struct.unpack('<d', struct.pack('<II', lo, hi))[0]))
    s['sounds'] = [tuple(r.take(3)) for _ in range(loaded + 1)]   # biossound first, then the loaded sounds
    rest = r.w[r.i:]
    if rest and rest[0] != DH_TAG:
        raise Bad('%s: %d unexpected trailing bytes after the SPU section' % (path, 4 * len(rest)))
    s['devhost'] = rest
    return s


def h(v):
    return '0x%08X' % v


def i32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def runs(a, b, lo, hi, base):
    """Yield (first, last) for each run of adjacent differing words of a and b within [lo, hi)."""
    n, i = hi, lo
    while i < n:
        if a[i:i + 4096] == b[i:i + 4096] and i + 4096 <= n:
            i += 4096
            continue
        if a[i] == b[i]:
            i += 1
            continue
        j = i
        while j + 1 < n and a[j + 1] != b[j + 1]:
            j += 1
        yield base + i, base + j
        i = j + 1


def diff(a, b, ign, ports_ign, ram_range):
    """Return {section: [lines]} of the differences between parsed states a and b."""
    out = {}

    def add(sec, key, line):
        if key not in ign:
            out.setdefault(sec, []).append(line)

    if a['bsum'] != b['bsum'] or a['csum'] != b['csum']:
        add('HEADER', 'header', 'HEADER bios checksum %s != %s, cartridge checksum %s != %s' % (h(a['bsum']), h(b['bsum']), h(a['csum']), h(b['csum'])))
    for i, (x, y) in enumerate(zip(a['reg'], b['reg'])):
        if x != y:
            add('REG', 'reg', 'REG %s: %s != %s' % (REG_NAMES[i] if i < NUM_REGS else 'rom_offset', h(x), h(y)))
    n = 0
    for cat, (pa, pb) in enumerate(zip(a['ports'], b['ports'])):
        for idx, (x, y) in enumerate(zip(pa, pb)):
            name = PORT_NAMES[n] if n < len(PORT_NAMES) else 'port%d.%d' % (cat, idx)
            n += 1
            if x != y and name not in ports_ign:
                add('PORT', 'port', 'PORT %s: %s != %s (%d != %d)' % (name, h(x), h(y), i32(x), i32(y)))
    lo, hi = ram_range
    for f, g in runs(a['ram'], b['ram'], lo, min(hi + 1, RAM_WORDS), 0):
        add('RAM', 'ram', 'RAM ' + span(a['ram'], b['ram'], f, f, g))
    if (a['card'] is None) != (b['card'] is None):
        add('CARD', 'card', 'CARD present: %s != %s' % (a['card'] is not None, b['card'] is not None))
    elif a['card'] is not None:
        for f, g in runs(a['card'], b['card'], 0, CARD_WORDS, 0x30000000):
            add('CARD', 'card', 'CARD ' + span(a['card'], b['card'], f - 0x30000000, f - 0x30000000, g - 0x30000000, 0x30000000))
    for t in sorted(set(a['tex']) | set(b['tex'])):
        ra, rb = a['tex'].get(t), b['tex'].get(t)
        if t not in a['tex'] or t not in b['tex']:
            add('GPU', 'gpu-regions', 'GPU texture %d: only in %s' % (t, 'B' if t not in a['tex'] else 'A'))
        elif (ra is None) != (rb is None):
            add('GPU', 'gpu-regions', 'GPU texture %d regions present: %s != %s' % (t, ra is not None, rb is not None))
        elif ra is not None:
            for k, (x, y) in enumerate(zip(ra, rb)):
                if x != y:
                    add('GPU', 'gpu-regions', 'GPU texture %d region %d: %s != %s' % (t, k, fmt_region(x), fmt_region(y)))
    nfb = sum(1 for x, y in zip(a['fb'], b['fb']) if x != y)
    if nfb:
        add('FRAMEBUFFER', 'framebuffer', 'FRAMEBUFFER %d of %d pixels differ' % (nfb, FB_W * FB_H))
    for k in ('loaded', 'selsound', 'selchannel'):
        if a['spu'][k] != b['spu'][k]:
            add('SPU', 'spu', 'SPU %s: %d != %d' % (k, i32(a['spu'][k]), i32(b['spu'][k])))
    for c, (x, y) in enumerate(zip(a['chan'], b['chan'])):
        for k, nm in enumerate(('state', 'sound', 'volume', 'speed', 'loop', 'position')):
            if x[k] != y[k]:
                f = (lambda v: CHAN_STATE.get(v, h(v))) if nm == 'state' else (lambda v: str(i32(v)) if isinstance(v, int) else '%g' % v)
                add('SPU', 'spu', 'SPU channel %d %s: %s != %s' % (c, nm, f(x[k]), f(y[k])))
    for k, (x, y) in enumerate(zip(a['sounds'], b['sounds'])):
        if x != y:
            add('SPU', 'spu', 'SPU sound %s loop/loopstart/loopend: %s != %s' % ('bios' if k == 0 else k - 1, tuple(map(i32, x)), tuple(map(i32, y))))
    for p, (x, y) in enumerate(zip(a['pads'], b['pads'])):
        for k, (u, v) in enumerate(zip(x, y)):
            if u != v:
                add('GAMEPAD', 'gamepad', 'GAMEPAD %d %s: %d != %d' % (p, 'connected' if k == 0 else BUTTONS[k - 1], i32(u), i32(v)))
    if a['devhost'] != b['devhost']:
        add('DEVHOST', 'devhost', 'DEVHOST block differs (%d != %d words)' % (len(a['devhost']), len(b['devhost'])))
    return out


def fmt_region(r):
    return 'minX=%d,minY=%d,maxX=%d,maxY=%d,hotX=%d,hotY=%d' % (r[0], r[1], r[2], r[3], r[4], r[5])


def span(a, b, addr, i, j, base=0):
    """One line for the differing run a[i..j] vs b[i..j] (indices) at address base+i."""
    if i == j:
        return '0x%08X: %s != %s' % (base + i, h(a[i]), h(b[i]))
    n = j - i + 1
    def vals(w):
        return ','.join('%08X' % v for v in w[i:min(j + 1, i + 4)]) + (',...' if n > 4 else '')
    return '0x%08X..0x%08X (%d words): %s != %s' % (base + i, base + j, n, vals(a), vals(b))


def main():
    ap = argparse.ArgumentParser(description='Compare two v32sim state files.', epilog='sections: ' + ', '.join(SECTIONS) + ' (checksums = header)')
    ap.add_argument('a'), ap.add_argument('b')
    ap.add_argument('--ignore', default='', help='comma-separated sections to leave out')
    ap.add_argument('--ignore-port', default='', help='comma-separated port names to leave out (e.g. TIM_FrameCounter)')
    ap.add_argument('--ram-range', help='compare only RAM addresses LO-HI (hex, inclusive)')
    ap.add_argument('--max-lines', type=int, default=200, help='lines printed per section (default 200)')
    ap.add_argument('--quiet', action='store_true', help='no output, exit status only')
    try:
        args = ap.parse_args()
        ign = {'header' if s == 'checksums' else s for s in args.ignore.split(',') if s}
        bad = ign - set(SECTIONS)
        if bad:
            raise Bad('unknown section(s): ' + ', '.join(sorted(bad)))
        ports_ign = {p for p in args.ignore_port.split(',') if p}
        if ports_ign - set(PORT_NAMES):
            raise Bad('unknown port(s): ' + ', '.join(sorted(ports_ign - set(PORT_NAMES))))
        rr = (0, RAM_WORDS - 1)
        if args.ram_range:
            try:
                lo, hi = (int(x, 16) for x in args.ram_range.split('-'))
            except ValueError:
                raise Bad('--ram-range expects LO-HI in hex')
            if not 0 <= lo <= hi < RAM_WORDS:
                raise Bad('--ram-range must lie within 0-3FFFFF and LO <= HI')
            rr = (lo, hi)
        res = diff(parse(args.a), parse(args.b), ign, ports_ign, rr)
    except Bad as e:
        print('v32sim-statediff: ' + str(e), file=sys.stderr)
        return 2
    total = sum(len(v) for v in res.values())
    if not args.quiet:
        for sec, lines in res.items():
            for l in lines[:args.max_lines]:
                print(l)
            if len(lines) > args.max_lines:
                print('%s ... %d more difference(s) not shown' % (sec, len(lines) - args.max_lines))
        print('%d difference(s)' % total if total else 'no differences')
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main())
