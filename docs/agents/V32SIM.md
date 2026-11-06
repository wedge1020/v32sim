# v32sim -- simulator, debugger and test bench (agent card)

A headless Vircon32 with a gdb-like debugger, a scriptable batch mode, a software GPU (screenshots), an SPU model (no audio), the
v32kbd keyboard and the v32dev mailbox. Checked instruction for instruction against the stock console logic. Version 20261103-dev.
Docs: `README.md`, `docs/TOOLS.md` (which tool for which question), `docs/TESTING.md`, `docs/PROFILING.md`,
`docs/AGENTS-SNIPPET.md` (a section to paste into your project's AGENTS.md).

## Run it

```
v32sim cart.v32                                          # debugger prompt at the first instruction
v32sim --batch --fast -x -B StandardBios.v32 cart.v32 < script.cmd     # scripted; stdout = what the script printed
v32sim --batch -r --fast -x --max-frames=1200 -B BIOS --dump=1@test_errors cart.v32   # self-check: status 0 + value 0 = pass
```

Exit status: 0 ok/halted, 1 an `expect` failed or `until` timed out, 2 frame limit (`-x`), 3 hardware error (`-x`), 4 bad command
(`--batch`), `quit N`. **Always use `-x` and `--max-frames` in tests.** A program that executes `HLT` ends the run: put checks
before it or use `--dump`.

## Commands (abbreviations work: s n c b p ...)

`break LABEL|FILE:LINE|0xADDR`, `tbreak`, `step`/`next` (by line in `set mode=src`, `-L C|lua`), `finish`, `until [LINE]`, `bt`,
`frame/up/down`, `list`, **`locals [NAME]`** (also `info locals`), `print/fmt X` (`/d /u /x /f /s /D`; registers, `[mem]`, ports,
symbols, locals), `x/d NAME [N]`, `display`, `watch LOC OP VALUE` (`watch/d` signed), `run N`, `until "TEXT" [timeout N]`,
`expect LOC OP VALUE` / `expect console "TEXT"`, `console [FILE]` / `console --attrs`, `screenshot FILE.png`, `gamepad N CONTROL`
(toggles), `kbd attach N` / `kbd type "..."` / `kbd key K`, `mouse attach N` / `mouse move DX DY` / `mouse click`, `mailbox`, `save` / `restore`, `asm` (patch an instruction),
`profile`, `quit N`.

## Options worth knowing

`--fast`, `--max-frames=N`, `-x`, `--batch`, `-B BIOS`, `-M card.memc`, `--fixed-clock --seed=1 --fixed-cycles` (reproducible),
`--input=BUTTON@FIRST-LAST,...` (gamepad by cartridge frame, V32_INPUT syntax), `--v32kbd=N`, `--kbd-input=...`, `--v32mouse=N`, `--tty`,
`--frame-stats=FILE` (per-frame CPU/GPU load), `--profile-out=FILE --profile-by=function --profile-top=N`, `--dump=N@SYM`,
`--v32trace=PREFIX` (translator traps, V32TRACE 2.1: `traps.txt`, `blocks.txt`, the `trace.txt` report; `mark NOTE` sets a marker), `--symbol-file`, `--debug-dir`, `-n` (no debug files).
Debug files are found next to the `.v32` (`NAME.vbin.debug`, `NAME.vbin.vars`, `NAME.asm.sym`, `NAME.asm`).

## Tools beside it (`tools/`)

`v32mkcart.sh` (build a cart with debug files), `v32sim-selfcheck.sh` (test_errors carts; `--oracle`), `v32sim-session.py` +
`v32sim-golden.sh` (typed sessions vs golden screens), `v32sim-corpus.py` (many carts: OK/HALT/CPU_ERROR/TRAP, load, screenshot),
`v32sim-peek.sh` (a global or text buffer after N frames), `v32sim-addrprof.py` (profile across kernel/BIOS/modules),
`vprof2cmd.py` (v32prof/v32peek input scripts -> commands), `v32io-script.py`, `oracle/` (runners on the stock console logic:
`v32orun` (the one headless runner; `refrun v32run vrun` are wrappers) `v32prof v32shot v32peek sysprof`).

## When v32sim and the emulator disagree

Run the cartridge on the oracle (`tools/oracle`) first. If the oracle agrees with the DesktopEmulator, it is a v32sim bug: add a
case to `tests/gaps/` (an `.asm` with a `;@ frames=N dump=...` header) and report it. Known, accepted differences:
host-undefined behaviour (shift counts >= 32, `INT_MIN / -1`) and pixel-exact rendering. Float-to-int of NaN or out-of-range
values differs between x86-64 and ARM64 builds of the emulator itself (v32lua's `tools/headless/README.md`): do not test on it.
