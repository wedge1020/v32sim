# v32opt -- Vircon32 assembly optimizer (agent card)

Rewrites assembly between the compiler and the assembler: output of the stock compiler, v32cc, v32c++ (through `compile`),
v32lua, or hand-written code. Version 20261030-dev. Docs: `README.md`, `USAGE.md` (choosing a level, checking, profiling),
`doc/hints.md` (the `;@` hints it reads), `doc/srcmarks.md`, `ISSUES` (open limits).

```
v32opt prog.asm -O2 -o progOpt.asm      # default output name: <input>Opt.asm;  -O0 is the default (nothing)
v32opt prog.asm -O3 -d -o out.asm       # -d marks every transformation as '; [DEBUG pass] ...' in the output
v32opt -L lua game.asm -O2              # v32lua output: NaN-box aware passes
v32opt --abi caller-save ...            # v32cc / v32lua convention; 'stock' is callee-save; 'auto' reads ;@hints
v32opt prog.asm --profile prog.prof -O3 # line profile from scripts/v32prof.py (runs v32sim --profile-out)
v32opt ... --trigger-max=N              # apply only the first N transformations: bisect a miscompile
```

## Levels

`-O1` local peepholes. `-O2` adds CSE, DCE, constant folding, omit-frame-pointers and **register promotion** (promote-regs,
promote-leaf, promote-loops: stack locals kept in registers). `-O3` adds inlining and loop passes (invert, unroll, index):
"aggressive; could break". `-Os` = `-O3` without the loop passes. Each pass: `-f<name>` / `-fno-<name>`; tuning `-f<name>=N`.

## What an agent should know

- All instructions cost 1 cycle: many rewrites save size and registers, not time. The big wins are promotion and loop passes on
  compiler output; hand-written, frameless code gains little (`ISSUES`).
- Passes recognize compiler shapes (`PUSH BP / MOV BP,SP / ISUB SP,N`, `__function_*` labels, C loop shapes). v32lua's loop shape
  is not matched: Lua gains from -O1/-O2 only.
- Hints from the compilers (`--emit-hints`, v32lua by default): calling convention, which locals escape, volatile accesses,
  `;@fn ... inline/hot/cold`, `;@loop`, `clobbers=`. Without hints v32opt is conservative.
- **Debug info**: `;@src` markers are kept correct through every pass (`-g`; `--check-src` verifies, exit 3). **`;@var` /
  `;@scope` are passed through unchanged**: after promotion a variable's `loc=BP-n` is stale (it lives in a register), so a
  debugger shows the wrong value at -O2 and up (repro in v32sim `docs/repro/v32opt-promote-var.c`). Debug at -O0/-O1.
- `scripts/v32srcmark.py prog.asm` puts `;@src` markers into the **stock compiler's** `-g` output (repairing its line map), so
  stock-compiled programs can be debugged by source line in v32sim.

## Checking

`testing/run_test.sh` and `testing/*` (one directory per pass), `testing/differential/` (random programs, optimized vs not, run in
v32sim), `testing/srcmarks/run.sh`. After optimizing a program, run its own tests at each level; `--trigger-max` + `-d` finds the
transformation that breaks it.
