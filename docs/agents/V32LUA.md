# v32lua -- Lua for Vircon32 (agent card)

A Lua-subset compiler: Lua in, Vircon32 assembly plus a cartridge XML out (no VM). Also compiles PICO-8 (`.p8`) and TIC-80
(`.tic`) cartridges, converting their assets. Version 20261030-dev. The project's own `doc/AGENTS.md` is the full guide; this is
the short form. Docs: `README.md`, `doc/API.md` (the intrinsic reference), `doc/USAGE.md`, `doc/PICO8.md`, `doc/TIC80.md`,
`doc/NaN_boxing.md`, `doc/DEBUGGING.md`, `doc/HINTS.md`.

```
v32lua -g -o obj/game.asm game.lua      # writes obj/game.asm, game.asm.debug, obj/game.xml and obj/gameOpt.xml
assemble obj/game.asm -o obj/game.vbin   # or v32as -g (for .vbin.debug / .vbin.vars); or v32opt -L lua first
png2vircon ... ; packrom obj/game.xml -o bin/game.v32
```

## The mistakes agents make most

- **Every number is a float32** (NaN-boxed values): integers above 2^24 are inexact; a raw word can read back as `nil`.
- `print(x, y, value)` draws at screen coordinates with the BIOS font; it is not stdout.
- A program must define `main()` or `game_loop()`; `init()` runs once first; top-level statements run once at start.
  `game_loop()` gets a `WAIT` per call; in `main()` call `system.wait()` yourself.
- A top-level `local` becomes a global. No `require` (use `--#include "f.lua"`), no `pcall`/`error`/`assert`, no `string.match`,
  no arithmetic/comparison metamethods, no coroutines, **no garbage collection** (a bump allocator: build tables once, not per
  frame; `s = s .. c` in a loop is quadratic).
- `--#title`, `--#texture NAME "x.png"`, `--#sound NAME "x.wav"`, `--#tilemap`, `--#api` are comments the compiler reads; texture /
  sound ids are assigned **in source order** and that order is the XML's. Do not reorder them.
- The API mode (`--api`, `--#api`, or detected: `.p8`, `.tic`, `TIC()`, `_draw()`/`_update()`) **replaces** the native API.
- XML paths are relative to the XML, which lands next to the `-o` file with bare file names; an `Opt` twin is always written.
  Harnesses must pass `--no-build-scripts` (otherwise `Make.sh`/`Make.bat` appear beside the sources).
- `-b` builds a BIOS (needs `error_handler()`, one texture, at most one sound).

## Debugging

`-g` writes `;@lang`, `;@src`, `;@var NAME loc=BP-n type=lua [boxed]`, `;@scope`, `;@fn` after each body. With `v32as -g` the
`.vbin.vars` lets v32sim show locals (`locals` in v32sim: numbers, strings, nil/true/false, tables as `table: 0xADDR`, boxed
captures read through their box). Not described: chunk-level locals (RAM words `var_NAME`) and upvalues inside a closure.
v32sim's `-L lua` reads tagged values in `print/B` and `print/s`.

## Tests

`make tests` (needs the DevTools and v32sim on `PATH`): every `testing/*/NN_*.lua` runs on v32sim and its `number_*`,
`string_*`, `boolean_*`, `hex_*` globals are compared with the file's `=== EXPECTED OUTPUT ===` block. `tests/xml/*.sh` need no
DevTools. `tools/headless/` (a stock-CPU runner with GPU/SPU logs, `render.py`, `spu_render.py`) builds from ComputerSoftware's
`SimplifiedEmulator`, which current ComputerSoftware no longer has.
