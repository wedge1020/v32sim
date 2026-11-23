[Spanish version / en español](USAGE.es.md) | [French version / en français](USAGE.fr.md)

# v32sim: command-line reference

v32sim runs a Vircon32 cartridge in its own CPU, GPU and SPU models and either gives you a single-step debugger prompt or runs
it unattended and reports. This page covers the command line, the environment, the files and the exit status. The debugger
command language (`step`, `break`, `print`, `watch`, `kbd`, ...) is in the [README](../README.md) ("The Debugger", "Commands",
"Scripting"); `help` at the prompt lists every command and `help COMMAND` explains one. Which tool answers which question:
[TOOLS.md](TOOLS.md). Testing recipes: [TESTING.md](TESTING.md). Profiling: [PROFILING.md](PROFILING.md).

## Synopsis

```
v32sim [OPTION]... [CARTFILE.v32]
v32sim --version | --help
```

`CARTFILE.v32` is the cartridge. Give `-` (or nothing) for no cartridge: the BIOS runs alone. Options may come before or after the
cartridge. Long options with an argument take `--name=VALUE` (or `--name VALUE`); `[=X]` below means the value is optional and
must be attached with `=`. Numbers given as OFFSET, OPCODE or ADDR are hexadecimal (`0x` optional).

## Quick start

Debug a cartridge (the prompt appears at the first instruction; `help` lists the commands):

```
v32sim -B StandardBios.v32 cart.v32
v32sim -B StandardBios.v32 -b main cart.v32      # break at the label main, then `continue`
v32sim -B StandardBios.v32 --source -L src cart.v32   # show the C / Lua source line at each stop
```

Debug files (labels, source lines, symbols) are found beside the cartridge automatically; build them with `v32as -g` and
`v32cc -g` / `v32lua -g`.

Run headless, check the exit status, print a few RAM words (`-r` run, `-q` quiet, `-x` meaningful exit status):

```
v32sim -B StandardBios.v32 -r -q -x --fast --max-frames=600 --dump=4@counter cart.v32 </dev/null; echo $?
```

Recipes:

```
# screenshot after cartridge frames 60 and 120 (v32sim-shot-00060.png, ...)
v32sim -B B.v32 -r -q --fast --max-frames=400 --screenshot-at=60,120 cart.v32 </dev/null

# play a gamepad script (A held on cart frames 40-44, Start on frame 100), then take a screenshot
v32sim -B B.v32 -r -q --fast --max-frames=450 --input='A@40-44,START@100' --screenshot-at=150 cart.v32 </dev/null

# frame statistics: one CSV row per frame, summary (mean, p95, max, regions) on stderr
v32sim -B B.v32 -r -q --fast --max-frames=1800 --frame-stats=load.csv --frame-stats-skip=250 cart.v32 </dev/null

# the hottest functions
v32sim -B B.v32 -r -q --fast --max-frames=1800 --profile-by=function --profile-top=20 cart.v32 </dev/null

# a script of debugger commands from a file, nothing but what `print`/`console` print on stdout
v32sim -B B.v32 --batch --fast -x --v32kbd dos.v32 < session.cmd

# the program's text screen when it ends; reproducible runs
v32sim -B B.v32 -r -q --fast -x --max-frames=900 --console --fixed-clock --seed=1 --fixed-cycles cart.v32 </dev/null

# a text program on your terminal (Ctrl-] returns to the prompt)
v32sim --tty -B B.v32 v32nix.v32

# a V32TRACE session: trap/event reports PREFIX.traps.txt, .blocks.txt, .trace.txt
v32sim -B B.v32 -r -q --fast --max-frames=3000 --v32trace=out/run cart.v32 </dev/null
```

Without `--batch`, a script piped to stdin is echoed with the prompt; give `-q` when a program reads the output. A `-r` run
with commands on stdin or `-C` runs those commands once at the start.

## Options

Options marked (short form) have one: `-B`, etc. "Default" is what applies when the option is absent.

### Loading and BIOS

