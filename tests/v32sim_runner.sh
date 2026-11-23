#!/bin/bash
# dev_rom_runner look-alike on top of v32sim, so that the v32dev real-cartridge suite
# (tests/v32dev/dev_rom_test.py of the v32io project) can run against the simulator:
#
#   dev_rom_test.py tests/v32sim_runner.sh <bios.v32> <cart dir> <workdir>
#
# usage: v32sim_runner.sh <bios.v32> <cart.v32> <card.memc> [microseconds per frame] [max frames]
# exit:  0 = the program ended (HLT), 1 = hardware error, 2 = usage/load error, 3 = frame limit
HERE=$(cd "$(dirname "$0")" && pwd)
SIM=${V32SIM:-$HERE/../bin/v32sim}
[ $# -ge 3 ] || { echo "usage: $0 <bios.v32> <cart.v32> <card.memc> [micros] [max frames]"; exit 2; }
MICROS=${4:-1000}
FRAMES=${5:-20000}
# V32DEV_SERVER with a serial:stdio channel (the v32io embedded-server scenarios): the program's console is this
# process's standard input and output, so they are passed through; otherwise the run is silent and reads nothing
if [ -n "${V32DEV_SERVER+x}" ] && [ "${V32DEV_SERVER:-}" != 0 ] && case "${V32DEV_CH0:-}${V32DEV_CH1:-}${V32DEV_CH2:-}${V32DEV_CH3:-}" in *serial:stdio*) true;; *) false;; esac; then
    "$SIM" -r -x -n -B "$1" -M "$3" --frame-us="$MICROS" --max-frames="$FRAMES" "$2" 2>"$3.sim.log"
else
    "$SIM" -r -x -n -B "$1" -M "$3" --frame-us="$MICROS" --max-frames="$FRAMES" "$2" </dev/null >"$3.sim.log" 2>&1
fi
rc=$?
case $rc in
    0) echo "RUNNER: program ended (v32sim)"; exit 0;;
    2) echo "RUNNER: frame limit reached ($FRAMES frames)"; exit 3;;
    3) echo "RUNNER: hardware error (v32sim)"; tail -3 "$3.sim.log"; exit 1;;
    *) echo "RUNNER: v32sim failed ($rc)"; tail -5 "$3.sim.log"; exit 2;;
esac
