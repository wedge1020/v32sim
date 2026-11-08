# V32DBG: source locations and language information across the Vircon32 toolchain

Status: proposal, 2026-10-30. `v32lua` already writes the parts marked **(v32lua: done)**.
Parties: `v32lua`, `v32cc` / `v32c++`, `v32opt`, `v32as`, `v32sim`, and the stock
DevelopmentTools C compiler (`compile`) with the stock `assemble`.

## Problem

Source-level debugging in `v32sim` (step by line, break at file:line, show the source,
show locals) needs three facts for every instruction: which source file and line it came
from, which function it is in, and where the named variables live. Today:

* `v32cc -g` and `v32lua -g` write a side file keyed by **assembly line number**
  (`asmfile,asmline,srcfile,srcline[,function]`), and `v32as -g --map` turns it into
  `address,file,line[,function]`.
* A side file keyed by assembly line dies the moment `v32opt` rewrites the assembly:
  lines move, vanish and are duplicated (unrolling, inlining), and `v32opt` has no way to
  update a file it does not know about. So source-level debugging works only for `-O0`.
* The stock compiler writes nothing of the kind (a side file only, with inexact asm lines; see tier B).
* `v32lua -g` used to guess the assembly line numbers from a running count and was
  wrong (one entry said line 32720, a 11,000-line program got 11 entries). **(v32lua: fixed)**

## The idea: put the location in the assembly stream

A `;@src` comment line precedes the instructions it describes. Everything after it, up to
the next `;@src`, belongs to that source line. Nothing is keyed by line number any more, so
any tool that reorders the file can keep the information correct, and any tool that
ignores comments (the assembler, the stock tools) passes it through untouched. This is
the same trick as `.loc` in GNU as, and the same rule as the `;@` hints that `v32opt`
already reads: comments only, a missing or unknown line is always safe.

```
;@src FILE:LINE            all following instructions come from FILE line LINE
;@src FILE:LINE:COL        (optional) with a column
;@src -                    the following instructions have no source (runtime, startup)
```

