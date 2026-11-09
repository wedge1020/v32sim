# V32DBG: where each tool stands (2026-11-09)

V32DBG is the source-level debug convention of the Vircon32 tools: comment lines in the assembly that every tool can
pass along, and side files the assembler computes. The spec is [`V32DBG-SPEC.md`](V32DBG-SPEC.md), **hosted in v32sim** (the
projects' own docs point here). Section numbers follow v32sim's TODO (1-3 and 5: lines, stepping, frames, `;@src -`;
4: locals and values).

## The stack, and the one command that builds it

```
 C source     ---- v32cc -g ------------------------------------+
 C++ source   -- v32c++ -g --compiler=v32cc (#line) -- v32cc -g -+--> NAME.asm  --[ v32opt -O1/-O2/-O3 ]--> v32as -g --> NAME.vbin
 Lua source   ---- v32lua -g ----------------------------------+        (;@lang ;@src ;@var ;@scope ;@fn ;@home)      + .debug .vars .sym
                                                                                                                             |
                                                                                           v32sim: break FILE:LINE, step, bt, locals, print
```

`tools/v32mkcart.sh [-O LEVEL] SRC.c|.cpp|.lua|.asm` runs the right chain for the language (the same chain each project's own
`Make.sh` runs with `-g`) and leaves a cartridge with every debug file beside it, which v32sim finds by itself. `tests/run_stack_tests.sh`
(the `stack` suite of `make check`) builds one program in all three languages at `-O0`, `-O2` and `-O3` and checks, in v32sim, that it
stops on source lines of the source language, names the function in `bt`, shows locals that are true (or `<optimized out>`, never wrong),
and puts a macro's statements on the line that uses the macro. Every combination that is viable is in that matrix; the stock
`compile`/`assemble` chain is the separate tier B/C below.

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
| **v32sim** | 1-3, 5: line table, `step`/`next`/`finish`/`until`, `break FILE:LINE`, `list`, `bt`, `frame`/`up`/`down`, `;@src -` gaps. **4 (new): `locals [NAME]` / `info locals`; locals before globals in `print`, `x`, `display`, `watch`, `expect`; int/uint/float/bool/ptr, `T[N]`, struct/union words, Lua NaN-boxed values, `boxed` locals through their box, `loc=Rn` in frame 0, `optimized-out`; innermost block wins** | struct members of Lua tables beyond one level (nested tables show only their address); `display NAME` that follows the frame; `;@lang` itself is not read (the extension decides, on purpose) |
| **v32cc** | `;@lang c`, `;@src`, `;@var` for parameters and locals, `;@scope`, `type=` incl. arrays, `struct:TAG:WORDS{x:int@0;...}` (SC-3), `.asm.sym`; **function end** (`;@src -` after each function, 2026-11-04); **globals** (`;@var NAME loc=global_NAME type=T`, SC-2, 2026-11-05); **`type=char`** (2026-11-08); **macro origins** (SC-5, 2026-11-09: a statement a macro writes is on the line that *uses* the macro, never in the header that defines it; `tests/debuginfo.py`) | nothing known. Stepping *into* a macro body is not offered (the use line is the unit, as in gdb) |
| **v32as** | `.vbin.debug` from markers with the FUNCTION column, `ADDRESS,-,0` for `;@src -`, `.vbin.vars` with ranges, depths, shadowing, `-C`/`v32asld -g`; `;@home` (v32opt's moves, 20261105); variables outside a function last to the end of the program; `.vbin.sym` | nothing known |
| **v32opt** | carries `;@src` through every pass (`-g`, `--check-src`); **`;@var loc=` is rewritten on promotion** (leaf, register, loop promotion) with `;@home FROM TO` records, inlining no longer copies the callee's `;@var` / `;@scope`, `--check-vars` (20261105-dev; checked 2026-11-08: `docs/repro/v32opt-promote-var.c` at `-O3` gives `i = 10 (R5)`, `s = 45 (R4)`, and `tests/run_harness_tests.sh` checks `-O0/-O2/-O3`) | **dead variables** (2026-11-09): a `;@var NAME loc=Rn` whose register no instruction of its region writes any more (unrolling, constant propagation, dead-register elimination) is rewritten `;@var NAME optimized-out type=T`; v32as writes the flag into `.vbin.vars`, v32sim prints `<optimized out>` (`testing/vars/dead.asm`). A later pass that renames a promoted register is not followed (ranges can be a few instructions approximate) |
| **v32lua** | `;@lang`, `;@src`, `;@var ... type=lua [boxed]`, `;@scope`, `;@fn` after the body; chunk-level `local`s and globals as `;@var NAME loc=@ADDR type=lua global`, closure upvalues as `loc=BP+N type=lua boxed upvalue`; **2026-11-09:** a function's prologue carries the line of its `function` keyword (without it v32opt gave the prologue `;@src -` and v32as ended the function at its label), and a name that is only assigned inside functions that declare it local is no longer listed as a global | nothing open for V32DBG |
| **v32c++** | **`-g` is the whole chain** (2026-11-09): with `--compiler=v32cc` (or `$V32CXX_COMPILER`, or a compiler named `v32cc*`) `-g` writes the `#line` markers by itself, and the generated `Make.sh`/`Make.bat` run `v32cc -S --target-compat=v32cxx -g` and `v32as -g`; `--compiler-style=stock|v32cc` overrides the guess. The C++ file and line reach `;@src`, locals and `this` keep their C++ names, methods show as `Class__method__sig`. `tests/debug_flow.sh`. With the stock `compile` (the default compiler) `-g` still writes only the `<out>.c.debug` C-line/C++-line map | `;@var` for the transpiler's own temporaries starts with `__v32_` (v32sim skips them); no register homes of its own |
| **stock `compile` / `assemble`** | `compile -g` writes `FILE.asm.debug` (asm line -> C line) | no markers, no variables; `v32srcmark.py` repairs and inserts `;@src` (Tier B). Nothing gives stock output `;@var` |
| **DesktopEmulator** | - | no source-level debugging at all (the V32TRACE recorder is the only debug feature in the patches) |

## Checked here

`tests/run_harness_tests.sh` builds `tests/examples/locals.c` (v32cc -g + v32as -g) and `locals.lua` (v32lua -g + v32as -g) and
checks: block shadowing, `expect` on a local, float and int parameters, `up` showing the caller's variables and not the
callee's, arrays, structs, pointers, Lua strings / booleans / nil / tables (with their contents), and a boxed Lua local. The stock chain with
`v32srcmark.py` is checked for a source-level `bt`. `tests/run_stack_tests.sh` is the matrix described at the top: C, C++ and Lua, each at
`-O0/-O2/-O3`, 33 checks. Each project checks its own producer: v32cc `make debugtest` (200 checks, incl. macro origins), v32c++
`tests/line_markers.sh` and `tests/debug_flow.sh`, v32opt `testing/srcmarks` and `testing/vars`, v32lua `tests/audit/hints/check_hints.py`.

## Against the spec (2026-11-09)

The spec's own summary table is the contract; v32sim's side of it:

| spec item | v32sim |
|---|---|
| `;@src FILE:LINE[:COL]`, `;@src -`; the last `:` separates the line, a drive letter may precede | read through `.vbin.debug` (v32as `-g`); a `-` entry ends the source (`run_src_tests.sh`) |
| `loc=` forms `BP-n`, `BP+n`, `Rn`, `@0xADDR`, `NAME` | all five (`src/locals.c` `locate`); `Rn` only in frame 0 (registers of outer frames are not saved) |
| `optimized-out` | shown as `<optimized out>`; v32opt writes it for a variable whose register nobody writes any more, so an optimized build never shows a stale register |
| `type=` `int uint float ptr bool lua` | shown; `char` **added 2026-11-08** (`65 'A'`); `lua`/`lua-table` values that are tables show their array part and up to 8 entries (`table: 0x79 {1.0000, 2.0000, 3.0000}`, `["x"] = 3.0000` for hash entries, 2026-11-08); unknown types print the raw word. v32cc writes it since 2026-11-08 for `char`, `signed char`, `unsigned char` and the 8-bit `__int8` types (checked end to end: `65 'A'`) |
| `;@lang` selects how values are shown | **not read, by decision**: the source file's extension decides (`source.c`); equivalent while every producer names its file honestly, and the `lang=` on v32lua's `;@hints` line says the same |
| `;@fn NAME lines=A..B` (source range) | not used, by decision: the line table already gives each function's first and last line; v32as only uses the `;@fn` placement |
| `loc=` rewritten by v32opt when promoting ("Locations after optimization") | **done by v32opt 20261105-dev** (checked at `-O0/-O2/-O3`, `tests/examples/vars_opt.c`); v32sim reads the `Rn` ranges and `optimized-out` |
| command-line convention (`-g`, `-g0`, `v32opt` keeps `;@src`, `v32as -g` reads markers) | v32sim only consumes; v32as, v32cc, v32lua and v32opt are checked by the harness (`run_harness_tests.sh`) |
| Tier B (stock `compile` + `v32srcmark`) | checked (source-level `bt` on the stock chain); tier C (labels only) works as function-level `break` / `bt` |

## What is still not covered

* **Stock `compile` + `assemble`** (the Vircon32 devtools, tier B/C): lines through `v32srcmark.py`, labels only otherwise; no variables. Not a gap
  of ours: the stock compiler rejects `#line` and writes no `;@var`. Use v32cc (and v32c++ with `--compiler=v32cc`) to debug.
* `Rn` locations are read in frame 0 only: the registers of outer frames are not saved on a `CALL`.
* `display NAME` does not follow the frame; Lua tables show their first level (nested tables: the address).
* A macro body cannot be stepped into (the use line is the unit); a statement written by v32c++ with no C++ counterpart (a destructor call, an
  allocator) takes the line of the nearest statement.
* DesktopEmulator has no source-level debugging.
