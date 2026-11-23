#!/usr/bin/env python3
"""The built-in v32dev server of v32sim: V32DEV_SERVER and V32DEV_CH0..3 (the environment of the
DesktopEmulator's built-in server, emulator/v32dev-server of the v32io project), serial:tcp=PORT, the
mailbox that never reaches the card file, and --v32dev-host next to them. Needs no toolchain and no v32io
project: the "program" is a probe cartridge, and the test plays its part by writing the card's words
from the debugger (a channel opened, bytes put in the program-to-host ring, the host's ring read).

usage: v32dev_server_test.py <v32sim> <bios.v32> <cart.v32 (any program that does not end)> <workdir>
"""
import os
import pty
import re
import select
import socket
import struct
import subprocess
import sys
import time

SIM, BIOS, CART, WORK = sys.argv[1:5]
os.makedirs(WORK, exist_ok=True)

CARD_WORDS = 262144
REGION = CARD_WORDS - 16384
HOST = REGION + 64
CART_AREA = HOST + 4 * 2040
BASE = 0x30000000
HEADER = [0x31564544, 1, 16384, 4, 2040, 16]
RING = 2024
checks = failures = 0


def check(cond, what):
    global checks, failures
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL: %s" % what)


def path(name):
    return os.path.join(WORK, name)


def blank_card(name, words=None):
    p = path(name)
    data = bytearray(b"V32-MEMC" + bytes(CARD_WORDS * 4))
    for w, v in (words or {}).items():
        struct.pack_into("<I", data, 8 + 4 * w, v)
    open(p, "wb").write(data)
    return p


def card_words(p):
    d = open(p, "rb").read()
    return struct.unpack("<%dI" % CARD_WORDS, d[8:])


def region_clean(p):
    w = card_words(p)
    return not any(w[REGION:])


def hp(ch):
    return HOST + ch * 2040


def cp(ch):
    return CART_AREA + ch * 2040


class Sim:
    """v32sim --batch with its commands on a pipe: every command is answered before the next one is sent."""

    def __init__(self, card, env=None, args=(), cart=CART):
        e = dict(os.environ)
        for k in [k for k in e if k.startswith("V32DEV_")]:
            del e[k]
        e.update(env or {})
        # the simulator's output goes to a pty: on a pipe its stdio buffers would hold the answers back
        self.master, slave = pty.openpty()
        self.p = subprocess.Popen([SIM, "--batch", "--fast", "-B", BIOS] + (["-M", card] if card else []) + list(args) + [cart],
                                  stdin=subprocess.PIPE, stdout=slave, stderr=slave, env=e)
        os.close(slave)
        self.lines = []
        self.pending = b""

    def readline(self, timeout):
        """One line from the simulator ('' at the end, None on a timeout)."""
        end = time.time() + timeout
        while b"\n" not in self.pending and time.time() < end:
            if select.select([self.master], [], [], 0.1)[0]:
                try:
                    chunk = os.read(self.master, 4096)
                except OSError:
                    chunk = b""
                if not chunk:
                    break
                self.pending += chunk
        if b"\n" not in self.pending:
            line, self.pending = self.pending, b""
            return line.decode(errors="replace").replace("\r", "") if line else None
        line, self.pending = self.pending.split(b"\n", 1)
        return line.decode(errors="replace").replace("\r", "") + "\n"

    def cmd(self, text):
        """Sends commands and returns what they printed (the 'version' command marks the end)."""
        self.p.stdin.write((text + "\nversion\n").encode())
        self.p.stdin.flush()
        out = []
        end = time.time() + 30
        while time.time() < end:
            line = self.readline(1)
            if line is None:
                continue
            if not line:
                break
            if line.startswith("v32sim 2"):
                break
            out.append(line.rstrip("\n"))
        self.lines += out
        return "\n".join(out)

    def word(self, w):
        m = re.search(r"\(0x([0-9A-F]+)\)", self.cmd("print [0x%08X]" % (BASE + w)))
        return int(m.group(1), 16) if m else None

    def put(self, w, v):
        self.cmd("set 0x%08X=0x%X" % (BASE + w, v & 0xFFFFFFFF))

    def finish(self):
        try:
            self.p.stdin.write(b"quit\n")
            self.p.stdin.flush()
        except OSError:
            pass
        try:
            self.p.wait(timeout=30)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        rest = self.pending.decode(errors="replace")
        while True:
            line = self.readline(0.2)
            if not line:
                break
            rest += line
        os.close(self.master)
        return rest + "\n".join(self.lines)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def recv_all(sock, count, timeout=5):
    sock.settimeout(timeout)
    data = b""
    try:
        while len(data) < count:
            chunk = sock.recv(count - len(data))
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    return data


