#!/usr/bin/env python3
"""v32sim-corpus.py -- run many cartridges headless, classify how each ended, measure its load, keep a screenshot.

    v32sim-corpus.py [-B BIOS] [-F FRAMES] [--warmup N] [--input none|fixed|SCRIPT] [--shots DIR] [-o results.csv]
                     [--jobs N] [--timeout SECONDS] CART.v32 ...

The Vircon32 side of GBtoV32's tools/testing-harness (test_v32.py: crashes and screenshots; bench_v32.py: CPU load with
one fixed input for every cart), generalized: any cartridges, any translator or compiler. Each cartridge runs once in
`v32sim --batch --fast -x` for FRAMES frames (default 1800 = 30 s, the BIOS logo included) with --frame-stats, and ends as

    OK          it ran every frame                       HALT        the program stopped (HLT) before that
    CPU_ERROR   a hardware error (the BIOS error screen)  TIMEOUT     the run took longer than --timeout seconds
    TRAP        a translated cartridge reached a trap routine (only when CART.v32trace.meta is beside it: the run
                records V32TRACE without restarts, and the `detail` column has the first TRAP line's fields)
    SIM_ERROR   v32sim stopped for another reason (the log says why)

Per cartridge: CPU mean / p95 / max %, frames at 100% (overruns: the game slows down on the console), GPU mean / p95 /
max %, wall-clock seconds. The first --warmup frames (default 250: the BIOS logo) are left out of the load figures.
--shots DIR writes DIR/NAME.png from the last frame (only for OK runs: a halted program has no last frame to show).

--input: none (default); fixed: the same presses for every cart, so loads compare fairly (Start at cart frame 240, A at 300,
Start at 360, then from 420 Right, Left, Up, Down held 30 frames each with A and B taps, repeated); or a V32_INPUT script
(`A@40-44,START@100`), cart frames, as v32sim --input takes. Results go to stdout as a table and to -o as CSV.
Exit status: 0 all OK (or HALT), 1 some CPU_ERROR / TRAP / TIMEOUT / SIM_ERROR, 2 usage.
"""
import argparse, concurrent.futures, csv, os, re, shutil, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))


def fixed_input(frames):
    runs = ["START@240-243", "A@300-303", "START@360-363"]
    f = 420
    while f < frames and len(runs) < 400:
        for b in ("RIGHT", "LEFT", "UP", "DOWN"):
            runs.append("%s@%d-%d" % (b, f, f + 29))
            runs.append("A@%d-%d" % (f + 10, f + 12))
            runs.append("B@%d-%d" % (f + 20, f + 22))
            f += 30
    return ",".join(runs)


def summary(text, what):
    m = re.search(r"\[frame-stats\] %s: mean ([0-9.]+)%%  p95 ([0-9.]+)%%  max ([0-9.]+)%%  frames at 100%%: (\d+)" % what, text)
    return m.groups() if m else ("", "", "", "")


