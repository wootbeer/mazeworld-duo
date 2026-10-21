#!/usr/bin/env python3
"""Decode an object handler's per-state tables in CODE 7 (see game/seg7_objects.c).
Usage: objtables.py <think_table> <act_table> <hits_base> <nstates>   (hex offsets)
Prints, per state: the think code address (table + 2*s + entry) and the D0 the dispatch leaves,
the act code address and its D0, and the collision list { type -> state }."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rsrc
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
res = rsrc.parse(rsrc.load_fork(os.path.join(ROOT, 'build/games/catacombs/Mazeworld Catacombs 1.0.5/Mazeworld Catacombs.rsrc')))
c = [d for t, i, n, a, d in res if t == 'CODE' and i == 7][0][4:]
w = lambda o: struct.unpack('>h', c[o:o + 2])[0]
th, ac, hb, n = (int(x, 16) for x in sys.argv[1:5])
for s in range(n):
    td = 2 * s + w(th + 2 * s); ad = 2 * s + w(ac + 2 * s); lst = hb + 2 * s + w(hb + 2 * s)
    hits = []
    o = lst
    while w(o) != -1 and len(hits) < 10: hits.append(f'{w(o) & 0xFFFF:#x}->{w(o + 2)}'); o += 4
    print(f'state {s:2d}: think {th + td:05X} (d0 {td:#x})  act {ac + ad:05X} (d0 {ad:#x})  hits {lst:05X}: {" ".join(hits)}')
