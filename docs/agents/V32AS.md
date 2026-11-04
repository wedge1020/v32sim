# v32as / v32asld -- an alternate assembler and linker (agent card)

Assembles Vircon32 assembly to a VBIN like the official `assemble`, in C (flex + bison), with a preprocessor, objects, a linker,
symbol tables and the V32DBG side files. Version 20261101-dev. Docs: `README.md` (rounds 9-12 list every addition by request),
`docs/OBJECT.md`, `docs/PREPROCESSOR.md`.

```
v32as prog.asm                          # -> prog.vbin (cartridge, origin 0x20000000)
v32as -b bios.asm -o bios.vbin          # BIOS (0x10000000);  -O ADDR: any origin (code run from RAM)
v32as -g -o prog.vbin prog.asm          # + prog.vbin.debug (and prog.vbin.vars when the source has ;@var)
v32as -C a.asm                          # relocatable object a.v32ob (global / extern)
v32asld -o prog.vbin a.v32ob b.v32ob    # link; -g, --symbols, -A archive, -L/-l, -R partial link, --entry
v32as --standard prog.asm               # exactly what the official assembler accepts
v32as -T prog.asm                       # listing with encodings;  no file at a terminal: interactive REPL
```

## Things that matter

- **Stock syntax plus extensions**: `%define`, `%include` with `-I`, macros, `.local` labels, `label+N` operands, `datafile`
  with a search path, `datasym`. v32cc's default output needs v32as (`--standard` output does not).
- `-D NAME=VALUE` against a `%define` in the file: the file wins and it warns; `-D!NAME=VALUE` makes the command line win.
- `-g [program|vbin|v32]` address modes as in the official tool. With `;@src` markers in the source, `.debug` has one entry per
  marker (`ADDRESS,FILE,LINE,FUNCTION`), `ADDRESS,-,0` for `;@src -`; `--map FILE` (a compiler `.asm.debug`) maps assembly lines
  to C lines for sources without markers and makes errors name the C line.
- `.vbin.vars`: `scope,START,END,DEPTH[,FUNCTION]` and `var,START,END,NAME,LOC,TYPE,FLAGS,DEPTH[,FUNCTION]`, END exclusive.
  A function ends at a `;@fn` written after its body (v32lua), at `;@src -`, or at the end of the program; a `;@fn` above a label
  (v32cc's form) does not end it.
- `--symbols FILE` (`address,name,kind`, F code / D data; `--symbols-all` adds locals, `global_*` and `DATASYM+N`), `-r FILE`
  (every word holding an address, `index,value[,C|D]`) for relocatable modules (V32DOS `.COM`, v32nix programs), `--v32x` (a v32nix
  executable), `-MD`/`-MF` dependency files, `-Wmarkers` (malformed `;@` lines).

## Tests

`make test` (differential against golden files, `.debug` files included; `sh`, `od`, `cmp` only). `tests/markers/` holds v32lua
samples with their `.debug` and `.vars`.
