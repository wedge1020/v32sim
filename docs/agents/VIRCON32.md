# Vircon32 -- the machine (agent card)

A 32-bit fantasy console: 640x360 screen, 16 sound channels, 4 gamepads, a memory card. Everything is a **32-bit word**:
memory is addressed in words, there are no bytes. Full reference: GBtoV32's `doc/v32asm_reference.md` (v32ref, 1.9k lines:
every instruction, every port); the official specification documents at vircon32.com.

## Timing: the thing that surprises people

- 15,000,000 cycles per second, 60 frames per second: **250,000 cycles per frame**. **Every instruction takes 1 cycle**
  (`MOVS`/`SETS`/`CMPS` take 1 per word moved). There are no flags: comparisons write 1 or 0 into a register, `JT`/`JF` test it.
- A program does a frame's work and executes `WAIT`; the CPU sleeps until the next frame. A frame that never reaches `WAIT`
  runs out of budget and simply continues in the next frame (an **overrun**: the game slows down).
- The GPU has a per-frame pixel budget of **9 screens** (`GPU_RemainingPixels`): a draw costs its whole region area (capped at
  the screen), x1.15 when scaled, +0.25 rotated, a clear half a screen; once spent, later draws of the frame are dropped silently.
- The standard BIOS shows its logo for about **250 frames** before it jumps to the cartridge.

## Memory map (word addresses)

| page | first | last | notes |
|---|---|---|---|
| RAM | `0x00000000` | `0x003FFFFF` | 4 M words, read/write; SP starts at `0x003FFFFF` and grows down |
| BIOS ROM | `0x10000000` | `0x100FFFFF` | `0x10000000` = error handler, `0x10000004` = entry |
| cartridge ROM | `0x20000000` | `0x27FFFFFF` | program; a program is loaded at `0x20000000` |
| memory card | `0x30000000` | `0x3003FFFF` | 256 K words, read/write, persistent |

## CPU

- 16 registers R0..R15: R11 = `CR`, R12 = `SR`, R13 = `DR` (count/source/destination for `MOVS`/`SETS`/`CMPS`), R14 = `BP`,
  R15 = `SP`. Each holds an int or a float bit pattern (`CIF`/`CFI` convert).
- 64 opcodes (6 bits, opcode 0 = `HLT`). Families: control `HLT WAIT JMP CALL RET JT JF`; compare `IEQ INE IGT IGE ILT ILE`
  and `F..` versions; data `MOV LEA PUSH POP IN OUT MOVS SETS CMPS`; convert `CIF CFI CIB CFB`; logic `NOT AND OR XOR BNOT SHL`
  (one shift instruction: a negative count shifts right, **logically**); integer `IADD ISUB IMUL IDIV IMOD ISGN IMIN IMAX IABS`;
  float `FADD FSUB FMUL FDIV FMOD FSGN FMIN FMAX FABS FLR CEIL ROUND SIN ACOS ATAN2 LOG POW`.
- An instruction is 1 word, plus 1 for an immediate. `IN`/`OUT` reach the I/O ports by number.

## Hardware errors

On an error the CPU stores state in R0..R3 and jumps to the BIOS error handler (`0x10000000`), which shows the error screen.
Codes: 0 invalid memory read, 1 invalid memory write, 2 invalid port read, 3 invalid port write, 4 stack overflow,
5 stack underflow, 6 division by zero, 7 arc cosine, 8 arc tangent 2, 9 logarithm, 10 power. Writing to ROM is error 1; reading
a write-only port error 2.

## I/O ports (groups)

| range | group | examples |
|---|---|---|
| `0x000` | TIM | `TIM_CurrentDate`, `TIM_CurrentTime`, `TIM_FrameCounter` (1 in the first frame), `TIM_CycleCounter` |
| `0x100` | RNG | `RNG_CurrentValue` (write to seed) |
| `0x200` | GPU | `GPU_Command`, `GPU_RemainingPixels`, `GPU_ClearColor`, `GPU_MultiplyColor`, `GPU_ActiveBlending`, `GPU_SelectedTexture` (-1 = BIOS texture), `GPU_SelectedRegion`, region and drawing ports (scale, angle) |
| `0x300` | SPU | `SPU_Command`, global volume, selected sound / channel, channel speed, loop, position, state |
| `0x400` | INP | `INP_SelectedGamepad`, `INP_GamepadConnected`, then Left Right Up Down Start A B X Y L R (`0x402`..`0x40C`): each a signed frame count, >0 held for that many frames, <0 released for that many (clamped to +-3600). Left+Right or Up+Down are never both held |
| `0x500` | CAR | cartridge connected, program size, number of textures and sounds |
| `0x600` | MEM | memory card connected |

## Calling convention (compiled code)

Both the stock compiler and v32cc: `PUSH BP; MOV BP, SP` prologue, return address at `[BP+1]`, caller's BP at `[BP]`,
parameters at `[BP+2]`, `[BP+3]`..., locals at `[BP-1]`, `[BP-2]`...; result in R0. The stock compiler saves the registers it
uses (callee-save); v32cc and v32lua are caller-save (v32opt's `--abi`). A C global lives at a fixed RAM address
(`%define global_NAME ADDRESS` in the assembly).

## Assets and cartridges

Textures (`.vtex`, from PNG by `png2vircon`, up to 1024x1024, 256 per cartridge, 4096 regions each) and sounds (`.vsnd`, from WAV
by `wav2vircon`, 44.1 kHz) are packed with the program (`.vbin`) into a `.v32` by `packrom` from an XML definition. A BIOS is
the same format with one texture and one sound.
