# Proposals from v32sim to the other Vircon32 projects (2026-11-04)

One file per project: features, gaps, issues and questions found while consolidating the testing / debugging tools into v32sim
and implementing V32DBG locals and V32TRACE 2.1. Every item says what was observed and on which version; items marked
"checked" were run here. IDs carry an `S` prefix ("from v32sim") so they cannot be confused with the projects' own request
series (V32DOS's `VC-`/`VA-`/`VS-`, v32nix's `VC-`/`VA-`/`VS-`/`VM-`, v32nix's `P-`).

Severity, as in the existing proposals: **A** blocks a stated goal, **B** forces a workaround or silently gives a wrong answer,
**C** convenience.

| file | to | items |
|---|---|---|
| [PROPOSAL-v32cc.md](PROPOSAL-v32cc.md) | v32cc 20261102-dev | SC-1 .. SC-5, 2 questions |
| [PROPOSAL-v32as.md](PROPOSAL-v32as.md) | v32as / v32asld 20261101-dev | SA-1 .. SA-4, 2 questions |
| [PROPOSAL-v32opt.md](PROPOSAL-v32opt.md) | v32opt 20261030-dev | SO-1 .. SO-4, 2 questions |
| [PROPOSAL-v32lua.md](PROPOSAL-v32lua.md) | v32lua 20261030-dev | SL-1 .. SL-4, 2 questions |
| [PROPOSAL-v32cxx.md](PROPOSAL-v32cxx.md) | v32c++ | SX-1 .. SX-3, 2 questions |
| [PROPOSAL-v32io.md](PROPOSAL-v32io.md) | v32io / v32dev 20261014-dev | SI-1 .. SI-3, 2 questions |
| [PROPOSAL-GBtoV32.md](PROPOSAL-GBtoV32.md) | GBtoV32 20261101-dev (and NEStoV32, for the shared spec) | SG-1 .. SG-5 (SG-5 and both questions answered 2026-11-04) |
| [PROPOSAL-V32DOS.md](PROPOSAL-V32DOS.md) | V32DOS | SD-1 .. SD-4, 1 question |
| [PROPOSAL-v32nix.md](PROPOSAL-v32nix.md) | v32nix 20261102-dev | SN-1 .. SN-3, 2 questions |
| [PROPOSAL-ComputerSoftware.md](PROPOSAL-ComputerSoftware.md) | Vircon32 ComputerSoftware (DesktopEmulator, DevTools) | SE-1 .. SE-4, 1 question |
| [PROPOSAL-v32sim.md](PROPOSAL-v32sim.md) | v32sim itself: the backlog these surveys produced | VS-10 .. VS-18 |

Where two projects could each solve one problem (a C function's end for debug ranges: v32cc SC-1 or v32as SA-1), both files say
so and either answer closes both items.
