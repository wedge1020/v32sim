#!/bin/bash
# tools/v32sim-check.sh -- run every v32sim suite from one toolchain directory; one summary line, exit 0 only if nothing failed.
#
#   tools/v32sim-check.sh TOOLCHAIN_DIR [options]          (make check TC=DIR [V32IO=DIR] ...)
#
#   TOOLCHAIN_DIR   has v32cc, v32as, packrom, StandardBios.v32 and, for the stock chain and the oracle suites, compile, assemble,
#                   refrun (v32nix's toolchain/bin, after `make bootstrap`, is exactly this)
#   --v32io DIR     the v32io project (lib/, tools/v32dev.py, tests/): enables run_tests.sh and the doorbell host checks
#   --srcmark FILE  v32opt's scripts/v32srcmark.py: source-level debugging of stock-compiler carts
#   --quick         skip the generated differential suites (fuzz)
# A suite whose inputs are missing is reported as SKIP (never as a pass). Exit: 0 all run suites passed, 1 a failure.
HERE=$(cd "$(dirname "$0")" && pwd); T=$HERE/../tests
TC=${1:?usage: v32sim-check.sh TOOLCHAIN_DIR [--v32io DIR] [--srcmark FILE] [--quick]}; shift
IO=; SM=; QUICK=0
while [ $# -gt 0 ]; do case $1 in --v32io) IO=$2; shift;; --srcmark) SM=$2; shift;; --quick) QUICK=1;; *) echo "unknown option $1" >&2; exit 2;; esac; shift; done
TC=$(cd "$TC" && pwd); export PATH=$TC:$PATH
BIOS=$TC/StandardBios.v32
[ -x "${V32SIM:-$HERE/../bin/v32sim}" ] || { echo "v32sim is not built (make)"; exit 2; }
pass=0; fail=0; skip=0; names=
run() { # name command...
    n=$1; shift; printf '%-34s' "$n"; out=$("$@" 2>&1); st=$?
    if [ $st = 0 ]; then echo "ok    $(echo "$out" | grep -E 'passed' | tail -1 | cut -c1-70)"; pass=$((pass+1))
    else echo "FAIL  (status $st)"; echo "$out" | grep -E "FAIL|fail" | head -5 | sed 's/^/        /'; fail=$((fail+1)); names="$names $n"; fi
}
skipit() { printf '%-34sSKIP  %s\n' "$1" "$2"; skip=$((skip+1)); }
run batch   bash "$T/run_batch_tests.sh" "$TC"
run src     bash "$T/run_src_tests.sh" "$TC"
ORACLE=; [ -x "$TC/refrun" ] && ORACLE=$TC
if [ -n "$ORACLE" ]; then
    run gaps     bash "$T/run_gaps.sh" "$TC"
    run romcheck bash "$T/run_romcheck.sh" "$TC"
    [ $QUICK = 1 ] && skipit fuzz "--quick" || run fuzz bash "$T/run_fuzz.sh" "$TC" "1" 100
else skipit "gaps/romcheck/fuzz" "no refrun in the toolchain dir"; fi
H="V32TC=$TC V32BIOS=$BIOS"; [ -n "$ORACLE" ] && H="$H ORACLE=$ORACLE"; [ -n "$SM" ] && H="$H V32SRCMARK=$SM"; [ -n "$IO" ] && H="$H V32IO=$IO"
run harness env $H bash "$T/run_harness_tests.sh"
if [ -n "$IO" ]; then run "v32io (kbd, v32dev, native)" bash "$T/run_tests.sh" "$TC" "$IO"; else skipit "v32io suites" "no --v32io"; fi
echo "---- $pass suites passed, $fail failed, $skip skipped${names:+ (failed:$names)}"
[ $fail = 0 ]
