#!/bin/bash
# Build the headless reference runner and the Vircon32 assembler / ROM packer.
#
#   tests/ref/build.sh <ComputerSoftware checkout> <output dir>
#
# Kept for the scripts that call it: the work is done by tools/oracle/build.sh, which builds these three
# (refrun, assemble, packrom) and the rest of the oracle tools. Produces <output dir>/{refrun,assemble,packrom}.
HERE=$(cd "$(dirname "$0")" && pwd)
ONLY="v32orun assemble packrom" exec "$HERE/../../tools/oracle/build.sh" "$@"
