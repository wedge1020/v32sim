# GPU and SPU trace files (`--gpu-trace`, `--spu-trace`)

`v32sim --gpu-trace=FILE` and `--spu-trace=FILE` record what the program writes to the GPU ports (0x200-0x2FF) and the SPU ports
(0x300-0x3FF), frame by frame. A trace needs no CPU and no cartridge to be replayed: `tools/oracle/bin/v32gpureplay` (built by
`tools/oracle/build.sh`, it is `v32orun --compat=gpureplay`) feeds it to the DesktopEmulator's own `V32GPU` and `V32SPU` and times it.
`v32sim --trace-dump=FILE` prints a trace as text.

## Format, version 1

Binary, little endian, no padding. Stable: a reader must refuse a version it does not know.

```
header   8 bytes "V32TRACE" | u16 version (1) | u16 kind (1 = GPU, 2 = SPU) | u32 reserved (0)
records  one tag byte, then the payload
  'N'  u16 count                       cartridge textures (GPU) or cartridge sounds (SPU) loaded; the first record
  'T'  i16 tex | u16 wide | u16 high | u16 n | n x ( u16 region | 6 x i16 minX minY maxX maxY hotX hotY )
                                       GPU: a texture (-1 / 0xFFFF = the BIOS') and its regions that are not all zero
  'S'  i32 id | u32 length | i32 loop | i32 loopstart | i32 loopend
                                       SPU: a sound (-1 = the BIOS')
  'W'  u16 port | u32 value            a write by the program; the bits written (a float port's value is the float's bits)
  'F'  u32 frame                       a frame ended (v32sim's frame count before it was counted)
  'E'  u32 frames                      the run ended (always last)
```

The 'N', 'T' and 'S' records come first (the state when the trace began: the first port write or frame end), then 'W' records with an
'F' after each frame's. Every write the program makes is recorded, including ones the console would ignore; the tables of regions the
BIOS or the program defines are rebuilt from the writes (`GPU_SelectedTexture`, `GPU_SelectedRegion`, `GPU_RegionMinX`...), as on the
console. Texture pixels and sound samples are not in the file: what the GPU and SPU logic spends on a command does not depend on them.

## Replay

```
v32gpureplay GPU.trace [--spu SPU.trace] [--csv FILE] [--repeat N] [--frame-us US]
gpu draws 920 dropped 150 clears 141 pixels 105967622
replay time per frame (us, the console logic with empty host callbacks): mean 9.6  p95 11.3  max 67.4  over 16667 us: 0
```

The numbers are the stock GPU's own: a draw is dropped when the frame's 9-screen pixel budget is spent, as in the live run
(`tests/run_harness_tests.sh` checks draws, drops, clears and pixels against `--stats` of the run that made the trace). The host
callbacks (`DrawQuad`, `ClearScreen`, ...) are empty. To measure a host's own drawing, link its callbacks in place of the empty ones in
`compat_gpureplay` (tools/oracle/orun.cpp): the function is a self-contained reproducer of "this exact draw stream".