def send_words(sim, ch, text, head):
    """The program writes text to the program-to-host ring of channel ch (head: the ring's head index)."""
    for i, c in enumerate(text):
        sim.put(cp(ch) + 16 + (head + i) % RING, c)
    head = (head + len(text)) % RING
    sim.put(cp(ch) + 1, head)
    return head


def ring_text(sim, ch, first, count):
    return bytes(sim.word(hp(ch) + 16 + (first + i) % RING) & 0xFF for i in range(count))


# ----------------------------------------------------------------------------
print("env: what turns the server on")
img = path("disk,x.img")                  # a comma in the file name

card = blank_card("a.memc")
s = Sim(card, {"V32DEV_CH1": "serial:echo"})
s.cmd("run 2")
out = s.cmd("mailbox")
check("none (no v32dev header" in out, "no V32DEV_SERVER: an ordinary card (V32DEV_CHn is not looked at)")
s.finish()
check(region_clean(card), "...and the card file is untouched")

s = Sim(card, {"V32DEV_SERVER": "0", "V32DEV_CH1": "serial:echo"})
s.cmd("run 2")
check("none (no v32dev header" in s.cmd("mailbox"), "V32DEV_SERVER=0: an ordinary card")
s.finish()

for value in ("1", "yes", ""):
    s = Sim(card, {"V32DEV_SERVER": value, "V32DEV_CH1": "serial:echo"})
    s.cmd("run 2")
    check("LIVE, v32dev server" in s.cmd("mailbox"), "V32DEV_SERVER=%r: the server is on" % value)
    s.finish()

s = Sim(card, {"V32DEV_SERVER": "1"})
s.cmd("run 2")
info = s.cmd("dev info")
check("channel 1: none (none kind 0), host absent" in info and "host attached" not in info,
      "enabled with nothing on the channels: the program sees a mailbox and no host")
check(s.word(REGION + 8) == 1 and s.word(REGION + 1) == HEADER[1] and s.word(REGION + 2) == HEADER[2], "header and live flag are in the card's memory")
s.finish()
check(region_clean(card), "the mailbox header never reaches the card file")

print("env: channels")
card = blank_card("b.memc")
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH0": "storage:%s,bytes,create=4" % img, "V32DEV_CH1": "serial:echo,hup=3",
               "V32DEV_CH2": "serial:bogus", "V32DEV_CH3": ""})
s.cmd("run 2")
info = s.cmd("dev info")
check("channel 0: storage (block kind 1), host attached" in info, "V32DEV_CH0=storage:...: served; a comma in the file name is fine")
check(os.path.exists(img) and os.path.getsize(img) == 2048, "create=4 made a 4 sector byte image (%s)" % (os.path.getsize(img) if os.path.exists(img) else None))
check("channel 1: serial (char kind 1), host attached" in info, "V32DEV_CH1=serial:echo: served")
check("channel 2: none (none kind 0), host absent" in info, "a wrong V32DEV_CH2: the channel is not served")
check("channel 3: none (none kind 0), host absent" in info, "an empty V32DEV_CH3 is as if unset")
check("sector 512 words, 4 sectors" in s.cmd("mailbox 0"), "byte mode sectors of 512 words")
log = s.finish()
check("V32DEV_CH2" in log and "unknown serial option: bogus" in log, "the wrong spec is reported")

print("env: --v32dev-host wins for its channel")
card = blank_card("c.memc")
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH1": "serial:echo", "V32DEV_CH2": "serial:echo"}, ["--v32dev-host=1=storage:%s,bytes" % img])
s.cmd("run 2")
info = s.cmd("dev info")
check("channel 1: storage (block kind 1)" in info and "channel 2: serial (char kind 1)" in info, "option on channel 1, environment on channel 2")
s.finish()
check(region_clean(card), "...and with the server on, --v32dev-host leaves the card file alone as well")

