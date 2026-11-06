#!/usr/bin/env python3
"""The native v32dev host of v32sim (--v32dev-host) against the real cartridges of the v32io project
(tests/v32dev/rom: storage_test, serial_test), and against the reference host, tools/v32dev.py.

usage: native_host_test.py <v32sim> <bios.v32> <directory with the compiled *_test.v32 carts> <workdir> <v32io project dir>
"""
import os
import struct
import subprocess
import sys
import time

SIM, BIOS, CARTS, WORK, IOD = sys.argv[1:6]
sys.path.insert(0, os.path.join(IOD, "tools"))
import v32dev  # noqa: E402

M32 = 0xFFFFFFFF
OK, EINVAL, ERANGE, EROFS, EIO, ETIMEOUT, ENODEV, EBUSY = range(8)
NOCARD, NOEMU, NOHOST, WRONG, READY = range(5)
checks = failures = 0
os.makedirs(WORK, exist_ok=True)


def check(cond, what):
    global checks, failures
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL: %s" % what)


def path(name):
    return os.path.join(WORK, name)


def pattern(i):
    return ((i << 17) ^ (i * 3) ^ 0x80000001) & M32


def make_image(p, sectors):
    with open(p, "wb") as f:
        f.write(struct.pack("<%dI" % (sectors * 128), *[pattern(i) for i in range(sectors * 128)]))


def plain_card(name, scenario):
    """An ordinary card (no mailbox), with the scenario word of the test cartridges."""
    p = path(name)
    with open(p, "wb") as f:
        f.write(v32dev.FILE_SIGNATURE + bytes(v32dev.CARD_WORDS * 4))
    c = v32dev.Card(p)
    c.write_word(64, scenario)
    c.close()
    return p


def report(card):
    c = v32dev.Card(card)
    r = c.read_words(65, 48)
    c.close()
    return r


def run(card, cart, hosts=(), frames=20000, extra=(), env=None):
    e = dict(os.environ)
    e.update(env or {})
    args = [SIM, "-r", "-x", "-n", "-B", BIOS, "-M", card, "--frame-us=2000", "--max-frames=%d" % frames]
    args += ["--v32dev-host=%s" % h for h in hosts] + list(extra)
    args.append(os.path.join(CARTS, cart + ".v32"))
    p = subprocess.run(args, capture_output=True, text=True, env=e, stdin=subprocess.DEVNULL, timeout=300)
    return p.returncode, p.stderr


