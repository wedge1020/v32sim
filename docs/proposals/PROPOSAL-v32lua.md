# Proposal to v32lua: variables v32sim cannot see yet, and the headless runner

From v32sim 20261103-dev, against v32lua 20261030-dev, v32as 20261101-dev and the current ComputerSoftware. Written 2026-11-04.
What works and is relied on: `-g` writes `;@lang`, `;@src`, `;@var NAME loc=BP+-n type=lua [boxed]`, `;@scope`, and `;@fn` after
each body; with `v32as -g` v32sim's new `locals` shows numbers, strings, nil/true/false, tables and functions, and reads a `boxed`
local through its box (checked in v32sim's `tests/run_harness_tests.sh` with `tests/examples/locals.lua`). `make tests` runs on v32sim.

| id | sev | what | v32sim stopgap today |
|---|---|---|---|
| SL-1 | **B** | upvalues have no `;@var` inside the closure | `locals` in a closure shows only its own locals |
| SL-2 | **B** | chunk-level `local`s (RAM words `var_NAME`) have no `;@var`; globals are `%define var_NAME 0x..` only | none: `print counter` fails ("unknown symbol") |
| SL-3 | **B** | `tools/headless/build.sh` compiles `SimplifiedEmulator/Emulator`, which current ComputerSoftware no longer has | v32sim `tools/oracle/` builds the stock-core runners from `DesktopEmulator/ConsoleLogic` |
| SL-4 | C | no way to tell a debugger which globals the program has, with their types | v32sim backlog VS-10 (read `%define var_`) |

---

## SL-1  `;@var` for upvalues  (B)

**What.** `register_upvalue()` (src/context.c) makes an upvalue a hidden trailing parameter at `[BP+N]`, always boxed. That is
exactly what a `;@var` can describe, but none is written, so a stop inside a closure cannot show `n` in

```lua
local function counter(start) local n = start; return function() n = n + 1; return n end end
```

**Sketch.** In the closure body, after the prologue: `;@var n loc=BP+N type=lua boxed upvalue` (the extra word is ignored by v32as
and v32sim today; v32sim could mark it in the listing). v32sim already follows `boxed` through the box.

**If declined.** Upvalues are visible only in the function that declared them.

## SL-2  chunk-level locals and globals  (B)

**What.** `doc/HINTS.md`: "a chunk-level `local` lives in a RAM word (`var_NAME`), not a frame slot, and has no `;@var` yet".
Globals are `%define var_NAME 0x00000075` lines; v32sim reads only `%define global_NAME` (v32cc's spelling), so `print counter`
and `print var_counter` both fail on

```lua
counter = 5
local top = 7
function game_loop() counter = counter + top end
```

(checked: `[ERROR] unknown symbol 'counter'`).

**Sketch.** At the top of the program, after `;@lang lua`: `;@var top loc=@0x76 type=lua` for each chunk-level local and, for
globals, either the same with a `global` flag or nothing (v32sim will read `%define var_NAME`: backlog VS-10). v32sim resolves
`loc=@ADDR` already; a symbol `loc=var_top` would need v32as to resolve it (v32as proposal SA-2).

**If declined.** v32sim reads the `%define var_` lines for globals; chunk-level locals stay invisible to `locals`.

## SL-3  the headless runner no longer builds  (B)

**What.** `tools/headless/build.sh` uses `$CS/SimplifiedEmulator/Emulator/VirconCPU.cpp` etc. The ComputerSoftware supplied
2026-11-03 has no `SimplifiedEmulator` folder (it has `DesktopEmulator/ConsoleLogic`). GBtoV32's `tools/testing-harness/test_v32.py`
also names this runner ("v32run -- the official Vircon32 CPU core (tools/headless in the v32lua repo)").

**Sketch.** Either port `v32run.cpp` to ConsoleLogic (the v32sim oracle tools show the stubs: video/audio callbacks), or move its
distinctive features into one shared runner in v32sim `tools/oracle` (GPU/SPU logs in the format `render.py` / `spu_render.py`
read, BP watch `-B`, traps at addresses, the GPU-dropped count, the CFI host choice) and keep `runlua`, `render.py`,
`spu_render.py`, `unittest.py` working against it. v32sim's backlog VS-12 is that runner; the log format would be v32lua's.

**If declined.** Pin an old ComputerSoftware for `tools/headless`.

## SL-4  (C)

Covered by SL-2 and v32sim VS-10; listed so the expected-output tests (`number_*`, `string_*` globals scraped by name) could, later,
read values through v32sim's typed `print` instead of formats that match v32sim's `%.4f` output by convention.

## Questions

1. Is `proposals/V32DBG-SPEC.md` (named in v32opt's `doc/README-v32srcmark.md`) in the v32lua repository? It was not in the archive.
2. For SL-2: should globals be described (they can be reassigned to any type, so `type=lua` is all there is), or is
   `%define var_NAME` enough for tools?
