# Proposal to v32opt: keep the variable records true

From v32sim 20261103-dev, against v32opt 20261030-dev (with v32cc 20261102-dev, v32as 20261101-dev). Written 2026-11-04. What
works and is relied on: `;@src` markers stay correct through every pass (`-g`, `--check-src`), and `scripts/v32srcmark.py` gives the
stock compiler's output markers (v32sim's harness checks a source-level `bt` on a stock-compiled cartridge with it: passes).

| id | sev | what | v32sim stopgap today |
|---|---|---|---|
| SO-1 | **A** (for debugging optimized code) | promote-regs / promote-leaf / promote-loops move a variable into a register and leave its `;@var loc=BP-n` | none: the debugger shows a stale frame word; "debug at -O0/-O1" in the docs |
| SO-2 | **B** | inlining copies a callee's `;@var` lines into the caller, where `BP+n` means something else | not seen in practice yet (inlining was skipped in the cases tried) |
| SO-3 | C | `--check-src` does not look at `;@var` / `;@scope` | none |
| SO-4 | C | a variable whose stores were removed (dead-store elimination) is still described as living in its slot | none |

---

## SO-1  rewrite `loc=` when a variable moves  (A)

**What.** `doc/hints.md` lists `;@var` / `;@scope` under "Reserved for later": they pass through unchanged. At `-O2` and up the
promote passes keep frame words in registers, so the record points at a word that is no longer written.

**Evidence (checked).** `docs/repro/v32opt-promote-var.c` in v32sim:

```
v32cc -g -o pr.asm v32opt-promote-var.c; v32opt -O3 -d pr.asm -o pr3.asm
  ; [DEBUG promote-regs] PROMOTE-REGS: [BP-1] lives in R5 (always written before read: no load)
  ; [DEBUG promote-regs] PROMOTE-REGS: [BP-2] lives in R4 ...
  ;@var i loc=BP-1 type=int      ;@var s loc=BP-2 type=int       (unchanged)
v32sim, at `return s;`:   i = 4194286 (int, BP-1)   s = 201 (int, BP-2)        <- wrong; R5 = 10, R4 = 45
same file, the two lines edited to loc=R5 / loc=R4:   i = 10 (int, R5)   s = 45 (int, R4)
```

**Sketch.** When a pass gives a frame word a register home for a whole function, rewrite that variable's `;@var` to `loc=Rn`. When
the home changes inside the function, write a new `;@var NAME loc=Rn` at the point it changes (v32as round 12: a same-name `;@var` in
the same block ends the previous range and starts a new one). v32sim already reads `loc=Rn` (frame 0 only: registers are not saved
per frame) and `optimized-out`.

**If declined.** Debug only unoptimized builds; say so in `USAGE.md`. (v32sim's docs already do.)

## SO-2  inlined code and its variables  (B)

**What.** An inlined body runs in the caller's frame; the callee's `;@var a loc=BP+2` lines, if copied, describe the caller's
words. **Sketch.** In an inlined copy, either drop the callee's `;@var` lines, or write them as `optimized-out`, or (best) rewrite
them to where the inliner put the values, inside a `;@scope { }` pair so they end with the copy. **If declined.** A stop inside
inlined code shows the caller's words under the callee's names.

## SO-3  `--check-vars`  (C)

Like `--check-src`: every `;@var` inside a function, balanced `;@scope`, no `loc=BP-n` for a slot the output never reads or writes
(a strong sign of SO-1). Exit status 3, as `--check-src`.

## SO-4  dead stores  (C)

When peephole-dead-stores removes every store to a variable's slot, mark it: `;@var NAME optimized-out` at that point.

## Questions

1. Where is `V32DBG-SPEC.md` kept? v32lua's `doc/HINTS.md` says "kept with v32opt"; v32opt's `doc/README-v32srcmark.md` points to
   "v32lua proposals/V32DBG-SPEC.md". Neither archive I was given has it. v32sim's `docs/V32DBG-STATUS.md` is reconstructed without it.
2. Would `v32srcmark.py` take `;@var` too (from the stock compiler's `-g` file, which has no variables)? Probably not possible; if
   so, the stock chain stays line-level only, and the docs can say so.
