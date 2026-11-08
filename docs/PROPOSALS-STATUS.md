# Proposals across the Vircon32 projects: status (2026-11-08; rows checked 2026-11-03 unless they say a later date)

Every proposal, request list and spec found in v32sim, v32cxx, v32nix (20261102-dev), V32DOS, v32cc (20261102-dev), v32as
(20261101-dev), v32opt (20261030-dev), v32lua (20261030-dev), v32io (20261014-dev), GBtoV32 (20261101-dev) and the current
ComputerSoftware, checked against those versions. "Checked" means run here; "documented" means the answering project says so
and the text was read, not exercised. Request IDs collide between documents (three different `VS-` series, two `VC-`, two
`VA-`): always quote the document with the ID.

## Aimed at v32sim

| proposal | item | status |
|---|---|---|
| V32DOS `docs/V32SIM-PROPOSAL.md` | VS-1 symbols, VS-2 `run N`, VS-3 `until`, VS-4 `--batch`, VS-5 `expect` + exit codes, VS-6 `console FILE`/`--attrs`, VS-7 `--profile-by=function`, VS-8 signed `watch/d` | **all done** (v32sim 20261025+; checked by `tests/run_batch_tests.sh`, 25/25). V32DOS's `docs/PROPOSAL-STATUS.md` and the proposal's header still say "outstanding": stale there |
| `v32sim-proposal-perf-metrics-20261030.md` (VS-9) | 4.1 per-frame counters: **CPU cycles and GPU pixels per frame, CSV + summary done** (`--frame-stats`, equal to v32prof on 400/400 frames, checked). GPU command/texture/region counts, SPU counters, bus counters, p50/p95/p99, stable names, JSON Lines, `expect stats NAME.AGG`, `stats save`, statdiff ratio sort: **done** (20261106). 4.2 `--profile-event`, 4.3 `--gpu-trace` / `--spu-trace` + `--trace-dump` + `v32gpureplay`, 4.4 `--time-breakdown` / `--frame-log`: **done** (20261106). Open: SPU channels-playing and memory-card counters |
| | 4.2 `--profile-event`, 4.3 GPU/SPU traces + `gpureplay`, 4.4 `--time-breakdown`/`--frame-log`, 4.5 `expect stats`, 4.6 `statdiff` | open. Note: v32lua's `tools/headless/v32run` already logs GPU (`-g`) and SPU (`-s`) writes and rebuilds frames (`render.py`) and audio (`spu_render.py`) from them; that is most of 4.3's format, on an oracle |
| V32TRACE spec 2.1 (`docs/V32TRACE-SPEC.md`, shared with GBtoV32 and NEStoV32) | recorder: build files, traps, events, restarts, `traps.txt`, `blocks.txt` | **done** (GBtoV32's `make test-trace`, 18 checks, all pass against this v32sim, checked) |
| | 2.1 `segstart=` / `input=` per hit; replay with `V32_INPUT` | **done today** (`--input`, `V32_INPUT`; round trip checked on GBtoV32's gap ROM, and the trap frame agrees with the stock console logic) |
| | series, markers, the human report (`.trace.txt`), host cost | **done 2026-11-04** (equal to the patched DesktopEmulator's files, `run_harness_tests.sh`) |
| | guest-button map in the meta | open (`input2=` done 20261106) (the spec's own "later" list includes "`v32sim` writing `input=` too", now done) |
| V32DBG (`V32DBG-SPEC.md`, hosted in v32sim 2026-11-08; `V32DBG-STATUS.md` checks v32sim against it) | sections 1-3, 5 | done earlier |
| | section 4, locals and values | **done today** for what the producers write; open items are mostly the producers' |
| v32nix `docs/PROPOSALS.md` | P-E6 headless runner with scripted v32io + v32dev serial | **met by v32sim** (`--batch`, `kbd`, `--kbd-input`, v32dev mailbox with a real `v32dev.py serve`; v32nix's own `tools/simtest.sh` and `docs/STORAGE.md` use it) |
| | P-I2 a `v32mouse` event queue | **done in v32sim (20261106, VS-11)**: `--v32mouse[=PORT]`, the `mouse` command, counters and Gray/trit encoding as the v32io protocol (`src/v32mouse.c`) |
| | P-E1 optional timer interrupt ("v32irq") | **declined by v32nix 2026-11-06** (D30/D37 preemption works on stock hardware, the kernel is not preemptible by design, safe points work on real hardware, sub-frame timers are not wanted); deferred indefinitely, reopen only for code with no safe points |
| | (VS-19) native v32dev host | **done in v32sim (20261106)**: `--v32dev-host=N=SPEC`, storage + serial, behaviour of `v32dev.py`; with `--v32dev-doorbell` a ring is answered at T = 0 |
| | P-E2 v32dev completion doorbell | **done in v32sim (20261106)**: `--v32dev-doorbell[=MS]`, interface by v32nix (header word 8 bit 1, counters at header words 10..13, c2h_head store = the bell); the kernel side (wait loop checking the counter) is v32nix's |
| | P-E5 the clock ports on both emulators | v32sim: leap years and `--fixed-clock` (checked by the differential suites) |
| v32nix `docs/HANDOFF.md` | "`until` patterns are not full regex", "a user program that executes `HLT` ends the emulator silently" | still true (documented in TESTING.md "Gotchas") |

## Aimed at the compilers and the assembler (documented by their owners)

| proposal | items | status |
|---|---|---|
| V32DOS `V32CC-PROPOSAL.md` | VC-1 .. VC-10 | all answered (v32cc `docs/TOOLS.md` table) |
| V32DOS `V32AS-PROPOSAL.md` | VA-1 .. VA-5, and the `--symbols-all` question about `__data_base+N` | all answered; `--symbols-all` now lists `DATASYM+N` as `N,DATASYM+N,V` (v32as README, round 9). V32DOS's status file predates it |
| v32nix `V32CC-PROPOSAL.md` | VC-1 .. VC-13 | answered (v32cc `docs/TOOLS.md`, "VC-1 .. VC-13") |
| v32nix `V32CC-PROPOSAL-STAGES.md` | VS-1 .. VS-6 (a *third* VS series) | done (v32cc `docs/STAGES.md`) |
| v32nix `V32CC-PROPOSAL-MEMORY.md` | VM-1 .. VM-9 | done (v32cc `docs/MEMORY.md`) |
| v32nix `V32CC-PROPOSAL-CURSES.md` | VC-1 .. VC-4 (curses series) | VC-1 / VC-3 done 2026-11-02 (v32cc `NOTES.md`, `docs/POSIX.md`); VC-2, VC-4 not confirmed in what I read |
| v32nix `V32AS-PROPOSAL.md` | VA-1 .. VA-6 | done (v32as README, round 9) |
| v32nix `PROPOSALS.md`, v32c++ section | P-C1, C3, C4, C7, C8, C9, C10, the `sleep` rename | listed as working in v32c++'s AGENTS.md (`--warn-error`, `__builtin_frame_address`, unsigned operators, `{&var}` operands, `--yield-points`, `__attribute__((rom))`, `--rename`); P-C2 `naked` and P-C5 `--origin` declined; P-C6 deferred |
| | P-B1 .. P-B4 (BIOS), P-F1 (formats) | P-B1 proved in v32nix's own BIOS; the V32BIOS side, P-B2, P-B3 and BOOT.md's type-3 line are not confirmed |

## Aimed at v32opt

| source | item | status |
|---|---|---|
| v32opt `doc/hints.md` "Reserved for later" | `;@var` / `;@scope` after promotion | **open, and it matters now**: promote-regs leaves stale `loc=` (repro `docs/repro/v32opt-promote-var.c`, checked). `noreturn` / `pure` hints: open |
| v32opt `ISSUES` | [limit]/[gap] items of the passes (Lua loop shapes, liveness R0..R10, ...) | open by design; none is a correctness bug in what I ran |

## Upstream (ComputerSoftware)

The copy supplied today has the same sources as the 20261101 V32DOS bootstrap archive's (only images, Windows runtime DLLs and the
`ExtendedDevTools` submodule pointers for v32lua, v32c++ and v32opt were added; the submodule folders are empty). Its
`SimplifiedEmulator` folder is gone, which **breaks v32lua's `tools/headless/build.sh`** (it compiles the CPU core from
`SimplifiedEmulator/Emulator`); the v32sim oracle tools build from `DesktopEmulator/ConsoleLogic` and are unaffected.
The V32TRACE recorder is a patch kept in GBtoV32 (`emulator/emulator-v32trace.patch`, v2 + 2.2), and stays one: it is not meant to
go upstream (answer of 2026-11-04). v32sim, meant as an alternative to the stock DesktopEmulator, writes the same files.
