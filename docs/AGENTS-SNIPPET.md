# Drop-in section for another project's AGENTS.md

Copy the block below into the `AGENTS.md` (or `CLAUDE.md`) of a project that builds Vircon32 cartridges, and fill in the
three `<...>` paths. It is written for an agent that has never seen v32sim.

---

## Testing and debugging with v32sim

`v32sim` (<path to v32sim>) is a headless Vircon32 simulator with a debugger. Use it instead of the DesktopEmulator window:
it runs unattended, takes scripted input, and its exit status says what happened. Tools and recipes are in
`<path to v32sim>/docs/TOOLS.md` and `docs/TESTING.md`.

- **Build a cartridge with debug files**: `<v32sim>/tools/v32mkcart.sh -o out src/prog.c` (v32cc + v32as by default). It leaves
  `prog.v32`, `prog.asm.sym` (data symbols), `prog.vbin.debug` (addresses to C lines/functions) beside each other; v32sim
  finds them by itself. The stock `compile`/`assemble` write no enhanced debug info: with them you get addresses and
  `%define global_X` data symbols only (`--stock`; add `--srcmark` with v32opt's `v32srcmark.py` for `;@src` markers).
- **Does a self-checking test pass?** (`int test_errors = -1;` set to 0 at the end, then halt):
  `<v32sim>/tools/v32sim-selfcheck.sh -B StandardBios.v32 out/t_x.v32`. Add `--oracle` to ask the stock console logic too.
  By hand: `v32sim --batch -r --fast -x --max-frames=1200 -B StandardBios.v32 --dump=1@test_errors cart.v32`, then read the
  exit status (0 halted, 2 frame limit = failed, 3 system error) and the printed value (0 = pass).
- **Look inside**: `v32sim cart.v32` is the debugger (`break main`, `step`, `next`, `finish`, `bt`, `print counter`,
  `x/d buf 10`, `watch [counter] == 6`, `display`, `console`, `screenshot out.png`). From a script, put the commands in a file
  and run `v32sim --batch --fast -x -B StandardBios.v32 cart.v32 < script.cmd`; stdout is only what the script printed,
  diagnostics go to stderr. A program that ends with `HLT` ends the run, so read its state with `--dump=N@SYMBOL`.
- **Locals**: with `v32cc -g` (or `v32lua -g`) and `v32as -g`, `locals` shows the variables of the selected frame (`up`/`down`);
  `print`/`expect` find a local before a global. Debug unoptimized code: v32opt -O2 and up leaves stale variable locations.
- **Wait on the screen, not on frame counts**: `until "TEXT" timeout 600`, `expect console "TEXT"`, `run 60`.
  Keyboard: `kbd attach 2` then `kbd type "ls\n"` (a v32io keyboard on gamepad port 2). Gamepad: `gamepad 1 left` toggles, or
  `--input=A@40-44,START@100` holds buttons by cartridge frame.
- **Is it fast enough?** `--frame-stats=load.csv` (per-frame CPU/GPU %, summary on stderr), `--profile-by=function --profile-top=15`.
  The BIOS logo takes ~250 frames before a cartridge starts; `--frame-stats-skip=250` leaves it out.
- **Reproducible runs**: add `--fixed-clock --seed=1 --fixed-cycles`.
- **If v32sim and the real emulator disagree**, run the cartridge on the oracle (`<v32sim>/tools/oracle`: `v32run`, `refrun`) and
  report a v32sim bug in `<v32sim>/TODO.md` or `docs/BUGS.md` with the cartridge and both outputs. Do not work around it in
  your project's source.
- **Always** use `-x` in tests (otherwise a frame limit or a system error exits 0) and `--max-frames` (otherwise a hung
  cartridge hangs the test).

---

Replace the paths; delete bullets your project does not need. If the project has its own debug-info producer (v32cc, v32lua),
say so next to the first bullet.
