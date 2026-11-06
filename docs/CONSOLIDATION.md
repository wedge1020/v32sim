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

## Round 2 (2026-11-03, evening): GBtoV32, v32lua, v32io, the new ComputerSoftware

| in v32sim | came from | what changed | verified how |
|---|---|---|---|
| `locals [NAME]` / `info locals`; locals in `print`, `x`, `display`, `watch`, `expect` (`src/locals.c`) | V32DBG section 4; v32as round 12 (`.vbin.vars`), v32cc `-g`, v32lua `-g` | new | C and Lua cases in `run_harness_tests.sh` (shadowing, frames, arrays, structs, pointers, floats, Lua values, boxed captures) |
| V32TRACE 2.1 `segstart=` / `input=` (`src/v32trace.c`), spec copy updated to 2.1 | GBtoV32 `doc/V32TRACE-SPEC.md`, the recorder patch | new | GBtoV32 `make test-trace` 18/18 against v32sim; input round trip on its gap ROM; trap frame equal to the stock core's |
| V32TRACE report `PREFIX.trace.txt`, `mark`, `series`, host cost, `--input=@FILE` (`src/v32trace.c`, `src/gpu.c`); `tools/oracle/v32trace-ref` (the recorder, headless) | GBtoV32 `emulator/emulator-v32trace.patch` (`V32Trace.cpp` BuildReport) | new | traps / blocks / trace.txt equal to the recorder's in 3 scenarios; `trace2input.py --session` reads it (`run_harness_tests.sh`) |
| `--input=SCRIPT`, `--input-port`, `V32_INPUT` (`src/padscript.c`) | GBtoV32's `tools/v32sim.c` and the recorders' `V32_INPUT` syntax | new | button counts per cart frame checked; the GB gate cart reaches its trap after the same number of frames as on `v32peek` |
| `tools/v32sim-corpus.py` | GBtoV32 `tools/testing-harness/test_v32.py` + `bench_v32.py` (the Vircon32 side) | any carts; OK/HALT/CPU_ERROR/TRAP/TIMEOUT, load, screenshot, CSV; `--input fixed` like `bench_v32.py` | harness check; a mixed set (crash, overrun-heavy, halting, translated) classified as expected |
| `v32sim-addrprof.py` reads `NAME.labels.txt` | GBtoV32's `V32_PCPROF` | `0xADDR name` maps | on GBtoV32's gap cart |
| `docs/agents/` (13 cards) | every project's README / AGENTS / `--help` | new | options checked against each tool's `--help` |
| `docs/V32DBG-STATUS.md`, `docs/PROPOSALS-STATUS.md`, `docs/repro/v32opt-promote-var.c` | survey of every proposal | new | the repro was run |

Bugs fixed: `--dump=N@NAME` read a name made only of hex digits (`ab`, `f`, `cafe`) as an address; `tools/vprof2cmd.py --frames N`
let events after frame N through.

### The oracle runners (one job, now one program)

`tools/oracle/orun.cpp` -> **`v32orun`** (20261105, central maintenance). It is built on `V32Console` (ConsoleLogic) and runs
a bare `.vbin` on logging stand-ins (v32lua's runner, ported unchanged: its output is byte-identical on the carts compared, and
`unittest.py` still gives 1811/1826), a real BIOS + cartridge (the stand-ins become proxies of the real GPU/SPU, so `-g -s -t -B -P` work
there too and `gpu_dropped` comes from the real GPU's counter), or a `.vbin` as the BIOS. Folded in: `refrun`, `v32run` (v32cxx), `vrun`
(v32cc: `--keys`, `--memc`, `.vbin` as BIOS), v32lua's `v32run` (GPU/SPU logs for `render.py`/`spu_render.py`, BP watch, traps, dropped
GPU draws, CFI choice, SPU state, fixed time, no-memcard) and `v32bench` (`-b`). `refrun`, `v32run` and `vrun` stay as one-line
wrappers (`v32orun --compat=NAME`), so `run_gaps.sh` (46/46), `run_romcheck.sh` (28/28), `run_harness_tests.sh` and v32cc's `make test`
(222 checks, `make card`) run unchanged. v32lua's `tools/headless/v32run.cpp`, `build.sh`, `sdlshim/` and v32cc's `tools/vrun.cpp`,
`build-vrun.sh` were deleted. `v32prof`, `v32peek` and `v32shot` (frame-stamped input scripts; v32shot keeps its software rasterizer) became `v32orun --compat=prof|peek|shot` on 2026-11-06 (output byte-identical, PPMs included). Still separate: `sysprof`, `v32trace-ref`.

## Open items

1. **V32DBG, owners outside v32sim**: v32opt must rewrite `;@var loc=` when it promotes (repro in `docs/repro/`); v32cc should end
   functions for v32as (or v32as should end a C function at the next top-level `__function_` label); v32cc globals and struct
   members in `type=`; v32lua chunk locals and upvalues; v32c++ `-g`. Details: `V32DBG-STATUS.md`.
2. **One oracle runner**: done for the runners (above); `v32prof`/`v32peek`/`v32shot` are folded in; `sysprof` and `v32trace-ref` remain.
3. **VS-9 beyond frame stats**: GPU/SPU counters, `--profile-event`, `expect stats`, traces and replay, `statdiff`.
4. **v32mouse in v32sim** (v32nix P-I2; today only gamepad toggles through `v32io-script.py`), and optional emulator extensions to
   prototype (v32nix P-E1 timer interrupt, P-E2 v32dev doorbell).
5. **Run the new runners on the real thing**: `v32sim-golden.sh` on a V32DOS build, `sysprof` on a v32nix cartridge, the corpus
   runner on GBtoV32's Homebrew Hub set; then make V32DOS's `tests/run.sh`, v32nix's `tools/simtest.sh`/`peek.sh`/`klog.sh`/
   `tools/v32sim/build.sh`, v32cxx's `check.sh` and GBtoV32's harness thin wrappers around these.
6. **Stale downstream text**: V32DOS `docs/V32SIM-PROPOSAL.md` / `PROPOSAL-STATUS.md` (VS-1..8 are done; the v32as `--symbols-all`
   question is answered). GBtoV32's `test_trace.sh` fixture says `cartframe=42` for the gate trap, which is its mini simulator's
   number; the console (stock core and v32sim) starts the trap routine in cart frame 41.
7. Two pinned ComputerSoftware commits (`5219741` for the v32cxx tools, `e75b537` for V32SIMGAPS); the current ComputerSoftware has
   the same sources as the 20261101 bootstrap archive.
