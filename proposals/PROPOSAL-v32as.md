# Proposal to v32as / v32asld: what v32sim's debugger needs from the side files

From v32sim 20261103-dev, against v32as 20261101-dev. Written 2026-11-04. What works and is relied on: `.vbin.debug` from `;@src`
markers with the FUNCTION column on every entry, `ADDRESS,-,0` for `;@src -`, `.vbin.vars` with ranges, depths and same-block
redefinition, `-C` + `v32asld -g` writing the same files. v32sim's new `locals` command reads `.vbin.vars` as round 12 specifies,
and its tests pass on v32cc and v32lua output assembled with this v32as.

| id | sev | what | v32sim stopgap today |
|---|---|---|---|
| SA-1 | **B** | a C function never ends, so its variables' ranges run to the end of the program | v32sim checks the function column of `.vbin.debug` |
| SA-2 | **B** | a symbol `loc=` (`var_NAME`, a label) is copied as text, and marker-mode `.debug` has no data labels to resolve it with | none: such a variable shows "not known in this frame" |
| SA-3 | C | no symbol file with final addresses from `-g` (functions, ROM data, globals) | v32sim reads `NAME.asm.sym` (`V` lines) and `%define global_` lines |
| SA-4 | C | the language is not in the side files | v32sim guesses from the source file's extension |

---

## SA-1  ending a function that has no `;@fn` after it  (B)

**What.** Round 12: "a function starts at a `__function_NAME:` label and ends at the line `;@fn __function_NAME ...` that the Lua
compiler writes after its body, at a `;@src -`, or at the end of the program. Functions nest". v32cc writes its `;@fn` *above* the
label (and only with `--emit-hints`), so no C function ends.

**Evidence.** v32cc output of a three-function program: all variables end at `0x20000091`, the end of the program (details in the
v32cc proposal, SC-1). The `.vbin.debug` FUNCTION column is right (each entry names its own function), so only `.vars` is affected.

**Sketch.** When the source says `;@lang c` (or any language but `lua`), a new top-level `__function_X:` label ends the previous
function, as it does for the FUNCTION column. Or v32cc writes `;@src -` after each function (SC-1); either is enough.

**If declined.** v32sim keeps its filter.

## SA-2  resolve symbol locations in `.vbin.vars`  (B)

**What.** A `;@var` may name a RAM word by symbol (v32lua's chunk-level locals live in `var_NAME`, see the v32lua proposal SL-2).
Only the assembler knows the address; in marker mode `.vbin.debug` carries no other labels ("no other label is written in marker
mode"), and v32lua's `var_NAME` are `%define`s, which v32sim does not read either.

**Sketch.** In `.vbin.vars`, write a symbol `LOC` as `@0xADDRESS` (or add a column with the resolved address and keep the name).
Same for `loc=@label+N`. **If declined.** v32sim would have to read `%define var_` lines and labels itself (it will, for globals:
v32sim backlog VS-10), but a resolved address in one place is simpler for every reader.

## SA-3  a resolved symbol file from `-g`  (C)

**What.** `--symbols FILE` exists (`address,name,kind`, `--symbols-all` adds locals, `global_*`, `DATASYM+N`), but it is a separate
option, and its kinds differ from v32cc's `.asm.sym` (`F`/`V`/`R` with sizes and source lines). **Sketch.** With `-g`, also write
`NAME.vbin.sym` in the `.asm.sym` format with every address resolved (functions, ROM data, globals with their word counts), so one
file serves `nm`, panic backtraces and the debugger. v32sim would read it before `.asm.sym`. **If declined.** v32sim could read
`--symbols` output (backlog VS-15).

## SA-4  the language in the side files  (C)

`;@lang c` / `;@lang lua` is read by v32as but not written out. A first line `# lang c` in `.vbin.debug` (a comment, which
v32sim's reader already skips) would let a debugger pick C or Lua display without `-L`. **If declined.** Extensions decide.

## Questions

1. Is a `;@fn` above a label meant to stay "not an end" for every language, or only for the runtime's `clobbers=` form? If the
   latter, a `;@fn` above the *next* function could end the previous one.
2. Should `.vbin.vars` scope lines carry the function too (`scope,START,END,DEPTH,FUNCTION`) in every case? v32sim does not
   need them today; it uses each `var` line's own range.
