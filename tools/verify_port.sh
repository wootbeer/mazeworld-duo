#!/bin/sh
# The native port (headless build) against the original under the test rig: each scripted game must
# end with globals, heap, screen and audio identical. Catacombs' tests, then Mazeworld Original's.
# Usage: tools/verify_port.sh [quick|full]
cd "$(dirname "$0")/.."
MODE=${1:-full}
GAME=original/mazeworld-catacombs-105.hqx
make -s -C harness && make -s -C port || exit 1
set -- "explore.txt 640" "highscore.txt 3400" "monkey_5.txt 14400" "instructions.txt 1500" "instructions_mouse.txt 400" "shell_clicks.txt 700" "shell_keys.txt 800" "shell_menus.txt 600"
[ "$MODE" = full ] && set -- "$@" "monkey_1.txt 14400" "monkey_2.txt 14400" "monkey_3.txt 14400" "monkey_4.txt 14400" "monkey_6.txt 14400" "level04.txt 14400" "level08.txt 14400" "level12.txt 14400"
set -- "$@" "mwo_shell.txt 2600" "mwo_menus.txt 1700" "mwo_monkey_21.txt 14400" "recordings/play_1.txt 5435"
[ "$MODE" = full ] && set -- "$@" "mwo_monkey_22.txt 14400" "recordings/play_2.txt 6261" "mwo_level2_31.txt 14400" "mwo_level3_31.txt 14400" "mwo_level4_31.txt 14400" "mwo_level5_31.txt 14400" "mwo_level6_31.txt 14400" "mwo_snail_story.txt 9000" "mwo_snail_story2.txt 10000" "mwo_snail_journey.txt 20000" "mwo_snail_rest.txt 6000" "mwo_george_states.txt 9700" "mwo_snail_states.txt 9110" "mwo_level3_states.txt 1610" "mwo_level4_states.txt 4610" "mwo_level5_states.txt 4210" "mwo_level5_end.txt 1710" "mwo_level4_room.txt 2510" "mwo_level4_end.txt 1410" "mwo_level3_states2.txt 4260" "mwo_level3_passage.txt 1410" "mwo_level2_states.txt 2610" "mwo_level2_grow.txt 1610" "mwo_level2_exit.txt 1210" "mwo_level6_states.txt 2310" "mwo_level4_rush.txt 3000" "mwo_blast.txt 1310"
fail=0
for t in "$@"; do
  name=${t%% *}; ticks=${t##* }; d=build/port-$(basename ${name%.txt})
  case $name in mwo_*|recordings/*) GAME=original/Mazeworld_v.1.0.3.sit;; *) GAME=original/mazeworld-catacombs-105.hqx;; esac
  rm -rf $d; mkdir -p $d/orig $d/port
  harness/mzharness -o $d/orig -script tests/$name -ticks $ticks $GAME >/dev/null 2>&1 &
  port/mzport-headless -o $d/port -script tests/$name -ticks $ticks $GAME >/dev/null 2>&1
  wait
  r=PASS; for f in a5world.bin heap.bin screen.ppm sound.wav; do cmp -s $d/orig/$f $d/port/$f || r="FAIL ($f differs)"; done
  echo "$r  $name"; [ "$r" = PASS ] || fail=1
done
exit $fail
