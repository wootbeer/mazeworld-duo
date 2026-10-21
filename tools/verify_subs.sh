#!/bin/sh
# Verify C reimplementations two ways on the same scripted game:
#  1. per call (-subcheck): every call runs both versions on identical inputs and must leave the
#     same registers and memory;
#  2. end to end (-sub): a run with the C functions in place must end with globals, heap, screen and
#     audio identical to a run of the original.
# Usage: tools/verify_subs.sh [sub-list|all] [script] [ticks] [archive]
set -e
cd "$(dirname "$0")/.."
SUBS=${1:-all}; SCRIPT=${2:-tests/explore.txt}; TICKS=${3:-640}; GAME=${4:-original/mazeworld-catacombs-105.hqx}
[ -n "$HARNESS" ] || make -s -C harness
H=${HARNESS:-./harness/mzharness}
V=${VERIFY_OUT:-build/verify}
rm -rf "$V" && mkdir -p "$V"
"$H" -o $V/orig -script "$SCRIPT" -ticks "$TICKS" "$GAME" >/dev/null 2>&1 || true
"$H" -o $V/c -sub "$SUBS" -script "$SCRIPT" -ticks "$TICKS" "$GAME" >/dev/null 2>&1 || true
"$H" -o $V/check -subcheck "$SUBS" -script "$SCRIPT" -ticks "$TICKS" "$GAME" >/dev/null 2>&1 || true
status=0
echo "per call:"
grep "subcheck" $V/check/harness.log | sed 's/^\[ *[0-9]*\] /  /'
grep -q "STOP: subcheck" $V/check/harness.log && status=1
echo "end to end:"
for f in a5world.bin heap.bin screen.ppm sound.wav; do
  if cmp -s "$V/orig/$f" "$V/c/$f"; then echo "  identical: $f"
  else echo "  DIFFERENT: $f"; status=1; fi
done
grep "STOP:" $V/orig/harness.log | sed 's/^\[ *[0-9]*\] /  original: /'
grep "STOP:" $V/c/harness.log | sed 's/^\[ *[0-9]*\] /  with C:   /'
grep "STOP:" $V/check/harness.log | sed 's/^\[ *[0-9]*\] /  checked:  /'
[ $status -eq 0 ] && echo "PASS" || echo "FAIL"
exit $status
