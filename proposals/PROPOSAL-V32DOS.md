# Proposal to V32DOS: status pages, and the shared test tools

From v32sim 20261103-dev, against the V32DOS project archive of 2026-11-02 and its bootstrap 20261101. Written 2026-11-04.
Context: V32DOS's test generator and runner are the model for v32sim's `tools/v32sim-session.py` and `tools/v32sim-golden.sh`;
`v32sim-session.py --prompt-symbol dos_print_prompt` writes **byte-identical** scripts to `tests/mkcmd.py` for sessions 1, 3, 5, 8
and 11 (checked).

| id | sev | what | stopgap |
|---|---|---|---|
| SD-1 | C | `docs/V32SIM-PROPOSAL.md` and `docs/PROPOSAL-STATUS.md` say VS-1 .. VS-8 are outstanding | none |
| SD-2 | C | `tests/run.sh`, `tests/mkcmd.py`, `tools/profile.py` duplicate tools now kept in v32sim | none needed |
| SD-3 | C | the bootstrap builds `v32run`/`v32shot`/`v32peek` from v32c++ and v32sim with its own `tools/v32sim/build.sh` | none needed |
| SD-4 | C | the open `--symbols-all` question in `PROPOSAL-STATUS.md` is answered | none |

---

## SD-1  status  (C)

All eight requests of `V32SIM-PROPOSAL.md` are in v32sim since 20261025 (`--batch`, symbols, `run N`, `until`, `expect` and exit
statuses, `console FILE`/`--attrs`, `--profile-by=function`, `watch/d`); v32sim's `tests/run_batch_tests.sh` checks them (25/25).
The two status lines can say "done in v32sim 20261025-dev".

## SD-2  thin wrappers  (C)

`tests/run.sh` can become a call of `v32sim/tools/v32sim-golden.sh --cart bin/V32DOS.v32 --bios $BIOS --sessions tests --golden
$G --prompt-symbol dos_print_prompt --mask tests/mask.sed --prepare tests/mkcard.sh -- --symbol-file=$SYM --cart-c-debug=$DBG`
(the masks move to a sed file, the card preparation to a script); `tests/mkcmd.py` stays as the documented syntax, or becomes
`v32sim-session.py`. `tools/profile.py`'s split is `v32sim-addrprof.py --map kernel:obj/V32DOS.vbin.debug --map module:M.debug@AUTO`.
Worth doing only after SD-2 is run once on a real build and the golden files compare (not done: no V32DOS build here).

## SD-3  bootstrap  (C)

`tools/bootstrap.sh` can build the stock-core runners with `v32sim/tools/oracle/build.sh` (one script, no v32c++ needed for them) and
v32sim with its own `tools/build-nogen.sh` (it uses v32sim's checked-in `gen/`), retiring `tools/v32sim/build.sh`, which is identical
to v32nix's copy.

## SD-4  `--symbols-all`  (C)

v32as 20261101 lists every `DATASYM+N` as `N,DATASYM+N,V` (README, round 9), which is what the open question asked for.

## Question

1. `tests/dev.py`'s `Host` helper (start, stop, re-serve `v32dev.py` between two commands) is the only tool of its kind. Would you
   like it generalized into v32sim `tools/` (a Python module other projects' host-device tests import), or kept as V32DOS's?
