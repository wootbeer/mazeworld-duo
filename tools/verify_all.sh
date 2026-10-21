#!/bin/sh
# Run tools/verify_subs.sh over the test scripts (two at a time).
# Usage: tools/verify_all.sh [quick|full] [sub-list|all]
#   quick: explore (640 ticks), highscore (3400), monkey_5 (14400; ends a game with a name dialog),
#          instructions / instructions_mouse (the instructions dialog, by keys and by mouse),
#          shell_clicks / shell_keys (the shell by mouse and by Command-key menu commands)
#   full:  quick plus monkey_1..6 and level04/08/12 (monkey_1 from later levels; 14400 ticks each)
cd "$(dirname "$0")/.."
MODE=${1:-quick}; SUBS=${2:-all}
make -s -C harness || exit 1
# run a private copy, so rebuilding the harness meanwhile does not change a run halfway
mkdir -p build && cp harness/mzharness build/mzharness-verify-$$ && export HARNESS=build/mzharness-verify-$$ || exit 1
trap 'rm -f build/mzharness-verify-$$' EXIT
set -- "explore.txt 640" "highscore.txt 3400" "monkey_5.txt 14400" "instructions.txt 1500" "instructions_mouse.txt 400" "shell_clicks.txt 700" "shell_keys.txt 800" "shell_menus.txt 600"
[ "$MODE" = full ] && set -- "$@" "monkey_1.txt 14400" "monkey_2.txt 14400" "monkey_3.txt 14400" "monkey_4.txt 14400" "monkey_6.txt 14400" "level04.txt 14400" "level08.txt 14400" "level12.txt 14400"
fail=0; n=0
for t in "$@"; do
  name=${t%% *}; ticks=${t##* }
  ( VERIFY_OUT=build/verify-${name%.txt} tools/verify_subs.sh "$SUBS" "tests/$name" "$ticks" > build/verify-${name%.txt}.txt 2>&1 ) &
  n=$((n + 1)); [ $((n % 2)) -eq 0 ] && wait
done
wait
for t in "$@"; do
  name=${t%% *}; r=$(tail -1 build/verify-${name%.txt}.txt)
  echo "$r  $name ($(grep -c 'matched the original' build/verify-${name%.txt}.txt) functions checked per call)"
  [ "$r" = PASS ] || fail=1
done
exit $fail
