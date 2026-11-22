# v32sim performance report, 20261122-dev

## Methodology
Host: 2 cores, gcc, Linux. Best of 3 wall times (`--fast --batch -r -x --max-frames=N`, stock BIOS; the first ~250 frames are the BIOS logo).
Workloads (stock-compiler C, `tests/turbo/` plus extras): cpuheavy (sieve + xorshift + calls, ~84 M instructions), gpuheavy (300 small + 4 full-screen draws/frame),
overdraw (12 full-screen draws, no clear), gfxmix (every draw kind/blend mode), waitheavy (`end_frame` loop), spin2 (polls TIM_FrameCounter + a button, no WAIT), flappy (real cart).
"before" = the 20261109 sources: built exactly as its Makefile does (no -O flag: **-O0**) and, to be fair, also at -O2. Reference = the stock ConsoleLogic via `v32orun` (tools/oracle),
which draws no pixels (the DesktopEmulator renders with OpenGL on top of that), so GPU rows compare v32sim's software pixels to nothing.
Correctness: before/after outputs (screenshots, --frame-stats, --profile-out, console, RAM dumps, debugger sessions with break/watch/finish/save/restore) byte-identical.

## Results (seconds, wall)
| workload | emulated cycles | before (-O0, Makefile default) | before (-O2) | after (default) | after `--turbo` | stock core (no pixels) |
|---|---|---|---|---|---|---|
| cpuheavy 600f | 83.9 M | 17.35 | 6.42 | **0.79** | 0.83 | 0.72 |
| gpuheavy 600f | 3.8 M | 15.18 | 9.11 | **0.057** | 0.061 | 0.064 |
| overdraw 600f | 0.14 M | 20.40 | 12.19 | **0.010** | 0.010 | 0.029 |
| gfxmix 600f | 1.9 M | 2.51 | 1.09 | **0.031** | 0.032 | 0.041 |
| waitheavy 600f (boot + idle) | 0.04 M | 0.52 | 0.22 | **0.009** | 0.008 | 0.023 |
| spin2 800f (poll loop) | 137.5 M | 25.49 | 10.32 | 1.33 | **0.012** | 1.00 |
| flappy 900f (real cart) | 0.06 M | 6.14 | 3.84 | **0.012** | 0.012 | 0.025 |
CPU-bound throughput: 4.8 M cycles/s (-O0) -> 13 M (-O2) -> **106 M cycles/s** (~9.4 ns/instruction, stock core 116 M).
Screenshot every frame on a no-clear GPU cart (worst case, pixels really painted): 3.2 s -> 0.62 s (overdraw), 2.6 -> 0.47 s (gpuheavy).

### v32nix (20261122-dev; own copy in /tmp, bootstrap 20261121b built, baseline v32sim = the bootstrap's 20261109-dev built -O2 by build-nogen.sh)
| flow | baseline | new default | new `--turbo` |
|---|---|---|---|
| `tools/simtest.sh` (boot, cc on the system, env, posix, term, font, curses, getty, home, shutdown, hostdisk x3) | 58.7 s | **12.3 s (4.8x)** | 13.2 s |
| `make test` (JOBS=2, 65 tests) | did not finish in 20 min; the 34 tests it did finish: 2267 s summed per-test time | **203 s wall, 65/65 pass**; same 34 tests: 149 s summed (**15x**) | 226 s, 65/65 pass |
| single on-system compile test (t_u_molay) | 436 s | 17 s | |
Same pass/fail everywhere. `sim/home-v32dev` FAILS identically with the old and the new v32sim (status 9, `until "^kept$"` times out): not caused by this work, not investigated.
`--turbo=report` on v32nix's cc.cmd: 0 spin cycles skipped, 0 draws dropped, and 13 % slower from the loop-observation overhead: **v32nix idles with WAIT, which was already free, so do not use --turbo for it.**

