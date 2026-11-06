# Tools: which one answers which question

v32sim is the front end; everything else in `tools/` either prepares something for it or asks the stock console
logic the same question as a second opinion. Every tool is also usable alone.

## Two kinds of engine

| | v32sim (`bin/v32sim`) | the oracle tools (`tools/oracle`) |
|---|---|---|
| what runs | its own CPU, memory, GPU and SPU models | the DesktopEmulator's `ConsoleLogic`, compiled unchanged, video/audio stubbed |
| strength | interactive, scriptable (`--batch`), breakpoints, watchpoints, source-level debugging, keyboard, memory card + v32dev host, text console, screenshots | it **is** the reference: what the console does, quirks included |
| use it to | debug, drive typed sessions, run test suites, profile | decide who is right when v32sim and a program disagree; differential tests |

v32sim is verified against the oracle (`tests/run_gaps.sh`, `run_fuzz.sh`, `run_romcheck.sh`, and `run_harness_tests.sh` for
the harness). When a cartridge behaves differently in v32sim than on the real emulator, run it on `v32run` or `refrun`
first: if they agree with the emulator, it is a v32sim bug (see `docs/V32SIMGAPS.md`, then report it).

## I want to...

| question | tool |
|---|---|
| build a cartridge with debug files from C or assembly | `tools/v32mkcart.sh` |
| step through it, set breakpoints, watch a variable | `v32sim cart.v32` (README "The Debugger", "Source-Level Debugging") |
| see the local variables of a frame | `locals` / `info locals` (needs `NAME.vbin.vars`: v32cc -g or v32lua -g, then v32as -g) |
| run a whole set of carts: which crash, trap, overrun | `tools/v32sim-corpus.py` (CSV, screenshots, `--input fixed` for fair load numbers) |
| know if a self-checking cartridge passes | `tools/v32sim-selfcheck.sh CART...` (`--oracle` for the stock core) |
| run it unattended and get an exit status | `v32sim --batch --fast -x --max-frames=N` ([TESTING.md](TESTING.md)) |
| read a variable / buffer after N frames | `tools/v32sim-peek.sh` (or `--dump=N@SYMBOL`); `v32peek` on the oracle |
| type at a shell/DOS/monitor and check the screen | `tools/v32sim-session.py` + `tools/v32sim-golden.sh`, or a hand-written `.cmd` ([TESTING.md](TESTING.md)) |
| replay gamepad / keyboard / mouse input from a script | `--input=A@40-44,...` (cart frames, V32_INPUT syntax), `tools/vprof2cmd.py` (v32prof scripts), `tools/v32io-script.py` (actions -> script), `kbd` and `mouse` commands |
| see CPU and GPU load per frame, p95, frames over budget | `--frame-stats=FILE` ([PROFILING.md](PROFILING.md)); `v32prof` on the oracle |
| find the hot functions | `--profile-by=function --profile-top=N`; `tools/v32sim-addrprof.py` across kernel / BIOS / loaded modules |
| profile an OS: kernel plus programs swapped into a window | `tools/oracle` `sysprof` |
| screenshot at chosen frames | `screenshot FILE` (PNG) in a session; `v32shot` (PPM) on the oracle |
| trace the traps of a translated cartridge | `--v32trace=PREFIX` (`docs/V32TRACE-SPEC.md`, 2.1); replay a hit with `--input` and its `input=`, a session with `--input=@PREFIX.trace.txt` |
| test v32dev / a host disk against the real `v32dev.py serve` | `tests/run_tests.sh`, `tests/v32sim_runner.sh` (a `dev_rom_runner` look-alike) |
| build v32sim without flex/bison | `tools/build-nogen.sh` |
| build the oracle tools | `tools/oracle/build.sh COMPUTERSOFTWARE [OUT]` |

## The oracle tools

All take a BIOS and a cartridge and run the console logic headless. Built by `tools/oracle/build.sh` (g++ with C++17;
nothing else: no SDL, OpenGL or OpenAL).

