# V32SIMGAPS: where v32sim differs from the Vircon32 DesktopEmulator

This is a comparison of v32sim (`20261018-dev`) with the stock DesktopEmulator
(`ComputerSoftware/DesktopEmulator/ConsoleLogic`, plus `Emulator/VideoOutput.cpp`
for rendering). The goal is that a program behaves identically under both.

**Reference revision.** `ComputerSoftware` commit `e75b537` ("Some minor or latent
emulation fixes"), which includes the fixes for the defects in `BUGS.md` (region `MinY`
reset, blending `default:`, `PlayChannel` target channel) and the 400-year leap rule.
Everything else in `ConsoleLogic` is unchanged from the revision first reviewed
(`ea62bc7`), so the other findings below stand.

**Status (this revision).** Every item except G12 and G14 is closed, and the closures
were verified by running the same cartridges on v32sim and on the stock ConsoleLogic
(compiled headless as `tests/ref/refrun`, see *Verification* at the end). The entries
below are kept as the original findings; each starts with a **Status** line. Four gaps
that the review missed were found by the differential runs (G15-G18, at the end), and a
fifth by the fuzzer (G19).

**Method and confidence (original review).** Both sources were read side by side. Nothing
in the entries was confirmed by running a cartridge on both at that point: the
reference BIOS and a flex binary were not available in the review environment, so
every item was "by reading the code". Each entry gives the DesktopEmulator behavior (file and function), the v32sim
behavior (file and line), the visible effect, and a suggested change. Items are
ordered by how likely an ordinary program is to notice them.

Line numbers refer to v32sim at the commit this file was added in.

**Policy (decided by the v32sim maintainer): v32sim matches the DesktopEmulator's
behavior, including its quirks.** Where the stock emulator does something odd, the
simulator should do the same, and note the oddity in a comment. The "Upstream quirks"
section at the end is the place where such behaviors are listed; it is currently empty
because the ones found have since been fixed upstream. The one exception to the policy
is host-undefined behavior (G12, shift counts of 32 or more), where there is no defined
console behavior to copy.

| ID  | Area                                   | Visible to ordinary programs? |
| --- | -------------------------------------- | ----------------------------- |
| G1  | RNG algorithm and seeding              | yes (any use of `RNG_CurrentValue`) |
| G2  | `TIM_FrameCounter` is one behind       | yes                           |
| G3  | `TIM_CycleCounter` is one behind       | yes (timing code)             |
| G4  | `TIM_CurrentDate` day numbering        | yes (date code)               |
| G5  | GPU port writes are not clamped or ignored | yes (out-of-range values) |
| G6  | Stack error semantics, `PUSH SP`       | rarely (error paths, `PUSH SP`) |
| G7  | Memory bus address decoding            | rarely (wild pointers)        |
| G8  | Bad jump targets: when the error occurs | error paths                  |
| G9  | A CPU error ends the frame             | error paths                   |
| G10 | `MOVS`/`SETS`/`CMPS` cost, partial progress, `CMPS` early exit | yes (`CMPS` with a large count) |
| G11 | ROM/BIOS/memory card validation        | loading                       |
| G12 | `IDIV`/`IMOD` of `INT_MIN` by -1       | corner case                   |
| G13 | Port numbers above 0x7FF               | corner case                   |
| G14 | Rendering fidelity                     | pixel-level, unverified       |

---

## G1. RNG algorithm and seeding

**Status: closed.** `ioports.c` implements minstd (start 1, write 0 ignored, sign bit cleared, sticky 0 from 0x80000000); `--seed=N` goes through the write rule. Tests `g01_rng`.

**DesktopEmulator** (`V32RNG.cpp`): a linear congruential generator, the C++11
`minstd_rand` constants. `CurrentValue` starts at **1** (also at every reset). A
read returns `CurrentValue`, then sets
`CurrentValue = (CurrentValue * 48271) % 0x7FFFFFFF` (64-bit intermediate). A write
of `0` is ignored; any other value is stored with its sign bit cleared
(`& 0x7FFFFFFF`).

**v32sim** (`ioports.c:132`, `:712`, `:748`): the port starts at 0 and each read
stores libc `rand()`; a write calls `srand(value)`.

**Effect.** Different values, different range, and non-reproducible against the
console. Known vectors from the console: after reset the first reads are
`1, 48271, 182605794, 1291394886, 1914720637, 2078669041`; after seeding 12345 they
are `12345, 595905495, 1558181227`. A seed of `0x7FFFFFFF` produces 0 on the second
read and forever after.

