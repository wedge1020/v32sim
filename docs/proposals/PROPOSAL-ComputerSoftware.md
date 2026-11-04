# Proposal to Vircon32 ComputerSoftware (DesktopEmulator, DevTools)

From v32sim 20261103-dev, against the ComputerSoftware copy supplied 2026-11-03 (compiler v26.04.24; same sources as the V32DOS
bootstrap of 2026-11-01; the `ExtendedDevTools` submodule folders are empty). Written 2026-11-04. Context: v32sim is checked
instruction for instruction against `DesktopEmulator/ConsoleLogic` compiled headless (`tests/run_gaps.sh` 46/46, `run_fuzz.sh`
3264/3264, `run_romcheck.sh` 28/28), so the console logic is the reference for several projects' tools.

| id | sev | what | stopgap |
|---|---|---|---|
| SE-1 | **B** | `CFI` of NaN or out-of-range floats is C++ undefined behaviour: x86-64 and ARM64 builds of the emulator differ | v32lua's runner emulates one host's behaviour by choice (`V32_CFI`) |
| SE-2 | C | the useful emulator patches live outside: v32io and v32dev (v32io) | users build patched emulators |
| SE-3 | C | no headless target: every project compiles ConsoleLogic with its own stubs (six runners exist) | v32sim `tools/oracle/build.sh` |
| SE-4 | C | `SimplifiedEmulator` is gone; v32lua's headless runner depended on it | port it (v32lua proposal SL-3) |

---

## SE-1  a defined float-to-int conversion  (B)

**What.** `V32CPUProcessors.cpp` line 469: `Register->AsInteger = (int32_t)Register->AsFloat;`. For NaN or values outside the int32
range this is undefined: x86-64 gives `0x80000000`, ARM64 gives 0 for NaN and saturates otherwise (measured and documented by v32lua's
`tools/headless/README.md`). A cartridge that converts such a value behaves differently on an Apple Silicon Mac than on a PC.

**Sketch.** Define it, matching what most players run today (x86-64): NaN and out-of-range give `0x80000000`
(`if (!(f >= -2147483648.0f && f < 2147483648.0f)) result = INT32_MIN; else result = (int32_t)f;`). v32sim would match.

**If declined.** Document it as unspecified in the CPU specification, so tools stop testing it.

## SE-2  optional builds of the known patches  (C)

`v32io.patch` and the v32dev patch (v32io: keyboard / mouse devices in the Gamepads menu; the memory-card mailbox). (GBtoV32's
V32TRACE recorder patch is not part of this request: it stays an optional patch kept in GBtoV32, by its owner's decision.) As CMake options (off by default), they would be built
and tested with upstream changes instead of being rebased by each project. Each already leaves the console's behaviour unchanged when
unused.

## SE-3  a headless target  (C)

A small `ConsoleLogic` program in the tree (`V32Headless`: BIOS + cartridge + card, N frames, a frame-stamped input script, RAM words and
CPU state at the end, optional logs of GPU and SPU writes) would replace the six copies: v32sim's `refrun`, v32c++'s `v32run`/`v32prof`/
`v32shot`/`v32peek`, v32cc's `vrun`, v32lua's `v32run`, and v32nix's `sysprof` built on the same core. v32sim's `tools/oracle/` shows
what the callbacks need (all stubbed; `shim/SDL.h` stands in for the one SDL call the dev tools make).

## SE-4  `SimplifiedEmulator`  (C)

If its removal was deliberate, nothing to do here; v32lua is told separately.

## Question

1. The copy supplied has no git metadata. Which commit is it? v32c++'s tools were verified at `5219741`, v32sim's gap document compares
   against `e75b537`; knowing the current one lets the tools pin it.