print("--v32dev-host without the server: in the card, as before")
card = blank_card("d.memc")
s = Sim(card, {}, ["--v32dev-host=1=serial:echo"])
s.cmd("run 2")
check("channel 1: serial (char kind 1), host attached" in s.cmd("dev info"), "served")
s.finish()
check(card_words(card)[REGION:REGION + 6] == tuple(HEADER), "the card file was formatted (header written)")
card = blank_card("d2.memc")
s = Sim(card, {"V32DEV_SERVER": "0"}, ["--v32dev-host=1=serial:echo"])
s.cmd("run 2")
s.finish()
check(card_words(card)[REGION:REGION + 6] == tuple(HEADER), "V32DEV_SERVER=0: --v32dev-host is as before")

print("the card file: nothing of the mailbox, the program's own data as always")
card = blank_card("e.memc")
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH2": "serial:echo"})
s.cmd("run 2")
s.put(100, 0xABCD1234)
s.put(cp(2), 1)                                # the program opens channel 2
s.put(cp(2) + 4, 77)
send_words(s, 2, b"hello", 0)
s.cmd("run 3")
check(ring_text(s, 2, 0, 5) == b"hello", "the echo device answered through the card's memory")
check(s.word(hp(2) + 3) == 5, "...host to program head moved to 5")
s.cmd("run 3")
s.finish()
w = card_words(card)
check(w[100] == 0xABCD1234, "the program's own word reached the card file")
check(not any(w[REGION:]), "no mailbox word reached the card file (header, rings, control words)")

print("a card that was formatted by v32dev.py: the server takes the host's place")
words = {REGION + i: v for i, v in enumerate(HEADER)}
words[hp(2) + 1] = 1                           # a stale 'attached' left by a host that is gone
words[hp(1) + 1] = 1
card = blank_card("f.memc", words)
before = open(card, "rb").read()
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH2": "serial:echo"})
s.cmd("run 2")
check(s.word(hp(2) + 1) == 1 and s.word(hp(1) + 1) == 0, "a served channel is attached; a stale one that nobody serves is withdrawn")
s.put(cp(2), 1)
send_words(s, 2, b"abc", 0)
s.cmd("run 3")
check(ring_text(s, 2, 0, 3) == b"abc", "served")
s.finish()
check(open(card, "rb").read() == before, "the card file is byte for byte as it was")

print("a card whose last 64 KiB hold data: left alone")
card = blank_card("g.memc", {REGION + 10: 0x1234})
before = open(card, "rb").read()
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH1": "serial:echo"})
s.cmd("run 2")
check("none (V32DEV_SERVER: the last 64 KiB of the card hold data)" in s.cmd("mailbox"), "not served")
check(s.word(REGION + 10) == 0x1234 and s.word(REGION) == 0, "the data is there and there is no header")
log = s.finish()
check("hold data: the v32dev server does not serve this card" in log, "a warning says why")
check(open(card, "rb").read() == before, "the card file is unchanged")

print("V32DEV_DISABLE")
card = blank_card("h.memc")
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH1": "serial:echo", "V32DEV_DISABLE": "1"})
s.cmd("run 2")
check("disabled (V32DEV_DISABLE)" in s.cmd("mailbox"), "the mailbox is off")
log = s.finish()
check("V32DEV_DISABLE is set: the v32dev server is not started" in log, "with a warning")
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH1": "serial:echo", "V32DEV_DISABLE": "0"})
s.cmd("run 2")
check("LIVE" in s.cmd("mailbox"), "V32DEV_DISABLE=0 does not disable")
s.finish()

print("no card: nothing to serve, no error")
s = Sim(None, {"V32DEV_SERVER": "1", "V32DEV_CH1": "serial:echo"})
s.cmd("run 2")
rc = s.p.poll()
s.finish()
check(s.p.returncode == 0, "the run goes on and ends normally (%s)" % s.p.returncode)

print("dev format and dev attach with the server")
card = blank_card("i.memc")
s = Sim(card, {"V32DEV_SERVER": "1"})
s.cmd("run 1")
out = s.cmd("dev format")
check("nothing to format" in out, "dev format: the server keeps the mailbox out of the file")
out = s.cmd("dev attach 3 serial:echo")
check("channel 3: serial:echo" in out, "dev attach works while running")
s.cmd("run 2")
check("channel 3: serial (char kind 1), host attached" in s.cmd("dev info"), "...and is served")
s.finish()
check(region_clean(card), "the card file is untouched")

