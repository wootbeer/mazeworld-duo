#!/bin/sh
# The native port (headless build) against the original under the test rig: each scripted game must
# end with globals, heap, screen and audio identical.
# Usage: tools/verify_port.sh [quick|full]
cd "$(dirname "$0")/.."
MODE=${1:-full}
GAME=original/mazeworld-catacombs-105.hqx
make -s -C harness && make -s -C port || exit 1
set -- "explore.txt 640" "highscore.txt 3400" "monkey_5.txt 14400" "instructions.txt 1500" "instructions_mouse.txt 400" "shell_clicks.txt 700" "shell_keys.txt 800"
[ "$MODE" = full ] && set -- "$@" "monkey_1.txt 14400" "monkey_2.txt 14400" "monkey_3.txt 14400" "monkey_4.txt 14400" "monkey_6.txt 14400" "level04.txt 14400" "level08.txt 14400" "level12.txt 14400"
fail=0
for t in "$@"; do
  name=${t%% *}; ticks=${t##* }; d=build/port-${name%.txt}
  rm -rf $d; mkdir -p $d/orig $d/port
  harness/mzharness -o $d/orig -script tests/$name -ticks $ticks $GAME >/dev/null 2>&1 &
  port/mzport-headless -o $d/port -script tests/$name -ticks $ticks $GAME >/dev/null 2>&1
  wait
  r=PASS; for f in a5world.bin heap.bin screen.ppm sound.wav; do cmp -s $d/orig/$f $d/port/$f || r="FAIL ($f differs)"; done
  echo "$r  $name"; [ "$r" = PASS ] || fail=1
done
exit $fail
