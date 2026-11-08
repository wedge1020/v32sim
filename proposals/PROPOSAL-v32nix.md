# Proposal to v32nix: shared tools, and the emulator extensions it asked for

From v32sim 20261103-dev, against v32nix 20261102-dev. Written 2026-11-04. Context: v32nix's `tools/sysprof/sysprof.cpp` is now in v32sim
`tools/oracle/` (built by its `build.sh`), its `peek.sh` / `klog.sh` have a v32sim equivalent (`tools/v32sim-peek.sh [-t]`), its
`tests/sim/*.cmd` are cited as the model command scripts in v32sim's `docs/TESTING.md`, and PROPOSALS P-E6 (a headless runner with
scripted v32io and v32dev) is met by v32sim.

| id | sev | what | stopgap |
|---|---|---|---|
| SN-1 | C | `tools/v32sim/build.sh`, `peek.sh`, `klog.sh`, the `sysprof` build duplicate v32sim tools | none needed |
| SN-2 | C | P-E1 (timer interrupt) and P-E2 (v32dev doorbell) have no implementation anywhere | cooperative / compiler safe points; one frame per request |
| SN-3 | C | `docs/HANDOFF.md`'s v32sim notes | documented in v32sim |

---

## SN-1  thin wrappers  (C)

`tools/bootstrap.sh` can build v32sim with v32sim's `tools/build-nogen.sh` and `sysprof` with `ONLY=sysprof tools/oracle/build.sh`;
`tools/peek.sh CART ASM GLOBAL COUNT FRAMES` is `v32sim-peek.sh -F FRAMES CART GLOBAL COUNT` (the address comes from `CART.asm.sym`,
so the `.asm` argument goes), `klog.sh` is `v32sim-peek.sh -t -F FRAMES CART klog N`. They run in v32sim instead of the stock core;
`--oracle` style runs stay possible with `v32peek`.

## SN-2  P-E1 / P-E2 in v32sim first  (C)

v32sim could implement both behind options so the kernel side can be written and tested before any emulator has them:
`--v32irq`: a port (`TIM_Alarm`?) holding cycles until an interrupt and a RAM cell holding the vector; when it fires, push IP and jump.
`--v32dev-doorbell`: a per-channel "completed" counter word in the mailbox, and the host's reply read within the frame. Needed from
v32nix: the exact interface (port number, cell addresses, what a fired interrupt saves, how it returns). v32sim's differential tests
would keep both off by default.

## SN-3  HANDOFF notes  (C)

"`until` patterns are not full regex" and "a user program that executes `HLT` ends the emulator silently": both still true; v32sim's
`docs/TESTING.md` lists them under Gotchas. With `-x` a halt is exit status 0 and a frame limit 2, so a test can tell them apart.

## Questions

1. For SN-2: is P-E1 still wanted now that `--yield-points` preemption works (D30)? If not, only P-E2.
2. Could a v32nix bootstrap archive be shared? v32sim's harness has not run `make test` / `make simtest` / `sysprof` on v32nix yet;
   with it, `sysprof` would get a real check and `simtest.sh` could be tried with `v32sim-golden.sh`.