**Suggested change.** Implement the generator above (`--seed=N` then maps onto the
write rule). A first read of the port before any seeding must return 1.

## G2. `TIM_FrameCounter` is one behind

**Status: closed.** `update_frame()` calls `timer_change_frame()` (frame start work: cycle 0, frame++, clock tick, date rollover), and `init_ioports` does it once for frame 1. Test `g02_timer`.

**DesktopEmulator** (`V32Console::RunNextFrame`, `V32Timer::ChangeFrame`): the
frame counter is incremented at the *start* of each frame, before the CPU runs. After
reset it is 0, so the first frame executes with `TIM_FrameCounter == 1`.
`TIM_CurrentTime` is incremented in the same step when the counter is a multiple of
60.

**v32sim** (`update_sys.c: update_frame`): the counter is incremented at the *end* of
each frame, so the first frame reads 0 and every frame reads one less than the console
would. The clock tick (`value % 60 == 0`) is applied against that lagging value.

**Effect.** A program printing the frame number, or doing `frame % N` work, is
shifted by one frame; the one-second clock ticks one frame later than on the console
relative to program execution.

**Suggested change.** Do the frame-start work (counter increment, cycle counter
reset, clock tick) *before* running the frame's instructions, as the console does, or
equivalently initialize the counter so the first frame observes 1.

## G3. `TIM_CycleCounter` is one behind

**Status: closed.** `cycle_begin()` increments the counter before the instruction executes. Test `g02_timer`.

**DesktopEmulator** (`V32Console::RunNextFrame`): `Timer.RunNextCycle()` runs
**before** `CPU.RunNextCycle()`, so an instruction reads a cycle counter that already
includes its own cycle: the first instruction of a frame reads **1**.

**v32sim** (`main.c:821` calls `update_cycle()` after the instruction has been
processed): the first instruction of a frame reads 0.

**Effect.** Off by one in every read; e.g. code that measures a section by subtracting
two reads is unaffected, but code that compares against a limit or reports the value
differs.

**Suggested change.** Increment the cycle counter before executing the instruction
(keeping the end-of-frame test at `CyclesPerFrame`).

## G4. `TIM_CurrentDate` day numbering (and leap years)

**Status: closed.** 0-based `tm_yday`, rollover on `Days >= DaysThisYear` with the 400-year rule, `--fixed-clock` day 0-based (default 0, clamped to 365). Test `g04_date_rollover` (7 dates).

**DesktopEmulator** (`V32Timer.cpp`): the lower half-word is `tm_yday`, which is
**0-based** (January 1st = 0). At midnight the date is incremented and when
`Days >= DaysThisYear` it becomes `(Year + 1) << 16` (day 0). The leap year test is
`((Year % 4 == 0) && (Year % 100 != 0)) || (Year % 400 == 0)` (the 400-year rule was
added in `e75b537`; earlier revisions treated 2000 as a non-leap year).
`V32Console::SetCurrentDate` accepts days 0 to 365.

**v32sim** (`ioports.c:89`, `update_sys.c:128`): `tm_yday + 1` (1-based, January 1st =
1); rollover when `value > daysinyear` back to day 1. Its leap year rule already
matches the console's. `--fixed-clock` defaults to day 1.

**Effect.** Every date read is one day higher than the console's, so the month and
day computed by the stock `date_*` helpers in `time.h` are one day off.

**Suggested change.** Use `tm_yday` (0-based) and treat `--fixed-clock Y,D,S` day `D`
as 0-based (default day 0), and make the rollover test `Days >= DaysThisYear` (days
counted from 0). The leap year rule needs no change.

## G5. GPU port writes are not clamped or ignored

**Status: closed.** All the clamps/rejections in the table are in `ioports_set`. Test `g05_gpu_ports` (98 cases).

**DesktopEmulator** (`V32GPUWriters.cpp`): out-of-range values written to GPU ports are
corrected, never stored raw and never an error:

| Port                              | Console behavior                                    |
| --------------------------------- | --------------------------------------------------- |
| `GPU_ActiveBlending`              | value not 0x20-0x22: ignored                        |
| `GPU_DrawingPointX` / `Y`         | clamped to -1000..1640 / -1000..1360                |
| `GPU_DrawingScaleX` / `Y`, `GPU_DrawingAngle` | NaN/Inf ignored; clamped to -1024.0..1024.0 |
| `GPU_RegionMinX/Y`, `MaxX/Y`      | clamped to 0..1023                                  |
| `GPU_RegionHotspotX` / `Y`        | clamped to -1024..2047                              |
| `GPU_SelectedTexture`, `GPU_SelectedRegion` | invalid: ignored (v32sim already does this) |