# ----------------------------------------------------------------------------
print("serial:tcp=PORT")
port = free_port()
card = blank_card("t.memc")
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH2": "serial:tcp=%d" % port})
s.cmd("run 2")
try:
    lines = open("/proc/net/tcp").read().splitlines()[1:]
    found = [l.split()[1] for l in lines if l.split()[3] == "0A" and int(l.split()[1].split(":")[1], 16) == port]
    check(bool(found) and all(a.startswith("0100007F") for a in found), "the port listens on 127.0.0.1 only")
except OSError:
    pass
check("channel 2: serial (char kind 1), host attached" in s.cmd("dev info"), "channel 2 is served")
check(s.word(hp(2) + 2) != 0 and s.word(hp(2) + 8) == 0, "type word and descriptor of a plain serial device")
s.put(cp(2), 1)                                  # the program opens the channel
s.put(cp(2) + 4, 5)
head = send_words(s, 2, b"lost\n", 0)            # nobody is connected: dropped
s.cmd("run 3")
check(s.word(hp(2) + 6) == 1, "no client yet: status READY, not hung up")

a = socket.create_connection(("127.0.0.1", port), timeout=5)
s.cmd("run 2")
a.sendall(b"ping\n")
time.sleep(0.2)
s.cmd("run 3")
check(s.word(hp(2) + 3) == 5 and ring_text(s, 2, 0, 5) == b"ping\n", "bytes from the client arrive as words in the host-to-program ring")
check(recv_all(a, 1, 0.3) == b"", "what the program wrote before the client came was dropped")
head = send_words(s, 2, b"pong\n", head)
s.cmd("run 3")
check(recv_all(a, 5) == b"pong\n", "what the program writes reaches the client (low 8 bits of each word)")
s.put(cp(2) + 2, 5)                              # the program has read the 5 words
s.cmd("run 1")

b = socket.create_connection(("127.0.0.1", port), timeout=5)
time.sleep(0.2)
s.cmd("run 3")
a.settimeout(5)
try:
    gone = a.recv(10)
except OSError:
    gone = b""
check(gone == b"", "a new client replaces the old one (the old one is closed)")
check(s.word(hp(2) + 6) == 1, "...and the program saw no hang up in between")
head = send_words(s, 2, b"two\n", head)
s.cmd("run 3")
check(recv_all(b, 4) == b"two\n", "the new client is served")
b.close()
time.sleep(0.2)
s.cmd("run 3")
check(s.word(hp(2) + 6) == 5, "the client went away: the program sees a hang up (status READY|HUP)")
c = socket.create_connection(("127.0.0.1", port), timeout=5)
time.sleep(0.2)
s.cmd("run 3")
check(s.word(hp(2) + 6) == 1, "a new client: the hang up is gone")
c.sendall(b"x")
time.sleep(0.2)
s.cmd("run 3")
check(s.word(hp(2) + 3) == 6, "the new client's bytes arrive")
c.close()
time.sleep(0.2)
s.cmd("run 3")
check(s.word(hp(2) + 6) == 5, "hung up again")
out = s.cmd("mailbox 2")
check("serial 'serial:tcp=%d'" % port in out, "mailbox names the device")
s.finish()
check(region_clean(card), "the card file has no trace of the mailbox")

print("serial:tcp=PORT: the port is taken, a bad port")
busy = socket.socket()
busy.bind(("127.0.0.1", 0))
busy.listen(1)
bport = busy.getsockname()[1]
card = blank_card("u.memc")
s = Sim(card, {"V32DEV_SERVER": "1", "V32DEV_CH2": "serial:tcp=%d" % bport, "V32DEV_CH1": "serial:echo"})
s.cmd("run 2")
info = s.cmd("dev info")
log = s.finish()
check("cannot listen on 127.0.0.1:%d (is the port in use?)" % bport in log, "reported")
check("channel 2: none (none kind 0), host absent" in info and "channel 1: serial (char kind 1), host attached" in info,
      "the other channel is served, this one is not")
busy.close()
r = subprocess.run([SIM, "--batch", "-B", BIOS, "--v32dev-host=2=serial:tcp=70000", CART], input=b"quit\n", capture_output=True)
check(r.returncode == 1 and b"bad TCP port" in r.stderr + r.stdout, "--v32dev-host=2=serial:tcp=70000: exit 1, bad TCP port")
r = subprocess.run([SIM, "--batch", "-B", BIOS, "--v32dev-host=2=serial:tcp=4000,echo", CART], input=b"quit\n", capture_output=True)
check(r.returncode == 1 and b"takes no other option" in r.stderr + r.stdout, "tcp with echo: exit 1")

