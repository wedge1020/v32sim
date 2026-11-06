# Proposal to v32io / v32dev: one reference encoder, and v32sim as a runner

From v32sim 20261103-dev, against v32io 20261014-dev. Written 2026-11-04. What works and is relied on: v32io's
`tests/v32dev/dev_rom_test.py` runs unchanged against v32sim through v32sim's `tests/v32sim_runner.sh` (a `dev_rom_runner`
look-alike): **41 checks, 0 failures** with this v32io (checked again 2026-11-03), and the v32kbd protocol in v32sim (`kbd`, `--v32kbd`,
`--kbd-input`, `--tty`) reads correctly through the real `keyboard.h`.

| id | sev | what | stopgap today |
|---|---|---|---|
| SI-1 | **B** | four independent encoders of the v32kbd / v32mouse frames, no shared test vectors | each is tested against the library separately |
| SI-2 | C | v32sim is not named as a supported runner of the v32dev and v32io tests | v32sim keeps its own runner script |
| SI-3 | C | the v32mouse event queue (v32nix P-I2) | none |

---

## SI-1  a reference encoder and test vectors  (B)

**What.** The device side of the protocols is implemented in the RP2350 firmware, the emulator patch (`v32io.patch`), v32sim
(`src/v32io.c`, keyboard only) and v32c++'s `tools/vircon32/v32io-script.py` (keyboard and mouse, now in v32sim `tools/`). A
difference between two of them shows up only as a misread key in some program.

**Sketch.** In v32io: `tools/v32io-encode.py` (or the existing script adopted) as the reference, and `tests/vectors/*.txt`: an event
list (`press a`, `type Hi!`, `char 0x8E`, `move 7 -3`, `click left`) and the 11-control state of every frame it produces. Each
implementation checks itself against the vectors; v32sim would run them in `tests/run_tests.sh`.

**If declined.** v32sim keeps checking against the `t_kbd` cartridge read through the real library (as now).

## SI-2  v32sim as a runner  (C)

Mention in `tests/README.md` that `dev_rom_test.py` accepts v32sim's `tests/v32sim_runner.sh` as its runner, so the suite can run
without building the patched emulator. No code change.

## SI-3  v32mouse queue  (C)

v32nix asked for a 64-event `v32mouse` queue like `v32kbd`'s (PROPOSALS P-I2). Status? v32sim emulates a v32mouse (VS-11, done 20261106: `--v32mouse`, `mouse` commands)
and would follow whatever the library expects.

## Questions

1. v32nix's P-E2 (a per-channel completion doorbell so a request can finish within a frame) and P-E3 (serial channel 0 as the kernel
   console): planned, declined, or open? v32sim plays the emulator's part of the mailbox and could prototype P-E2 behind an option.
2. Is the mouse's "at most 5 steps per axis per frame" (as `v32io-script.py` encodes it) part of the protocol or of that tool? An
   emulator moving faster needs to know.
