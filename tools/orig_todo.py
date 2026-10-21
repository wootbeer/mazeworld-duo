#!/usr/bin/env python3
"""What remains of Mazeworld Original: the functions its test runs executed (traces of the
original runs under build/vo-*/orig/trace, from tools/verify_original.sh) that no C code covers
yet, with how each relates to Catacombs (exact / near pair, or Original's own), largest first.
A function is "ready" when everything it calls runs as C already (translate those first).
Usage: orig_todo.py [--all] [--ready]"""
import json, os, re, glob, sys
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
fns = json.load(open(os.path.join(ROOT, 'build', 'disasm-original', 'functions.json')))
pairs = json.load(open(os.path.join(ROOT, 'build', 'orig_map.json')))['functions']
inv = {v['original']: (k, v['similarity']) for k, v in pairs.items()}
run = set()   # Original functions that run as C: shared ones (orig_map.h flag) and Original's own
for l in open(os.path.join(ROOT, 'game', 'orig_map.h')):
    m = re.match(r'\s*\{\d+, 0x[0-9A-F]+, (\d+), 0x([0-9A-F]+), 1\}', l)
    if m: run.add(f'fn_S{m.group(1)}_{m.group(2)}')
for l in open(os.path.join(ROOT, 'game', 'subs.txt')):
    w = l.split('#')[0].split()
    if len(w) > 2 and 'orig' in w: run.add(f'fn_S{w[0]}_{w[1].upper()}')
pcs = {}
for f in glob.glob(os.path.join(ROOT, 'build', 'vo-*', 'orig', 'trace', 'seg*.pcs')):
    n = int(re.search(r'seg(\d+)', os.path.basename(f)).group(1))
    pcs.setdefault(n, set()).update(int(x, 16) for x in open(f).read().split())
byid = {f['id']: f for f in fns}
rows = []
for f in fns:
    hit = f['off'] in pcs.get(f['seg'], ())
    if (not hit and '--all' not in sys.argv) or f['id'] in run: continue
    cat, sim = inv.get(f['id'], ('-', 0))
    ready = all(c in run for c in f['callees'])
    if '--ready' in sys.argv and not ready: continue
    rows.append((f['size'], f['id'], cat, sim, hit, ready))
for size, fid, cat, sim, hit, ready in sorted(rows, reverse=True):
    kind = 'exact' if sim == 1.0 else f'near {sim:.2f}' if cat != '-' else 'own'
    print(f'{fid:12s} {size:5d} bytes  {kind:9s} {cat:12s}{" ready" if ready else ""}{"" if hit else "  (not run)"}')
print(f'{len(rows)} functions, {sum(r[0] for r in rows)} bytes', file=sys.stderr)
