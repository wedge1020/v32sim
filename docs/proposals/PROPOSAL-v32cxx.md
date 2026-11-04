# Proposal to v32c++: source-level debugging, and the headless tools that moved

From v32sim 20261103-dev, against the v32cxx project of late October 2026, v32cc 20261102-dev and the stock compiler v26.04.24.
Written 2026-11-04. Context: v32c++'s `tools/vircon32/{v32run,v32prof,v32shot,v32peek}.cpp` and `v32io-script.py` are now in v32sim
(`tools/oracle/`, `tools/`), built by one `tools/oracle/build.sh`; `v32sim/tools/v32sim-selfcheck.sh` runs the self-checking samples
(`int test_errors = -1;`, a `.input` script beside it) on v32sim or, with `--oracle`, on the stock core.

| id | sev | what | stopgap today |
|---|---|---|---|
| SX-1 | **B** | no `-g`: a C++ program cannot be debugged by its own lines | `v32srcmark.py` on `compile -g` output gives lines of the *generated* C |
| SX-2 | C | no variable records for C++ locals (`this`, parameters, locals) | none |
| SX-3 | C | `tools/vircon32/` duplicates the oracle tools now kept in v32sim | none needed; both build |

---

## SX-1  carry C++ lines through: `#line` for the v32cc route  (B)

**What.** The output is Vircon32 C for the stock `compile`. The stock compiler rejects `#line` (checked: `fatal error: unsupported
preprocessor directive "line"`), but v32cc honours it and writes the original file and line into `;@src` (checked: `#line 100
"orig.cpp"` -> `;@src orig.cpp:101`). v32cc reads v32c++ output with `--target-compat=v32cxx` (NULL is 0).

**Sketch.** `v32c++ -g` (or `--line-markers`) writes `#line N "file.cpp"` before each statement it emits; documented as "compile with
v32cc -g". Without the option nothing changes for the stock route. Then `v32cc -g --target-compat=v32cxx out.c` + `v32as -g` gives
v32sim `break game.cpp:42`, `step`, `bt` with C++ lines.

**If declined.** Source-level debugging of C++ stays on the generated C (readable, but not the user's file).

## SX-2  variables  (C)

With SX-1 through v32cc, `;@var` comes for free for every C local v32cc sees, under the names v32c++ gives them. Two asks: keep
source names for locals and parameters where possible (and `this`), and say in the docs which mangled names a user will meet
(`Shape__area__void`, `__v32_ret`, `__v32_byval_x`).

## SX-3  one copy of the headless tools  (C)

`tools/vircon32/build-tools.sh` can keep building `compile`, `assemble`, `packrom` and the SDK headers, and take `v32run`/`v32prof`/
`v32shot`/`v32peek` from v32sim (`ONLY="v32run v32prof v32shot v32peek" v32sim/tools/oracle/build.sh CHECKOUT OUT`). `check.sh`'s
emulator half can be `v32sim-selfcheck.sh --oracle out/*program.v32` (it finds `test_errors` from the `.asm` and plays `.input`). The
SDL shim was merged (the `/proc/self/exe` one).

## Questions

1. Is v32c++ output meant to be compiled by v32cc as well as by the stock compiler? SX-1 depends on that route being supported.
2. `AGENTS.md` says v32c++ will get `-g` "eventually": is a different marker plan intended (for example writing `;@src` lines into
   `asm { }` blocks, which the stock compiler passes to its output)? That would work on the stock route too.
