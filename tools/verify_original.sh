#!/bin/sh
# The shared C code run for Mazeworld Original (game/orig_map.h): tools/verify_subs.sh over its tests
# and recordings, and random play started on levels 2-6 (two at a time).
cd "$(dirname "$0")/.."
make -s -C harness || exit 1
mkdir -p build && cp harness/mzharness build/mzharness-orig-$$ && export HARNESS=build/mzharness-orig-$$ || exit 1
trap 'rm -f build/mzharness-orig-$$' EXIT
G=original/Mazeworld_v.1.0.3.sit; fail=0; i=0
set -- "mwo_shell.txt 2600" "mwo_menus.txt 1700" "mwo_monkey_21.txt 14400" "mwo_monkey_22.txt 14400" "recordings/play_1.txt 5435" "recordings/play_2.txt 6261" "mwo_level2_31.txt 14400" "mwo_level3_31.txt 14400" "mwo_level4_31.txt 14400" "mwo_level5_31.txt 14400" "mwo_level6_31.txt 14400" "mwo_snail_story.txt 9000" "mwo_snail_story2.txt 10000" "mwo_snail_journey.txt 20000" "mwo_snail_rest.txt 6000" "mwo_george_states.txt 9700" "mwo_snail_states.txt 9110" "mwo_level3_states.txt 1610" "mwo_level4_states.txt 4610" "mwo_level5_states.txt 4210" "mwo_level5_end.txt 1710" "mwo_level4_room.txt 2510" "mwo_level4_end.txt 1410" "mwo_level3_states2.txt 4260" "mwo_level3_passage.txt 1410" "mwo_level2_states.txt 2610" "mwo_level2_grow.txt 1610" "mwo_level2_exit.txt 1210"
for t in "$@"; do
  n=${t%% *}; k=${t##* }; o=build/vo-$(basename ${n%.txt})
  ( VERIFY_OUT=$o tools/verify_subs.sh all tests/$n $k $G > $o.txt 2>&1 ) &
  i=$((i + 1)); [ $((i % 2)) -eq 0 ] && wait
done
wait
for t in "$@"; do
  n=${t%% *}; o=build/vo-$(basename ${n%.txt})
  echo "$(tail -1 $o.txt)  $n ($(grep -c 'matched the original on [1-9]' $o.txt) functions checked per call)"
  [ "$(tail -1 $o.txt)" = PASS ] || fail=1
done
exit $fail
