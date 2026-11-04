#!/usr/bin/env python3
"""v32sim-addrprof.py -- name the hot spots of a v32sim address profile, across code that lives in several places.

    v32sim --batch -r --fast -x --max-frames=N --profile-out=run.prof [--fixed-clock --seed=1 --fixed-cycles] ... cart.v32
    v32sim-addrprof.py run.prof --map cart:cart.vbin.debug [--map bios:bios.debug] [--map module:m.debug@AUTO] [-n 30]

--profile-by=function already sums one cartridge's functions. This tool is for programs whose code is not all in the
cartridge: an operating system that loads programs into RAM, a DOS that loads modules, a BIOS you also want to see.
Each --map NAME:DEBUGFILE[@BASE] says "addresses in this region are named by this debug file":

    NAME     the group shown in front of the function name (kernel:, module:, bios: ...)
    FILE     a debug file (`address,file,line,label` lines: v32as -g, assemble -g program) or a translator's
             `NAME.labels.txt` (`0xADDR name`: GBtoV32's V32_PCPROF view, guest blocks and runtime routines)
    @BASE    how the file's addresses relate to where the code ran: a number (hex or decimal) is added to the file's
             addresses (0 for code that runs where it was linked); @AUTO means "the lowest RAM address executed is the
             load base" and is the right choice for one module loaded into RAM (its file is assembled at origin 0)

A region is chosen by address: cartridge (0x20000000 and up) uses the first map named `cart*` or `kernel*`, the BIOS
(0x10000000..) the one named `bios*`, RAM addresses the one named `module*` or `ram*`. Addresses no map covers are
reported as NAME:? so nothing is lost. The output is a table: instructions, share, name; and a line per region.

Derived from V32DOS's tools/profile.py (the kernel / BIOS / module split).
"""
import argparse, bisect, collections, re, sys


def load_names(path, strip=True):
    L = []
    for line in open(path, errors="replace"):
        p = line.rstrip("\n").split(",")
        if len(p) == 1 and len(line.split()) == 2:      # a translator's labels.txt: "0xADDR name" (GBtoV32, NEStoV32)
            a, n = line.split()
            p = [a, "", "", n]
        if len(p) >= 4 and p[0].startswith("0x") and p[3]:
            name = p[3]
            if strip:
                name = re.sub(r"^__function_", "", name)
                if re.search(r"_s\d+__|_return$", name):      # a label inside a function, not the function
                    continue
                name = re.sub(r"_s\d+$", "", name)
            L.append((int(p[0], 16), name))
    return sorted(L)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("profile")
    ap.add_argument("--map", action="append", default=[], help="NAME:DEBUGFILE[@BASE|@AUTO]")
    ap.add_argument("-n", type=int, default=30, help="rows to show (default 30)")
    ap.add_argument("-o", help="also write the full table to this file")
    a = ap.parse_args()
    maps = {}
    for m in a.map:
        name, _, rest = m.partition(":")
        path, _, base = rest.partition("@")
        if not name or not path:
            sys.exit("--map NAME:DEBUGFILE[@BASE]: %s" % m)
        maps[name] = (load_names(path), base)
    counts, total = {}, 0
    for line in open(a.profile):
        if line.startswith("#"):
            mm = re.match(r"# cycles (\d+)", line)
            if mm:
                total = int(mm.group(1))
            continue
        p = line.split()
        if len(p) >= 2:
            counts[int(p[0], 16)] = int(p[1])
    if not total:
        total = sum(counts.values())

    def pick(prefixes):
        for name in maps:
            if name.startswith(prefixes):
                return name
        return None
    region = {"cart": pick(("cart", "kernel")), "bios": pick(("bios",)), "ram": pick(("module", "ram"))}
    ram = [x for x in counts if x < 0x10000000]
    agg, cls = collections.Counter(), collections.Counter()
    for addr, c in counts.items():
        r = "cart" if addr >= 0x20000000 else "bios" if addr >= 0x10000000 else "ram"
        name = region[r]
        if name is None:
            label = "%s:?" % r
        else:
            names, base = maps[name]
            if base == "AUTO":
                off = min(ram) if ram else 0
            else:
                off = int(base, 0) if base else 0
            i = bisect.bisect_right(names, (addr - off, "\xff")) - 1
            label = "%s:%s" % (name, names[i][1] if i >= 0 else "?")
        agg[label] += c
        cls[name or r] += c
    head = "%d instructions" % total + "".join(", %s %d" % (k, v) for k, v in sorted(cls.items()))
    rows = ["%10d %5.1f%%  %s" % (c, 100.0 * c / total, n) for n, c in agg.most_common()]
    print(head)
    print("\n".join(rows[: a.n]))
    if a.o:
        open(a.o, "w").write(head + "\n" + "\n".join(rows) + "\n")


if __name__ == "__main__":
    main()