def run_one(cart, a):
    name = os.path.splitext(os.path.basename(cart))[0]
    w = tempfile.mkdtemp(prefix="v32corpus.")
    try:
        cmds = ["run %d" % (a.frames - 1)]
        if a.shots:
            cmds.append("screenshot %s" % os.path.join(os.path.abspath(a.shots), name + ".png"))
        cmds += ["run 1", "quit 5"]                       # 5: every frame ran (0 would be HLT)
        args = [a.sim, "--batch", "--fast", "-x", "--max-frames=%d" % (a.frames + 1), "-B", a.bios,
                "--frame-stats=%s" % os.path.join(w, "fs.csv"), "--frame-stats-skip=%d" % a.warmup]
        if a.input and a.input != "none":
            args.append("--input=%s" % (fixed_input(a.frames) if a.input == "fixed" else a.input))
        meta = os.path.splitext(os.path.abspath(cart))[0] + ".v32trace.meta"
        if os.path.isfile(meta):
            args += ["--v32trace=%s" % os.path.join(w, "t"), "--v32trace-norestart"]
        args.append(os.path.abspath(cart))
        t0 = time.time()
        try:
            r = subprocess.run(args, input="\n".join(cmds) + "\n", capture_output=True, text=True, timeout=a.timeout)
            rc, err = r.returncode, r.stderr
        except subprocess.TimeoutExpired:
            rc, err = None, ""
        secs = time.time() - t0
        status = {5: "OK", 0: "HALT", 3: "CPU_ERROR", None: "TIMEOUT"}.get(rc, "SIM_ERROR")
        if status == "SIM_ERROR" and a.logs:
            os.makedirs(a.logs, exist_ok=True)
            open(os.path.join(a.logs, name + ".log"), "w").write("v32sim status %s\n%s" % (rc, err))
        detail = ""
        tp = os.path.join(w, "t.traps.txt")
        if os.path.isfile(tp):
            for line in open(tp, errors="replace"):
                if line.startswith("TRAP "):
                    status = "TRAP" if status in ("OK", "HALT") else status
                    detail = re.sub(r" (stack|tile|trail|input)=\S*", "", line.strip())[5:200]
                    break
        cpu, gpu = summary(err, "cpu"), summary(err, "gpu")
        frames = re.search(r"\[frame-stats\] (\d+) frames", err)
        return {"cart": name, "status": status, "frames": frames.group(1) if frames else "",
                "cpu_mean": cpu[0], "cpu_p95": cpu[1], "cpu_max": cpu[2], "overruns": cpu[3],
                "gpu_mean": gpu[0], "gpu_p95": gpu[1], "gpu_max": gpu[2], "seconds": "%.1f" % secs, "detail": detail}
    finally:
        shutil.rmtree(w, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("carts", nargs="+")
    ap.add_argument("-B", dest="bios", default=os.environ.get("V32BIOS", "StandardBios.v32"))
    ap.add_argument("-F", dest="frames", type=int, default=1800)
    ap.add_argument("--warmup", type=int, default=250)
    ap.add_argument("--input", default="none")
    ap.add_argument("--shots")
    ap.add_argument("--logs", help="directory for the stderr of runs that ended in SIM_ERROR")
    ap.add_argument("-o", dest="out")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    ap.add_argument("--timeout", type=float, default=600)
    a = ap.parse_args()
    a.sim = os.environ.get("V32SIM") or os.path.join(HERE, "..", "bin", "v32sim")
    if not os.access(a.sim, os.X_OK):
        a.sim = shutil.which("v32sim") or a.sim
    if not os.path.isfile(a.bios):
        a.bios = shutil.which(a.bios) or a.bios
    if not os.access(a.sim, os.X_OK) or not os.path.isfile(a.bios):
        sys.exit("v32sim-corpus: need v32sim (V32SIM) and a BIOS (-B or V32BIOS)")
    if a.shots:
        os.makedirs(a.shots, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        res = list(ex.map(lambda c: run_one(c, a), a.carts))
    cols = ["cart", "status", "frames", "cpu_mean", "cpu_p95", "cpu_max", "overruns", "gpu_mean", "gpu_p95", "gpu_max", "seconds", "detail"]
    print("%-24s %-9s %6s %8s %8s %8s %8s %8s %8s" % ("cart", "status", "frames", "cpu avg", "cpu p95", "cpu max", "overrun", "gpu avg", "gpu max"))
    for r in res:
        print("%-24s %-9s %6s %8s %8s %8s %8s %8s %8s" % (r["cart"][:24], r["status"], r["frames"], r["cpu_mean"], r["cpu_p95"],
                                                         r["cpu_max"], r["overruns"], r["gpu_mean"], r["gpu_max"]))
    if a.out:
        with open(a.out, "w", newline="") as f:
            wr = csv.DictWriter(f, fieldnames=cols)
            wr.writeheader()
            wr.writerows(res)
    for r in res:
        if r["detail"]:
            print("  %s: %s" % (r["cart"], r["detail"]))
    return 1 if any(r["status"] in ("CPU_ERROR", "TIMEOUT", "SIM_ERROR", "TRAP") for r in res) else 0


if __name__ == "__main__":
    sys.exit(main())
