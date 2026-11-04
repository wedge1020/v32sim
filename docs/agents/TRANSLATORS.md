# Trans-assemblers (GBtoV32, NEStoV32) and V32TRACE (agent card)

An `XtoV32` tool translates another console's ROM ahead of time into a native Vircon32 cartridge: guest code found by a profile is
decoded in blocks and re-emitted as Vircon32 code, the guest video is redrawn with the Vircon32 GPU, sound is mapped onto the SPU.
Code the profile never saw cannot run: the cartridge enters a **trap routine** (a red screen). Closing those gaps is the daily loop.
GBtoV32's `AGENTS.md` is its full guide; `doc/V32TRACE-SPEC.md` (v2.1, byte-identical in GBtoV32 and NEStoV32, copied into v32sim
`docs/`) is the interface.

## V32TRACE in one paragraph

The translator writes **build files** beside the cartridge: `<cart>.labels.txt` (`0xADDR name`) and `<cart>.v32trace.meta` (the trap
routines, events, what to capture, the guest block label pattern, `resume`, the cartridge's size and hash). A **recorder** -- the
patched DesktopEmulator (`emulator/emulator-v32trace.patch` in GBtoV32) or `v32sim --v32trace=PREFIX` -- runs the cartridge and
writes `PREFIX.traps.txt` (one `TRAP`/`EVENT` line per distinct hit: `cartframe`, captured guest state, the trail of guest blocks,
and from 2.1 `segstart=` and `input=`, the gamepad history that leads back to the hit) and `PREFIX.blocks.txt` (instructions per
guest block). The translator's **consumer** tools (`gapfill.py`, `trace2input.py`, `profmerge.py`) turn traps into profile roots and
rebuild. After a trap the recorder restarts the cartridge (60 frames later) so one run meets several gaps.

## With v32sim

```
printf 'run 3000\nquit\n' | v32sim -q --fast -B StandardBios.v32 --v32trace=out/round0 game.v32      # unattended gap hunt
v32sim ... --input="$(sed -n 's/^TRAP .* input=\([^ ]*\).*/\1/p' out/round0.traps.txt | head -1)" ...  # back to the first hit
python3 tools/v32sim-corpus.py -F 1800 --input fixed carts/*.v32      # every cart: OK / TRAP / CPU_ERROR, load, screenshot
```

`--input` frames are **cartridge frames** (0 = the first frame the cartridge runs, after the ~250-frame BIOS logo), as in the
recorder. The guest usually sees a press a frame after the cartridge frame it is held in (the runtime reads the pad at the end of
its frame). GBtoV32's `tools/trace_loop.sh` and `make test-trace` use v32sim this way (V32SIM=path/to/v32sim, not GBtoV32's own
small `bin/v32sim`, which is a different program).

GBtoV32's own `tools/v32sim.c` is a miniature interpreter for smoke tests (no BIOS, frame 0 = first frame), with `V32_INPUT`,
`V32_STATS`, `V32_PCPROF`, `V32_SHOTS`, `V32_TILEDUMP`: its frame numbers can differ from the console's by one.
