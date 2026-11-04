# v32io and v32dev -- keyboard, mouse, host files and a terminal (agent card)

Two independent libraries (C headers you include in a cartridge or BIOS), version 20261014-dev. Docs: `README.md`,
`lib/v32io/README.md`, `lib/v32dev/README.md`, `docs/v32io/PROTOCOLS.md`, `docs/v32dev/PROTOCOL.md`, `man v32io`, `man v32dev`.

| | v32io | v32dev |
|---|---|---|
| gives a program | a keyboard (`keyboard.h`, `v32kbd`) and a mouse (`mouse.h`, `v32mouse`) | host files (`storage.h`, block device) and a terminal (`serial.h`, char device) |
| carried by | the 11 controls of a gamepad port | a mailbox in the last 64 KiB of the memory card (4 channels) |
| needs | an RP2350 USB adapter (real hardware) or the patched DesktopEmulator | the patched DesktopEmulator (or v32sim) and the host tool `tools/v32dev.py` |

## v32io: the device is a gamepad

The 11 controls are packed into bits in INP port order (bit = port - `0x402`). **Keyboard**: one key event per frame; the
*strobe* (Left, then Right, alternately) marks a new event; Up = pressed, Down = released; Start..R = 7-bit key code. Codes are
keys, not characters (`a`-`z`, digits, punctuation as unshifted US keys; 1-4 arrows, 6/7 shifts, 8 backspace, 9 tab, 13 enter,
14-25 F1-F12, 27 escape, 127 delete; 30/31 codepage prefixes for typed characters outside ASCII). **Mouse**: X and Y movement as
trit counters plus a Gray-coded pair per axis, at most 5 steps per axis per frame; Start/A/B are middle/left/right.
**Every driver must be probed once per frame** (`v32kbd_probe`, then `v32kbd_read` until 0), or events are lost.

```c
v32kbd *keyboard = v32kbd_init (1);      // gamepad port 1
while (true) { v32kbd_probe (&keyboard); while ((key = v32kbd_read (&keyboard)) > 0) { ... } end_frame (); }
```

## v32dev: the memory card is a mailbox

Each channel is a pair of rings plus control words; the host announces a device (block, char; pipe and socket reserved). A request
and its reply take at least one frame. Probe every driver once per frame; the waiting calls end frames themselves.

```
tools/v32dev.py serve game.memc --ch0 storage:disk.img --ch1 serial:stdio      # the host, beside the running emulator
```

## Testing without hardware

- **v32sim** emulates a `v32kbd` on any port (`--v32kbd=N`; `kbd type "ls\n"`, `kbd key`, `--kbd-input=stdin|FIFO|file`, `--tty`) and
  plays the emulator's part of the v32dev mailbox, so a real `v32dev.py serve` talks to a cartridge in v32sim (the v32io suite
  `tests/v32dev/dev_rom_test.py` passes 41/41 through v32sim's `tests/v32sim_runner.sh`). v32sim has **no v32mouse**: script
  mouse input as gamepad presses with `v32cxx`'s `tools/v32io-script.py` (in v32sim `tools/`) and `tools/vprof2cmd.py`.
- The v32io project's own tests: `tests/run.sh`, `tests/v32dev/run.sh` (unit, order, stress, end-to-end with the host),
  `tests/v32io/run.sh`.
