# Agent quick references for the Vircon32 ecosystem

One short, AGENTS.md-style card per machine, language and tool: what an agent needs in its first ten minutes, the mistakes
agents make most, and where the full documentation is. Each card stands alone; import the ones a project needs from its own
`AGENTS.md` (for example a line `@path/to/v32sim/docs/agents/V32CC.md` in a `CLAUDE.md`, or copy the text).

Written 2026-11-03 against: v32sim 20261103-dev, v32cc 20261102-dev, v32as 20261101-dev, v32opt 20261030-dev, v32lua
20261030-dev, v32c++ (v32cxx, late October), v32io/v32dev 20261014-dev, GBtoV32 20261101-dev, Vircon32 ComputerSoftware
(DevTools / compiler v26.04.24). Options were taken from each tool's `--help`; when a card and a tool disagree, the tool wins.

| card | for |
|---|---|
| [VIRCON32.md](VIRCON32.md) | the machine: CPU, memory map, I/O ports, frame and cycle model, BIOS behaviour, error codes |
| [TOOLCHAIN.md](TOOLCHAIN.md) | how a cartridge is built, which tool does which step, which files go where, debug files |
| [STOCK-C.md](STOCK-C.md) | the Vircon32 C dialect of the stock `compile` (what it rejects, what it does differently) |
| [V32CC.md](V32CC.md) | v32cc, the alternate C compiler (ordinary C, objects, hosted libc, `-g`) |
| [V32AS.md](V32AS.md) | v32as / v32asld, the alternate assembler and linker |
| [V32OPT.md](V32OPT.md) | v32opt, the assembly optimizer |
| [V32LUA.md](V32LUA.md) | v32lua, the Lua compiler (and its PICO-8 / TIC-80 modes) |
| [V32CXX.md](V32CXX.md) | v32c++, the C++ / C to Vircon32 C transpiler |
| [V32IO.md](V32IO.md) | v32io (keyboard and mouse through gamepad ports) and v32dev (host files and terminal through the memory card) |
| [V32SIM.md](V32SIM.md) | v32sim, the simulator and debugger, and the testing / profiling tools around it |
| [DEBUG-INFO.md](DEBUG-INFO.md) | V32DBG markers and the side files (`.vbin.debug`, `.vbin.vars`, `.asm.sym`) |
| [TRANSLATORS.md](TRANSLATORS.md) | GBtoV32 / NEStoV32 style trans-assemblers and the V32TRACE interface |
| [SYSTEMS.md](SYSTEMS.md) | v32nix and V32DOS: what they are, and how they are tested headless |

Projects that already have a full `AGENTS.md` (read it when working *on* them): v32c++ (`AGENTS.md`), v32lua
(`doc/AGENTS.md`), GBtoV32 (`AGENTS.md`), v32sim (`AGENTS.md`). The cards for those are summaries for people working *with*
them.