* `FILE` is the path as given to the compiler (relative paths stay relative; `v32sim`
  already resolves them against the debug file's directory and its parents).
  It may contain `:` only if it is a drive letter; the *last* `:` or the last two
  separate FILE from LINE (and COL).
* A marker is written when the line changes, at the first instruction of each function,
  and after a `;@src -`. Several instructions of one line share one marker.
* The marker is a **statement-granularity** hint: a compiler writes it where a debugger
  should be allowed to stop. (Instructions of a multi-line expression normally keep the
  statement's first line; a compiler that wants finer stops writes more markers.)
  A `is_stmt`-style flag is not needed in this version.
* **(v32lua: done)** `v32lua -g` writes them. `FILE.asm.debug` is still written, now made
  from the markers in the finished file, for tools that want the old side file.

### Companion lines

```
;@lang lua                         once, with ;@hints: the language of the source below
;@fn __function_f lines=12..30     (existing ;@fn, new attribute) the function's source range
;@var NAME loc=BP-3 [type=...]     before the first instruction where the variable is live
;@scope {  ...  ;@scope }          lexical blocks (so `info locals` shows the visible ones)
```

`;@lang` selects how `v32sim` shows values (Lua's NaN-boxed values, C's `int`/`float`
by `type=`). `;@var` and `;@scope` are what a debugger needs for `print x` and
`info locals`; they are the second step and are only meaningful for unoptimized code
(see "Locations after optimization").

`loc=` forms: `BP-3` (frame word), `BP+2` (parameter), `R4` (register), `@0x20003000` (a
fixed RAM address, for globals) and `NAME` (an assembly symbol). `type=` is
`int|uint|float|ptr|bool|char|lua|lua-table|...`; unknown types print as raw words.

## Locations after optimization

`v32opt` promotes frame slots into registers and then a variable's home moves around. The
honest design: `v32opt` rewrites `loc=` when it promotes a slot (the same `BP-3` becomes
`R5` over the range it is promoted for, and a `;@var NAME loc=BP-3` is written again where
it goes back), or drops the `;@var` and says so with `;@var NAME optimized-out`. Until
`v32opt` does that, a debug build is `-O0`, which is what everyone expects.
Line stepping works at every level once `v32opt` carries the markers (next section).

## Three tiers, because not every compiler can do all of this

| Tier | Who | What the debugger gets |
|---|---|---|
| **A** full | `v32lua` (today, markers), `v32cc`, `v32c++` | line stepping, file:line breakpoints, function names, `bt` with lines; with `;@var`/`;@scope`: locals |
| **B** wrapper | stock `compile` + a marking pre-pass `v32srcmark` | line stepping at statement granularity, no locals (details below) |
| **C** labels only | stock `compile`, nothing else | function-level: `break main`, `bt` with function names (the labels are already in the assembly), the C file shown by function, no lines |

**Tier B, the stock compiler: already has most of it.** I built the stock `compile` from the
ComputerSoftware zip (`CCompiler/`; the `SDL.h` include in `Main.cpp` needs a stub, and
`VirconCEmitBinaryOps.cpp` is stale and not part of the build). Findings:

* `compile -g` already writes `FILE.asm.debug` in exactly the format `v32cc` and `v32lua` use
  (`asmfile,asmline,cfile,cline[,function]`; `DebugInfo.cpp`), one entry per statement, and
  `v32as -g --map` turns it into the address file `v32sim` reads. So stock C is already tier A
  for *line* information. Inline `asm { ... }` blocks are copied verbatim (`EmitAssemblyBlock`).
* But the asm line numbers are positions recorded while emitting (`ProgramLines.size() + 2`),
  and `EmitFunction` later cuts the body out and puts it back after the register saves
  and the `isub SP` (`BodyLines`). Entries inside such a function are therefore a line or
  more off. Example from my build: `int add(int a, int b) { int c = a * b; ... }` has its
  statement entry on the `push R1` that was inserted afterwards instead of the first instruction
  of the statement. This is the same class of bug `v32lua`'s map had.
* **The stock tools cannot be changed, so the fix is outside them: `v32srcmark`**
  (`scripts/v32srcmark.py`, hosted with `v32opt`; a post-pass on the compiler's own output, independent of v32lua).
  `compile prog.c -g` is run as is; `v32srcmark prog.asm` then reads `prog.asm` and
  `prog.asm.debug`, moves the entries to where the statements really are, writes
  `;@src FILE:LINE` in front of those instructions (a `;@src -` after the last function) and
  rewrites `prog.asm.debug` from the markers, so it is exact for the file as rewritten.
  The drift is regular, which is what makes this possible: `EmitFunction` records every entry
  before it puts the frame lines (`isub SP, n`, `push Rk`, in either order) in front of the
  body, so each entry after a function's first is early by the number of those lines, less
  one for the compiler's own bias. The tool counts them from the code (the `pop Rk` after the
  `_return:` label say how many `push Rk` are saved registers and not the body's own) and moves
  the entries. Statements that are only a label (`start:`, a loop head) land on the next
  instruction; if two do, the later line wins.
* **How I checked it:** I patched a copy of the stock compiler so that it writes the markers
  itself (the instructions of its output are identical to the stock compiler's apart from the
  marker lines) and compared on the 44 programs of `CCompiler/Tests`: of the 34 that compile
  (the others are error tests or need include paths) all 34 put every marker on the same
  instruction. `testing/srcmark/check_srcmark.py` (v32opt tree) keeps three canned cases (prologue
  with `isub` and `push`, `push` before `isub`, label-only lines). The patched compiler is
  not part of the deliverables, since the stock tools stay as they are.
* **Limits:** no `;@var`/`;@scope` for stock C: the variables' frame offsets are not in
  the side file and the assembly does not name them. Statement stepping, `bt` and
  breakpoints on lines work; `info locals` does not. If the stock compiler's side file or
  prologue ever changes, the tool refuses to guess where it cannot match (`--check`
  reports entries it cannot place; exit status 1).
* `-g` is the only switch the stock compiler needs; the marking is a separate step in the
  build (`compile -g && v32srcmark && v32opt && v32as -g`).

**Tier C** needs nothing: the stock assembly has `__function`-style labels for every C
function, and `v32as -g` already exports labels. `v32sim` can show `file.c` at the function
definition (found with a text search for the function name) and stop treating that as line
information. No locals.

## Command-line convention

Every compiler: `-g` writes the markers (and the `;@var` lines where it can); `-g0` none.
`v32opt`: `-g` keeps and updates them (default **on** when the input has any; `--strip-src`
removes them). `v32as`: `-g` writes `address,file,line[,function]` into `NAME.vbin.debug`
from the markers (no `--map` needed); `--map` keeps working. `v32sim`: loads it as now.

## Who has to change what (summary; details in the per-tool proposals)

| Tool | Change |
|---|---|
| `v32lua` | **done:** `;@src`, `;@lang`, a correct `-g` side file. Next: `;@var`/`;@scope`. |
| `v32cc`, `v32c++` | write `;@src` instead of (or as well as) the side file; later `;@var`/`;@scope`. Small: they already know the line per instruction. |
| `v32opt` | carry `;@src` through every pass; rewrite `loc=` when promoting. Medium. |
| `v32as` | `-g` reads the markers directly. Small. |
| `v32sim` | line-level `step`/`next`/`finish`/`until`, `break file:line`, `list`, Lua mode for the overlay and for values, `info locals`. Medium. |
| stock compiler | **nothing** (it cannot change): `v32srcmark` (hosted with `v32opt`) marks its output after the fact; no locals. |
