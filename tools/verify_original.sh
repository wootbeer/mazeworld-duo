#!/bin/sh
# The shared C code run for Mazeworld Original (game/orig_map.h): tools/verify_subs.sh over its tests.
cd "$(dirname "$0")/.."
make -s -C harness || exit 1
mkdir -p build && cp harness/mzharness build/mzharness-orig-$$ && export HARNESS=build/mzharness-orig-$$ || exit 1
trap 'rm -f build/mzharness-orig-$$' EXIT
G=original/Mazeworld_v.1.0.3.sit; fail=0
for t in "mwo_shell.txt 3000" "mwo_monkey_21.txt 14400" "mwo_monkey_22.txt 14400" "recordings/play_1.txt 5435" "recordings/play_2.txt 6261"; do
  n=${t%% *}; k=${t##* }; o=build/vo-$(basename ${n%.txt})
  VERIFY_OUT=$o tools/verify_subs.sh all tests/$n $k $G > $o.txt 2>&1
  echo "$(tail -1 $o.txt)  $n ($(grep -c 'matched the original on [1-9]' $o.txt) functions checked per call)"
  [ "$(tail -1 $o.txt)" = PASS ] || fail=1
done
exit $fail