**v32sim** (`ioports.c: ioports_set`): only `GPU_SelectedTexture` and
`GPU_SelectedRegion` are validated; everything else falls to the default
`*iptr = i32` / `*fptr = f32` and is stored as written. `grep -n -i clamp
src/ioports.c src/gpu.c` finds nothing.

**Effect.** Reading a port back after writing an out-of-range value returns the raw
value instead of the clamped one, region rectangles with huge or negative corners,
or a scale of 5000, behave differently when drawn, and an invalid blending value
changes the blend mode.

**Suggested change.** Add the clamping and rejection above in `ioports_set` (or in a
`gpu_write` function like `spu_write`). Note `GPU_DrawingPointX/Y` upper bounds are
1640 and 1360 in the source (the specification text and older versions of the
README said 1639 and 1359).

## G6. Stack error semantics, and `PUSH SP`

**Status: closed.** `PUSH`/`POP`/`CALL`/`RET` mirror `Push`/`Pop` (value taken before SP moves, SP<0 overflow, failed read changes nothing, underflow after the register is written). `decode_check` (`-e`) follows the same rules. Tests `g06a-i`, `g16d`.

**DesktopEmulator** (`V32CPUProcessors.cpp: Push, Pop`, used by `PUSH`, `POP`, `CALL`
and `RET`):

  * `Push`: `SP--`; if `SP < 0` raise **StackOverflow**; otherwise write to `[SP]`
    (a write that fails, e.g. `SP` pointing into ROM, is **InvalidMemoryWrite**).
    `PUSH` and `CALL` push the *value* of the register/IP as it was before the
    decrement, so `PUSH SP` pushes the old `SP`.
  * `Pop`: read `[SP]` (a failure is **InvalidMemoryRead**), then `SP++`; if
    `SP >= 0x00400000` raise **StackUnderflow**. `RET` is a `Pop` into the
    instruction pointer, so `RET` (and `POP`) with an empty stack (`SP` at the initial
    `0x003FFFFF`) reads the word and then raises StackUnderflow.

**v32sim** (`decode.c:1532-1543, 1594-1610` checks; `:1133-1141` execution):

  * `PUSH` reports **StackOverflow** for *any* invalid write location (including
    `SP` pointing into ROM, which should be InvalidMemoryWrite).
  * `POP` reports **StackUnderflow** for an invalid read (should be
    InvalidMemoryRead).
  * `CALL` reports InvalidMemoryWrite when `SP-1` is invalid (for `SP-1 < 0` the
    console reports StackOverflow).
  * `RET` only checks that `[SP]` is readable: there is **no StackUnderflow** for a
    `RET` that pops the last word of RAM.
  * `PUSH SP` stores the *decremented* `SP` (execution does `SP = SP - 1` and then
    `MEMSET(SP, DSTREG)` with `DSTREG` being `SP` itself).

**Effect.** Error codes differ in the cases above; `PUSH SP` pushes a different value;
a stray `RET` from `main` with an empty stack silently jumps to garbage instead of
raising StackUnderflow (which the BIOS handler reports).

**Suggested change.** Mirror `Push`/`Pop` exactly (including copying the pushed value
before decrementing) and use them for `PUSH`, `POP`, `CALL` and `RET`.

## G7. Memory bus address decoding

**Status: closed.** `memory_chk` and the accessors decode `(address>>28)&3` and `address&0x0FFFFFFF`. Tests `g07a-d`.

**DesktopEmulator** (`V32Buses.cpp: V32MemoryBus`): `device = (address >> 28) & 3`,
`local = address & 0x0FFFFFFF`. Address **bits 30 and 31 are ignored**, so
`0x40000000` is RAM, `0x90000000` is BIOS, `0xE0000000` is CART, and there is no
"invalid page" case; invalid accesses come only from the slave's range check
(`local >= MemorySize`, or a write to ROM).

**v32sim** (`memory.c:149-197`): the page is `(address & 0xF0000000) >> 28`; pages 4 to
15 are invalid (InvalidMemoryRead/Write).

**Effect.** A wild pointer that lands in `0x4xxxxxxx`-`0xFxxxxxxx` is a memory error in
v32sim but silently reads or writes RAM, BIOS (error on write), CART or the memory card
on the console. Negative addresses (e.g. -1) are in the same family.

