# Consolidation log: what moved into v32sim, from where

Goal: v32sim is the central repository for profiling, debugging, testing and simulation tooling for Vircon32 projects;
the projects that grew their own keep thin wrappers or standalone tools. Surveyed 2026-11-03: v32cxx, v32nix
(20261102-dev), V32DOS, and the v32cc / v32as / v32opt projects for the debug-info chain.

## Folded in

| in v32sim | came from | what changed | verified how |
|---|---|---|---|
| `tools/oracle/{v32run,v32prof,v32shot,v32peek}.cpp`, `shim/SDL.h` | v32cxx `tools/vircon32/` | unchanged but for build comments | built; v32prof and v32peek compared with v32sim (below); v32shot made a 640x360 PPM |
| `tools/oracle/sysprof.cpp` | v32nix `tools/sysprof/` | unchanged | built; prints usage. **Not run** on a v32nix cartridge (needs its kernel map and user images) |
| `tools/oracle/refrun.cpp` | v32sim `tests/ref/` | moved; `tests/ref/build.sh` is a wrapper | `run_gaps.sh` 46/46, `run_romcheck.sh` 28/28 with the rebuilt tool |
| `tools/oracle/build.sh` | v32cxx `build-tools.sh` + v32sim `tests/ref/build.sh` | one script builds refrun, v32run/prof/shot/peek, sysprof, compile, assemble, packrom, include/, StandardBios; the two SDL shims merged (the `/proc/self/exe` one); no git clone | full build from a bootstrap archive's ComputerSoftware |
| `tools/v32io-script.py` | v32cxx `tools/vircon32/` | unchanged | - |
| `tools/build-nogen.sh` + checked-in `gen/` + `make gen` | v32nix and V32DOS `tools/v32sim/build.sh` (identical copies) | takes v32sim's own tree; no `toolchain/src` layout assumed | built the simulator with it, repeatedly |
| `tools/v32sim-session.py` | V32DOS `tests/mkcmd.py` | DOS-specific prompt symbol and keyboard port became options; adds `@run`, `@console`, `--prompt-text` | output **byte-identical** to the original on V32DOS sessions 1, 3, 5, 8, 11 |
| `tools/v32sim-golden.sh` | V32DOS `tests/run.sh` | cart, BIOS, directories, card preparation, masks and extra args are options | pass, update and (deliberately corrupted) fail on the example. **Not run** against a real V32DOS build |
| `tools/v32sim-addrprof.py` | V32DOS `tools/profile.py` | the kernel/BIOS/module split as `--map NAME:FILE[@BASE\|@AUTO]`; no session machinery | agrees with `--profile-by=function` on the example. **Not run** on a DOS module |
| `tools/v32sim-peek.sh` | v32nix `tools/peek.sh`, `klog.sh` | uses `--dump` on v32sim instead of v32peek; `-t` decodes a text ring | integer and text reads on the example |
| `tools/v32sim-selfcheck.sh` | v32cxx `tools/vircon32/check.sh` (the emulator half) | any cartridge, any `test_errors` global; v32sim by default, `--oracle` for the stock core; plays `CART.input` | pass/fail agree on both engines |
| `tools/vprof2cmd.py` | new | v32prof scripts -> v32sim commands | per-frame gamepad history **identical** to v32peek, 600 words, with press/release/tap |
| `tools/v32mkcart.sh` | new (the build lines of v32cxx `check.sh`, v32nix `cart.sh`, v32cc `cartrun.py`) | v32cc/v32as default, `--stock`, `--srcmark` | all three chains on the example |
| `--frame-stats=FILE`, `--frame-stats-skip=N` | v32prof's per-frame load; VS-9 proposal 4.1 (first slice) | new, `src/framestats.c` | CPU % and GPU % **identical to v32prof on 400 of 400 frames** |
| `AGENTS.md`, `CLAUDE.md`, `docs/{TOOLS,TESTING,PROFILING,AGENTS-SNIPPET}.md` | v32cxx `AGENTS.md` (pattern), v32nix `tests/README.md` and `simtest.sh` header, V32DOS `V32SIM-PROPOSAL.md` | new text | - |
| `tests/run_harness_tests.sh`, `tests/examples/` | new | 13 checks of everything above | 13 / 13 |

