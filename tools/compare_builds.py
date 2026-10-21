#!/usr/bin/env python3
"""Compare two disassembled builds function by function.
Instruction text is normalised: A5 offsets, jump-table slots, PC-relative and branch targets, absolute
addresses and trap selectors that only move between builds are masked. Reports exact and near matches."""
import re, sys, glob, os, difflib, json
from collections import defaultdict

def load(d):
    funcs = {}
    for f in sorted(glob.glob(os.path.join(d, 'seg*.s'))):
        cur = None
        for l in open(f):
            m = re.match(r'(fn_S\d+_[0-9A-F]+):', l)
            if m: cur = m.group(1); funcs[cur] = []; continue
            m = re.match(r' .[0-9A-F]{5}. ([0-9a-f]+) +(\S.*?)\s*(;.*)?$', l)
            if cur and m and not m.group(2).startswith('dc.b'):
                t = m.group(2)
                t = re.sub(r'-?\$[0-9a-f]+\(a5\)', 'G(a5)', t)
                t = re.sub(r'\$[0-9a-f]+\(pc\)', 'PC', t)
                t = re.sub(r'^(b\w+|db\w+|jmp|jsr|bsr)(\.\w)? (.*?)\$[0-9a-f]+$', r'\1 \3T', t)
                t = re.sub(r'\$[0-9a-f]+\.l', 'ABS', t)
                funcs[cur].append(t)
    return {k: v for k, v in funcs.items() if len(v) >= 4}

a, b = load(sys.argv[1]), load(sys.argv[2])
ia = sum(len(v) for v in a.values())
exact = {}; bykey = defaultdict(list)
for k, v in b.items(): bykey[tuple(v)].append(k)
for k, v in a.items():
    if tuple(v) in bykey: exact[k] = bykey[tuple(v)][0]
near = {}
blist = list(b.items())
for k, v in a.items():
    if k in exact: continue
    best, br = None, 0.0
    sm = difflib.SequenceMatcher(autojunk=False); sm.set_seq2(v)
    for k2, v2 in blist:
        if abs(len(v2) - len(v)) > max(8, len(v) // 3): continue
        sm.set_seq1(v2)
        if sm.real_quick_ratio() < br or sm.quick_ratio() < br: continue
        r = sm.ratio()
        if r > br: best, br = k2, r
    if br >= 0.75: near[k] = (best, br)
ie = sum(len(a[k]) for k in exact); inr = sum(len(a[k]) * near[k][1] for k in near)
print(f"A: {len(a)} functions, {ia} instructions;  B: {len(b)} functions")
print(f"exact matches: {len(exact)} functions ({100*ie/ia:.0f}% of A's instructions)")
print(f"near matches (>=75% similar): {len(near)} functions (~{100*inr/ia:.0f}% more)")
segs = defaultdict(lambda: [0, 0, 0])
for k, v in a.items():
    s = int(k.split('_')[1][1:]); segs[s][0] += len(v)
    if k in exact: segs[s][1] += len(v)
    elif k in near: segs[s][2] += len(v) * near[k][1]
for s in sorted(segs):
    t, e, n = segs[s]; print(f"  A seg {s}: {t:6} insns  exact {100*e/t:4.0f}%  near {100*n/t:4.0f}%")
json.dump({'exact': exact, 'near': {k: [v[0], round(v[1], 3)] for k, v in near.items()}}, open(sys.argv[3], 'w'), indent=1) if len(sys.argv) > 3 else None