**Suggested change.** Mask the page with 3 (and the offset with `0x0FFFFFFF`) before
the range check.

## G8. Bad jump targets: when the error is raised

**Status: closed.** Jumps do not validate; the fetch faults (with a stale `IR`/`IV` kept in `R2`/`R3`). `decode_check` no longer reports bad targets. Tests `g08a-g`.

**DesktopEmulator** (`V32CPU::RunNextCycle`, `ProcessJMP` and friends): `JMP`, `CALL`,
`JT`, `JF` and `RET` just load the instruction pointer; an unreadable target is not
detected until the next instruction **fetch**, which raises InvalidMemoryRead with
`R1 = target + 1`, `R2` and `R3` still holding the `JMP`'s own instruction word and
immediate.

**v32sim** (`decode.c:1527-1560`): the target is validated at the jump itself, so the
error occurs on the jump instruction with `R1` = the address after the jump and
`R2`/`R3` describing the jump.

**Effect.** The BIOS error screen (and any program that inspects `R1`-`R3` after an
error) shows different values; the faulting address displayed differs.

**Suggested change.** Do not validate the target at the jump; let the fetch of the
next instruction fail, raising the error with the post-increment instruction
pointer and the previous instruction/immediate in `R2`/`R3`. A fetch of an immediate
word that fails does the same.

## G9. A CPU error ends the frame

**Status: closed.** An error loads R0-R3 and ends the frame; the handler starts next frame. Tests `g09a-c`.

**DesktopEmulator** (`V32CPU::RaiseHardwareError`, `V32Console::RunNextFrame`): the error
handler is entered by throwing an exception that exits the frame's cycle loop, so
the rest of the frame is not executed; the BIOS error handler's first instruction runs
at the start of the *next* frame (with the frame/cycle counters advanced as usual).

**v32sim** (`main.c:777-815`): after an error the registers are loaded and execution
continues at `0x10000000` in the same frame (when running; in the debugger it stops at
the prompt).

**Effect.** After an error, the sim spends the remainder of the frame running the
handler; `TIM_FrameCounter`/`TIM_CycleCounter` seen by the handler, and GPU/SPU
side effects ordering relative to frames, differ slightly.

**Suggested change.** End the frame (perform the frame-end work) when a hardware error
occurs.

## G10. `MOVS`/`SETS`/`CMPS`: cycles and partial progress

**Status: closed.** One word per execution/cycle, `CMPS` stops at the first difference, an error part way leaves the progress in `SR`/`DR`/`CR`; `decode_check` checks only the next word. Tests `g10a-d` (basic, huge-count `CMPS` early exit, partial progress, a `SETS` spanning frames).

**DesktopEmulator** (`ProcessMOVS/SETS/CMPS`): one word per *instruction execution*
(and per cycle): the instruction pointer is moved back by one until the count reaches
0, so a string instruction with `CR = N` takes N cycles, can be interrupted by the
frame boundary or by `WAIT`-free frame end mid-copy, and an error part-way leaves
`SR`/`DR`/`CR` showing the partial progress.

**v32sim** (`decode.c:1169-1210` execution, `:1622-1655` `decode_check`): the whole
operation runs inside one step and costs one cycle, and `decode_check` validates
**every address in the `CR` range up front**, before any word is moved.

**Effect.** Cycle budgets (a 4000-word `MOVS` costs 4000 cycles on the console, 1 in the
sim), `TIM_CycleCounter` reads after a string op, and register state after a failed
copy (the console leaves the words before the failing one copied and `SR`/`DR`/`CR`
advanced; v32sim changes nothing) all differ. Most importantly for ordinary code,
**`CMPS` with a large `CR`**: the console stops at the first differing word, so
`CR = 1000` against two strings that differ in their first word never touches memory
beyond them; v32sim raises InvalidMemoryRead if any of the 1000 addresses is invalid.
The "compare up to N words" idiom therefore faults in the sim only. Cycle accuracy is
already in `TODO.md`; the `CMPS` early-out is not.

**Suggested change.** Execute one word per step as the console does (leave `IP` on the
instruction until the count is exhausted), counting a cycle each.

## G11. ROM, BIOS and memory card validation

**Status: closed.** `load.c: rom_check()` applies the console's checks and messages to the BIOS and cartridge before anything is allocated; failure exits with code 21. The memory card checks were already there. A ROM whose sections run past its own file is refused too (the console reads garbage). Test `tests/run_romcheck.sh` (28 malformed files, compared with refrun).

