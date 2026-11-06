# TODO

Status as of version `20261106-dev`.

## DONE (removed from this list)

  * **20261106 (central maintenance), VS-13 part 2 + VS-18**: `perfstats.c` is table-driven with stable dotted names (`gpu.draws`, `spu.plays`,
    `bus.movs_words`, `cpu.instructions`, `cpu.cycles`, `gpu.pixels`...) and the per-frame history (`stats on|off`): aggregates `.last .mean .min
    .max .p50 .p95 .p99 .total` for every counter, `expect stats gpu.draws.p95 <= 600` (rewritten to `stats expect`; fails like `expect`, exit 1
    in batch), `stats save FILE [json]`, `--stats-format=json` (JSON Lines + summary line), `--stats`/`--stats-skip`/`--stats-top` synonyms.
    New counters: scaled / rotated draws, region definition writes, multiply-colour and blending changes, SPU assignments and parameter writes,
    MOVS / SETS words, gamepad reads, instructions. Summary: mean p50 p95 p99 max total. `v32sim-statdiff.py`: `--sort ratio`, reads CSV and
    JSON Lines, stable names. VS-18: V32TRACE `input2=` / `V32_INPUT2=` (only when pad 2 was used), `--input2=SCRIPT|@FILE`. Left of VS-13:
    `--profile-event`, GPU/SPU traces + replay, `--time-breakdown`, `--frame-log`, channels-playing gauge, memory-card access counter.

  * **20261106 (central maintenance), VS-13 part 1, perf counters**: `src/perfstats.c`: GPU (draws, dropped, clears, texture / region switches,
    distinct regions) and SPU (commands, plays) counters per frame and the draws / pixels / drops per texture:region over the run;
    `--frame-stats` CSV gets the counters as columns after the original six, its summary the counters and `--frame-stats-top=N` regions
    (the skip applies); `stats` / `stats regions` / `stats reset` / `stats expect NAME OP VALUE` in a session; `tools/v32sim-statdiff.py`
    (compare two CSVs, `--fail-over`, `--json`). Checked on overdraw.c (12 draws, 3 dropped, agrees with v32orun) and on NEStoV32's translated
    SMB1 (1,110 draws, 790 region switches, 20 % GPU, 55 % CPU per frame). The rest of VS-13 is listed in the entry above.

  * **20261106 (central maintenance), screenshot highlight**: `highlight TEX:REGION [COLOR]` (`bios:N`, `clear`, `on|off`, list) outlines the
    quads drawn from a region in `screenshot` PNGs only (`src/gpu.c`: outline list, painted onto a copy). Test: harness "screenshot highlight".
    Also spelled `boundbox` (the name in the old `screenshot` help). `screengrid` (grid over screenshots: on/off, spacing, color, alpha; 40x40
    default) and `noextra` / `screenshot --plain` (no grid, no outlines). Then (same day): `--highlight=T:R[:COLOR]`, `--screengrid[=SPEC]`, `--noextra`, `--screenshot-at=FRAMES` (+ `--screenshot-file=PATTERN`; cart frames as for `--input`;
    `src/shots.c`), and coordinate labels on the grid (3x5 digits at 2x, `screengrid labels on|off`; default grid green at 55 %). Tests: harness "screenshot highlight".

  * **20261106 (central maintenance), VS-12 leftover, part 1**: `v32prof` and `v32peek` are `v32orun --compat=prof|peek` (scripted input,
    segments, per-function cycles, RAM dump; output byte-identical to the old programs on the stock-chain example: report, CSV and RAM dump);
    `v32prof.cpp`/`v32peek.cpp` deleted, `build.sh` writes the two wrappers. Part 2: `v32shot` too (`--compat=shot`, software rasterizer ported as is; PPMs identical to the old program's on hello_text frames 3 10 40 300). `make clean` removes any `../v32sim-project-YYYYMMDD.zip`.

  * **20261106 (central maintenance), VS-11 v32mouse**: `src/v32mouse.c`, `--v32mouse[=PORT]`, the `mouse` command (`attach detach move
    click|down|up wait clear`, status); counters and Gray/trit encoding as the v32io protocol, <=5 steps per axis per frame, a 4096-action
    queue delivered one per frame; savestate restore resyncs. Tests: `tests/vectors/mouse_{move,buttons}.vec` through
    `tests/run_mouse_vectors.py` + `tests/examples/pad_bits.c`, `--v32mouse`, same-port refusal (harness section "v32mouse"). `make archive` now writes `../v32sim-project-YYYYMMDD.zip`.

  * **20261105 (central maintenance), VS-12 one oracle runner**: `tools/oracle/orun.cpp` -> `v32orun`, on `V32Console`: a bare `.vbin`
    on logging stand-ins (v32lua's runner: `-g -s -d -D -p -t -B -P -m -G -c -q`, `V32_SPU_STATE`, `V32_CFI`, `V32_FIXED_TIME`, byte-identical
    output), a real BIOS + cart with the real GPU/SPU behind logging proxies (`gpu_dropped` from the real GPU), or `--as-bios`;
    `-b` per-frame cycles (v32bench); `refrun`/`v32run`/`vrun` are `--compat` wrappers. v32lua (`runlua`, `trap`, `unittest.py`, Windows
    `testing_harness`) and v32cc (`make vrun`) lost their own runners. Tests: `run_harness_tests.sh` section "v32orun", gaps 46/46, romcheck 28/28.

  * **20261105 (central maintenance)**: VS-10 `%define var_NAME` (v32lua) is read, and `var_NAME` resolves; VS-15 `NAME.vbin.sym` is read before
    `NAME.asm.sym`; `#` lines are skipped in the `.debug`/`.sym` files (`.vars` ignored all but `var,` rows already); VS-14 (structs):
    `struct:TAG:WORDS{x:int@0;...}` types are parsed and printed as `{x = 3, y = 5}` (upvalue and global records still open);
    VS-16 `ram:0x75` addresses a low RAM word (`[ram:0x75]`), and a cartridge argument `-` runs the BIOS alone;
    `make version VERSION=...` stamps, `install`/`uninstall`/`sysinstall`/`sysuninstall` (tools go to share/v32sim);
    `v32sim-peek.sh -M CARD`; `v32sim-selfcheck.sh -M` blank card; V32TRACE spec copy updated.

  * **V32TRACE (spec 2)** - `--v32trace=PREFIX`, `--v32trace-meta`, `--v32trace-labels`, `--v32trace-norestart`: traps, events, restart after a trap,
    `traps.txt` and `blocks.txt` (`src/v32trace.c`); since 2026-11-04 also the human report `PREFIX.trace.txt`, `series`, markers (`mark`), host cost,
    compared with the patched DesktopEmulator in `run_harness_tests.sh` (needs `GBTOV32_TREE` and `tools/oracle/v32trace-ref`). `input2=` done (20261106).

  * **Command parsing** - regex front end replaced by `lexer.l`, `parser.y`
    and `command.c`.
  * **Version** - `VERSION` in `inc/version.h`; `--version`/`-V`.
  * **`ignore`** - skips the current instruction.
  * **Watchpoints** - registers, IOPorts and memory words, compared with a
    number, a register, an IOPort or a memory word; edge-triggered, checked
    after every instruction and on state changes.
  * **GPU framebuffer and `screenshot`** - software renderer, PNG writer.
  * **SPU and VSND** - channel/sound model and loading of VSND data; no audio.
  * **MEMC** - header and size validated; changes written at every frame end;
    live **v32dev mailbox** (heartbeat, host area read back each frame,
    commit-ordered writes); `mailbox` command.
  * **v32io keyboard** - `v32kbd` on a gamepad port, `kbd` command,
    `--v32kbd`, `--kbd-input` (terminal / pipe / FIFO / file).
  * **Unattended runs** - `--fast`, `--frame-us`, `--max-frames`, `-x`.
  * **`load bios:PATH`**, **`replace`** refreshes IR/IV, **float disassembly**
    prints shortest round-trip values.
  * **Text console** - `console` / `--console` capture BIOS-font text, both the
    64x18 BIOS layout and v32c++'s 80x24 terminal; `--tty` is a full-screen
    interactive mode with the v32kbd. Colors (16/256/24 bit) and a
    recolorable cursor: `--console-color`, `--cursor-color`, `console color|cursor`.
  * **GPU** - scale/angle ports were set from the register's bits converted as
    an integer; zoomed and rotated draws were wrong for real programs.
  * **Tests** - `tests/run_tests.sh` (v32dev suite, v32kbd, persistence).
  * **Scripting (V32DOS proposal VS-1..VS-8)** - symbols in `print`/`display`/`watch`/
    `expect`/`x`, `run N`, `until`, `--batch`, `expect`, exit statuses, `quit N`,
    `console FILE`/`--attrs`, `--profile-by=function`, signed `watch/d`;
    `tests/run_batch_tests.sh`.
  * **TIM leap years**.
  * **Debug file location** - directory of the V32 file, current directory,
    `obj/`, `../obj/`, and `--debug-dir`; loaded automatically (`-n` disables).
  * **Assembly and C overlay** - `--source`, `set source`, `set mode`.
  * **Source-level debugging (V32DBG sections 1-3, 5)** - `mode src` (and `lua`/`C` as aliases) for
    the overlay in any language; `step`/`next` by line in the source modes, `finish`, `until [LOC]`, `break`/`tbreak
    FILE:LINE`, `list`, `bt` with function names and lines, `frame`/`up`/`down`; `-` entries for
    code with no source; `tests/run_src_tests.sh`.
  * **SIGSEGV** (and SIGBUS/SIGFPE/SIGABRT) state dump.
  * **Save / restore**.
  * **Live assembly** - `asm` (flex/bison, verified against the whole BIOS).
  * **CPU audit** against DesktopEmulator: `SETS` stored the register number
    instead of the register's value, `SHL` used an arithmetic right shift,
    `MOVS`/`SETS`/`CMPS` counted CR differently, `ACOS`/`ATAN2`/`LOG`/`POW`
    did not raise their errors, the error-handler reset SP/BP to 0 and R1
    held the instruction's own address, and `print/D` showed float immediates
    converted instead of reinterpreted.

  * **Consolidation (2026-11-03)** - oracle tools, harness scripts, AGENTS/CLAUDE and docs from v32cxx, v32nix and V32DOS moved here
    (`docs/CONSOLIDATION.md`); `--frame-stats` (first slice of VS-9: per-frame CPU/GPU load, equal to v32prof's);
    `%define global_NAME` symbol fallback fixed (it never parsed a newline-terminated line). V32DOS's VS-1..VS-8 are done;
    its `docs/PROPOSAL-STATUS.md` still says outstanding.

  * **Locals (V32DBG section 4, 2026-11-03)** - `locals [NAME]` / `info locals` from `NAME.vbin.vars`, locals before globals in
    `print`/`x`/`watch`/`expect`; C and Lua types, boxed Lua locals; `tests/run_harness_tests.sh` checks C and Lua.
  * **`;@src -` ranges** - v32as (round 11) writes `ADDRESS,-,0` for each; v32sim already understood it (checked 2026-11-03).
  * **V32TRACE 2.1** - `segstart=` / `input=` on every TRAP/EVENT line; `--input=SCRIPT` (`V32_INPUT` syntax) replays them.
  * **Fixed** - `--dump=N@NAME` read a name made of hex digits (`ab`, `f`, `cafe`) as an address.

## STILL LACKING


  * **VS-9 beyond `--frame-stats`** - GPU/SPU counters, texture/region switches, `--profile-event`, `expect stats`, traces + replay.
  * **Fold `sysprof` and `v32trace-ref` into `v32orun`** (`sysprof` is v32nix-specific; `v32trace-ref` needs the patched checkout). `v32prof`/`v32peek`/`v32shot` are done.

  * **Locals, what is left (V32DBG section 4)** - Lua table walking (`locals` shows `table: 0xADDR`), struct members (needs
    member names in `type=`), globals' `type=` (`loc=@ADDR` from v32cc), upvalues in closures (v32lua), `display NAME` that
    follows the frame (today it keeps the address the name had).
  * **Symbols** - only variables with a numeric address (v32cc globals) are data
    symbols; function-local variables and struct members have no names, and a
    `.asm.sym` entry without a size counts as one word.
  * **`until`** - needs the screen to change; it matches text, not colours.
  * **Audio** - the SPU has no output; a WAV capture of the mixed channels
    would be a natural first step.
  * **Debug-file loader** - `load_labels()` is hand-rolled; it could be moved
    onto flex.
  * **Entry point** - should the default start be the cartridge
    (`0x20000000`) rather than the BIOS?
  * **Text console** - only BIOS-texture draws are captured, as characters:
    alpha, blinking and the guest's clear color are ignored; the cursor is only
    what the guest draws (recolorable); text drawn with a program's own font is
    not captured.
  * **v32mouse** - the protocol is not emulated (only the keyboard).
  * **Rendering fidelity** - the software renderer's rasterization of rotated and
    scaled quads has not been compared with the OpenGL output (V32SIMGAPS G14).