print("--v32dev-host=N=serial:tcp=PORT (in the card, as the other options)")
port = free_port()
card = blank_card("v.memc")
s = Sim(card, {}, ["--v32dev-host=3=serial:tcp=%d" % port])
s.cmd("run 2")
s.put(cp(3), 1)
s.put(cp(3) + 4, 3)
send_words(s, 3, b"hi", 0)
cl = socket.create_connection(("127.0.0.1", port), timeout=5)
s.cmd("run 3")
s.cmd("run 3")
check(recv_all(cl, 2) == b"hi", "served by the option as well")
cl.close()
s.finish()
check(card_words(card)[REGION:REGION + 6] == tuple(HEADER), "(this one formatted the card file)")

# ----------------------------------------------------------------------------
print("SPEC: storage options are read from the right")
card = blank_card("w.memc")
s = Sim(card, {"V32DEV_SERVER": "1"})
s.cmd("run 1")
for spec, want in (("storage:%s" % img, "storage '%s'" % img),
                   ("storage:%s,ro" % img, "storage '%s' ro" % img),
                   ("storage:%s,sector=64,bytes" % img, "sector 64 words"),
                   ("storage:%s,nonsense" % img.replace(".img", ".x"), None)):
    out = s.cmd("dev attach 0 %s\nrun 1\nmailbox 0" % spec)
    if want:
        check(want in out, "%s -> %s" % (spec, want))
    else:
        check("cannot open" in out, "an unknown trailing text is part of the file name (%s)" % spec)
    s.cmd("dev detach 0")
out = s.cmd("dev attach 0 storage:%s,sector=x" % img)
check("bad sector size" in out, "bad sector=")
out = s.cmd("dev attach 0 storage:%s,sector=3000" % img)
check("bad sector size" in out, "sector=3000 is too big")
out = s.cmd("dev attach 0 storage:%s,create=-1" % img)
check("bad sector count" in out, "bad create=")
out = s.cmd("dev attach 0 serial:tcp=0")
check("bad TCP port" in out, "port 0")
out = s.cmd("dev attach 0 serial:")
check("serial needs echo[,hup=N], stdio or tcp=PORT" in out, "bare serial:")
s.finish()

print("dev format with types, dev info names (v32dev.py format --chN / describe_type)")
card = blank_card("x.memc")
s = Sim(card, {})
s.cmd("run 1")
out = s.cmd("dev format 0=block 1=char 2=pipe 3=netplay")
check("the memory card is a v32dev mailbox" in out and "channel 3: netplay (socket kind 3)" in out, "format with types")
info = s.cmd("dev info")
for want in ("channel 0: block (block kind 0)", "channel 1: char (char kind 0)", "channel 2: pipe (pipe kind 1)", "channel 3: netplay (socket kind 3)"):
    check(want in info, want)
s.finish()
w = card_words(card)
check([w[hp(c)] for c in range(4)] == [0x01000000, 0x02000000, 0x03000001, 0x04000003], "the type words are in the card file")
check(w[REGION:REGION + 6] == tuple(HEADER), "and the header")
card = blank_card("y.memc")
s = Sim(card, {})
s.cmd("run 1")
check("channel 0: udp (socket kind 2)" in s.cmd("dev format ch0=udp 1=0x05000007 2=socket 3=none"), "the ch0= form")
info = s.cmd("dev info")
check("channel 0: udp (socket kind 2)" in info and "channel 1: class 5 kind 7" in info and "channel 2: socket (socket kind 0)" in info
      and "channel 3: none (none kind 0)" in info, "numbers and names")
out = s.cmd("dev format 0=bogus")
check("unknown device type 'bogus'" in out, "unknown type")
out = s.cmd("dev format 7=storage")
check("[ERROR] dev: format takes" in out, "bad channel")
s.finish()
card = blank_card("z.memc")
s = Sim(card, {})
s.cmd("run 1")
check("the memory card is a v32dev mailbox" in s.cmd("dev format"), "plain dev format as before")
check("channel 0: none (none kind 0)" in s.cmd("dev info"), "...with no types")
s.finish()

print("v32dev server tests: %d checks, %d failures" % (checks, failures))
sys.exit(1 if failures else 0)