**DesktopEmulator** (`V32Console::LoadBios`, `LoadCartridge`, `LoadMemoryCard`): files
are rejected unless: the size is a multiple of 4; the signature is `V32-CART` (cartridge)
or `V32-BIOS` (BIOS), and not the other kind; the version/revision is not newer than 1.0;
the program, video and audio sections are located exactly one after the other starting
at byte 0x80 and the sizes add up to the file size; texture and sound counts and sizes
are within limits (cartridge: 256 textures of 1x1 to 1024x1024, 1024 sounds of 1 to 256M
samples with at most 256M samples in total; BIOS: exactly 1 texture and 1 sound);
each VBIN/VTEX/VSND signature is checked; the memory card file must be exactly
`8 + 1MB` bytes with a `V32-MEMC` signature.

**v32sim** (`load.c`): reads fixed header positions and the memory card signature; the
other checks were not found.

**Effect.** v32sim loads files the console refuses (and may misbehave on truncated or
inconsistent ones). Low priority, but useful when the simulator is used to test cartridge
packing tools.

**Suggested change.** Add the same checks and error messages, or at least size/offset
consistency and the BIOS/cartridge signature distinction.

## G12. `IDIV`/`IMOD` of `INT_MIN` by -1

**Status: unchanged by policy** (host-undefined). Shift counts >= 32 and `INT_MIN / -1` stay defined as before; the fuzzer avoids them.

**DesktopEmulator** performs a host `int32_t` division; `-2147483648 / -1` is undefined
in C++ and traps (SIGFPE) on x86 hosts. **v32sim** defines the result (negation wraps,
remainder 0). This is a deliberate safety choice in the sim; recorded for completeness.
No change suggested unless exact host behavior is wanted.

## G13. Port numbers above 0x7FF

**Status: closed.** `ioports_get`/`ioports_set` (and the `IN`/`OUT` decode) mask the port to 0x7FF. Test `g13_port_alias`.

**DesktopEmulator** (`V32Buses.cpp: V32ControlBus`): `device = (port >> 8) & 7`,
`local = port & 0xFF`, so a 14-bit port number such as `0x1200` is an alias of `0x200`
(`GPU_Command`) and the full semantics apply.

**v32sim**: `ioports_chk` and the table lookups mask the same way, but the special-case
`switch (portaddr)` in `ioports_set`/`ioports_get` (and the `SPU` dispatch) compare the
unmasked number, so an aliased `GPU_Command` write would be stored as a plain value and
not execute. Only reachable through numeric `OUT` with a port above `0x7FF`, which the
assembler warns about. Suggested change: mask `portaddr` to `0x7FF` at the start of
`ioports_get`/`ioports_set`.

## G14. Rendering fidelity (not verified)

**Status: still unverified.** The reference renders through OpenGL, which the headless runner stubs, so pixels were not compared.

The console renders through OpenGL (`Emulator/VideoOutput.cpp`): textured quads with
`GL_NEAREST` sampling, a fragment shader computing `MultiplyColor * texel` in floating
point, and these blend functions: Alpha `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`; Add
`SRC_ALPHA, ONE` (`FUNC_ADD`); Subtract `SRC_ALPHA, ONE` (`FUNC_REVERSE_SUBTRACT`).
`ClearScreen` draws a full-screen quad in the clear color with the **active blend
mode** (v32sim already does this). The quad vertices apply, in order, scaling,
rotation, then translation; for negative scale factors the console adds +1 pixel to
the vertex on that axis after the transforms; for scales above 1 the texture
coordinates are adjusted (`PixelCorrection`) so that magnified pixels keep their size.

v32sim's software renderer follows the same cost accounting (verified identical in
`GPU_RemainingPixels`) and blend equations. Whether its rasterization of rotated and
scaled quads (pixel-center rule, the +1 shift for negative scales, the magnification
correction) is pixel-identical to the GL output was **not** checked; differences of one
pixel at edges would be expected from the GL rasterization rules (the console's
`DrawQuad` and the texture-coordinate correction in `V32GPU::DrawRegion` are the
reference).

---

## Found by running both (not in the original review)

**G15. `R3` after an error is the CPU's stale immediate register.** The console's `R3` is
`ImmediateValue`, which only changes when an instruction with an immediate is fetched and
is never cleared. After a bad jump (G8) or any error on an instruction without an
immediate, `R3` is the last immediate fetched, possibly the BIOS's own `JMP` before the
cartridge ran any. v32sim keeps `cpu_immediate` the same way. Tests `g15`, `g15b`.

