# Profiling with v32sim

What a Vircon32 program costs, in the console's own terms: instructions (one cycle each, 250,000 per frame) and GPU
pixels (9 screens, 2,073,600 per frame). Host-emulator cost (OpenGL, audio) is out of scope: v32sim has no such path.

| question | use |
|---|---|
| how loaded is each frame? p95? how many frames are over budget? | `--frame-stats=FILE` |
| how many draws, region switches, dropped draws, which regions cost the pixels? | the counter columns / summary of `--frame-stats`; `stats`, `stats regions`, `stats expect` |
| did my change make it slower? | `tools/v32sim-statdiff.py BEFORE.csv AFTER.csv` |
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

After those six columns the CSV has the GPU / SPU counters of `src/perfstats.c` (VS-13): `draws` (draw commands, dropped ones
included), `dropped` (not rendered: the frame's pixel budget was spent), `clears`, `tex_switches`, `region_switches` (a draw whose
texture / region differs from the previous draw's), `regions` (distinct texture:region pairs drawn), `spu_cmds`, `spu_plays`. The
summary adds each counter's mean, p50, p95, p99, maximum and total, and the `--frame-stats-top=N` (default 8) regions charged the most GPU
pixels:

```
[frame-stats] counter per frame         mean     p50     p95     p99     max        total
[frame-stats] gpu.draws              1109.98    1111    1130    1136    1139       665989
[frame-stats] gpu.region_switches     789.20     790     820     840     846       473522
[frame-stats]       0:4001          600       95385000   37.4%        0      <- tex:region, draws, pixels, share, dropped
```

(translated SMB1 from NEStoV32, 600 frames after the logo: 1,110 draws and 790 region switches a frame, 20 % of the GPU budget,
55 % of the CPU budget; the biggest cost is the full-screen bezel region; the p50/p99 figures above are illustrative.) More columns follow
`spu_plays`: `scaled_draws`, `rotated_draws` (a rotozoomed draw is both), `region_defs` (writes to the region ports), `color_changes`
(GPU_MultiplyColor), `blend_changes`, `spu_assigns`, `spu_params` (channel volume / speed / loop / position writes), `movs_words`,
`sets_words`, `gamepad_reads`, `instructions`. Every counter also has a stable dotted name: `gpu.draws gpu.dropped gpu.clears
gpu.tex_switches gpu.region_switches gpu.regions gpu.scaled_draws gpu.rotated_draws gpu.region_defs gpu.color_changes gpu.blend_changes
spu.cmds spu.plays spu.assignments spu.param_writes bus.movs_words bus.sets_words bus.gamepad_reads cpu.instructions`, plus `cpu.cycles` and
`gpu.pixels` (the two budget columns). `--stats-format=json` writes JSON Lines with those names (`{"frame":N,"gpu.draws":..., ...}`, then a
`{"type":"summary","frames":N,"stats":{"gpu.draws":{"mean","min","p50","p95","p99","max","total"},...}}` line); `--stats`, `--stats-skip`,
`--stats-top` are synonyms of the `--frame-stats` options. `--frame-stats-skip=N` leaves the first N frames out of the counters too.

In a session: `stats` (last frame, mean, p95, max), `stats regions [N]`, `stats reset`, `stats on|off` (every frame's counters are kept for
the percentiles, `off` stops that), `stats save FILE [json]` (every frame, by column name), and `expect stats NAME[.AGG] OP VALUE`
(a gate like `expect`: exit status 1 in batch mode; `stats expect ...` is the same). AGG is `last` (the default), `mean`, `min`, `max`, `p50`,
`p95`, `p99` or `total`, over the frames since the start or since `--frame-stats-skip`; the old `total_NAME`, `max_NAME`, `frames` and
`total_pixels` still work. The percentile is the nearest rank of the sorted per-frame values. `tools/v32sim-statdiff.py BEFORE AFTER
[--skip N] [--sort ratio] [--fail-over PCT] [--fail-columns a,b] [--json]` compares two runs (CSV or JSON Lines; mean, p95, max per column;
`--sort ratio` lists the biggest changes first; exit 1 if the mean of cycles, gpu_pixels or draws rose by more than PCT percent).

Not there yet (the VS-9 proposal): `--profile-event`, GPU/SPU traces and replay (v32orun's logs are most of the format), the
time breakdown and `--frame-log`, channels-playing and memory-card counters. The counters only watch commands go by (a test checks that
`stats off` leaves the machine's output identical).
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
