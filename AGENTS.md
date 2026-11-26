# v32sim -- notes for agents

(`AGENTS.md` is the shared agent-instruction file; `CLAUDE.md` only imports it.)

v32sim is the **central tool repository for debugging, profiling, testing and simulating Vircon32 software**: a headless
simulator with an interactive single-step debugger (C, flex/bison), plus the scripts, oracle tools and test harnesses that
the other Vircon32 projects (v32cxx, v32nix, V32DOS, v32cc, NEStoV32...) grew on their own and that now live here.
Other projects keep thin wrappers; the tools themselves are here. `docs/TOOLS.md` says which tool answers which question, and what stays in the other projects.

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
  `expect` failed / `until` timed out / `--expect-reach` missed (`EXPECT MISSED`), 2 frame limit (`-x`), 3 system error (`-x`), 4 unparsable command (`--batch`), 5 `--dump`
  could not print what was asked, `quit N` overrides; 11 / 12 / 21 at startup (file not opened / not read / not a V32 file).

## Layout

```
src/ inc/            the simulator (README "Project Layout"); gen/ is the flex/bison output, checked in
tools/               scripts for people who test other projects with v32sim (docs/TOOLS.md)
  v32mkcart.sh         C/C++/Lua/asm -> cartridge with debug files (v32cc/v32c++/v32lua + v32as, optional v32opt -O, or the stock tools)
  v32sim-check.sh      every suite from one toolchain directory, one summary line (`make check TC=DIR`)
  v32sim-selfcheck.sh  run test_errors-style cartridges, pass/FAIL (or --oracle: on the stock core)
  v32sim-session.py    typed-session script generator (V32DOS's mkcmd.py, generalized)
  v32sim-golden.sh     sessions + golden screens (V32DOS's run.sh, generalized)
  v32sim-addrprof.py   hot spots across kernel / BIOS / RAM-loaded modules (V32DOS's profile.py, generalized)
  v32sim-peek.sh       read a global or text buffer after N frames (v32nix's peek.sh / klog.sh)
  v32sim-corpus.py     many carts: OK/HALT/CPU_ERROR/TRAP, CPU/GPU load, screenshot (GBtoV32's harness, generalized)
  vprof2cmd.py         v32prof/v32peek input scripts -> v32sim commands
  v32io-script.py      keyboard/mouse actions -> input script (from v32cxx)
  v32sim-statdiff.py   compare two --frame-stats CSVs (mean/p95/max per column, --fail-over gate)
  v32sim-statediff.py  compare two `save` state files section by section (--ignore, --ram-range; exit 0 same, 1 differ)
  build-nogen.sh       build v32sim with cc + readline only, using gen/ (from v32nix / V32DOS)
  oracle/              C++ tools on the stock ConsoleLogic: v32orun (the one runner; refrun v32run vrun v32prof v32peek v32shot are wrappers) sysprof (+ build.sh)
bios/                StubBios.v32 (+ .vbin.debug; the bundled BIOS, the default when no -B), src/ (its source and build.sh), debuggerBIOS.v32; bios/README.md
tests/               the suites; run_harness_tests.sh covers tools/ (examples/ holds its cartridges)
man/                 v32sim.1 and its Spanish / French translations (v32sim.es.1, v32sim.fr.1)
docs/                USAGE (command-line reference; USAGE.es.md, USAGE.fr.md) TOOLS TESTING PROFILING PERFORMANCE AGENTS-SNIPPET
                     V32DBG-STATUS PROPOSALS-STATUS TRACE-FORMAT, the specs (V32DBG-SPEC, V32SIMGAPS, V32TRACE-SPEC 2.1, BUGS),
                     docs/repro/ (cases for other projects), agents/ (one card per tool),
                     docs/proposals/ (open requests to v32c++, v32io, GBtoV32 and v32nix, IDs S*-n;
                     resolved ones were removed before the first release)
```

## Working on it

