#!/bin/sh
# A quick per-call check of the C code for Mazeworld Original on one test (default mwo_shell):
# prints the first differences, or the functions checked. Usage: tools/quick_original.sh [test ticks]
cd "$(dirname "$0")/.."
T=${1:-tests/mwo_shell.txt}; K=${2:-2600}
make -s -C harness || exit 1
O=build/quick-original; rm -rf $O
./harness/mzharness -o $O -subcheck all -script "$T" -ticks "$K" original/Mazeworld_v.1.0.3.sit > /dev/null 2>&1
grep -m6 "subcheck f[no]_.* call\|STOP" $O/harness.log | sed 's/^\[ *[0-9]*\] //'
echo "$(grep -c 'matched the original on [1-9]' $O/harness.log) functions checked per call"
