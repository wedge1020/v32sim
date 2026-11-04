# Proposal to GBtoV32 (and NEStoV32, for the shared spec): frame numbers, names, and the shared harness

From v32sim 20261103-dev, against GBtoV32 20261101-dev. Written 2026-11-04. What works and is relied on: **`make test-trace` passes
18/18 with `V32SIM=` v32sim** (`tools/trace_loop.sh`, gap filling, key replay, the cart hash check); v32sim now writes V32TRACE 2.1's
`segstart=` / `input=` and takes `--input=SCRIPT` (the `V32_INPUT` syntax), and `docs/V32TRACE-SPEC.md` in v32sim is the 2.1 text.
Since 2026-11-04 v32sim also writes the human report (`.trace.txt`), series, markers and host cost; all three session files are equal
to the patched DesktopEmulator's on the gap ROM (below).

| id | sev | what | stopgap today |
|---|---|---|---|
| SG-1 | **B** | frame numbers of `bin/v32sim` (the mini interpreter) are one more than the console's for the same input | the test fixture uses the mini's number |
| SG-2 | C | two different programs named `v32sim` | the scripts say "NOT the small bin/v32sim" |
| SG-3 | C | the testing harness depends on `v32bench` and v32lua's headless `v32run`, the latter no longer builds | none |
| SG-4 | C | the spec's "candidates for version 3" list has an item that is now done | none |
| SG-5 | C | ~~the V32TRACE recorder exists only as a patch~~ **answered 2026-11-04: it stays an optional patch kept in GBtoV32** | v32sim writes the same files |

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
GBtoV32 and NEStoV32 and "not to be edited here"; whoever owns it can strike the item and add a line on `--input`. Three more lines
are now out of date: section 3's `series` row ("`v32sim` ignores it"), section 4's `.trace.txt` row ("DesktopEmulator only") and
section 9's v32sim row ("Not: `series`, markers, the human report, host cost"). v32sim's copy is left byte-identical; the README says
what v32sim does.

## SG-5  the recorder patch  (C) -- answered

`emulator/emulator-v32trace.patch` (v2 + v2.2, a test build that records by itself and disables restart) is the only DesktopEmulator
recorder. **Answer (2026-11-04): not meant to go upstream; it remains an optional patch kept in GBtoV32.** ComputerSoftware proposal
SE-2 no longer lists it. v32sim's `tools/oracle/build.sh` builds `v32trace-ref` (the recorder, headless) when pointed at a
ConsoleLogic with the patch applied, and `tests/run_harness_tests.sh` compares v32sim with it.

One thing that would help the comparison: `v32trace-ref` drives the recorder through `V32Trace`'s public calls (`Arm`, the
`OutputFolder`/`ContinueAfterTrap` fields, `AddMarker`, `EndSession`). If the patch's next version renames them, the harness section
is skipped rather than failing (it builds only when `V32Trace.cpp` is there), but a note in the patch's header would keep them in step.

## Questions (both answered 2026-11-04)

1. ~~Does the recorder keep the first or the last `segstart`/`input` of a repeated hit?~~ Checked in the patch: `CaptureHit` only
   increments `Count` for a known hit and keeps the first occurrence's cart frame, segment start, captures and trail. v32sim does the
   same; `traps.txt` is byte-equal to the recorder's, `count=3` of a give-up included.
2. ~~Should v32sim write `.trace.txt`?~~ Yes: v32sim is meant to be an alternative to the stock DesktopEmulator, so it is used the
   same way. **Done**: `PREFIX.trace.txt` in the recorder's layout, `mark [NOTE]` for markers, `series`, host cost (quads and flushes
   per frame by the patch's model), `--input=@FILE` to replay a report's `V32_INPUT=` line; `trace2input.py --session` reads it.
   Line-for-line equal to the recorder's report in three scenarios; the only differences are the replay comment (`v32sim
   --input=@this_file` instead of the emulator's) and the order of hot spots with equal counts (`std::sort` is not stable).

## A note for GBtoV32's scripts

GBtoV32 reads `GBTOV32` as the path of its translator binary (`tools/gbbuild.sh`) and `V32SIM` as its mini interpreter. A harness
that exports either for its own purposes breaks `BUILD.sh` ("Permission denied" running a directory); v32sim's harness now calls its
variable `GBTOV32_TREE` and unsets both when it calls `BUILD.sh`. A clearer name (`GBTOV32_BIN`) would avoid the trap.
