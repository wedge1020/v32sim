# Changelog

## 20261123-dev
- **Downstream: V32DOS 20261123-dev uses v32sim's tools** (proposal SD-1..SD-4 resolved, `docs/proposals/PROPOSAL-V32DOS.md` removed): its bootstrap builds v32sim with `tools/build-nogen.sh` and the stock runners with `tools/oracle/build.sh` (v32c++ no longer needed), its sessions run through `tools/v32sim-golden.sh`, and its v32dev disk tests use the native host (`--v32dev-host`, `dev detach/attach/swap`) instead of `v32dev.py serve`.
- **`make check` with `V32IO=` runs v32io's suites again** (13: v32dev device test 98 checks incl. the embedded-server scenarios, native host 125); harness count corrected to 98 with `ORACLE` and `V32IO`.
- **Built-in v32dev server: `V32DEV_SERVER` and `V32DEV_CH0..3`**, as the DesktopEmulator patch of the v32io project reads them (`src/devhost.c`, `src/memc.c`). With `V32DEV_SERVER` set (not `0`) v32sim serves the card that is loaded with the SPECs of the four variables, on any card: no formatting, the mailbox header is put in memory only, the whole region is left out of every save (the card file never has a trace of it), a card with data in its last 64 KiB is left alone with a warning. `--v32dev-host=N=SPEC` wins for its channel and works as before without the variable.
- **`serial:tcp=PORT`** (`--v32dev-host` and the variables): 127.0.0.1 only, one client at a time (a new one replaces the old), output dropped while nobody is connected, a hang up for the program when the client goes away.
- **`storage:` SPEC options are read from the right** (the server's rule: a file name may hold commas; `storage:f.img,wat` is the file `f.img,wat`, and a failure to open it says so; it was "unknown storage option"). Bad `sector=`, `create=` and port numbers are range-checked.
- **`dev format [force] [N=TYPE ...]`** types channels as `v32dev.py format --chN` (`none storage block serial char pipe tcp udp netplay socket` or a number); `dev info` names every type as `v32dev.py info` does (it only knew storage and serial); the storage descriptor's sector count is capped at 32 bits.
- **`tests/run_tests.sh` no longer passes `--no-embedded`**: the v32io `dev_rom_test.py` runs its embedded-server scenarios against v32sim (98 checks; `tests/v32sim_runner.sh` passes standard input and output through for `serial:stdio`). New no-toolchain suite `tests/run_v32dev_server_tests.sh` (84 checks; not yet in the Makefile `test` target, but run by `tests/run_tests.sh`).
- **First release candidate.** `docs/USAGE.md` (every option, command, environment variable and exit status in one place) and the man page `man/v32sim.1`; Spanish and French translations of the README, USAGE and man page (`README.es.md`, `docs/USAGE.fr.md`, `man/v32sim.es.1`, ...). English-only docs: the rest of `docs/`.
- **`make sysinstall` / `make install` install the man page**, gzipped as `v32sim.1.gz`; `DEST_LANG=en|es|fr` picks the language (unset or `all`: English). `make version VERSION=X` also stamps the `.TH` line of the three man pages.
- **`make dist`**: a release zip `v32sim-<version>-<os>-<arch>.zip` (binary, README*, LICENSE, `docs/USAGE*.md`, man pages, tools). `READLINE_PREFIX=DIR` points the build at another readline (macOS: `make READLINE_PREFIX=$(brew --prefix readline)`).
- **GitHub release workflow** (`.github/workflows/release-builds.yml`, manual): builds linux and macos binaries, checks that the version in `inc/version.h` is stamped in the man pages, and leaves a release as a draft unless `publish_release` is set.
- **Fix: `-b` with a name made of hex digits.** `-b add`, `-b face`, `-b dead` were taken as addresses and an unknown name was dropped silently. `-b` now takes what `break` takes, once the debug files are loaded: a known label or function first, then hex (with or without `0x`), then `FILE:LINE`; anything else is an `[ERROR]` on stderr (tests/run_cli_tests.sh, `-b`).
- **Fix: `-L MODE` with an unknown mode** was ignored; it is now an error with exit status 1 (`C`, `lua` and `src` are accepted). `-S` is unchanged (the BIOS code start is the default); `--help` now says so.
- **`--help` lists every long option of `src/args.c`**: `--quiet`, `--stats`, `--stats-top`, `--stats-skip` and `--frame-log` have their own lines; typo "addres" fixed (tests/run_cli_tests.sh loops over all long options).
- **Fix: in-sim `help` drift.** `help set` lists mode `asm|src|C|lua`, the `set` error lists every setting, `help print` has `/s`, `help display` the SYMBOL form, `help break` FUNCTION / `FILE:LINE` / `-a`, `help until` `[LINE | FILE:LINE]`, `help quit` status 5 (tests/run_cli_tests.sh, "in-sim help against the parser"). The single-letter abbreviations (`r` is `replace`, `ru` is `run`) are written down in `help`, README and man.
- **Fix: `tbreak` with no argument** was a parse error; it lists the breakpoints, as `break` does (grammar rule in `src/parser.y`, `gen/parser.c` regenerated).
- **`--screenshot-at` warns when frames are never reached**: at exit, `[WARNING] --screenshot-at: N of M screenshots taken ...` (`shots_report()` in `src/shots.c`). The documented recipes used `--max-frames=130`, which counts from power-on (the BIOS logo takes about 250 frames) and wrote nothing; they now use 400 and 450.
- **Exit status table completed** in `--help`, README, USAGE and man: 11 a file cannot be opened, 12 not read, 21 not a V32 file, 2 also when `/dev/null` cannot be opened, and the internal 4, 5, 7, 13, 14, 15 (tests/run_cli_tests.sh: missing BIOS 11, junk file 21, bad option 1).
- **Fix: `continue` after `up` kept the caller's frame selected**, so `locals` (and `print` of a local) at the next breakpoint showed the wrong frame; stopping now selects the innermost frame again (`src/srcdebug.c`; `tests/run_harness_tests.sh`, 3 new checks).
- **Start-up clock clamped as the console does** (no `--fixed-clock`): year 0-32767, day 0-365, hour 0-23, minute and second 0-59, so a leap second reads 59 (`init_ioports`). The date and time ports otherwise already matched the console, daylight-saving changes included.
- **Checked against the current DesktopEmulator `ConsoleLogic`**: every V32SIMGAPS item re-read and measured (no difference left; the three `docs/BUGS.md` reports are fixed upstream). New differential suites `tests/run_clock.sh` (37 instants in four time zones, across DST changes, leap days, year ends and 2100) and `tests/run_memcard.sh` (14: memory card and v32dev mailbox), both run by `make check`; new gaps `g12_shift_counts`, `g21_spu` and more `g04` dates (gaps 46 -> 56).
- **Locals: documentation brought up to date.** Struct members by name, globals with `type=`, Lua globals and closure upvalues are shown (the producers write them and v32sim reads them); still missing: `print p.x`, nested Lua tables (address only), and `display NAME` following the frame (TODO.md, `docs/V32DBG-STATUS.md`).
- **`tests/run_cli_tests.sh` (128 checks, no toolchain) is part of `make test`**, with `tests/run_kbd_sync_tests.sh`.
- **Docs audited against the code**: README usage block regenerated from `--help`, real install and build targets, debug-file suffixes and search order, `V32DEV_DISABLE` (a warning with `--v32dev-host`), `--input2` / `V32_INPUT2`, stale `docs/BUGS.md`, `V32DBG-*` and `agents/` cards corrected, PROPOSALS-STATUS rows that contradicted each other fixed.
- **Removed**: the top-level `proposals/` copy (`docs/proposals/` stays), proposals that are resolved, `docs/CONSOLIDATION.md` (its open items moved to `TODO.md` and `docs/TOOLS.md`), and `screenshots/`.

## 20261122-dev
- **v32sim is 5-100x faster** on the workloads measured (`docs/PERFORMANCE.md`): `--debug` / `--verbose` text is formatted only when read (it was built for every instruction), shorter paths for memory, port, fetch and the cycle counter, hooks (watchpoints, source stepping, address profile) skipped unless armed, free-listed CALL/RET backtrace nodes. CPU-bound: 4.8 M to 106 M cycles/s. Output is byte-identical to the 20261109 build.
- **Makefile builds with `-O2 -flto -ffp-contract=off`** (it had no `-O` at all); `make` falls back to `gen/` without flex/bison; `make version` also stamps `TODO.md`. `sysinstall` puts tools in `$(VROOT)/v32sim/tools`, `install` in `$(USRLIB)/tools`.
- **Deferred GPU painting**: draws are queued until a screenshot or `save` needs pixels, an opaque clear drops the queue; `--gpu-eager` paints at once. The dead `turboflag` stub was removed.
- **`--turbo[=report]`** (implies `--fast`): skips spin loops that repeat exactly and drops a full GPU queue; `=report` prints the skipped cycles and dropped draws. Test: `tests/run_turbo_tests.sh` (in `make check`).
- **`--kbd-sync` / `kbd sync on|off`**: key events wait until the program has read the previous one. The "lost key" after a v32dev round trip (`cat` arriving as `at`) was not a bug: one event per frame replaces the last, as in the DesktopEmulator. `kbd` shows events replaced unread (tests/kbd_probe.py).
- **`--warn-halt` and `--halt-status=N`**: `HLT` can be reported on stderr and set the exit status (both off by default).
- **`regex:` patterns for `until` and `expect console`** (POSIX ERE); the plain-text rules are documented. Test: `tests/run_kbd_sync_tests.sh` (22 checks).

## 20261109-dev
- **`tools/v32mkcart.sh`** builds C, C++ (`v32c++ -g --compiler=v32cc`) and Lua (`v32lua -g`) cartridges and takes `-O LEVEL` (v32opt). New suite `tests/run_stack_tests.sh` (33 checks): `bt`, `break FILE:LINE`, `locals` and macro lines across C, C++ and Lua at `-O0`, `-O2`, `-O3`.

## 20261108-dev
- **`docs/V32DBG-SPEC.md` is hosted here**; `V32DBG-STATUS.md` has an "Against the spec" table. `--dump` failures exit with status 5 (tests/run_batch_tests.sh, 31 checks).
- **`make check TC=DIR [V32IO=DIR] [SRCMARK=FILE]`** (`tools/v32sim-check.sh`) runs every suite from one toolchain directory. `type=char` values print as `65 'A'`; Lua tables print their contents.
- **VS-19 leftovers**: `dev [info]`, `dev format [force]`, `dev attach|swap N SPEC`, `dev detach N`; `save` / `restore` keep the native v32dev host's state (tests/native_host_test.py, 125 checks).

## 20261106-dev
- **Native v32dev host** (VS-19): `--v32dev-host=N=SPEC` (`src/devhost.c`), storage and serial, a port of `v32dev.py`; same report and image as `v32dev.py serve` (tests/native_host_test.py).
- **`--v32dev-doorbell[=MS]`** (VS-17 / P-E2): a `c2h_head` store rings the host; interface as v32nix specified (tests/doorbell_host.py, `tests/examples/bell.c`). v32nix declined the timer interrupt (P-E1).
- **Perf counters** (VS-13): GPU and SPU counters per frame in `--frame-stats` and per texture:region (`--frame-stats-top=N`); in a session `stats`, `stats regions`, `stats reset`, `stats expect NAME OP VALUE`; `tools/v32sim-statdiff.py` compares two runs.
- **Stable dotted counter names and aggregates** (`gpu.draws`, `cpu.cycles`, `bus.movs_words`, ...: `.last .mean .min .max .p50 .p95 .p99 .total`), `stats save FILE [json]`, `--stats-format=json`, `--stats` / `--stats-skip` / `--stats-top` synonyms, `stats on|off`. `expect stats gpu.draws.p95 <= 600` fails like `expect`.
- **VS-13 4.2-4.4**: `--profile-event=instr|cycles|gpu-draw|gpu-portwrite|spu-write`, `--gpu-trace` / `--spu-trace` with `--trace-dump` (`docs/TRACE-FORMAT.md`) and `v32gpureplay`, `--time-breakdown` and `--frame-log=FILE`; counters `bus.memcard_accesses`, `spu.channels_playing` (`tests/examples/counts.c`).
- **`--v32mouse[=PORT]` and the `mouse` command** (VS-11, `src/v32mouse.c`): `attach detach move click|down|up wait clear`, v32io Gray/trit encoding (tests/run_mouse_vectors.py, harness "v32mouse").
- **Screenshot tools**: `highlight TEX:REGION [COLOR]`, `screengrid` (with coordinate labels), `noextra` / `screenshot --plain`; options `--highlight=T:R[:COLOR]`, `--screengrid[=SPEC]`, `--noextra`, `--screenshot-at=FRAMES` and `--screenshot-file=PATTERN` (`src/shots.c`; harness "screenshot highlight").
- **One oracle runner**: `v32prof`, `v32peek` and `v32shot` became `v32orun --compat=prof|peek|shot`, output identical to the old programs. `make archive` writes `../v32sim-project-YYYYMMDD.zip`.
- **V32TRACE**: `input2=` in the meta and `--input2=SCRIPT|@FILE` / `V32_INPUT2` (VS-18).

## 20261105-dev
- **`tools/oracle/orun.cpp` -> `v32orun`** (VS-12), on the stock `V32Console`: a bare `.vbin`, a real BIOS + cart, or `--as-bios`; `refrun`, `v32run` and `vrun` are `--compat` wrappers (harness section "v32orun", gaps 46/46, romcheck 28/28).
- **Symbols and debug files**: `%define var_NAME` is read (VS-10), `NAME.vbin.sym` is read before `NAME.asm.sym` (VS-15), `#` lines are skipped, and struct types `struct:TAG:WORDS{x:int@0;...}` print as `{x = 3, y = 5}` (VS-14).
- **`ram:0x75`** addresses a low RAM word (VS-16); a cartridge argument of `-` runs the BIOS alone.
- **`make install` / `uninstall` / `sysinstall` / `sysuninstall`** and `make version VERSION=...`; `v32sim-peek.sh -M CARD`; `v32sim-selfcheck.sh -M`.

## 20261104-dev
- **V32TRACE spec 2**: `--v32trace=PREFIX`, `--v32trace-meta`, `--v32trace-labels`, `--v32trace-norestart`: traps, events, restart after a trap, `traps.txt`, `blocks.txt` (`src/v32trace.c`); also the human report `PREFIX.trace.txt`, `series`, markers (`mark`) and host cost, compared with the patched DesktopEmulator in `tests/run_harness_tests.sh`.

## 20261103-dev
- **Consolidation**: the oracle tools, harness scripts, AGENTS/CLAUDE and docs of v32cxx, v32nix and V32DOS moved here (what stays elsewhere is in `docs/TOOLS.md`).
- **`--frame-stats`**: per-frame CPU / GPU load, equal to v32prof's on 400/400 frames (first slice of VS-9).
- **`locals [NAME]` / `info locals`** (V32DBG section 4) from `NAME.vbin.vars`, locals before globals in `print`, `x`, `watch` and `expect`; C and Lua types, boxed Lua locals (tests/run_harness_tests.sh).
- **V32TRACE 2.1**: `segstart=` / `input=` on every TRAP / EVENT line and `--input=SCRIPT` (`V32_INPUT` syntax) to replay them.
- **Fix: `--dump=N@NAME`** read a name made of hex digits (`ab`, `f`, `cafe`) as an address. **Fix: `%define global_NAME`** symbol fallback never parsed a newline-terminated line. `;@src -` ranges (`ADDRESS,-,0`) are understood.

## Before 20261103
- **Command parser** in flex/bison (`lexer.l`, `parser.y`, `command.c`) replaced the regex front end; `ignore`; `--version` / `-V` from `inc/version.h`.
- **Watchpoints** on registers, IOPorts and memory words, compared with a number, register, IOPort or memory word; edge-triggered, checked after every instruction.
- **GPU software renderer** (640x360 framebuffer) and `screenshot` (PNG); scale and angle ports were fixed (bits were converted as an integer, so zoomed and rotated draws were wrong).
- **SPU model and VSND loading** (no audio output); **MEMC** with header and size checks, changes written at every frame end, and a live v32dev mailbox (`mailbox` command).
- **v32io keyboard**: `v32kbd` on a gamepad port, `kbd` command, `--v32kbd`, `--kbd-input` (terminal, pipe, FIFO, file).
- **Unattended runs**: `--fast`, `--frame-us`, `--max-frames`, `-x`. `load bios:PATH`, `replace` refreshes IR/IV, float disassembly prints shortest round-trip values, TIM leap years.
- **Text console**: `console` / `--console` capture BIOS-font text (64x18 BIOS layout and v32c++'s 80x24 terminal); `--tty` is a full-screen interactive mode; `--console-color`, `--cursor-color`, `console color|cursor`.
- **Scripting** (V32DOS proposal VS-1..VS-8): symbols in `print` / `display` / `watch` / `expect` / `x`, `run N`, `until`, `--batch`, `expect`, exit statuses, `quit N`, `console FILE`, `--profile-by=function`, signed `watch/d` (tests/run_batch_tests.sh).
- **Debug files** found beside the V32 file, in `obj/`, `../obj/`, the current directory or `--debug-dir`, loaded automatically (`-n` disables); assembly and C overlay (`--source`, `set source`, `set mode`).
- **Source-level debugging** (V32DBG sections 1-3, 5): `mode src` (`lua` / `C` aliases), `step` / `next` by line, `finish`, `until [LOC]`, `break` / `tbreak FILE:LINE`, `list`, `bt` with function names, `frame` / `up` / `down` (tests/run_src_tests.sh).
- **Robustness and tools**: SIGSEGV / SIGBUS / SIGFPE / SIGABRT state dump, `save` / `restore`, live assembly with `asm` (flex/bison, checked against the whole BIOS), `tests/run_tests.sh`.
- **CPU audit against the DesktopEmulator**: `SETS` stored the register number, `SHL` used an arithmetic shift, `MOVS` / `SETS` / `CMPS` counted CR differently, `ACOS` / `ATAN2` / `LOG` / `POW` raised no errors, the error handler reset SP/BP to 0 and R1 held the instruction's own address, `print/D` converted float immediates instead of reinterpreting them (tests/run_gaps.sh, run_fuzz.sh, run_romcheck.sh).
