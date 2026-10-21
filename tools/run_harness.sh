#!/bin/sh
# Boot a game in the 68k harness, then refresh its disassembly with the accumulated trace.
# Usage: tools/run_harness.sh [catacombs|original] [harness options]
set -e
cd "$(dirname "$0")/.."
GAME=${1:-catacombs}; [ $# -gt 0 ] && shift
[ -d build/games ] || tools/extract.sh
make -s -C harness
case $GAME in
  catacombs) ARC=original/mazeworld-catacombs-105.hqx; APP="build/games/catacombs/Mazeworld Catacombs 1.0.5/Mazeworld Catacombs.rsrc"; OUT=build/disasm ;;
  original)  ARC=original/Mazeworld_v.1.0.3.sit;      APP="build/games/original/Mazeworld v.1.0.3/Mazeworld.rsrc";             OUT=build/disasm-original ;;
  *) echo "unknown game $GAME"; exit 2 ;;
esac
mkdir -p build/run-$GAME
./harness/mzharness -o build/run-$GAME "$@" "$ARC" || true
python3 tools/merge_traces.py $GAME
python3 tools/disasm.py "$APP" "$OUT"
