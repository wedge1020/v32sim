# Proposal: host-independent performance metrics, GPU/SPU command traces and replay for v32sim

Status: proposal for discussion; first slice (4.1 per-frame CPU/GPU CSV and summary, `--frame-stats`) implemented 2026-11-03, see docs/PROFILING.md. Proposed ID: VS-9 (follows the V32DOS scripting items VS-1..VS-8).
Author: wedge1020 (drafted with Claude). Target: v32sim `20261018-dev` or later.

## 1. Summary

v32sim can tell whether a cartridge fits the console's *nominal* budgets (cycles and GPU pixels per
frame). It cannot tell where a cartridge costs the *host* emulator time. A real case shows the gap:
a translated cartridge is within every nominal budget yet runs unusably slowly on the stock
DesktopEmulator, while other carts run at full speed on the same machine.

This proposal adds observability only, with no change to emulated behavior:

1. per-frame counters for CPU, GPU, SPU and bus activity, with summaries and CSV/JSON export;
2. attribution of those counters to the code (function/address) that causes them;
3. a GPU/SPU command trace plus a small replay harness that can run on the host that shows the
   slowdown, so the cost of the video/audio path can be measured without any CPU emulation;
4. budget assertions usable from `--batch` scripts, so toolchains can gate on them in CI.

## 2. Motivating case

NEStoV32 (a profile-guided NES to Vircon32 translator) produces carts for Super Mario Bros. 2,
Mega Man 2 and DuckTales. On an M4 MacBook Air with the latest stock DesktopEmulator:

- the DesktopEmulator CPU gauge reads about 52% on SMB2;
- slowness is immediate, with the macOS spinning beach ball, and sound plays slow and choppy;
- other Vircon32 carts, including GBtoV32 output, run smoothly on the same machine.

Measured in v32sim (SMB2, steady state):

| Metric (per frame)                     | SMB2      | Nominal budget |
| -------------------------------------- | --------- | -------------- |
| CPU instructions (1 cycle each)        | ~126k     | 250,000        |
| GPU pixels drawn                       | ~548k     | 2,073,600      |
| GPU draw commands                      | 965       | no limit       |
| Draws rejected for capacity            | 0         | -              |
| SPU command writes                     | 2-4       | -              |

Port writes over the first 520 frames of SMB2 (about 270 cart frames after the BIOS logo), counted
with an out-of-tree patch of about 20 lines:

| Port group                               | Writes  |
| ---------------------------------------- | ------- |
| `GPU_Command` (draws)                    | 253,304 |
| `GPU_MultiplyColor`                      | 252,984 |
| `GPU_SelectedRegion`                     | 278,005 |
| `GPU_SelectedTexture`                    | 178,443 |
| region definition ports (each of 6)      | 481,359 |

Running the same cart on the stock ConsoleLogic compiled headless with the existing
`tests/ref/refrun` (CPU, buses, GPU/SPU state; video and audio callbacks stubbed) took 0.42 s for
520 frames, about 20 times real time. So the console logic is not the bottleneck. The remaining
suspects are the host video and audio paths: per-draw OpenGL cost (about 58,000 draws/s here),
texture switches, per-quad state changes, or audio buffering. Nothing in v32sim can say which, and
the maintainer's policy (match the DesktopEmulator, quirks included) is about behavior, not host cost.

## 3. What v32sim has today, and the gap

Today: `--profile` / `--profile-by=function` (instruction counts), `GPU_RemainingPixels` capacity
model, `TIM_CycleCounter`, `screenshot`, `--max-frames`, `--fast`, `--frame-us`.

Missing:

- No counters for GPU commands by kind, texture and region switches, state changes, or SPU activity.
- No way to ask which function issues the draws.
- No per-frame time series (only end-of-run or sampled values), so spikes are invisible.
- No machine-readable output for comparing two carts (a slow one against a smooth one).
- No way to hand the host emulator an exact, CPU-free workload to time.

## 4. Proposed features

### 4.1 `--stats[=FILE]` and the `stats` command

Per-frame counters, written as CSV (default) or JSON Lines (`--stats-format=json`), one row per
frame, plus a summary (mean / p50 / p95 / p99 / max) printed at exit or by `stats show`:

- **CPU:** instructions executed, cycles charged (including `MOVS`/`SETS`/`CMPS` extra cost),
  budget used as a percentage of 250,000.
- **GPU:**
  - draw commands, and draws rejected for capacity;
  - pixels drawn and remaining pixels;
  - `GPU_Command` count by command (draw, clear, ...);
  - texture switches (`GPU_SelectedTexture` writes that change the value) and region switches;
  - distinct (texture, region) pairs drawn;
  - region-definition writes;
  - multiply-color and blending changes;
  - draws with scale != 1, draws with rotation != 0, and draws that use blending.
- **SPU:** play/stop/pause commands, sound and channel assignments, speed/volume/position writes,
  channels playing at frame end, sample frames mixed.
- **Bus/IO:** words moved by `MOVS`/`SETS`, gamepad port reads, memory-card accesses.

Commands: `stats on|off|reset|show|save FILE`. Options: `--stats-skip=N` (skip the first N frames,
for example the BIOS logo) so steady state is not diluted.

Counters are plain integer increments in the existing port-write and command paths. Cost with the
feature off must be one predictable branch or less (see 6.2).

### 4.2 Attribution

