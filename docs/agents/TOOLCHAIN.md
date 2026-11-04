# Building a Vircon32 cartridge (agent card)

```
 source            compiler                     optimizer (optional)   assembler                  packer
 .c        ----->  compile (stock) | v32cc  -->                     -->  assemble (stock) | v32as -->  packrom  -->  .v32
 .cpp/.c   ----->  v32c++  --> Vircon32 C ---^      v32opt -O1..3         (.asm -> .vbin)            (.xml + .vbin + .vtex + .vsnd)
 .lua .p8 .tic --> v32lua  (writes .asm and .xml) --^
 .gb  ...  ----->  GBtoV32 / NEStoV32 (write .asm .vbin .xml .v32 themselves)
 assets:   png2vircon (.png -> .vtex), wav2vircon (.wav -> .vsnd), Tiled2Vircon, PNGJoiner
```

| step | stock (ComputerSoftware DevTools) | alternate | notes |
|---|---|---|---|
| C -> asm | `compile prog.c -o prog.asm [-g]` | `v32cc -g -o prog.asm prog.c` | v32cc reads the stock dialect *and* ordinary C (STOCK-C.md, V32CC.md) |
| C++ -> C | - | `v32c++` / `v32c` | output is Vircon32 C for `compile` (V32CXX.md) |
| Lua -> asm | - | `v32lua -g -o obj/game.asm game.lua` | also writes `game.xml` and `gameOpt.xml` |
| optimize | - | `v32opt -O2 prog.asm -o progOpt.asm` | any compiler's output; `-g` keeps `;@src` |
| asm -> vbin | `assemble prog.asm -o prog.vbin [-g program] [-b]` | `v32as -g -o prog.vbin prog.asm` | v32as reads the stock syntax plus extensions (`--standard` for strict) |
| link objects | - | `v32asld`, `v32cc a.v32o b.v32o` | separate compilation (V32AS.md, V32CC.md) |
| pack | `packrom prog.xml -o prog.v32` | - | paths in the XML are relative to the XML |

The packer's XML, minimal:

```xml
<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<rom-definition version="1.0">
<rom type="cartridge" title="NAME" version="1.0" />
<binary path="NAME.vbin" />
<textures /><sounds />
</rom-definition>
```

`v32sim/tools/v32mkcart.sh [-o DIR] [--stock [--srcmark]] SRC.c|SRC.asm` does compile + assemble + pack with the debug files.

## Debug and symbol files (where they come from)

| file | from | content |
|---|---|---|
| `NAME.asm.debug` | `compile -g`, `v32cc -g` | `asmfile,asmline,cfile,cline[,function]` |
| `NAME.vbin.debug` | `assemble -g program`, `v32as -g` | `ADDRESS,FILE,LINE[,FUNCTION]` (from `;@src` markers with v32as) |
| `NAME.vbin.vars` | `v32as -g` (from `;@var`) | locals with address ranges |
| `NAME.asm.sym` | `v32cc -g` | `kind,name,address,words,cfile,cline` |
| `NAME.labels.txt`, `NAME.v32trace.meta` | translators | V32TRACE build files |

**The stock tools write no source markers and no variables.** v32opt's `scripts/v32srcmark.py prog.asm` gives stock `compile -g`
output `;@src` markers (then assemble with `v32as -g`). See DEBUG-INFO.md.

## Building the tools

- Stock DevTools: `cmake` in `ComputerSoftware/DevelopmentTools`, or headless without SDL: `v32sim/tools/oracle/build.sh CHECKOUT`
  (also builds the stock-console runners).
- v32cc, v32opt: `make` (a C99 compiler). v32as, v32lua, v32sim: `make` needs flex and bison; v32sim also builds without them
  (`tools/build-nogen.sh`, using `gen/`). v32lua and v32c++ ship their generated parsers.
- Bootstrap archives (V32DOS, v32nix) carry all the sources plus `gen/` output and build everything with `cc`, `g++`, `python3`.
