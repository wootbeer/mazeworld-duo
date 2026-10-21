#!/bin/sh
# Unpack both original archives with the project's own reader (dev use only; never redistributed):
#   build/games/catacombs/...   Mazeworld Catacombs 1.0.5
#   build/games/original/...    Mazeworld (Original) 1.0.3
# and dump their resources to build/res/<game>/.
set -e
cd "$(dirname "$0")/.."
make -s -C archive
rm -rf build/games build/res && mkdir -p build/games build/res
archive/tools/arcdump original/mazeworld-catacombs-105.hqx build/games/catacombs >/dev/null
archive/tools/arcdump original/Mazeworld_v.1.0.3.sit build/games/original >/dev/null
python3 -I tools/rsrc.py "build/games/catacombs/Mazeworld Catacombs 1.0.5/Mazeworld Catacombs.rsrc" --dump build/res/catacombs/app >/dev/null
python3 -I tools/rsrc.py "build/games/catacombs/Mazeworld Catacombs 1.0.5/Mazeworld Catacombs Sound File.rsrc" --dump build/res/catacombs/snd >/dev/null
python3 -I tools/rsrc.py "build/games/original/Mazeworld v.1.0.3/Mazeworld.rsrc" --dump build/res/original/app >/dev/null
echo "extracted both games"
