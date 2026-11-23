**v32sim** is a headless Vircon32 simulator with an interactive single-step debugger, written in C. It runs a cartridge and BIOS the way the stock console does (the CPU, timer, RNG, memory bus, error timing and GPU port behaviour are checked against the DesktopEmulator's own logic), and gives you far more to look at while it runs: registers, memory, watchpoints, source lines, frame statistics and traces. It is the debugging companion to the rest of the Vircon32 toolchain (v32cc, v32c++, v32lua, v32io).

## Highlights

- **Headless and interactive.** Run a cartridge unattended, or drop to a prompt to step, break, inspect and change the machine. Watchpoints on registers, IOPorts and memory words, a live assembler (`asm`), `ignore`, and save/restore of the whole machine state.
- **Source-level debugging for C, C++ and Lua.** `step`/`next`/`finish`/`until` by source line, `break FILE:LINE`, `list`, and `bt` with function names; V32DBG `locals` show the variables of the selected frame with their types and values (from the `.vars` files `v32as -g` writes).
- **Scripting and batch mode.** `--batch` with symbols wherever a number is accepted, `run N`, `until "TEXT"`, `expect`, `quit N` and defined exit statuses, console capture, and profiles by function: made for tests and CI.
- **Screenshots, statistics and traces.** `screenshot` writes a PNG (no library needed); `--frame-stats` gives per-frame CPU and GPU load as CSV with mean / p95 / max; profiling, GPU and SPU traces; the V32TRACE recorder; scripted gamepad input by cartridge frame (`--input`).
- **v32io keyboard and mouse.** A `v32kbd` and a `v32mouse` on any gamepad port, driven from the prompt or from a terminal, pipe, FIFO or file.
- **Memory card and v32dev.** The memory card file is kept current every frame; a card formatted for v32dev talks to a real `v32dev.py serve` host process.
- **Turbo mode.** `--turbo` for speed when you do not need a faithful frame-by-frame GPU.
- **Oracle tools and a tools folder.** Helper scripts and the stock-console ("oracle") comparison tools that other Vircon32 projects use to test, debug and profile their programs.
- **Documented.** The README, the command-line reference (`docs/USAGE.md`) and the `v32sim(1)` manual page in English, Spanish and French.

## Downloads

Pick the zip for your system: `v32sim-@VERSION@-<os>-<arch>.zip`.

| System | Architectures |
| --- | --- |
| Linux | x86_64, arm64 |
| macOS | x86_64 (Intel), arm64 (Apple silicon) |

Windows is not offered yet: v32sim currently relies on POSIX features (termios, signals, regex) that need porting first.

Each zip unpacks into a folder with `bin/v32sim`, the README and LICENSE, `docs/`, the man pages in `man/` and the helper scripts in `tools/` (they find `bin/v32sim` beside themselves). Check a download against `SHA256SUMS.txt`.

**Runtime requirements.** GNU readline is linked dynamically: on Debian/Ubuntu install `libreadline8`, on Fedora `readline`, and on macOS `brew install readline` (the system libedit is not used). Some tools in `tools/` need `python3`; running source-level and cartridge tests needs the Vircon32 DevTools.

## Quick start

```sh
bin/v32sim --help
bin/v32sim -B StandardBios.v32 cart.v32  # interactive prompt
man -l man/v32sim.1                      # the manual page
```

See the [README](https://github.com/wedge1020/v32sim#readme) for the full feature list, the command reference and the known limitations. Bug reports and feature requests are welcome in the issues.