## A bug found and fixed on the way

`--dump=1@NAME`, `print NAME`, `x/d NAME` and the rest of the symbol lookups loaded **nothing** from a compiler-written
`NAME.asm` when there was no `NAME.asm.sym`: `sym_load_asm` handed the number with its trailing newline to
`sym_parse_number`, which rejects it. The documented fallback (`%define global_NAME ADDRESS` lines) therefore never worked
for the stock compiler's output (and for v32cc's, when `.asm.sym` is absent). Fixed in `src/symbols.c`; regression check in
`run_harness_tests.sh` (fails on the unfixed simulator, passes on the fixed one).

## Left where it is (and why)

| item | where | reason |
|---|---|---|
| `v32srcmark.py`, `v32prof.py` (line profile for `v32opt --profile`) | v32opt `scripts/` | belong to the optimizer; v32sim documents and calls them (`v32mkcart.sh --srcmark`). `v32prof.py` is a consumer of `--profile-out` |
| `v32sym.py` (nm-style listing with final addresses), `cartrun.py`, `nixcheck.py`, `vrun.cpp` | v32cc `tools/` | compiler-chain tools. `vrun` and `cartrun.py` duplicate `v32run` (see below) |
| `gapcheck.sh`, `asmcheck.sh`, `inventory.sh`, `stagemeasure/` | v32nix `tools/` | test the compilers / assembler, not the simulator |
| `tests/sim/*.cmd`, `tools/simtest.sh` | v32nix | v32nix-specific scripts; they are the best worked examples of command scripts and are cited from TESTING.md |
| `tests/dev.py`, `bios/tests/hostdisk.py` | V32DOS | DOS-specific, but the `Host` helper (start/stop `v32dev.py serve` between commands) is reusable: candidate for `tools/` |
| `tests/*sample.*`, `check.sh` compile half, `AGENTS.md` language rules | v32cxx | transpiler tests and guidance |
| `tests/measure_machine.c` | v32nix | measures `MOVS` at 1 cycle/word; v32sim already matches (V32SIMGAPS), but it would make a good differential case in `tests/gaps` |

## Open items

1. **Locals (V32DBG section 4)** are unblocked: `v32as -g` now writes `NAME.vbin.vars` (`var,START,END,NAME,BP+2,TYPE,-,0,FUNCTION`)
   from v32cc's `;@var`. v32sim reads none of it yet: `info locals`, `print NAME` in the selected frame. TODO.md.
2. **One oracle runner.** `refrun`, `v32run`, v32cc's `vrun` and v32cc's `cartrun.py` all run a cartridge on ConsoleLogic.
   Proposal: `refrun` (or one merged tool) with `--keys`, `--memc` and the `halted=` line, and the others become wrappers.
3. **VS-9 beyond frame stats**: GPU/SPU counters, `--profile-event`, `expect stats`, traces and replay, `statdiff`. The CSV is the
   place to add columns; counters belong on the port-write paths.
4. **Run the new runners on the real thing**: `v32sim-golden.sh` on a V32DOS build and `tests/run.sh` golden files; `sysprof` on
   a v32nix cartridge; then make V32DOS's `tests/run.sh` and v32nix's `tools/simtest.sh`, `peek.sh`, `klog.sh`,
   `tools/v32sim/build.sh` thin wrappers around these.
5. **v32cxx**: `check.sh`'s emulator half can call `v32sim-selfcheck.sh` (it already understands `.input`), and
   `tools/vircon32/` can shrink to the compiler build.
6. Stale downstream text: V32DOS `docs/V32SIM-PROPOSAL.md` and `PROPOSAL-STATUS.md` say VS-1..VS-8 are outstanding; TODO.md
   here lists them as done (the v32nix `simtest.sh` already requires `--batch`). The v32as `--symbols-all` question in that file
   (list `__data_base+N` variables as kind `V`) is for v32as.
7. Two pinned ComputerSoftware commits: v32cxx's tools were verified at `5219741`, V32SIMGAPS compares at `e75b537`.
   `tools/oracle/build.sh` builds against either; the differential suites were re-run here against the checkout in the
   V32DOS bootstrap archive (20261101), whose revision was not recorded.
