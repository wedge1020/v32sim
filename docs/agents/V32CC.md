# v32cc -- an alternate C compiler for Vircon32 (agent card)

Reads the stock Vircon32 C dialect **and ordinary C** (structs by value, function pointers, designated initializers,
compound literals, `_Static_assert`, statement expressions, `#embed`, `__atomic_*`), writes assembly for `v32as` (default,
"enhanced" syntax) or for the stock `assemble` (`--standard`). Version 20261102-dev. Docs: `README.md`, `docs/TOOLS.md`
(options by request), `docs/DEBUG.md`, `docs/OBJECT-FORMAT.md`, `docs/STAGES.md`, `docs/POSIX.md`, `inc/hosted/README.md`.

```
v32cc game.c -o game.asm                 # one step, like `compile`
v32cc -g -o game.asm game.c              # + ;@src/;@var/;@scope, game.asm.debug, game.asm.sym
v32cc -c a.c; v32cc -c b.c               # objects a.v32o b.v32o
v32cc a.v32o b.v32o libx.v32a -o game.asm
v32cc --standard -o game.asm game.c      # only what the stock assembler reads
v32as -g -o game.vbin game.asm           # then packrom
```

## What to know first

- The **stock headers** (`video.h`, `input.h`, ...) are on the default path, unmodified: programs for `compile` build as they are.
  `NULL` is **-1** and a pointer is true when it is not NULL, as in the stock compiler (`--null 0` or `--target-compat=v32cxx`
  for code that assumes 0, such as v32c++ and V32DOS sources).
- `-O1`..`-O3` only turn on `--emit-hints` (hints for v32opt); **v32cc does not optimize**. Use v32opt for that.
- Whole-program linking removes what nothing references; `-k NAME` / `-k 'PREFIX*'` keeps it; `--print-kept` lists what survived.
- `long long` is 32 bits; a plain `char`/`short` is a whole word (only `int8_t`..`uint16_t` and `signed char`/`short` wrap).
  RAM is assumed zeroed at start-up. Globals start at RAM address 1 (`--data-base N`; address 0 stays null).
- `--hosted` links an ISO C library (`stdio.h`, `stdlib.h`, `string.h`, `math.h`, `setjmp.h`, ...; stdout goes through a hook,
  files live in memory; a text terminal, curses and memory-card files on the console). `--hosted-libc=v32nix` is the flavour for
  programs under the v32nix kernel (POSIX layer, curses on a tty).
- Warnings have kinds: `--list-warnings`, `-Wkind`/`-Wno-kind`, `--warn-error[=kinds]`, `-Wall`/`-Wextra` (unused,
  uninitialized, missing return).
- OS conventions: `-e FUNCTION` entry, `--yield-points[=N|=loops]` (preemption checks; `__attribute__((no_yield))`),
  `__attribute__((address(N)))` (fixed RAM address), `--pic-data` / `--data-after-text` (relocatable data), `--asm-objects`
  (link with hand-written v32as modules), `#texture NAME "f.png"` / `#sound NAME "f.wav"` with `--rom-xml`.
- `make stages` builds the pipeline `v32cpp | v32cc1 | v32ld` (`-` = stdin/stdout, `-E` writes `# line` markers); v32cc compiled
  by itself runs on the console (self-hosting).

## Debug output (`-g`)

`;@lang c`, `;@src FILE:LINE` per statement, `;@src -` for code without source, `;@var NAME loc=BP+-N type=...` for parameters
and locals, `;@scope { }` around blocks, and `NAME.asm.sym` (data symbols with sizes). `-g` changes only comments. Known gaps:
no `;@var` for globals, struct members are not named in `type=`, and without `--emit-hints` there is no `;@fn` marker (v32as then
runs every variable's range to the end of the program; v32sim copes). See DEBUG-INFO.md.

## Tests

`tests/run.py ASSEMBLE VRUN V32CC WORKDIR` (self-checking programs, whole-program and separate builds), `make debugtest`,
`tests/difftest.py` (same result as the stock compiler), `tests/selfhost.py`. Tools: `tools/vrun.cpp` (a stock-core runner),
`tools/cartrun.py` (build + run, prints a global), `tools/v32sym.py` (`.asm.sym` with final addresses, or an `nm`-style list).
