# V32DBG: where each tool stands (2026-11-03)

V32DBG is the source-level debug convention of the Vircon32 tools: comment lines in the assembly that every tool can
pass along, and side files the assembler computes. The spec itself, `V32DBG-SPEC.md`, is said to be kept with v32opt (v32lua
`doc/HINTS.md`) or in v32lua's `proposals/` (v32opt `doc/README-v32srcmark.md`); **it is in neither archive I was given**, so
this status is reconstructed from what each tool documents and from running them. Section numbers follow v32sim's TODO
(1-3 and 5: lines, stepping, frames, `;@src -`; 4: locals and values).

## The pieces

| what | written by | carried by | read by |
|---|---|---|---|
| `;@lang LANG` | v32cc `-g` (`c`), v32lua `-g` | v32opt | v32sim (language label from the file extension today) |
| `;@src FILE:LINE[:COL]`, `;@src -` | v32cc `-g`, v32lua `-g`; stock `compile -g` output via v32opt's `scripts/v32srcmark.py` | v32opt (`-g`, `--check-src`) | v32as `-g` -> `NAME.vbin.debug` (`ADDRESS,FILE,LINE[,FUNCTION]`, `ADDRESS,-,0` for `;@src -`); v32sim |
| `;@fn __function_NAME [lines=A..B] [hints]` | v32cc (with `--emit-hints`, *above* the label), v32lua (*after* the body) | v32opt (reads the hints) | v32as: a `;@fn` after a body ends the function; one above a label does not |
| `;@var NAME loc=... type=... [boxed]`, `;@scope { / }` | v32cc `-g`, v32lua `-g` | v32opt: **kept as text, not updated** | v32as `-g` -> `NAME.vbin.vars`; v32sim `locals` |
| `NAME.asm.sym` (data symbols with sizes) | v32cc `-g` | - | v32sim (`print counter`, `x/d buf`, `--dump=1@counter`) |

## Per tool

| tool | done | missing (owner: this tool) |
|---|---|---|
| **v32sim** | 1-3, 5: line table, `step`/`next`/`finish`/`until`, `break FILE:LINE`, `list`, `bt`, `frame`/`up`/`down`, `;@src -` gaps. **4 (new): `locals [NAME]` / `info locals`; locals before globals in `print`, `x`, `display`, `watch`, `expect`; int/uint/float/bool/ptr, `T[N]`, struct/union words, Lua NaN-boxed values, `boxed` locals through their box, `loc=Rn` in frame 0, `optimized-out`; innermost block wins** | Lua table walking (shows `table: 0xADDR`); struct members (needs member names in `type=`); globals' types; `display NAME` that follows the frame; `;@lang` itself is not read (the extension decides) |
| **v32cc** | `;@lang c`, `;@src`, `;@var` for parameters and locals, `;@scope`, `type=` incl. arrays and `struct:TAG:WORDS`, `.asm.sym` | (a) **no function end**: without `--emit-hints` there is no `;@fn`, and with it the `;@fn` is above the label, so v32as runs every C variable's range to the end of the program (v32sim filters by the function column to cope). Wanted: a `;@src -` or an end-form `;@fn` after each function, or (b) in v32as; (c) globals have no `;@var` (`loc=@ADDR`); (d) struct member names in `type=`; (e) macro origins (a statement from a macro gets the macro's line) |
| **v32as** | `.vbin.debug` from markers with the FUNCTION column, `ADDRESS,-,0` for `;@src -`, `.vbin.vars` with ranges, depths, shadowing, `-C`/`v32asld -g` | the C function end above: a new top-level `__function_X:` could end the previous function when the source says `;@lang c` (v32lua's nested functions are why it does not today) |
| **v32opt** | carries `;@src` through every pass (`-g`, `--check-src`) | **`;@var` / `;@scope` are "reserved, not read"** (`doc/hints.md`): promote-regs / promote-leaf / promote-loops move a variable into a register and leave `loc=BP-n`, so the debugger shows a stale word. Repro: `docs/repro/v32opt-promote-var.c` (at `-O3`: `i = 4194286` from `BP-1` while R5 holds 10; with `loc=R5` written by hand v32sim shows 10). Also inlining copies a callee's `;@var` lines into the caller's frame (not seen in the repro, where inlining was skipped) |
| **v32lua** | `;@lang`, `;@src`, `;@var ... type=lua [boxed]`, `;@scope`, `;@fn` after the body | chunk-level `local`s (in `var_NAME` RAM words) have no `;@var`; upvalues inside a closure have none, so `locals` in a closure shows only its own locals |
| **v32c++** | - | no `-g` markers yet (owner says it will get them); its output goes through the stock `compile`, so today only `v32srcmark.py` gives it `;@src` |
| **stock `compile` / `assemble`** | `compile -g` writes `FILE.asm.debug` (asm line -> C line) | no markers, no variables; `v32srcmark.py` repairs and inserts `;@src` (Tier B). Nothing gives stock output `;@var` |
| **DesktopEmulator** | - | no source-level debugging at all (the V32TRACE recorder is the only debug feature in the patches) |

## Checked here

`tests/run_harness_tests.sh` builds `tests/examples/locals.c` (v32cc -g + v32as -g) and `locals.lua` (v32lua -g + v32as -g) and
checks: block shadowing, `expect` on a local, float and int parameters, `up` showing the caller's variables and not the
callee's, arrays, structs, pointers, Lua strings / booleans / nil / tables, and a boxed Lua local. The stock chain with
`v32srcmark.py` is checked for a source-level `bt`.