| Option | Meaning |
|---|---|
| `-B, --biosfile=FILE` | BIOS V32 file. Default: `/usr/local/Vircon32/Emulator/Bios/StandardBios.v32` (the DesktopEmulator's install). |
| `-M, --memcfile=FILE` | Memory card file (`V32-MEMC` + 262144 words). Changes are written back at the end of each frame. See *Memory card* below. |
| `-E, --entry-point=OFFSET` | Start executing at OFFSET (hex) instead of the BIOS start. |
| `-S, --bios-start` | Start at the BIOS code start (the default; kept for compatibility, and `-E` overrides it). |
| `CARTFILE.v32` or `-` | The cartridge; `-` = none. |
| `-V, --version` | Print `v32sim VERSION` and exit. |
| `-h, --help` | Print the usage text and exit (status 0). |

### Debug information and source

| Option | Meaning |
|---|---|
| `--cart-asm-debug=FILE`, `--cart-c-debug=FILE` | Cartridge labels / line table from FILE (assembler `.vbin.debug`; compiler `.asm.debug`). |
| `--bios-asm-debug=FILE`, `--bios-c-debug=FILE` | The same for the BIOS. |
| `--debug-dir=DIR` | Also look for debug files in DIR, tried first. Without these options debug files are searched beside the V32 file, in `obj/` and `../obj/` of it, in `./obj` and in `.`. |
| `--symbol-file=FILE` | Data symbols (v32cc's `NAME.asm.sym`). `NAME.asm.sym` / `NAME.vbin.sym` beside the cartridge are read automatically. |
| `-n, --no-debug` | Process no debug files (labels, lines, symbols). |
| `--source` | Show the source line of each stop (assembly; C or Lua with `-L`). |
| `-L, --langmode=MODE` | `C`, `lua` or `src`: the source language of the overlay. `C` and `src` also turn `--source` on; `lua` shows Lua values in memory dumps. `step`/`next` then go by source line. Any other value is an error (message on stderr, exit status 1). |
| `-d, --deref-addr` | Show the address of a dereference. |
| `-c, --colors` | Colorized output at the prompt. |

### Breakpoints, entry and watching

| Option | Meaning |
|---|---|
| `-b, --break=OFFSET\|LABEL\|FILE:LINE` | Set a breakpoint (repeatable), as the `break` command does once the debug files are loaded. A name that is a known label (or function) is that label even when it is also a hex number (`add`, `face`, `dead`; write `0xdead` for the address); otherwise the value is a hex OFFSET, with or without `0x`. `FILE:LINE` needs line information. A value that is none of these is reported on stderr. |
| `-w, --watch-for=OPCODE` | Run (`-r`) until this 32-bit instruction word is executed. |
| `-C, --command-file=FILE` | Run the debugger commands in FILE. They are read by the prompt after the first instruction is fetched (so `step`, `next`, `continue` act); with `-r` they run once at start. |
| `-e, --errorcheck` | Runtime error checking. |
| `-p, --profile` | Instruction profiling; `profile` shows the report. |

### Run control, speed and turbo

| Option | Meaning |
|---|---|
| `-r, --run` | Do not start in single-step mode: run until a breakpoint, watchpoint, HLT or the frame limit. |
| `--max-frames=N` | Stop after N frames, counted from power-on (the BIOS logo, about 250 frames, included; exit status 2 with `-x`). |
| `--fast` | Do not throttle to real time. |
| `--frame-us=N` | Wall-clock microseconds per frame. Default 16666; 0 = `--fast`. |
| `--turbo[=report]` | `--fast`, plus spin-loop fast-forwarding and dropping a full GPU pixel queue instead of painting it (screenshots may be incomplete). `=report` prints what was skipped to stderr at exit. README "Speed and Turbo Mode". Off while anything watches single instructions. |
| `--gpu-eager` | Paint every GPU command at once (same pictures; for comparing and timing). |
| `--fixed-clock[=Y,D,S]` | `TIM_CurrentDate/Time` start at year Y, day D (0 = Jan 1st), S seconds (default `2026,0,0`) instead of the host clock. |
| `--seed=N` | Seed `RNG_CurrentValue` with N at startup. |
| `--fixed-cycles` | Reads of `TIM_CycleCounter` return 0. |
| `--warn-halt` | Say on stderr when the program executes HLT (the emulator then stops). |
| `--halt-status=N` | Exit status N (1-255) when the run ends at HLT and nothing else went wrong. |

### Headless, batch and unattended

| Option | Meaning |
|---|---|
| `-q, --batch` (also `--quiet`) | Script mode: commands from stdin or `-C` with no prompt or echo; no banner, instruction line, display list or end-of-run register summary. stdout holds only what `print`, `console`, ... print; acknowledgements, stop reasons and diagnostics go to stderr. Blank lines are skipped; end of input ends the run; the first failed `expect`, or an `until` that times out, ends it with status 1; a command that cannot be parsed ends it with 4. |
| `-x, --exit-status` | The exit status says how the program ended (see *Exit status*). |
| `--dump=N[@ADDR]` or `ADDR:N` | After the run print N RAM words (signed, one per line) from ADDR: hex, or a label (also tried as `global_NAME`); default start of RAM. |
| `--console` | Print the BIOS-font text screen (64x18, or v32c++'s 80x24) when the run ends. |

### Input

| Option | Meaning |
|---|---|
| `--input=SCRIPT` | Hold gamepad buttons by **cartridge frame** (0 = first frame the cartridge runs): `BUTTON[+BUTTON]@FIRST-LAST,...` (`@FRAME` for one). Buttons `LEFT RIGHT UP DOWN START A B X Y L R` (`SELECT` = X). `@FILE` reads the `V32_INPUT=` line of FILE (a `.trace.txt` report). Default: `$V32_INPUT`. |
| `--input-port=N` | Gamepad port `--input` drives (default 0); it is connected. |
| `--input2=SCRIPT` | The same for gamepad 2 (port 1); `@FILE` reads the `V32_INPUT2=` line. Default: `$V32_INPUT2`. With `--input-port=1` it is an error. |
| `--v32kbd[=PORT]` | Emulate a v32io keyboard on gamepad PORT (0-3, default 0). The program must read it every frame. |
| `--kbd-input=SOURCE` | Host keys: `stdin`, `fd:N`, or a FIFO/pipe/file path. Implies `--v32kbd`. |
| `--kbd-sync` | Deliver the next key event only after the program has read the gamepad since the last one (default: the new event replaces an unread one, as the real keyboard does). |
| `--v32mouse[=PORT]` | Emulate a v32io mouse on gamepad PORT (0-3, default 1); drive it with the `mouse` command. |
| `--tty` | Full-screen interactive mode: the text console on this terminal, keys to a v32kbd (port 0 unless `--v32kbd=N`), implies `-r`. Ctrl-] = debugger prompt, `continue` returns. Needs a terminal on stdin and stdout. |

Gamepad, keyboard and mouse can also be driven from the prompt (`gamepad`, `kbd`, `mouse`). README "The v32io Keyboard",
"The v32io Mouse", "Scripted gamepad (--input)".

### Memory card and v32dev

`-M FILE` is the card. A v32dev mailbox lives in the last 16384 words of it; the host side is either an external process
(`v32dev.py serve`) or v32sim itself:

| Option | Meaning |
|---|---|
| `--v32dev-doorbell[=MS]` | Header flag bit 1, per-channel reply counters (header words 10..13); a store to a cart part's `c2h_head` rings: flush, give the host MS ms (default 2; 0 = no wait) to answer, import the reply, go on. Off by default. |
| `--v32dev-host=N=SPEC` | Serve channel N (0-3) natively. Repeatable. Needs `-M FILE` holding a writable v32dev card (or last 64 KiB all zero: it is formatted). SPEC: `storage:FILE[,bytes][,ro][,sector=N][,create=S]` or `serial:stdio\|echo[,hup=N]\|tcp=PORT[,in=FILE][,out=FILE][,hangup]` (`tcp=PORT`: 127.0.0.1 only, one client at a time; `storage:` options are read from the right, so FILE may hold commas). Wins over `V32DEV_CHN` for its channel. |
| `--v32dev-host-force` | Format the card over other data in its last 64 KiB. |

### Output, screenshots and the text console

| Option | Meaning |
|---|---|
| `--screenshot-at=FRAMES` | Write the screen (PNG) after these cartridge frames: `N`, `A-B`, `A-B/STEP`, comma separated. Frame 0 is the first frame that ends with the program counter in the cartridge, that is after the BIOS has handed over (the stock BIOS's logo takes about 250 frames, which `--max-frames` also counts). Frames the run never reached are reported on stderr at exit. |
| `--screenshot-file=PAT` | File name for those; `%d` = the frame (printf-style, e.g. `%05d`). Default `v32sim-shot-%05d.png`. |
| `--screengrid[=SPEC]` | Grid over screenshots: `DX[xDY]`, `color=C`, `alpha=N`, `labels`/`nolabels`, comma separated. Default 40x40. |
| `--highlight=T:R[:COLOR]` | Outline texture T region R (`bios:R` for the BIOS texture); comma separated. |
| `--noextra` | No grid and no outlines. |
| `--console-color=MODE` | Text console colors: `none`, `16`, `256`, `true`, `auto`. Default: `auto` with `--tty`, plain text otherwise. |
| `--cursor-color=COLOR` | Recolor the cursor block (BIOS character 20): a name, `#RRGGBB` or `off`. |
| `-v, --verbose` | More output (to stderr). |
| `-D, --debug` | Simulator debug output (development; to stderr). |

### Profiling, statistics and traces

| Option | Meaning |
|---|---|
| `--frame-stats=FILE` (`--stats`) | One CSV row per frame (cycles, CPU %, GPU pixels, GPU %, then GPU/SPU/bus counters); summary (mean, p50, p95, p99, max, total, busiest regions) to stderr at exit. |
| `--frame-stats-skip=N` (`--stats-skip`) | Leave the first N frames (the BIOS logo, about 250) out of the summary. |
| `--frame-stats-top=N` (`--stats-top`) | List the N regions charged the most GPU pixels (default 8; 0 = none). |
| `--stats-format=csv\|json` | `json`: JSON Lines with stable dotted names and a summary line last. |
| `--profile-out=FILE` | At the end of the run write how often each instruction address was executed (needs no prompt and no `-p`); a debug file adds a label column. |
| `--profile-format=text\|bin` | `bin`: `V32PROF1` header, then little-endian 32-bit (address, count) pairs. Default text. |
| `--profile-top=N` | Print the N hottest addresses (or functions) to stderr at the end. |
| `--profile-by=address\|function` | With `function`, `--profile-top` and the text `--profile-out` add up each function (nearest label). Default address. |
| `--profile-event=KIND` | What is counted: `instr` (default), `gpu-draw`, `gpu-portwrite`, `spu-write`, `cycles`. |
| `--time-breakdown` | At exit, where v32sim's own wall-clock time goes per frame (cpu/gpu/spu/output). |
| `--frame-log=FILE` | With it, one row per frame of that breakdown. |
| `--gpu-trace=FILE`, `--spu-trace=FILE` | Record the GPU / SPU port writes by frame (binary, [TRACE-FORMAT.md](TRACE-FORMAT.md)). |
| `--trace-dump=FILE` | Print a trace file as text and exit. |
| `--v32trace=PREFIX` | V32TRACE recorder ([V32TRACE-SPEC.md](V32TRACE-SPEC.md) 2.1): reads `<cart>.v32trace.meta` and labels, writes `PREFIX.traps.txt`, `.blocks.txt`, `.trace.txt`. The cartridge restarts after a trap. |
| `--v32trace-meta=FILE`, `--v32trace-labels=FILE` | Name those files when they are not beside the cartridge. |
| `--v32trace-norestart` | Stay in the trap. |

## Environment

| Variable | Used for |
|---|---|
| `V32_INPUT`, `V32_INPUT2` | Gamepad scripts used when `--input` / `--input2` are not given (a note goes to stderr). |
| `V32DEV_DISABLE` | Any value but `0`: a card is an ordinary card, no v32dev mailbox; with `--v32dev-host` or `V32DEV_SERVER` a warning, the host does not start. |
| `V32DEV_SERVER` | Set (and not `0`): the built-in v32dev server, as in the DesktopEmulator patch of the v32io project: the card needs no formatting, the mailbox lives in memory only and never reaches the card file, and the channels come from `V32DEV_CH0`..`V32DEV_CH3`. Unset or `0`: those variables are not looked at. |
| `V32DEV_CH0` ... `V32DEV_CH3` | With `V32DEV_SERVER`: what channel N serves, a SPEC as for `--v32dev-host` (`storage:FILE[,bytes][,ro][,sector=N][,create=S]`, `serial:echo[,hup=N]`, `serial:stdio`, `serial:tcp=PORT`). Empty or unset: nothing. A wrong one is reported and its channel left out. |
| `NO_COLOR`, `COLORTERM`, `TERM` | What `--console-color=auto` (and `--tty`) chooses: none if `NO_COLOR` is set non-empty; true color for `truecolor`/`24bit`; 256 for `*256color`; 16 for other terminals except `dumb`. |
| `HOME`, `INPUTRC`, `TERM` | Read by GNU readline for the prompt (line editing, history in memory only). |

The helper scripts in `tools/` also read `V32SIM` (path of the binary), `V32SIM_FLAGS` (extra options), `V32BIOS`, `V32TC`
(toolchain directory) and `V32SIM_HOME` (for the oracle tools); see [TOOLS.md](TOOLS.md).

## Files

Read:

| File | |
|---|---|
| BIOS | `-B FILE`, else `/usr/local/Vircon32/Emulator/Bios/StandardBios.v32`. |
| Cartridge | `CARTFILE.v32`. |
| `NAME.vbin.debug`, `NAME.asm.debug` | Labels and line table (assembler, compiler) of the cartridge and the BIOS, beside it, in `obj/`, `../obj/`, `--debug-dir`. |
| `NAME.asm.sym`, `NAME.vbin.sym`, `NAME.asm` | Data symbols (v32cc `-g`; v32as `-g`; the `%define global_NAME` lines). |
| `NAME.vbin.vars` | Local variables for `locals` (v32as `-g`, from v32cc / v32lua `-g`). |
| `NAME.v32trace.meta`, labels | Translator files for `--v32trace`. |
| `-C FILE`, script files, `--input=@FILE`, `--kbd-input=PATH` | As named. |

Written:

| File | |
|---|---|
| Memory card `-M FILE` | Updated at the end of each frame (read-only cards warn and are not saved). |
| `v32sim-shot-NNNNN.png` | `--screenshot-at` (`--screenshot-file` renames). |
| `v32sim.YYYYMMDD-HHMMSS.png`, `.sav` | Default names of the `screenshot` and `save` commands. |
| `--frame-stats`, `--profile-out`, `--frame-log`, `--gpu-trace`, `--spu-trace` | As named. |
| `PREFIX.traps.txt`, `PREFIX.blocks.txt`, `PREFIX.trace.txt` | `--v32trace`. |
| `--v32dev-host` `storage:FILE`, `out=FILE` | Disk image and serial output of a native device. |

## Exit status

| Status | Meaning |
|---|---|
| 0 | Normal end: the program executed HLT (unless `--halt-status`), or `quit`, or end of input. |
| 1 | A check failed: `expect` failed or `until` timed out. Startup problems that exit 1: a bad option or option value (`-L` with an unknown mode too), a `-C` file that cannot be opened, `--tty` without a terminal, a bad `--v32dev-host`. |
| 2 | Frame limit reached (`-x` only). Also the status when `/dev/null` cannot be opened at startup. |
| 3 | System error (`-x` only; the run stops instead of entering the BIOS error handler). |
| 4 | `--batch` could not parse a command (or a `dev` command failed). |
| 5 | `--dump` could not print what was asked (bad spec, unknown symbol, unreadable memory), when nothing else set a status. |
| 11 | Startup: a BIOS, cartridge or memory card file cannot be opened. |
| 12 | Startup: such a file cannot be read (truncated, bad position). |
| 21 | Startup: such a file is not a valid V32 file. |
| 4, 5, 7, 13, 14, 15 | Internal failures (out of memory, an invalid IOPort access mode), with an `[ERROR]` message on stderr; they should not happen. 4 and 5 are also the statuses above. |
| N | `quit N` overrides all of the above; `--halt-status=N` sets the status of an HLT. |

## See also

[README](../README.md) (debugger commands, source-level debugging, memory card and v32dev, v32io keyboard and mouse, text
console, running unattended, scripting), [TOOLS.md](TOOLS.md), [TESTING.md](TESTING.md), [PROFILING.md](PROFILING.md),
[PERFORMANCE.md](PERFORMANCE.md), [TRACE-FORMAT.md](TRACE-FORMAT.md), [V32TRACE-SPEC.md](V32TRACE-SPEC.md). Manual page:
`man v32sim` (`man/v32sim.1`).
