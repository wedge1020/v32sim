# Profiling with v32sim

What a Vircon32 program costs, in the console's own terms: instructions (one cycle each, 250,000 per frame) and GPU
pixels (9 screens, 2,073,600 per frame). Host-emulator cost (OpenGL, audio) is out of scope: v32sim has no such path.

| question | use |
|---|---|
| how loaded is each frame? p95? how many frames are over budget? | `--frame-stats=FILE` |
| which functions use the cycles? | `--profile-by=function --profile-top=N` / `--profile-out` |
| the same when code lives in the kernel, the BIOS and loaded modules | `tools/v32sim-addrprof.py` |
| the same for an OS whose programs are swapped through a window | `sysprof` (oracle) |
| CPU load per script segment, cycles of heavy frames only | `v32prof` (oracle) |

## Frame statistics

```
v32sim --batch -r --fast -x --max-frames=400 -B StandardBios.v32 --frame-stats=load.csv cart.v32     # tests/examples/load_pattern.c
[frame-stats] 400 frames (first 0 skipped), csv load.csv
[frame-stats] cpu: mean 27.29%  p95 100.00%  max 100.00%  frames at 100%: 74
[frame-stats] gpu: mean 3.29%  p95 6.84%  max 6.84%  frames at 100%: 0
```

`load.csv`: `frame,cycles,cpu_pct,gpu_pixels,gpu_pct,end` per frame (`end` is `w` when the program waited for the next
frame, `b` when the budget ran out). The skip only affects the summary. The numbers equal v32prof's, frame for frame
(`tests/run_harness_tests.sh`, 400 of 400 rows). It only reads ports: the run is not changed.

Not there yet (the VS-9 proposal, `v32sim-proposal-perf-metrics-20261030.md`): GPU command counts, texture and region
switches, SPU counters, `--profile-event`, `expect stats`, GPU/SPU traces and replay, run comparison. The per-frame
CSV is phase 1's first slice; the rest should be added to `src/framestats.c` as new columns.
Segments (v32prof's named script segments) are not modelled: use `--frame-stats-skip`, or cut the CSV by frame number.

## Instruction profile

`--profile-out=FILE` writes how often each address ran; `--profile-by=function` sums by function; `--profile-top=N` prints
the hottest. They need no prompt. For reproducible profiles add `--fixed-clock --seed=1 --fixed-cycles`. Details: README
"Address profile".

`tools/v32sim-addrprof.py run.prof --map cart:cart.vbin.debug --map bios:bios.vbin.debug --map module:m.debug@AUTO`
names addresses in several regions: cartridge/kernel (0x20000000+), BIOS (0x10000000+) and code loaded into RAM (the
lowest RAM address executed is the load base with `@AUTO`). From V32DOS's `tools/profile.py`, which profiled `BASIC.COM`
and `ASM.COM` running under DOS this way.

## Oracle profilers

`v32prof BIOS CART CART.vbin.debug SCRIPT [heavy%] [csv]` reports per-segment CPU/GPU load and cycles per function over all
frames and over heavy frames (>= `heavy%`) only, which is what explains slowdowns. It needs `__function_` labels in the
debug file (`assemble -g program`, or `v32as -g`). `sysprof` is v32nix's whole-system version: it recognizes the program
resident in the user window by comparing words with each candidate image and charges cycles to `k:FUNCTION` / `PROG:FUNCTION`.
