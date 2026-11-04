# Proposal to GBtoV32 (and NEStoV32, for the shared spec): frame numbers, names, and the shared harness

From v32sim 20261103-dev, against GBtoV32 20261101-dev. Written 2026-11-04. What works and is relied on: **`make test-trace` passes
18/18 with `V32SIM=` v32sim** (`tools/trace_loop.sh`, gap filling, key replay, the cart hash check); v32sim now writes V32TRACE 2.1's
`segstart=` / `input=` and takes `--input=SCRIPT` (the `V32_INPUT` syntax), and `docs/V32TRACE-SPEC.md` in v32sim is the 2.1 text.

| id | sev | what | stopgap today |
|---|---|---|---|
| SG-1 | **B** | frame numbers of `bin/v32sim` (the mini interpreter) are one more than the console's for the same input | the test fixture uses the mini's number |
| SG-2 | C | two different programs named `v32sim` | the scripts say "NOT the small bin/v32sim" |
| SG-3 | C | the testing harness depends on `v32bench` and v32lua's headless `v32run`, the latter no longer builds | none |
| SG-4 | C | the spec's "candidates for version 3" list has an item that is now done | none |
| SG-5 | C | the V32TRACE recorder exists only as a patch | users build the patched emulator |

---

## SG-1  the console's frame numbers  (B)

**What.** On the gap ROM (`tools/gaprom.py`), the gate built without the 4C00 root, with A held from cart frame 40:

| | trap routine starts in cart frame |
|---|---|
| stock console logic (`v32peek` from v32sim `tools/oracle`, cart frame 0 = the first frame that ends in the cartridge, absolute frame 250) | **41** |
| v32sim `--v32trace` with `--input=A@40-44` (`cartframe=41`; feeding its `input=A@40-41` back gives the same trap) | **41** |
| `bin/v32sim` (`V32_INPUT=A@40-44 V32_STRICT=1`, "TRAP at frame 42") and `test_trace.sh`'s `gap_rec.traps.txt` fixture | 42 |

The cause is not established. A likely one: the mini has no BIOS, so its frame 0 is a whole frame of cartridge code, while on the
console the cartridge's frame 0 is only what is left of the frame in which the BIOS hands over, so the runtime's per-frame schedule
sits one frame differently against the scripted input. **Sketch.** Find the cause with the mini and `v32peek` side by side (v32sim's
`tools/oracle` builds it; the comparison above takes a minute); then either make the mini start its cartridge the way the console does,
or document the offset next to `trace2input.py --delay`, and make the fixture say 41 with a note. **If declined.** A script recorded on the console and
replayed in the mini can land one frame off; the delay setting hides it today.

## SG-2  the name  (C)

`bin/v32sim` and v32sim are different programs with different options; `trace_loop.sh` warns about it. Renaming the mini
(`bin/v32mini`, `bin/gbsim`) removes the trap for agents and users. Its `V32_*` options stay as they are.

## SG-3  the harness's runners  (C)

`tools/testing-harness/test_v32.py` names "v32run -- the official Vircon32 CPU core (tools/headless in the v32lua repo)", which no
longer builds against current ComputerSoftware (v32lua proposal SL-3); `bench_v32.py` names `v32bench` (`C:/github/v32bench`), which
I could not see. v32sim now has a generic version of both (`tools/v32sim-corpus.py`: OK / HALT / CPU_ERROR / **TRAP** via the
`.v32trace.meta` / TIMEOUT, CPU and GPU mean / p95 / max, overrun frames, the last screen, `--input fixed` as in `bench_v32.py`;
`v32sim-addrprof.py` reads `NAME.labels.txt` like `V32_PCPROF`), and the stock-core runners are in v32sim `tools/oracle/`. The
harness could call those and keep its Game Boy parts (Homebrew Hub download, `--root` saving, the site).

## SG-4  the spec text  (C)

`V32TRACE-SPEC.md` 2.1, section 10 ("Candidates for version 3"): "`v32sim` writing `input=` too" is done (2026-11-03). The spec is byte-identical in
GBtoV32 and NEStoV32 and "not to be edited here"; whoever owns it can strike the item and add a line on `--input`.

## SG-5  the recorder patch  (C)

`emulator/emulator-v32trace.patch` (v2 + v2.2, a test build that records by itself and disables restart) is the only DesktopEmulator
recorder. Is it meant to go upstream (ComputerSoftware proposal SE-2), or stay a patch kept here?

## Questions

1. `traps.txt` keeps the first `segstart`/`input` of a hit that repeats (count > 1). v32sim does the same; is that what the recorder
   does too, or does it keep the last?
2. Should v32sim write the human report (`.trace.txt`, spec 4.3) as well? `trace2input.py --session` needs it; for v32sim runs the
   input is already known from the script, so it was left out.
