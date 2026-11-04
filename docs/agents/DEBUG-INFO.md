# Debug information: V32DBG markers and side files (agent card)

The compilers describe their output with **comment lines** in the assembly, so every tool on the way can carry them and the
assembler, which alone knows the addresses, turns them into side files that v32sim reads. Status per tool: v32sim
`docs/V32DBG-STATUS.md`. The spec, `V32DBG-SPEC.md`, is kept with v32opt / v32lua (not in v32sim).

## In the assembly

```
;@lang c                                   the source language (c, lua)
;@src FILE:LINE[:COL]                      the instructions that follow come from this line
;@src -                                    the instructions that follow have no source (runtime, start-up code)
;@fn __function_NAME lines=7..15 ...       function hints; v32lua writes it after the body (ends the function)
;@var NAME loc=BP-3 type=int [boxed]       NAME lives at BP-3 (words) from the next instruction on
;@scope {   ...   ;@scope }                a block with variables of its own
```

`loc=`: `BP+N` (parameter), `BP-N` (local), `Rn` (register), `@ADDR` / a symbol (RAM word), `-` (optimized out).
`type=`: `int uint float bool ptr`, `T[N]`, `T[]`, `struct:TAG:WORDS`, `union:TAG:WORDS`, `lua` (NaN-boxed). Flags: `boxed`
(the word points to the value), `optimized-out`. Writers: `v32cc -g`, `v32lua -g`, and for `;@src` only, v32opt's
`scripts/v32srcmark.py` on stock `compile -g` output. v32opt keeps `;@src` exact; it does **not** update `;@var` (wrong values at
-O2 and up).

## Side files

| file | written by | lines |
|---|---|---|
| `NAME.asm.debug` | `compile -g`, `v32cc -g` | `asmfile,asmline,cfile,cline[,function]` (assembly line -> C line) |
| `NAME.vbin.debug` | `v32as -g`, `assemble -g program` | `ADDRESS,FILE,LINE[,FUNCTION]`; `ADDRESS,-,0` = no source from here |
| `NAME.vbin.vars` | `v32as -g` | `scope,START,END,DEPTH[,FUNC]`, `var,START,END,NAME,LOC,TYPE,FLAGS,DEPTH[,FUNC]` (END exclusive) |
| `NAME.asm.sym` | `v32cc -g` | `# v32cc symbols v1`, then `kind,name,address,words,cfile,cline` (F function, V variable, R ROM data) |
| `%define global_NAME ADDRESS` | every C compiler, in the `.asm` | the fallback for data symbols |

## What v32sim does with them

Line stepping (`step`/`next`/`finish`/`until`), `break FILE:LINE`, `list`, `bt` with function names and lines, `frame`/`up`/`down`,
`locals` and locals in `print`/`x`/`watch`/`expect` (innermost block wins; registers only in frame 0), data symbols everywhere a
number is accepted. Files are searched next to the `.v32`, in `obj/`, `../obj/`, `--debug-dir`; `-n` disables them.