## Hotspots found (in order of cost)
1. `fprintf(debug/verbose, ...)` in decode/memory/ioports hot paths: a formatted message per instruction written to /dev/null (78 k write syscalls per 300 frames). Fix: `DPRINTF` tests the stream first. 6.4 -> 4.1 s.
2. `memory_chk` did three `sprintf` per memory access (building "RAM "/"read" strings for an error that never happens). Plus double checks in fetch, `ioports_chk` per port access. -> 1.4 s.
3. Makefile had **no -O flag**: -O0 by default (`make` and the bootstrap's build-nogen.sh differ: -O2). -> `-O2 -flto -ffp-contract=off`.
4. GPU: per-pixel float blend with a switch inside, texel fetch through memory_texel, 4 divides per pixel; painted for every frame though pixels are only read by screenshot/save. 9 s -> 0.06 s.
5. CALL: malloc + linear label-list walk + strdup per call for the backtrace; per-instruction calls to watchpoint / srcdebug / addrprofile hooks even when idle; `decode()` wrapper; cycle counter via two port-decoder trips.
Remaining profile (callgrind): main loop ~37 %, decode_process ~28 %, fetch ~15 %: the per-instruction cost is now spread flat.

## Changes
* `DPRINTF` (inc/globals.h) replaces `fprintf(debug|verbose,...)` at 92 sites. memory_chk lazy error text; fast paths in memory_get/set/fmemory_set, fetch_instruction, ioports_get (per-port "readable" cache), cycle counter by pointer.
* main loop: watchpoint / srcdebug / addrprofile only when armed (`wpoint != NULL`, `srcdebug_stepping`, `addrprof_active`); `decode_process` called directly.
* backtrace nodes free-listed; label resolved when `backtrace` prints (only visible difference: a label added by hand between the CALL and `bt` now shows).
* gpu.c: deferred pixel queue (flush on screenshot / save / load / unload / 16384 entries; opaque clear discards the queue), table-driven colour products, a==0 skip, a==1 copy, axis-aligned fast path, direct texel base. Results identical (verified by screenshots on every draw kind, dense screenshots, queue overflow, and eager vs deferred in tests/run_turbo_tests.sh). `--gpu-eager` = old paint-at-once behaviour.
* `--turbo[=report]` (below). Removed the dead `turboflag` stub (an unreachable "30 MHz" cycle count in update_sys.c).
* Makefile: -O2, LTO auto-detect, `-ffp-contract=off`, `make` falls back to `gen/` without flex/bison, debug/asan use -O0/-O1 without LTO, `version` also stamps TODO.md, install layout VS-INST-1 (below).
* tools/v32sim-peek.sh: `V32SIM_FLAGS` env (opt-in extra options). tools/build-nogen.sh: `-ffp-contract=off`.
* New `tests/run_turbo_tests.sh` (45 checks, stock toolchain only; added to `make check`), `tests/turbo/*.c`.
* VS-INST-1 (V32DOS request) applied: `sysinstall` tools -> `$(VROOT)/v32sim/tools` (`VROOT ?= $(PREFIX)/Vircon32/v32tools`), `install` tools -> `$(USRLIB)/tools` (`USRLIB ?= ~/.local/lib/v32sim`); both remove old `share/v32sim`; uninstall targets remove new and old; no `.o` copied. Verified with DESTDIR=/tmp/root and HOME=/tmp/home.
* Version 20261122-dev (inc/version.h, TODO.md via `make version`).

## `--turbo` semantics
Implies `--fast`. (1) Spin-loop fast-forward: when a loop repeats exactly (same target, 16 registers equal, every stored RAM word restored, no port write, no TIM_CycleCounter/RNG read, same frame) whole trips are skipped; cycle counter, instruction count, per-frame instruction/gamepad/card counters advance as if run; the final trips run for real, so frame end state is identical. Off when breakpoints, watchpoints, stepping, profilers, V32TRACE, -v/-D, -w are active. Loops that count/store new values every trip are not skipped. (2) A full pending GPU queue (16384 draws with no clear/screenshot) is dropped, not painted; a later screenshot warns on stderr. Everything observable by the program, counters, text console, traces is unchanged. Verified: identical RAM, `--frame-stats`, exit status, screenshots, 46 gaps tests, on turbo. `--turbo=report` prints skipped cycles/dropped draws.

## Tests run
gaps 46/46, romcheck 28/28, fuzz seeds 1,2,3 (100/300/300 programs) + 3264 port programs all against the stock core: 0 failures; run_turbo_tests 45/45; gaps also under --turbo; byte-compare of screenshots/stats/profile/console/RAM against the 20261109 build on 8 carts, plus 3 scripted debugger sessions; v32nix simtest 14/15 (the 1 failure pre-exists), v32nix `make test` 65/65.
NOT run (no v32cc/v32as/v32io here): batch, src, stack, harness, v32io suites. Run `make check TC=...` on your side.

## Risks
* Deferred painting: anything new that reads the framebuffer must call `gpu_flush()` first. Textures are assumed immutable between draw and flush (loaders flush).
* `-flto` needs a working LTO toolchain (auto-detected; `make LTO=`). `-ffp-contract=off` may change last-bit pixels on FMA hosts (arm64) relative to older builds; x86-64 unchanged.
* Spin skipping is reasoning-heavy code: keep it opt-in; the conditions are in src/turbo.c.
* Behavior of `bt` labels as noted.

## Next steps
Decoded-instruction cache / computed-goto and a hook-free inner loop (est. 1.5-2x CPU), span-copy painting and dead-draw culling (makes turbo's drop exact), PGO target, `--bench`. On v32nix: profile `make test` per test with `--profile-by=function`/`tools/v32sim-addrprof.py` to see whether the kernel or on-system compiler dominates; useful info: which tests spin without WAIT; whether a busy v32dev mailbox makes `memc_frame` (per-frame pread/pwrite) matter (use `--time-breakdown`); typical frames per test.
