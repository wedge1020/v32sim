# Proposal to v32cc: what v32sim's debugger needs from `-g`

From v32sim 20261103-dev, against v32cc 20261102-dev with v32as 20261101-dev. Written 2026-11-04, after v32sim gained `locals`
(V32DBG section 4: the variables of a frame, from `NAME.vbin.vars`). What works and is relied on, so nobody "fixes" it: `;@lang c`,
`;@src` per statement, `;@src -`, `;@var` for parameters and locals with `type=` (scalars, `T[N]`, `struct:TAG:WORDS`), `;@scope`,
`NAME.asm.sym`, and `#line N "file"` carried into `;@src` (checked: `#line 100 "orig.cpp"` gives `;@src orig.cpp:101`).
v32sim's `tests/run_harness_tests.sh` builds `tests/examples/locals.c` with v32cc and checks shadowing, frames, arrays, structs,
pointers and floats: all pass.

| id | sev | what | v32sim stopgap today |
|---|---|---|---|
| SC-1 | **B** | no end of a function for v32as: every C variable's range runs to the end of the program | v32sim also requires the `.vbin.debug` function to match the variable's |
| SC-2 | **B** | globals have no `;@var`, so their types are unknown | `print g` shows the raw word; `x/f`, `x/d` by hand |
| SC-3 | C | `struct:TAG:WORDS` names no members | `locals` prints a struct as raw words |
| SC-4 | C | `NAME.asm.sym` gives functions and ROM data as `@label` / `@rodata+OFF`, not addresses | v32sim uses only the `V` lines; `tools/v32sym.py` resolves them out of band |
| SC-5 | C | a statement from a macro body gets the macro's file and line | none (known limit in `docs/DEBUG.md`) |

---

## SC-1  a function end that v32as understands  (B)

**What.** v32as (round 12) ends a function's variables at a `;@fn` written *after* the body (v32lua's form), at `;@src -`, or at
the end of the program; a `;@fn` *above* the label (v32cc's form, and only with `--emit-hints`) is a hint, not an end. v32cc
writes neither an end-form `;@fn` nor a `;@src -` after a function, so every range ends at the end of the program.

**Evidence.** `tests/examples/locals.c` (three functions) built with `v32cc -g` + `v32as -g`:

```
var,0x20000007,0x20000091,f,BP+2,float,-,0,scale        <- scale's variables
var,0x20000018,0x20000091,a,BP+2,int,-,0,work           <- work's: the same END
var,0x20000074,0x20000091,r,BP-1,int,-,0,main
```

With `--emit-hints` the file is identical. Without the stopgap, `locals` in `work` listed `scale`'s `f`, `n`, `r` as well.

**Sketch.** Either (a) `;@src -` after each function's last instruction (it already means "no source from here"; v32as writes
`ADDRESS,-,0` for it, which v32sim reads as a gap), or (b) an end-form `;@fn __function_NAME` after the body, as v32lua writes. Or
v32as does it (v32as proposal SA-1); either closes this item.

**If declined.** v32sim keeps filtering by the debug file's function column. A tool without that filter shows wrong variables.

## SC-2  `;@var` for globals  (B)

**What.** `DEBUG.md`: "Globals are not described yet (`loc=@ADDR` needs the link's layout)". The address is known at link time
(`%define global_NAME ADDRESS`), so the line can be written by the link: `;@var counter loc=@2 type=int` before the first function,
or one per translation unit. v32sim's `locals` code already resolves `loc=@ADDR` and symbol locations.

**Evidence.** `print counter` works (from `.asm.sym`) but shows hex; a `float` global or an `int[8]` global needs a format by hand,
and a struct global cannot be shown as members.

**Sketch.** At the link, one `;@var` per kept global with `loc=@ADDRESS` and the same `type=` strings as locals; v32as copies such a
line into `.vbin.vars` with a range covering the program. v32sim would add `info globals` and typed `print NAME`.

**If declined.** Globals stay untyped in the debugger; `.asm.sym`'s `words` still gives their size.

## SC-3  struct members in `type=`  (C)

**What.** `struct:pt:2` gives the size, not the members. **Sketch.** Either `struct:pt:2{x:int@0,y:int@1}` or a separate
`;@struct pt x:int@0 y:int@1` line once per type (smaller files; v32as would pass it to `.vbin.vars` as a `type,...` line).
v32sim would print `p = {x = 3, y = 5}` and `print p.x`. **If declined.** Raw words.

## SC-4  final addresses for functions and ROM data  (C)

**What.** `F,main,@__function_main,...` and `R,...,@rodata+OFF` need assembling to resolve. **Sketch.** Best done by the assembler
(v32as proposal SA-3: a resolved symbol file from `-g`); for v32cc only: keep the format, so both tools agree on one file.
**If declined.** `tools/v32sym.py` stays the way to get them.

## SC-5  macro origins  (C)

Known limit; noted for completeness. A `;@src FILE:LINE` of the *use* with the macro's line as a column or a second marker
(`;@src x.c:12 from=m.h:3`) would let `step` stop at the use. No v32sim change needed (it ignores unknown words).

## Questions

1. Is `#line` the supported way for a front end (v32c++, a translator) to hand its own file and line through `v32cc -g`? It works;
   v32c++ proposal SX-1 relies on it.
2. Can a C function's variables ever be meant to stay in scope past its last instruction (nested functions, a statement expression
   that becomes a function)? If not, SC-1 (a) is safe for all v32cc output.
