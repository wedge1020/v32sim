# v32sim -- notes for agents

(`AGENTS.md` is the shared agent-instruction file; `CLAUDE.md` only imports it.)

v32sim is the **central tool repository for debugging, profiling, testing and simulating Vircon32 software**: a headless
simulator with an interactive single-step debugger (C, flex/bison), plus the scripts, oracle tools and test harnesses that
the other Vircon32 projects (v32cxx, v32nix, V32DOS, v32cc, NEStoV32...) grew on their own and that now live here.
Other projects keep thin wrappers; the tools themselves are here. `docs/CONSOLIDATION.md` says what came from where.

**If you are new to the ecosystem**, `docs/agents/` has one short card per machine, language and tool.
**If you are using v32sim from another project**, read `docs/AGENTS-SNIPPET.md` (a drop-in section for that project's own
AGENTS.md) and `docs/TOOLS.md` (which tool for which question). The rest of this file is for working on v32sim itself.

## Ground rules

- **Match the DesktopEmulator, quirks included.** v32sim's behaviour is checked against the stock console logic, not against
  the manual. Differences are bugs unless `docs/V32SIMGAPS.md` lists them (host-undefined behaviour, pixel-level rendering).
  Before "fixing" a CPU/memory/GPU behaviour, run it on the oracle (`tools/oracle`: `v32orun`, or its wrappers `refrun`, `v32run`) and compare.
- **Observability must not change the machine.** Profiling, statistics, traces and `--frame-stats` only read state; a run with
  and without them must give identical results (RAM dumps, exit status).
- **The stock `compile` / `assemble` write no enhanced debug info.** Source-level debugging, `bt`, `print counter`, locals
  need what `v32cc -g` + `v32as -g` write (`NAME.asm.sym`, `NAME.vbin.debug`, `NAME.vbin.vars`). For a cartridge built with
  the stock tools, v32opt's `scripts/v32srcmark.py` adds `;@src` markers; without it v32sim only has addresses and
  the `%define global_X N` data symbols. `tools/v32mkcart.sh` builds either way.
- stdout is for what the script asked for, stderr for diagnostics (`--batch`). Exit statuses are an interface: 0 ok, 1 an
  `expect` failed / `until` timed out, 2 frame limit (`-x`), 3 system error (`-x`), 4 unparsable command (`--batch`).

## Layout

```
src/ inc/            the simulator (README "Project Layout"); gen/ is the flex/bison output, checked in
tools/               scripts for people who test other projects with v32sim (docs/TOOLS.md)
  v32mkcart.sh         C/asm -> cartridge with debug files (v32cc/v32as, or the stock tools)
  v32sim-selfcheck.sh  run test_errors-style cartridges, pass/FAIL (or --oracle: on the stock core)
  v32sim-session.py    typed-session script generator (V32DOS's mkcmd.py, generalized)
  v32sim-golden.sh     sessions + golden screens (V32DOS's run.sh, generalized)
  v32sim-addrprof.py   hot spots across kernel / BIOS / RAM-loaded modules (V32DOS's profile.py, generalized)
  v32sim-peek.sh       read a global or text buffer after N frames (v32nix's peek.sh / klog.sh)
  v32sim-corpus.py     many carts: OK/HALT/CPU_ERROR/TRAP, CPU/GPU load, screenshot (GBtoV32's harness, generalized)
  vprof2cmd.py         v32prof/v32peek input scripts -> v32sim commands
  v32io-script.py      keyboard/mouse actions -> input script (from v32cxx)
  v32sim-statdiff.py   compare two --frame-stats CSVs (mean/p95/max per column, --fail-over gate)
  build-nogen.sh       build v32sim with cc + readline only, using gen/ (from v32nix / V32DOS)
  oracle/              C++ tools on the stock ConsoleLogic: v32orun (the one runner; refrun v32run vrun v32prof v32peek v32shot are wrappers) sysprof (+ build.sh)
tests/               the suites; run_harness_tests.sh covers tools/ (examples/ holds its cartridges)
docs/                TOOLS TESTING PROFILING AGENTS-SNIPPET CONSOLIDATION V32DBG-STATUS PROPOSALS-STATUS, the specs
                     (V32DBG-SPEC, V32SIMGAPS, V32TRACE-SPEC 2.1, BUGS), docs/repro/ (cases for other projects), agents/ (one card per tool),
                     docs/proposals/ (one request list per other project, IDs S*-n; v32sim's own backlog VS-10..)
```

## Working on it

- Build: `make` (needs flex + bison + readline) or `tools/build-nogen.sh` (readline only). After editing `src/*.l` /
  `src/*.y`: `make gen` and commit `gen/`, because v32nix and V32DOS build v32sim from it.
- Tests (each needs the toolchain; see the headers): `tests/run_batch_tests.sh` (scripting), `run_src_tests.sh` (source
  level), `run_gaps.sh`, `run_romcheck.sh`, `run_fuzz.sh` (differential against `refrun`), `run_tests.sh` (v32dev,
  v32kbd, card persistence), `run_harness_tests.sh` (the tools/ scripts and their agreement with the oracle).
  Passing one suite proves little: a CPU/GPU change needs the differential ones, a scripting change the batch one.
- Add an option: `src/args.c` (long option number), `src/help.c`, `inc/globals.h`, README "Usage", and a test.
  A new per-frame hook goes in `src/update_sys.c: update_frame` before `timer_change_frame` (ports still hold the
  frame that ended). `src/framestats.c` is the model of a read-only observer.
- Version: `inc/version.h` (`YYYYMMDD-dev`). Bump it when you change behaviour others depend on.
- Docs: README (features, options), TODO.md (done / lacking), `docs/`. Say in TODO.md when a downstream request
  (V32DOS VS-x, the perf proposal) is done: downstream status documents go stale otherwise.
