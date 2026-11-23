#!/bin/bash
# tools/prune-release.sh [--apply] -- remove stale, duplicate and generated files before a release.
#
# Dry run by default: prints each action with the reason. --apply performs it. Idempotent: paths that no longer exist
# are skipped silently. A tracked path inside a git work tree is removed with `git rm -r -q` (the removal is then staged);
# anything else with `rm -rf`. Link fixes in the docs that point at removed files are NOT done here (see the report).
# Items that need a human decision are commented out below with the reason ("review").
set -euo pipefail

APPLY=0
case "${1:-}" in
    "") ;;
    --apply) APPLY=1 ;;
    -h|--help) sed -n '2,8p' "$0"; exit 0 ;;
    *) echo "usage: $0 [--apply]" >&2; exit 2 ;;
esac

cd "$(dirname "${BASH_SOURCE[0]}")/.."

INGIT=0
if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then INGIT=1; fi

N=0
prune() {   # prune PATH REASON
    local p=$1 why=$2
    [ -e "$p" ] || [ -L "$p" ] || return 0
    N=$((N + 1))
    local how="rm -rf"
    if [ $INGIT = 1 ] && git ls-files --error-unmatch -- "$p" >/dev/null 2>&1; then how="git rm -r -q"; fi
    if [ $APPLY = 1 ]; then
        printf 'remove  %-62s %s\n' "$p" "($why)"
        if [ "$how" = "git rm -r -q" ]; then git rm -r -q -- "$p"; else rm -rf -- "$p"; fi
    else
        printf 'would %s %-52s %s\n' "$how" "$p" "($why)"
    fi
}

# ---------------------------------------------------------------------------------------------------------------------
# 1. Duplicates
# ---------------------------------------------------------------------------------------------------------------------
# Top-level proposals/ is an older copy of docs/proposals/: the same 12 files, 6 byte-identical and the other 6 (ComputerSoftware,
# GBtoV32, v32io, v32lua, v32sim, README) older versions of the docs/proposals/ text; nothing exists only in proposals/.
prune proposals "older copy of docs/proposals/; every differing file is superseded there, nothing unique"

# ---------------------------------------------------------------------------------------------------------------------
# 2. Resolved proposals (every item done, answered or declined; open leftovers are tracked in TODO.md / the status docs)
# ---------------------------------------------------------------------------------------------------------------------
prune docs/proposals/PROPOSAL-v32cc.md   "SC-1..SC-5 and both questions done (v32cc 20261105-dev..20261109; docs/V32DBG-STATUS.md)"
prune docs/proposals/PROPOSAL-v32as.md   "SA-1 done, SA-2/SA-3 done (loc= symbols, NAME.vbin.sym), SA-4 closed by decision (extension decides)"
prune docs/proposals/PROPOSAL-v32opt.md  "SO-1..SO-4 done (loc= rewrite, ;@home, --check-vars, optimized-out; V32DBG-STATUS.md)"
prune docs/proposals/PROPOSAL-v32lua.md  "SL-1..SL-4 done (upvalues, chunk locals, v32orun replaces the headless runner); file's own header says Done"
prune docs/proposals/PROPOSAL-ComputerSoftware.md "SE-1 and SE-2 declined by the author; SE-3/SE-4 moot (upstream unchanged, v32sim tools/oracle covers them)"
prune docs/proposals/PROPOSAL-v32sim.md  "backlog VS-10..VS-19 all done (TODO.md DONE entries); open leftovers are in TODO.md STILL LACKING"

# ---------------------------------------------------------------------------------------------------------------------
# 3. Stale docs
# ---------------------------------------------------------------------------------------------------------------------
# None is safe to delete outright. Review (see /home/claude/work/prune-report.md):
# prune docs/CONSOLIDATION.md "review: a 2026-11-03 log, open items 3-4 are done, but items 5-7 and the 'Left where it is' table are not recorded elsewhere; move them to TODO.md first (AGENTS.md lines 8 and 46 link it)"
# prune docs/PERFORMANCE.md   "review: point-in-time report, but the only place with the measured numbers; README and TODO.md link it; keep unless the numbers move into README 'Speed and Turbo Mode'"
# prune docs/proposals/v32sim-proposal-perf-metrics-20261030.md "review: every feature is implemented; only an overhead measurement (<2%) and a host-drawing replay build are left, which are not in TODO.md yet"
# (kept: TODO.md, docs/PROPOSALS-STATUS.md (only record of v32nix P-E* answers; needs two stale rows edited), docs/V32DBG-STATUS.md, docs/AGENTS-SNIPPET.md, docs/BUGS.md)

# ---------------------------------------------------------------------------------------------------------------------
# 4. Orphans (nothing references them)
# ---------------------------------------------------------------------------------------------------------------------
# prune screenshots "review: v32cat_*.jpg/png show a hex view of a cartridge from a tool (v32cat) that is not in this repo; only the README layout tree mentions the directory (3.3 MB)"
# prune cart       "review: 09_hello, flappy, hello_bitwise are not used by any test or script (flappy only named as a benchmark in PERFORMANCE.md); README layout line only"
# prune bios       "review: debuggerBIOS.v32 and its .vbin.debug are not used by any script; input.c only quotes its name in a comment; README layout line only"
# (kept: gen/ (build-nogen.sh and the Makefile fallback need it), bin/.filler (harmless; zip -r stores bin/ anyway), CLAUDE.md, docs/repro/ (cited by docs/agents/V32OPT.md, V32DBG-STATUS.md, tests/examples/vars_opt.c), tests/ref/build.sh (wrapper, cited by run_gaps.sh and TOOLS.md))

# ---------------------------------------------------------------------------------------------------------------------
# 5. Build leftovers (what `make clean` removes)
# ---------------------------------------------------------------------------------------------------------------------
prune obj     "generated objects, .d files and flex/bison output; make clean removes it"
prune bin/v32sim "built binary; bin/.filler stays"

if [ $APPLY = 1 ]; then echo "done: $N path(s) removed"; else echo "dry run: $N path(s) would be removed (run with --apply to do it)"; fi
