#!/usr/bin/env python3
"""List functions ready for translation: every function they call is already in C (game/subs.txt)
and they have executed in a recorded run. Sorted by size.
Usage: ready.py [catacombs|original] [max_size]"""
import json, os, re, sys
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
game = sys.argv[1] if len(sys.argv) > 1 else 'catacombs'
maxsize = int(sys.argv[2]) if len(sys.argv) > 2 else 400
d = os.path.join(ROOT, 'build', 'disasm' if game == 'catacombs' else 'disasm-original')
fns = {f['id']: f for f in json.load(open(os.path.join(d, 'functions.json')))}
done = set()
for line in open(os.path.join(ROOT, 'game', 'subs.txt')):
    m = re.match(r'\s*(\d+)\s+([0-9A-Fa-f]+)\s', line.split('#')[0])
    if m: done.add(f'fn_S{m.group(1)}_{int(m.group(2), 16):04X}')
traced = {}
for f in os.listdir(os.path.join(ROOT, 'build', 'trace' if game == 'catacombs' else 'trace-original')):
    n = int(re.search(r'seg(\d+)', f).group(1))
    traced[n] = {int(x, 16) for x in open(os.path.join(ROOT, 'build', 'trace' if game == 'catacombs' else 'trace-original', f)).read().split()}
rows = []
for fid, f in fns.items():
    if fid in done or f['size'] > maxsize: continue
    if f['off'] not in traced.get(f['seg'], ()): continue
    if all(c in done for c in f['callees']):
        rows.append((f['size'], fid, len(f['callers']), ','.join(sorted(set(f['traps'])))))
for size, fid, nc, traps in sorted(rows):
    print(f'{fid}  {size:5d} bytes  {nc:3d} callers  {traps}')