| tool | from | does |
|---|---|---|
| `v32orun PROG.vbin [options]` / `v32orun --bios B --cart C [options]` | v32sim; v32lua's `v32run`, v32bench, v32cc's `vrun` | **the** headless runner: bare `.vbin` on logging stand-ins (`-g` GPU log, `-s` SPU log, `-d/-D` dump globals, `-p` pad script, `-t` traps, `-B` BP watch, `-P` profile, `-b` per-frame cycles, `--cfi=arm`, `--spu-state`), or a real BIOS + cartridge with the real GPU/SPU (same options), or `--as-bios PROG.vbin`; `gpu_dropped` in the STATUS line. Options: header of `tools/oracle/orun.cpp` |
| `refrun BIOS CART FRAMES [--date Y,D] [--time S] [--dump N@HEXADDR]` | v32sim `tests/ref` | `v32orun --compat=refrun`: run, print RAM words; the oracle of the differential tests |
| `v32run BIOS CART CARD\|- ADDRESS [FRAMES]` | v32cxx | `--compat=v32run`: run to HLT, print one RAM word and `halted=`; the oracle's `test_errors` check |
| `vrun --bios B --cart C [--memc F] [--keys S] [-f N] [-d N] [-a ADDR]` / `vrun PROG.vbin` | v32cc | `--compat=vrun`: RAM words and `STATUS halted\|timeout`; `.vbin` run as the BIOS |
| `v32prof BIOS CART CART.vbin.debug SCRIPT [heavy%] [csv]` | v32cxx | per-frame CPU/GPU load by script segment; cycles per function (all frames and heavy frames); scripted input |
| `v32shot BIOS CART SCRIPT PREFIX FRAME...` | v32cxx | PPM screenshots at chosen frames, scripted input |
| `v32peek BIOS CART SCRIPT FRAME ADDR COUNT...` | v32cxx | CPU state and RAM words at a frame, scripted input |
| `v32trace-ref BIOS CART SCRIPT FRAMES OUTDIR [--norestart]` | new | the DesktopEmulator V32TRACE recorder, headless (built only from a ConsoleLogic patched with GBtoV32's `emulator-v32trace.patch`); script lines may also be `FRAME mark NOTE` |
| `sysprof BIOS CART KERNEL.map CART.asm FRAMES PROG.x:PROG.sym...` | v32nix | whole-system profile: BIOS, kernel and every user program resident in the swapped window, self and inclusive time |

The input script of `v32prof`, `v32shot` and `v32peek` is frame-stamped (`320 press 1 left`, `end`); its format is in
the header of `tools/oracle/v32prof.cpp`. `tools/vprof2cmd.py` turns the same script into v32sim commands.

The other projects keep no runner of their own: v32cc's `make vrun` and v32lua's `runlua`, `trap`, `unittest.py` and the Windows
`scripts/testing_harness` call `v32orun` (v32lua's `v32bench` is its `-b`). They find it through `$V32SIM_HOME/tools/oracle/bin`.
`v32prof`, `v32shot` and `v32peek` (frame-stamped input scripts) are still separate programs.
Short cards for every tool of the ecosystem, for agents: `docs/agents/`.

## Debug information (who writes what)

| file | written by | read by v32sim for |
|---|---|---|
| `NAME.asm.sym` | `v32cc -g` | data symbols with sizes: `print counter`, `x/d buf`, `watch [n]`, `--dump=1@n` |
| `NAME.vbin.debug` | `v32as -g` (or the stock `assemble -g program`) | labels, `break FILE:LINE`, `bt`, `--source` |
| `NAME.vbin.vars` | `v32as -g` from the `;@var` lines of `v32cc -g` / `v32lua -g` | `locals`, and locals in `print`/`x`/`watch`/`expect` |
| `;@src FILE:LINE` lines in the `.asm` | `v32cc -g`, `v32lua -g`; for stock-compiler output `v32opt/scripts/v32srcmark.py` | the source-level modes |

The stock `compile` / `assemble` write no enhanced debug info on their own. With the stock chain v32sim still finds data
globals through the `%define global_NAME ADDRESS` lines the compiler writes into `NAME.asm`.
