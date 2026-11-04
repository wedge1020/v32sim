# Testing Vircon32 software with v32sim

Patterns that v32cxx, v32nix and V32DOS each built for themselves, collected so a new project can start from them.
Each recipe says which tool does the work (`docs/TOOLS.md`) and where a working example lives.

## 1. Self-checking cartridges (the unit test)

A test is a cartridge with `int test_errors = -1;` that sets it to 0 when every check passed and then halts (`HLT` or
returning from `main`, depending on the toolchain). Starting at -1 means a crash, a hang, or a cartridge that never
reaches its end fails too.

```
tools/v32mkcart.sh -o out tests/t_foo.c            # cartridge + .asm.sym + .vbin.debug in out/
tools/v32sim-selfcheck.sh -B StandardBios.v32 out/t_foo.v32
tools/v32sim-selfcheck.sh --oracle out/t_foo.v32   # the same on the stock console logic, as a second opinion
```

`-F FRAMES` bounds the run (default 1200: a cartridge that needs the BIOS logo, about 250 frames, still fits), `-M` gives
it a fresh memory card, a `CART.input` beside the cartridge is played as input. Exit status 0 = all passed.
Raw form, if you want your own loop: `v32sim --batch -r --fast -x --max-frames=N --dump=1@test_errors cart.v32`; the status
is 0 (halted), 2 (frame limit: failed), 3 (system error); the last stdout line is the value.

Example: `tests/examples/hello_text.c`. Big collections of such tests: v32cxx `tests/*sample.cpp` (+ `check.sh`), v32nix `tests/t_*.c`.

## 2. Command scripts (the integration test)

`v32sim --batch --fast -x -B BIOS cart.v32 < script.cmd`. In batch mode stdout holds only what the script asked for, a failed
`expect` or a timed-out `until` ends the run with status 1. A script is the debugger's own commands, one per line:

```
kbd attach 2                                    # a v32kbd on gamepad port 2 (v32io keyboard)
until "^$$" timeout 1500                        # wait for the shell's prompt: a row that is just "$"
kbd type "ls | wc\n"
until "^9 36 110$" timeout 600
expect console "9 36 110"
expect counter == 5                             # symbols, registers, IOPorts and [memory] all work
quit
```

`until` waits on the *screen* (BIOS-font text, 64x18 or v32c++'s 80x24), not on a frame count, so a script does not break
when the program gets slower. Every `*.cmd` in v32nix `tests/sim/` is a worked example (boot, compile on the system, type
at a shell, curses); v32nix's `tools/simtest.sh` is the loop around them.

## 3. Typed sessions with golden screens (the regression test)

For programs with a prompt (a shell, a DOS): write what a user types in `sessionN.txt`, let
`tools/v32sim-session.py` turn it into a script that types each line, waits for the prompt, and dumps the screen;
compare with the screen you accepted earlier:

```
tools/v32sim-golden.sh -u --cart cart.v32 --bios StandardBios.v32 --sessions tests --golden tests/golden \
        --prompt-symbol shell_prompt  -- --symbol-file=cart.asm.sym         # -u writes the golden files
tools/v32sim-golden.sh      --cart ... (same)                                # compares; exit 1 on any difference
```

Session syntax (see the header of `tools/v32sim-session.py`): `command` types it and waits, `{TEXT}command` waits for TEXT
instead of the prompt, `=TEXT` checks the screen, `@key ctrl+c`, `@hold KEY FRAMES`, `@run N`, `@console`. "The prompt" is
either a code symbol the program runs each time it prints its prompt (`--prompt-symbol`: exact, no screen scraping) or a
screen pattern (`--prompt-text '^MEM:\>$'`). `--mask FILE` (a sed script) hides what changes every build (sizes, dates).
`--prepare PROG` builds the memory card for each session. Worked example: V32DOS `tests/` (11 sessions, with and without
V32BIOS); the framework test is `tests/examples/sessions/`.

Reproducibility: add `--fixed-clock --seed=1 --fixed-cycles` after `--` so dates, random numbers and cycle-counter seeding
are the same on every run.

A cartridge that halts ends the simulator run, so a session can only look at a program that stays alive (the example is
built with `-DKEEP_RUNNING` for that).

## 4. Input

| from | to v32sim |
|---|---|
| a v32prof / v32peek / v32shot script (`320 press 1 left`) | `tools/vprof2cmd.py SCRIPT [--frames N]`: toggles `gamepad N CONTROL` between `run N` steps; checked against v32peek (`run_harness_tests.sh`) |
| keyboard and mouse actions (`kbd 1 type Hi!`) | `tools/v32io-script.py actions.txt` makes a v32prof-style script; or use `kbd type` / `kbd key` directly, `--kbd-input=stdin\|FIFO\|file` |
| a V32TRACE `input=` or a `V32_INPUT` script (`A@40-44,START@100`) | `--input=SCRIPT` (cartridge frames: 0 = the first frame after the BIOS logo); `--input-port=N` |
| an interactive terminal | `--tty` (text console full screen, keys to a v32kbd, Ctrl-] returns to the debugger) |

## 4b. Many cartridges at once

`tools/v32sim-corpus.py -B StandardBios.v32 -F 1800 --input fixed --shots shots -o results.csv carts/*.v32` runs each cart, says how it
ended (OK, HALT, CPU_ERROR, TRAP for a translated cart with a `.v32trace.meta`, TIMEOUT), its CPU and GPU load (mean, p95, max,
overrun frames) and keeps its last screen. `--input fixed` gives every cart the same presses (Start, A, then the d-pad), so loads
compare. From GBtoV32's Homebrew Hub harness, without the Game Boy parts.

## 5. Host devices

`tests/v32sim_runner.sh` makes v32sim a drop-in for the v32io project's `dev_rom_runner`, so its `dev_rom_test.py` (41 checks:
storage, serial, host crash and recovery) runs against v32sim unchanged. For tests that must act on the host between two
commands (stop `v32dev.py serve`, serve another image, start again), see V32DOS `tests/dev.py`: v32sim at 2.5 ms per frame
as the emulator, `v32dev.py serve` as a separate process on the card *file*.

## 6. Differential tests (is v32sim right?)

`tests/run_gaps.sh TOOLS [filter]` runs each `tests/gaps/*.asm` on v32sim and on `refrun` and requires identical RAM dumps;
`run_fuzz.sh` does it with generated programs; `run_romcheck.sh` for ROM validation. Add a case when you find a divergence:
a short `.asm`, the `;@ frames=N dump=...` header, and it fails until v32sim is fixed.

## 7. What exit statuses mean

`0` success · `1` an `expect` failed or an `until` timed out · `2` frame limit reached (`-x`) · `3` a system error (`-x`) ·
`4` a command could not be parsed (`--batch`) · `quit N` overrides all. Without `-x` a frame limit or a system error exits 0.
Always use `-x` in tests.

## Gotchas

- The standard BIOS shows its logo for about 250 frames before the cartridge starts: count from there, or use
  `--frame-stats-skip=250`, or wait with `until`.
- `v32sim` finds `NAME.asm.sym`, `NAME.vbin.debug`, `NAME.asm` next to the `.v32`; `-n` turns all that off (so `--dump=1@x`
  fails with it).
- A program that ends with `HLT` ends the run: later commands in the script never execute. Put `x`/`print`
  before it, use `--dump`, or keep the program in a loop.
- `until` patterns are not full regular expressions (`^`, `$` anchors only).