`--profile-event=KIND` selects what `--profile` counts: `instr` (today's behavior), `gpu-draw`,
`gpu-portwrite`, `spu-write`, `cycles`. With `--profile-by=function`, the existing profile report then
answers "which function issues 965 draws per frame?". For translated carts this maps straight back
to a guest address through the `.labels`/`.asm` debug files.

### 4.3 `--gpu-trace=FILE` / `--spu-trace=FILE` and a replay harness

Record every write to the GPU (0x200-0x2FF) and SPU port ranges with its frame number and value, plus
a snapshot of the texture/region tables at start and each change. Format: a documented, versioned,
little-endian binary file with a text dump mode (`v32sim --trace-dump FILE`).

Add `tests/ref/gpureplay` beside `refrun`: it links the stock ConsoleLogic and, optionally, the
DesktopEmulator's `VideoOutput` / audio path on the host, replays the trace, and reports wall-clock
per frame (mean / p95 / max, frames over 16.7 ms). Because it needs no CPU emulation and no cartridge, it
isolates "how much does the host pay to execute this exact draw stream?" and can be run by any user
on their own machine. It also gives the DesktopEmulator maintainers a self-contained reproducer.

### 4.4 Self-timing and frame pacing

`--time-breakdown` reports how much wall-clock v32sim spends in CPU, GPU raster, SPU and output per
frame, and `--frame-log=FILE` records per-frame lateness against `--frame-us`. These are relative
indicators only: v32sim's software renderer is not OpenGL, so absolute numbers must not be read as
DesktopEmulator performance. The documentation should say so.

### 4.5 Budget assertions for scripts

Extend `expect` to accept stat names, evaluated over the run so far:

```
run 600
expect stats gpu.draws.p95 <= 600
expect stats cpu.cycles.max <= 200000
expect stats gpu.texture_switches.mean <= 50
```

Failure sets exit status 1 as for the existing `expect`. NEStoV32 and similar toolchains can then
gate regressions on cost, not only on correctness.

### 4.6 Comparing two runs

`tools/statdiff` (or a `stats diff A B` command) compares two stats files and lists the metrics that
differ most, ordered by ratio. Running a smooth cart and a slow cart through it is the quickest way to find
which metric separates them.

## 5. Why this belongs in v32sim

- It is the one tool that already runs carts unattended, deterministically, with scripted input.
- Toolchain authors (NEStoV32, GBtoV32, v32cc programs) have no other way to get these numbers; the
  DesktopEmulator's single CPU gauge is not enough, as the motivating case shows.
- The counters sit on code paths v32sim already has (port writes, `gpu_command`, `spu_write`,
  `gpu_new_frame`), so they are small and low risk.

## 6. Design constraints

### 6.1 No behavior change

Metrics must not alter emulated state, timing counters, RNG, or save-states. A test runs the existing
suite with `--stats` on and off and requires identical output hashes.

### 6.2 Overhead

With stats off, a normal run must not slow measurably. Counters are guarded by one flag or compiled
in as unconditional increments of a small static struct, whichever benchmarks better on the existing
test carts. Target: under 2% on a CPU-bound cart.

### 6.3 Stable names

Stat names (`gpu.draws`, `cpu.cycles`, ...) are documented and treated as an interface once released.

## 7. Implementation sketch

| Area                  | Where                                                           |
| --------------------- | --------------------------------------------------------------- |
| Counters and summary  | new `src/stats.c`, `inc/stats.h`                                |
| Port-write counting   | `ioports.c` write path (one hook after address masking)         |
| Draw/clear counting   | `gpu.c`: `gpu_command`, `gpu_draw`; frame rollover in `gpu_new_frame` |
| SPU counting          | `spu.c`: `spu_write`, `spu_new_frame`                           |
| Options and commands  | `main.c`, lexer/parser (`stats`, `expect stats`)                |
| Profile event select  | existing `--profile` code                                       |
| Traces                | `src/trace.c`; format documented in `docs/`                     |
| Replay harness        | `tests/ref/gpureplay.cpp`, `build.sh` target                    |
| Docs and tests        | README section, `tests/run_stats_tests.sh`                       |

A throwaway prototype of the GPU/SPU counters (`GPUSTAT`, `SPUSTAT` and `PORTCNT` environment
variables) is about 20 lines across `gpu.c`, `spu.c` and `ioports.c`; the numbers in section 2 came
from it. If the maintainer agrees on the interface, it can become the first PR.

## 8. Phasing

1. **Phase 1:** 4.1 and 4.5 (counters, CSV, `expect stats`). Useful on its own for budget gating.
2. **Phase 2:** 4.2 and 4.6 (attribution and comparison).
3. **Phase 3:** 4.3 (traces and the replay harness), which answers the host-side question.
4. **Phase 4:** 4.4 (self-timing and frame log).

## 9. Test plan

- Unit-style carts with known counts (N draws, M texture switches, K SPU writes); assert exact stats.
- Determinism: all existing suites with `--stats` on vs off, identical outputs.
- Trace round trip: record, dump to text, replay in `refrun`-style headless mode, and compare final
  GPU state hashes with the original run.
- `expect stats` pass/fail exit statuses.
- Overhead check on a CPU-bound cart.

## 10. Open questions for the maintainer

1. Should counters live in v32sim only, or also be offered as an optional hook in ConsoleLogic so the
   DesktopEmulator could show the same numbers?
2. Preferred trace format: binary with a text dump, or text only?
3. Is the replay harness welcome under `tests/ref/`, or should it live in a separate tools directory?
4. Any objection to the stat names in 4.1 being treated as a stable interface?

## 11. Out of scope

Predicting absolute DesktopEmulator frame times; changing the emulated GPU capacity model; audio
output (v32sim does not mix audio); fixing the host-side slowdown itself, which would be a
DesktopEmulator issue once the metrics point at a cause.
