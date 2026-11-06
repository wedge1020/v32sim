# v32sim backlog: what the surveys asked of v32sim itself

From the 2026-11-03 consolidation and V32DBG / V32TRACE work, against v32sim 20261103-dev. IDs continue the V32DOS series
(VS-1 .. VS-8 done; VS-9 is the perf-metrics proposal, partly done). Each item names who asked or what found it.

| id | sev | what | asked by / found by |
|---|---|---|---|
| VS-10 | **B** | read `%define var_NAME ADDR` (v32lua globals and chunk locals) as data symbols, as `%define global_NAME` is read | found: `print counter` fails on a v32lua cart (checked) |
| VS-11 | **B** | a v32mouse device (`--v32mouse=PORT`, `mouse move dx dy`, `mouse click left`), as the v32kbd is | v32nix P-I2; v32lua, v32c++ mouse APIs cannot be tested in v32sim except through `v32io-script.py` toggles |
| VS-12 | **B** | one merged oracle runner in `tools/oracle` (BIOS or bare `.vbin`, frame-stamped input, peek/dump, GPU/SPU logs in v32lua's format, traps at addresses, BP watch) | six runners in five projects (CONSOLIDATION.md); v32lua's no longer builds |
| VS-13 | B (part 1 done 20261106: GPU/SPU counters, region table, `stats`, `stats expect`, `tools/v32sim-statdiff.py`) | VS-9 remainder: GPU command / texture / region / SPU counters as `--frame-stats` columns, `--profile-event`, `expect stats`, GPU/SPU traces + replay, `statdiff` | `v32sim-proposal-perf-metrics-20261030.md` (NEStoV32's slow carts) |
| VS-14 | C | locals, what is left: Lua table walking, struct members (when SC-3 lands), `display NAME` that follows the frame, `info globals` (when SC-2 / SL-2 land), mark upvalues (SL-1) | V32DBG section 4 |
| VS-15 | C | read v32as `--symbols` / a resolved `NAME.vbin.sym` (v32as SA-3) for function and ROM-data addresses | marker-mode `.debug` carries no data labels |
| VS-16 | C | a way to name a low RAM word unambiguously: `print [0x75]` is an IOPort (up to 3 hex digits), `[0x00000075]` works; e.g. `[ram:0x75]` | found while testing v32lua globals |
| VS-17 | C | prototypes of v32nix P-E1 (timer interrupt) and P-E2 (v32dev doorbell) behind options | v32nix PROPOSALS, v32nix proposal SN-2 |
| VS-18 | C | V32TRACE: ~~the human report (`.trace.txt`), series, markers, host cost~~ (done 2026-11-04, equal to the recorder's); `input2=` | V32TRACE spec 2.1, GBtoV32 proposal question 2 |

---

## VS-10  Lua data symbols  (B)

`sym_load_asm()` (src/symbols.c) takes only `%define global_NAME`. v32lua writes `%define var_NAME 0x00000075` for every global and
chunk-level local. Read those too, registering `NAME` (and `var_NAME`), with size 1. Three lines; a test in `run_harness_tests.sh`
with a v32lua global. Independent of SL-2.

## VS-11  v32mouse  (B) -- DONE 20261106 (`mouse` command, `--v32mouse`; README "The v32io Mouse")

The protocol (v32io `lib/v32io/README.md`, v32c++'s `v32io-script.py`): X and Y as a trit counter (Left/Right, Up/Down) plus a Gray-coded
pair per axis (X/Y, L/R), at most 5 steps per axis per frame, buttons on Start/A/B. Commands `mouse move DX DY`, `mouse down|up|click
BUTTON`, a host source like `--kbd-input` later; check with v32io's mouse demo through the real `mouse.h`, as the keyboard is checked
with `t_kbd`.

## VS-12  one oracle runner  (B)

Merge `refrun`, `v32run`, `v32prof`/`v32shot`/`v32peek` and the features of v32lua's runner and v32cc's `vrun` into one program with
subcommands or options, keep the old names as thin wrappers (scripts in v32c++, v32nix, V32DOS, v32cc, GBtoV32 call them), and give it
the GPU/SPU logs VS-13's traces need (one format, v32lua's, so `render.py` and `spu_render.py` keep working). The ComputerSoftware
proposal SE-3 asks upstream for the same thing; whichever comes first.

## VS-13 .. VS-18

As in the table; VS-13's design is the perf-metrics proposal (sections 4.1-4.6, phases 1-4).