def image_words(p):
    d = open(p, "rb").read()
    return list(struct.unpack("<%dI" % (len(d) // 4), d))


def expected_rw_image():
    words = [pattern(i) for i in range(64 * 128)]
    for i in range(700):
        words[2000 + i] = 0xC0DE0000 + i
    for i in range(5 * 128):
        words[40 * 128 + i] = (~i) & M32
    return words


def rw_checks(r, label):
    check(r[0] == 0x54504552 and r[2] == 1 and r[1] == 99, label + ": report complete")
    check(r[40] == READY and r[41] == 1, label + ": channel became ready (state %d)" % r[40])
    check(r[42] == v32dev.CLASS_BLOCK and r[43] == 1, label + ": class block, kind storage")
    check(r[3:8] == [128, 64, 0, 0, 15], label + ": geometry %s" % r[3:8])
    check(r[8] == OK and r[9] == 0 and r[10] == 0, label + ": reads match")
    check(r[12] == OK and r[13] == OK and r[14] == OK and r[15] == 0, label + ": write, flush, read back")
    check(r[16] == OK and r[17] == OK and r[18] == OK and r[19] == 0, label + ": 5 sector write")
    check(r[20] == ERANGE and r[21] == ERANGE and r[22] == ERANGE and r[23] == OK, label + ": range errors %s" % r[20:24])
    check(r[24] == OK and r[25] == EBUSY and r[26] == OK and r[28] == 0, label + ": async %s" % r[24:29])
    check(r[29] == 0, label + ": last words readable")


def test_rw():
    print("native host: read and write a word mode image, with and without the doorbell, and the Python host")
    results = {}
    for label, extra, hosts in (("native", (), ["1=storage:%s"]), ("native+bell", ("--v32dev-doorbell",), ["1=storage:%s"]),
                                ("native+bell0", ("--v32dev-doorbell=0",), ["1=storage:%s"])):
        img = path("rw-%s.img" % label)
        make_image(img, 64)
        card = plain_card("rw-%s.memc" % label, 1)
        code, err = run(card, "storage_test", [h % img for h in hosts], extra=extra)
        check(code == 0, label + ": program ended normally (%d) %s" % (code, err.strip()[-200:]))
        r = report(card)
        rw_checks(r, label)
        check(image_words(img) == expected_rw_image(), label + ": image holds exactly the program's writes")
        results[label] = r
    # the reference host, serving the same program through the card file
    img = path("rw-py.img")
    make_image(img, 64)
    card = path("rw-py.memc")
    v32dev.format_card(card, {1: v32dev.TYPES["storage"]})
    c = v32dev.Card(card)
    c.write_word(64, 1)
    c.close()
    host = subprocess.Popen([sys.executable, "-I", os.path.join(IOD, "tools", "v32dev.py"), "serve", card, "--ch1", "storage:" + img],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    c = v32dev.Card(card)
    for _ in range(500):
        if c.read_word(v32dev.host_part(1) + v32dev.HP_FLAGS) & 1:
            break
        time.sleep(0.02)
    c.close()
    code, err = run(card, "storage_test")
    host.terminate()
    host.wait()
    pr = report(card)
    check(code == 0, "python host: program ended normally")
    same = [i for i in range(48) if i not in (11, 27) and pr[i] != results["native"][i]]
    check(not same, "the python host and the native host give the program the same report (differs at %s)" % same)
    check(image_words(img) == image_words(path("rw-native.img")), "...and the same image")
    # the timing of the doorbell: with it, replies are in the same frame
    n, b = results["native"], results["native+bell"]
    check(b[11] <= n[11] and b[27] <= n[27], "doorbell frames %d/%d <= polled %d/%d" % (b[11], b[27], n[11], n[27]))
    print("  frames for a request: polled %d/%d, with the doorbell %d/%d" % (n[11], n[27], b[11], b[27]))


def test_repeat():
    print("native host: runs are repeatable (the card file is identical, byte for byte)")
    cards = []
    for k in range(2):
        img = path("rep%d.img" % k)
        make_image(img, 64)
        card = plain_card("rep%d.memc" % k, 1)
        code, err = run(card, "storage_test", ["1=storage:" + img], extra=("--v32dev-doorbell",))
        check(code == 0, "run %d ended normally" % k)
        cards.append(open(card, "rb").read())
    check(cards[0] == cards[1], "two runs leave the same card")


def test_bytes():
    print("native host: byte mode, read only")
    img = path("text.img")
    text = b"Hello from the host!\nline two\n"
    with open(img, "wb") as f:
        f.write(text + bytes((i * 7) & 255 for i in range(16 * 512 - len(text))))
    before = open(img, "rb").read()
    card = plain_card("bytes.memc", 2)
    code, err = run(card, "storage_test", ["1=storage:%s,bytes,ro" % img])
    r = report(card)
    check(code == 0, "program ended normally " + err.strip()[-200:])
    check(r[3:8] == [512, 16, 1, 1, 3], "geometry %s" % r[3:8])
    check(r[8] == OK and r[16:48] == list(before[:32]), "first 32 bytes arrive as words")
    check(r[9] == EROFS and r[10] == EROFS and r[11] == EROFS, "writes refused %s" % r[9:12])
    check(open(img, "rb").read() == before, "read only image untouched")


def test_states():
    print("native host: what the program sees in the other situations")
    img = path("st.img")
    make_image(img, 4)
    card = plain_card("wrong.memc", 6)
    code, err = run(card, "storage_test", ["1=serial:echo"], frames=3000)
    r = report(card)
    check(code == 0 and r[40] == WRONG, "serial device where a block device was expected: state %d" % r[40])
    card = plain_card("disabled.memc", 5)
    code, err = run(card, "storage_test", ["1=storage:" + img], frames=3000, env={"V32DEV_DISABLE": "1"})
    check(code == 1, "V32DEV_DISABLE with a native host is an error (exit %d)" % code)
    card = plain_card("nocard.memc", 5)
    p = subprocess.run([SIM, "-r", "-x", "-n", "-B", BIOS, "--max-frames=10", "--v32dev-host=1=storage:" + img,
                        os.path.join(CARTS, "storage_test.v32")], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    check(p.returncode == 1, "no card: error (exit %d)" % p.returncode)
    # data in the mailbox region protects against formatting
    card = plain_card("data.memc", 5)
    c = v32dev.Card(card)
    c.write_word(v32dev.REGION_START + 100, 0x1234)
    c.close()
    code, err = run(card, "storage_test", ["1=storage:" + img], frames=3000)
    check(code == 1 and "already contain data" in err, "other data in the region: refused (exit %d)" % code)
    code, err = run(card, "storage_test", ["1=storage:" + img], frames=3000, extra=("--v32dev-host-force",))
    check(code == 0 and report(card)[40] == READY, "...unless forced")
    # a bad spec
    p = subprocess.run([SIM, "-r", "-n", "--v32dev-host=1=floppy:x"], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    check(p.returncode == 1 and "unknown device" in p.stderr, "a bad spec is refused")
    p = subprocess.run([SIM, "-r", "-n", "--v32dev-host=1=storage:%s,wat" % img], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    check(p.returncode == 1 and "unknown storage option" in p.stderr, "a bad storage option is refused")


def test_create():
    print("native host: create=SECTORS and sector=N")
    img = path("new.img")
    if os.path.exists(img):
        os.remove(img)
    card = plain_card("create.memc", 1)
    code, err = run(card, "storage_test", ["1=storage:%s,create=64" % img], frames=3000)
    check(os.path.exists(img) and os.path.getsize(img) == 64 * 512, "a file of 64 sectors was created")
    r = report(card)
    check(r[3:6] == [128, 64, 0], "geometry %s" % r[3:6])


def test_serial():
    print("native host: serial echo, 5000 words each way, hangup after 5006")
    for extra in ((), ("--v32dev-doorbell",)):
        card = plain_card("serial.memc", 8)
        code, err = run(card, "serial_test", ["2=serial:echo,hup=5006"], extra=extra)
        check(code == 0, "program ended normally (%d) %s" % (code, err.strip()[-200:]))
        r = report(card)
        check(r[1] == 99 and r[2] == 1 and r[3] == READY, "channel ready (state %d)" % r[3])
        check(r[4] == v32dev.CLASS_CHAR and r[5] == 1, "class char, kind serial")
        check(r[6] == 5000 and r[7] == 5000 and r[8] == 0, "sent %d, got back %d, %d bad" % (r[6], r[7], r[8]))
        check(r[10] == 5 and r[11] == 6 and r[16:22] == [104, 101, 108, 108, 111, 90], "text and single word echoed")
        check(r[12] == 1 and r[13] == 0, "hung up once everything had been echoed (hup=%d, read %d)" % (r[12], r[13]))
        print("  frames: %d%s" % (r[9], " (doorbell)" if extra else ""))
    # output to a file, input from a file
    card = plain_card("serial2.memc", 8)
    open(path("in.txt"), "wb").write(b"hello" + bytes(range(1, 200)))
    code, err = run(card, "serial_test", ["2=serial:echo,out=%s,hup=5006" % path("out.bin")])
    out = open(path("out.bin"), "rb").read()
    check(len(out) == 5006, "out= file holds the program's %d words" % len(out))
    # serial:stdio writes what the program sends to standard output
    card = plain_card("serial3.memc", 8)
    p = subprocess.run([SIM, "-r", "-x", "-n", "-B", BIOS, "-M", card, "--frame-us=2000", "--max-frames=20000", "--v32dev-host=2=serial:stdio,echo",
                        os.path.join(CARTS, "serial_test.v32")], capture_output=True, stdin=subprocess.DEVNULL)
    check(p.returncode == 0 and b"hello" in p.stdout and len(p.stdout) >= 5000, "serial:stdio: %d bytes on standard output" % len(p.stdout))


def main():
    test_rw()
    test_repeat()
    test_bytes()
    test_states()
    test_create()
    test_serial()
    print("native host tests: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
