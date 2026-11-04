# v32nix and V32DOS -- operating systems on Vircon32, and testing them headless (agent card)

## v32nix

A Unix-like OS descended from xv6, built with v32cc: processes with `fork`/`exec`/`wait`, preemption at compiler-inserted safe
points (`--yield-points`), pipes, a logged file system (the root is read-only, inside the cartridge; `/tmp`, `/var` RAM disks),
users, signals, `select`/`poll`, an 80x24 terminal with `termios` and curses, a self-hosting C toolchain (`/usr/bin/cc`). Processes
run at the same addresses in a **user window** and are swapped out (PDP-11 style). Docs: `README.md`, `USAGE.md`, `docs/BUILD.md`,
`docs/ARCHITECTURE.md`, `docs/MACHINE.md`, `docs/DECISIONS.md`, `docs/ROADMAP.md`, `tests/README.md`.

```
make bootstrap BOOT=v32nix-bootstrap-DATE.tar.gz   # once: the toolchain (v32cc, v32as, v32sim, ...)
. tools/env.sh; make; make test                    # 45 self-checking cartridges (int test_errors), headless
make simtest                                       # tools/simtest.sh: v32sim scripts in tests/sim/*.cmd type at the shell
make run                                           # boot in this terminal (v32sim --tty; Ctrl-] = debugger)
```

The keyboard is a v32kbd on gamepad port 2. `tests/sim/*.cmd` are the best examples of v32sim command scripts (`until "^$$"` waits
for the shell prompt, `expect console`). `tools/sysprof.sh` profiles the whole system (BIOS, kernel, the program in the window) on
the stock core (`sysprof` is now in v32sim `tools/oracle/`). `tools/peek.sh` / `klog.sh` read a global / the kernel log ring after
N frames (v32sim `tools/v32sim-peek.sh [-t]` does the same in v32sim).

## V32DOS

A DOS: `COMMAND.COM` with batch files, pipes, redirection; drives `MEM:` (memory card), `RAM:`, `C:` (the cartridge), `B:` (V32BIOS
ROM), `DISK:` (host disks through v32dev); commands are loadable modules (`.COM`); `EDIT`, `BASIC`, `ASM`, a Z-machine, `CHKDSK`,
`DEFRAG`, `SETUP`, `FDISK`. Optional companion BIOS **V32BIOS** (POST, SETUP, boot menu). Docs: `README.md`, `USAGE.md`, `BUILD.md`,
`docs/COMMAND.md`, `docs/MODULES.md`, `docs/BOOT.md`, `docs/V32DEV.md`.

```
make bootstrap BOOT=v32dos-bootstrap-DATE.tar.gz; . tools/env.sh; make full
sh tests/run.sh [CART [BIOS]]             # 11 typed sessions in v32sim, screens compared with tests/golden/ (-u to update)
V32DEV_PY=.../v32dev.py python3 tests/dev.py   # DISK: against the real v32dev host, in v32sim
python3 tools/profile.py basic            # where BASIC.COM's instructions go (kernel / BIOS / module split)
```

`tests/session*.txt` is the session language of v32sim `tools/v32sim-session.py` (`=TEXT` checks, `{TEXT}` waits, `@key ctrl+c`);
the prompt is detected by a breakpoint on `dos_print_prompt` (`--prompt-symbol dos_print_prompt`). Sizes and dates are masked
before comparing.
