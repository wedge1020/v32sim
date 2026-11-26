# bios/ — BIOS files shipped with v32sim

| file | what |
|---|---|
| `StubBios.v32` | the bundled BIOS: StandardBios's texture and sound, no logo, a debugging-friendly error handler |
| `StubBios.vbin.debug` | its labels and source lines (v32as `-g`); v32sim loads it by itself, beside the `.v32` |
| `src/` | `StubBios.asm`, `build.sh`, `StubBios.xml`, and the texture / sound taken from StandardBios.v32 v1.2 |
| `debuggerBIOS.v32` (+ `.vbin.debug`) | the interactive debugger BIOS (debuggerBIOS-ng) |

## StubBios

**When   it   is  used.**   `-B   FILE`   always  wins.   Without   `-B`,
v32sim    takes,    in    order:   `$V32BIOS`;    the    stock    install
`/usr/local/Vircon32/Emulator/Bios/StandardBios.v32`; then  this file —
`StubBios.v32`  beside the  binary (release  zip), `../bios/StubBios.v32`
(a  checkout's  `bin/`),  `~/.local/lib/v32sim/bios/`  (`make  install`),
`$(VROOT)/v32sim/bios/` (`make  sysinstall`). `-v`  prints which  one was
loaded.  So  a  fresh  checkout   or  release  runs  cartridges  with  no
ComputerSoftware install and no `-B`.

**Boot.**  It  defines  the  BIOS  texture's  regions,  sets  the  ports,
the  registers  and  the  top   of  the  stack  exactly  as  StandardBios
leaves  them  when  it  jumps  to  the  cartridge,  waits  for  the  next
frame  and  jumps  to  `0x20000000`. The  cartridge's  first  instruction
sees  the  same  machine  as  with  StandardBios  —  checked  word  for
word  by  `tests/run_stubbios_tests.sh` with  `tools/v32sim-statediff.py`
(registers, every port, all GPU region tables, SPU, RAM, framebuffer) —
except for  the timers: the cartridge  starts at frame 2  instead of 251,
and no boot sound plays. So:

- `--frame-stats-skip=250` and  logo allowances in `--max-frames`  are no
  longer needed (they do no harm).

- A cartridge that seeds its RNG from the frame counter or the clock gets
  other numbers than with StandardBios; everything else runs the same.

- With no  cartridge it prints  StandardBios's "NO CARTRIDGE  FOUND" text
  and halts.

**Errors.** With  `-x` v32sim stops at  a hardware error (exit  3) before
any BIOS code runs, as before. Without `-x` (the interactive debugger, or
`continue` past the error stop in `--batch`) the handler runs and:

1. writes  a record  at the  top  of RAM,  using  no stack  before it  is
   complete;

2. draws  the error  on the  10x20  BIOS-font grid,  which v32sim's  text
   console  reads  (`console`,  `--console`,  `until  "ERROR:"`,  `expect
   console "DIVISION  BY ZERO"`):  StandardBios's title  and description,
   the fault address, next IP,  instruction, immediate, R4–R13, and one
   line for scripts:

   `V32ERR <code> <fault address> <instruction> <immediate>` (all `0x%08X`);

3. stops the sound, leaves the code in R0, waits for the end of the frame
   and halts.

It writes  only RAM `0x3FFFDF`–`0x3FFFFF`  (the record, and  one return
address below it). Everything else in RAM  is as the program left it. The
CPU has already  reset SP and BP,  so their old values are  lost. It also
redefines the BIOS texture's regions, in case the program changed them.

| RAM | record word |
|---|---|
| `0x3FFFE0` | magic `0x45323356` ("V32E") |
| `0x3FFFE1` | error code (R0): 0 memory read, 1 memory write, 2 port read, 3 port write, 4 stack overflow, 5 stack underflow, 6 division, 7 arc cosine, 8 arc tangent, 9 logarithm, 10 power |
| `0x3FFFE2` | fault address: the first word of the instruction that failed |
| `0x3FFFE3` | next IP, as the CPU passes it (R1) |
| `0x3FFFE4` / `0x3FFFE5` | instruction word (R2) / immediate value (R3) |
| `0x3FFFE6`–`0x3FFFEF` | R4–R13 at the fault |
| `0x3FFFF0` / `0x3FFFF1` | `TIM_FrameCounter` / `TIM_CycleCounter` when the handler started |
| `0x3FFFF2` | errors seen: 1, or 2 after a double fault |
| `0x3FFFF3`–`0x3FFFF6` | R0–R3 of a fault inside the handler (the first record is kept) |
| `0x3FFFFF` | state: `0x56330001` busy, `0x56330002` done, `0x56330003` double fault |

```
    v32sim --batch --fast --max-frames=600 --console --dump=32@0x3FFFE0 cart.v32 <<< $'continue\ncontinue'
```

The fault address  is computed from the immediate bit  of the instruction
word. For an error raised while  the instruction itself is being fetched,
the CPU  passes the  previous instruction  word, so  the address  is only
approximate in that case.

**Rebuild.**  `bios/src/build.sh  [TOOLCHAIN_DIR]`  (v32as or  the  stock
`assemble`,  with `-b`;  `packrom`  or python3).  The  output is  checked
in,  so  nothing  needs  a  toolchain to  use  it.  `BiosTexture.vtex`  /
`BiosSound.vsnd` are StandardBios's own and are not decoded or converted.
The VBIN is identical from either assembler.