**G16. Failed reads leave the destination alone.** A `MOV` load, `IN` or `POP` that
fails does not touch its register (and a failed store changes nothing else); `POP` that
reads fine but underflows keeps the loaded value. Tests `g16a-d`.

**G17. Port access matrix.** Every control-bus port of the 8 devices (local 0-0x30,
0x7F, 0xFF; one read and seven different writes each) raises or does not raise the same
errors, with the same register state, as the console: no difference was found beyond
G5/G13. Generator `tests/ports_gen.py` (3264 programs).

**G18. ROM pages were sized to the file.** v32sim sized the BIOS/CART pages to the file
length in bytes (counted as words), so the program ROM looked 5-6 times larger than it is
and execution ran on into the texture data instead of faulting at the end of the program.
Now the page is exactly the VBIN word count; the texture pixels are kept past it, outside
the addressable range (`memory_texel()`). Tests `g08f`, `g08g`.

**G19. `IMIN`/`IMAX` with an immediate compared unsigned** (a negative immediate was
"larger" than any positive register). Found by the fuzzer; now signed as on the console.

---

## Verification

`tests/ref/build.sh <ComputerSoftware checkout> <dir>` builds `refrun` (the console logic
at `e75b537` with stubbed video/audio/log callbacks), the assembler and the ROM packer.
Then, with that directory as the tools dir:

  * `tests/run_gaps.sh` - the hand-written cartridges (`tests/gaps`): 46 runs
  * `tests/run_romcheck.sh` - malformed ROM files
  * `tests/run_fuzz.sh` - random instruction programs (900 over three seeds) and the
    port matrix (3264 programs)

Each runs a BIOS and a cartridge on both and diffs the RAM (including the error
handler's register dump). All pass. The fuzzer deliberately avoids host-undefined
behavior and the wall clock.

## Checked and equivalent

For reference, these were compared and no difference was found:

  * All 64 instruction semantics: comparisons, `MOV` modes, `LEA`, `IN`/`OUT`, the
    string instructions' register effects (the `CR` rule: decrement while above 0,
    repeat while above 0), conversions, logic (including the **logical** right shift
    of `SHL`), integer and float arithmetic, and the `ACOS`/`ATAN2`/`LOG`/`POW`
    domain errors. (`SHL` counts of 32 or more are undefined behavior on the
    console's host; v32sim masks the count to 5 bits, which is what x86 does.)
  * Error codes and the `R0`-`R3` loading, with `SP`/`BP` reset to `0x003FFFFF` and a jump
    to `0x10000000`, for errors other than those in G6, G8.
  * Reset state: `SP = BP = 0x003FFFFF`, entry at BIOS `0x10000004`.
    (Correction: not entirely. R3 on an error was not `0`/the current immediate but the
    CPU's stale immediate register, see G15. And BIOS/CART "sized to the loaded program
    ROM" was wrong, see G18. The ALU claim also missed G19.)
  * Gamepad semantics: +/-3600 frame counts, opposite directions mutually exclusive,
    ignored input for disconnected pads, selection of an out-of-range pad ignored.
  * GPU: pixel budget accounting (draw, scale and rotation penalties, clear costs half
    a screen, -1 when exhausted, refilled each frame), selected texture/region
    validation and per-region storage, unknown commands ignored.
  * SPU: selection, assigned sound only while stopped, volume/speed/position clamps,
    loop semantics (loop flag copied from the sound on play), end-of-sound stop,
    pause/resume/stop commands, 735 samples per frame.
  * Memory card: write-back at the end of each frame; BIOS and CART are read-only and
    sized to the loaded program ROM.

## Upstream quirks

None outstanding. The quirks found in the first review have been fixed in
`ComputerSoftware` `e75b537`, so there is nothing to reproduce, and v32sim should
**not** copy the old behavior:

  * the leap year test lacking the 400-year rule (G4);
  * `V32GPU::Reset` never clearing region `MinY` (the console now clears it, so a
    region's `MinY` is 0 after a reset, as the other fields are);
  * `WriteGPUActiveBlending` passing an unknown blending value on to the video layer
    (now ignored outright, which v32sim already does);
  * `V32SPU::PlayChannel` taking the loop flag from the selected channel's sound instead
    of the target channel's (now the target channel's).

v32sim needs no change for these: its per-texture region tables are allocated zeroed
when a ROM is loaded, it has no console-reset command that could leave stale regions
behind (if one is added, it must clear all six region fields, `MinY` included), and
`play_channel (channel_t *c)` in `spu.c` already uses the channel it is given.
