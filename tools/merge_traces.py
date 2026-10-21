#!/usr/bin/env python3
"""Merge harness traces of one game into its accumulated trace so coverage grows across runs.
Usage: merge_traces.py [catacombs|original]
  catacombs: build/run-catacombs*/trace, build/run[0-9]*/trace, build/run-m*/trace (monkey),
             build/run-hs*/trace, build/verify/*/trace -> build/trace
  original:  build/run-original*/trace, build/mwo*/trace                            -> build/trace-original"""
import glob, os, re, sys
game = sys.argv[1] if len(sys.argv) > 1 else 'catacombs'
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build')
if game == 'catacombs':
    srcs = ['run-catacombs*', 'run[0-9]*', 'run', 'run-m[0-9]*', 'run-hs*', 'verify/*', 'lvrun*', 'verify-*/orig']; dst = 'trace'
else:
    srcs = ['run-original*', 'mwo*']; dst = 'trace-original'
acc = {}
files = [f for s in srcs for f in glob.glob(os.path.join(root, s, 'trace', 'seg*.pcs'))] + glob.glob(os.path.join(root, dst, 'seg*.pcs'))
for f in files:
    n = int(re.search(r'seg(\d+)\.pcs$', f).group(1))
    acc.setdefault(n, set()).update(int(x, 16) for x in open(f).read().split())
os.makedirs(os.path.join(root, dst), exist_ok=True)
for n, s in acc.items():
    open(os.path.join(root, dst, f'seg{n}.pcs'), 'w').write(''.join(f'{o:05X}\n' for o in sorted(s)))
print(game, 'traced instruction starts:', sum(len(s) for s in acc.values()))
