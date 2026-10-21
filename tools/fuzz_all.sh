#!/bin/sh
# Fuzz the routines listed in tests/fuzz.txt: after the first real call of each, run it again many
# times with random register inputs, comparing the C version with the original every time.
cd "$(dirname "$0")/.."
make -s -C harness || exit 1
mkdir -p build && cp harness/mzharness build/mzharness-fuzz
fail=0
grep -v '^#' tests/fuzz.txt | while read fn count regs script ticks; do
  [ -n "$fn" ] || continue
  out=build/fuzz-$fn
  build/mzharness-fuzz -o $out -subcheck $fn -fuzz $fn:$count:$regs -script tests/$script -ticks $ticks original/mazeworld-catacombs-105.hqx >/dev/null 2>&1
  if grep -q "fuzz: $fn matched" $out/harness.log; then echo "PASS  $fn ($count random inputs)"
  else echo "FAIL  $fn"; grep "fuzz input\|subcheck $fn\|STOP" $out/harness.log | head -8; fail=1; fi
done
exit $fail