- Build: `make` (needs flex + bison + readline) or `tools/build-nogen.sh` (readline only). After editing `src/*.l` /
  `src/*.y`: `make gen` and commit `gen/`, because v32nix and V32DOS build v32sim from it (V32DOS's bootstrap runs `tools/build-nogen.sh` and `tools/oracle/build.sh`).
- Tests (most need the toolchain; see the headers): `tests/run_batch_tests.sh` (scripting), `run_src_tests.sh` (source
  level), `run_stack_tests.sh` (C / C++ / Lua debug stack), `run_gaps.sh` (56 checks), `run_romcheck.sh`, `run_fuzz.sh` (differential
  against `refrun`), `run_clock.sh` (start-up clock vs the oracle across time zones/DST, `LD_PRELOAD` time shim, 37 checks),
  `run_memcard.sh` (memory card + v32dev mailbox vs the oracle via `vrun`, 14 checks), `run_tests.sh` (v32dev, v32kbd, card persistence), `run_turbo_tests.sh` (`--turbo`, deferred GPU), `run_heap_tests.sh` (`--heap`, `--expect-reach`; needs v32lua, 69 checks),
  `run_harness_tests.sh` (the tools/ scripts and their agreement with the oracle; 98 checks with ORACLE and V32IO set). `make test` runs
  `run_kbd_sync_tests.sh`, `run_cli_tests.sh` (command line, help texts, exit statuses; 144 checks) , `run_stubbios_tests.sh` (21 checks, 22 with `STOCK=StandardBios.v32`) and `run_present_tests.sh` (3 checks), which need no toolchain; `make check TC=DIR` runs them all.
  Passing one suite proves little: a CPU/GPU change needs the differential ones, a scripting change the batch one.
- BIOS: after editing `bios/src/StubBios.asm` run `bios/src/build.sh` and commit `bios/StubBios.v32` and `bios/StubBios.vbin.debug`.
- Add an option: `src/args.c` (long option number), `src/help.c`, `inc/globals.h`, README "Usage", `docs/USAGE.md`, `man/v32sim.1`
  (and the translations, see Docs), and a test. A new prompt command goes in `src/lexer.l`, `src/parser.y`, `src/command.c`
  (its command table) and `src/help.c`, and in README "Commands".
  A new per-frame hook goes in `src/update_sys.c: update_frame` before `timer_change_frame` (ports still hold the
  frame that ended). `src/framestats.c` is the model of a read-only observer.
- Version: `inc/version.h` (`YYYYMMDD-dev`). Bump it when you change behaviour others depend on, with `make version VERSION=X`: it
  also writes TODO.md's "Status as of version" line and the `.TH` line (version and month) of the man pages; nothing else carries the number.
- Docs: README (features, options), TODO.md (done / lacking), `docs/`. Say in TODO.md when a downstream request
  (V32DOS VS-x, the perf proposal) is done: downstream status documents go stale otherwise. `README.md`, `docs/USAGE.md`
  (command-line reference) and the man page `man/v32sim.1` have Spanish and French translations (`README.es.md`, `README.fr.md`,
  `docs/USAGE.es.md`, `docs/USAGE.fr.md`, `man/v32sim.es.1`, `man/v32sim.fr.1`): when you change an English one, update the
  translations or say they are stale. `make version` stamps the man pages' `.TH` lines. The other docs are English only. When a
  limitation is lifted or an option added, update README "Known Limitations", TODO.md and this file.
- Releases: `.github/workflows/release-builds.yml` ("Generate Global Binaries", run by hand) builds Linux and macOS (x86_64
  and arm64, natively, from `gen/` with `HAVE_GEN_TOOLS=no`; macOS uses Homebrew readline through `READLINE_PREFIX`), runs
  `make test` and packs `make dist`; a `release` run then makes the GitHub release `v<VERSION>` from `.github/release-notes.md`
  (`@VERSION@` is replaced), a draft unless `publish_release` is ticked, and refuses man pages not stamped by `make version`.
  Keep the release text's feature list in step with the README. Windows x86_64 is an optional job built in MSYS2's
  POSIX "MSYS" environment (unchanged sources; msys-2.0.dll and the readline/ncurses DLLs are zipped beside the binary).
  A native UCRT64/MinGW build would need a small `#ifdef _WIN32` layer first: the termios keyboard source in `src/v32io.c`
  (console API instead), BSD sockets and `fcntl(O_NONBLOCK)` in `src/devhost.c` (winsock, `ioctlsocket`), `O_BINARY` on the
  card and storage `open()`s (`src/memc.c`, `src/devhost.c`), `getline()` (`src/input.c`), SIGBUS (`src/main.c`,
  `src/signal.c`), and `<regex.h>` from MSYS2's libsystre (`src/gpu.c`, link `-lsystre`).
