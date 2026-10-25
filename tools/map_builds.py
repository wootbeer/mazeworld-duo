#!/usr/bin/env python3
"""Maps Mazeworld Catacombs onto Mazeworld Original: matches functions (as compare_builds.py does) and,
inside each matched pair, aligns the instructions to pair up what only moved between the builds:
A5 offsets of globals, jump-table slots, and the functions called. Output (JSON on stdout, or
--header for game/orig_map.h): the global offset map Catacombs -> Original, the function map, and
the conflicts (a Catacombs global that maps to two Original ones).
Usage: map_builds.py build/disasm build/disasm-original [--header]"""
import re, sys, glob, os, difflib, json
from collections import defaultdict, Counter

INSN = re.compile(r' .([0-9A-F]{5}). ([0-9a-f]+) +(\S.*?)\s*(;.*)?$')

def load(d):
    funcs, order, falls = {}, [], {}
    for f in sorted(glob.glob(os.path.join(d, 'seg*.s'))):
        cur = None
        for l in open(f):
            m = re.match(r'(fn_S\d+_[0-9A-F]+):', l)
            if m: cur = m.group(1); funcs[cur] = []; order.append(cur); continue
            m = INSN.match(l)
            if cur and m:   # does the function's last line (code or data) run on into the next one?
                falls[cur] = not re.match(r'(dc\.|rts|rte|rtd|jmp|bra)', m.group(3))
            if cur and m and not m.group(3).startswith('dc.'):
                t, c = m.group(3), m.group(4) or ''
                g = [int(x, 16) for x in re.findall(r'-\$([0-9a-f]+)\(a5\)', t)]      # globals below A5
                j = [int(x, 16) for x in re.findall(r'\$([0-9a-f]+)\(a5\)', re.sub(r'-\$[0-9a-f]+\(a5\)', '', t))]   # JT slots
                callee = re.findall(r'-> (?:JT\[\d+\] )?(fn_S\d+_[0-9A-F]+)', c)
                n = re.sub(r'-?\$[0-9a-f]+\(a5\)', 'G(a5)', t)
                n = re.sub(r'\$[0-9a-f]+\(pc', '(PC', n)
                n = re.sub(r'^(b\w+|db\w+|jmp|jsr|bsr)(\.\w)? (.*?)\$[0-9a-f]+$', r'\1 \3T', n)
                n = re.sub(r'\$[0-9a-f]+\.l', 'ABS', n)
                funcs[cur].append((n, g, j, callee[0] if callee else None, t + ('  ' + c if c else '')))
    # a function that runs on into the next one (no rts / jmp / bra at its end) includes it
    full = {}
    for i, k in enumerate(order):
        body, j = list(funcs[k]), i
        while falls.get(order[j]) and j + 1 < len(order) and order[j + 1].rsplit('_', 1)[0] == k.rsplit('_', 1)[0]:
            j += 1; body += funcs[order[j]]
        full[k] = body
    return full

MANUAL = {
    0x1EBC: 0x54C4,   # the 50 graphics handles: fn_S8_04FA / fn_S8_08E8 clear them through A0 = A5 + 4i
    0x4E68: 0x723E,   # the 50 byte flags: fn_S8_01CE / fn_S8_0332 and fn_S8_01F2 / fn_S8_0356 through A0
    # locked resource pointers (fn_S8_083A / fn_S8_0C28): the wall outline's visibility list (fn_S2_4F1C
    # G_1DD8, fn_S2_41E8 G_53E0) and the sprite pass's step table (fn_S2_5810 G_1DD4, fn_S2_4A9A
    # G_53DC) place them; the near pairs' alignment is off by one here
    0x1DD8: 0x53E0, 0x1DD4: 0x53DC, 0x1DD0: 0x53D8,
    0x1DA8: 0x53AC,   # the 50 graphics pointers (fn_S8_02F0 / fn_S8_0452, through A1)
    0x1EE0: 0x54EC,   # the map's handle (fn_S8_04FA / fn_S8_08E8: NewHandle $8000; locked into G_53D4)
    0x215C: 0x573E,   # the view's right edge (fn_S2_040A / fn_S2_03B0, with G_215A..G_215E)
}

MANUAL_FUNCS = {   # pairs the matching cannot find (checked by hand: same callers, callees and traps)
    'fn_S2_040A': 'fn_S2_03B0',   # the frame onto the window
    'fn_S2_4F1C': 'fn_S2_41E8',   # the wall outline
    'fn_S8_0A24': 'fn_S8_0E2A',   # one frame of play
    'fn_S7_3FA6': 'fn_S7_1D90',   # weighing a neighbour cell (exact; Original has more copies)
    'fn_S7_0E46': 'fn_S7_03FE',   # a shot's flight (Original: four steps)
    'fn_S8_02F0': 'fn_S8_0452',   # lock / unlock the 50 graphics handles
    'fn_S8_0360': 'fn_S8_04C2',
    'fn_S8_0136': 'fn_S8_0156',   # mark the level's graphics
    'fn_S2_5810': 'fn_S2_4A9A',   # the objects in view
    'fn_S8_1192': 'fn_S8_177C',   # suspend / resume
}

def header(a, out, pairs, callee):
    """game/orig_map.h: every Catacombs global (unmapped ones 0, so their neighbours' ranges end at
    them), the jump-table slots, and every function pair, flagged when the translated function can
    run for Original as it is: an exact pair whose callees all qualify too."""
    seg = lambda n: int(re.match(r'fn_S(\d+)', n).group(1))
    calls = {k: {x[3] for x in v if x[3]} for k, v in a.items()}
    memo = {}
    def ok(f, stack=()):
        if f in memo or f in stack: return memo.get(f, True)
        p = pairs.get(f)
        # exact, and a callee in the caller's segment stays in it (no load call there in the C code)
        # (short functions pair by their text alone, so the calls must agree: in Original the call
        # goes to the function paired with the callee)
        # (a function marked "both" takes care of what it calls for Original itself)
        r = bool(p) and (f in both or (p[1] == 1.0 and (f in MANUAL_FUNCS or callee.get(f, p[0]) == p[0]) and
            all(ok(c, stack + (f,)) and (seg(c) != seg(f) or seg(pairs[c][0]) == seg(p[0])) for c in calls.get(f, ()))))
        memo[f] = r
        return r
    subs, both = set(), set()
    for l in open(os.path.join(os.path.dirname(__file__), '..', 'game', 'subs.txt')):
        w = l.split('#')[0].split()
        if len(w) > 2 and w[0].isdigit() and 'orig' not in w:
            subs.add(f'fn_S{w[0]}_{w[1].upper()}')
            if 'both' in w: both.add(f'fn_S{w[0]}_{w[1].upper()}')   # handles Original's differences
    allg = sorted({g for v in a.values() for x in v for g in x[1]} | set(MANUAL))
    print('/* Generated by tools/map_builds.py: Catacombs -> Mazeworld Original. */')
    print('static const uint16_t orig_globals[][2] = {   /* Catacombs global, Original (0: none) */')
    for g in allg: print(f'    {{0x{g:04X}, 0x{out["globals"].get(g, 0):04X}}},')
    print('};\nstatic const uint16_t orig_jt[][2] = {')
    for p, q in sorted(out['jt'].items()): print(f'    {{0x{p:04X}, 0x{q:04X}}},')
    print('};\nstatic const uint16_t orig_funcs[][5] = {   /* Catacombs segment, offset; Original segment, offset; runs as translated */')
    for f in sorted(pairs, key=lambda f: (seg(f), f)):
        o = pairs[f][0]
        print(f'    {{{seg(f)}, 0x{f.split("_")[2]}, {seg(o)}, 0x{o.split("_")[2]}, {int(f in subs and ok(f))}}},')
    print('};')

def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    key = lambda f: tuple(x[0] for x in f)
    byb = defaultdict(list)
    for k, v in b.items(): byb[key(v)].append(k)
    byb_all = {k: list(v) for k, v in byb.items()}
    pairs = {k: (v, round(difflib.SequenceMatcher(None, key(a[k]), key(b[v]), autojunk=False).ratio(), 3)) for k, v in MANUAL_FUNCS.items()}
    for k, v in a.items():
        if k in pairs: continue
        c = byb.get(key(v))
        if c and len(c) == 1: pairs[k] = (c.pop(), 1.0)
        # identical twins (the same code on other tables) stay unpaired: Original's are its own (fo_)
    # near matches: same length class, best ratio >= .75, among B functions not taken (and not
    # identical twins)
    twins = {k for ks in byb_all.values() if len(ks) > 1 for k in ks}
    taken = {v[0] for v in pairs.values()} | twins
    blen = sorted(((len(v), k) for k, v in b.items() if k not in taken))
    for k, v in a.items():
        if k in pairs or len(v) < 4: continue
        best, br = None, 0.75
        for L, bk in blen:
            if bk in taken or L < len(v) * 0.8 or L > len(v) * 1.25: continue
            r = difflib.SequenceMatcher(None, key(v), key(b[bk]), autojunk=False).quick_ratio()
            if r < br: continue
            r = difflib.SequenceMatcher(None, key(v), key(b[bk]), autojunk=False).ratio()
            if r >= br: best, br = bk, r
        if best: pairs[k] = (best, br); taken.add(best)
    # align instructions in each pair; equal normalised instructions pair their operands. Then a
    # function still unpaired is paired with what Original calls in its place, and again.
    def align():
        gmap, jmap, fmap = defaultdict(Counter), defaultdict(Counter), defaultdict(Counter)
        for ka, (kb, r) in pairs.items():
            fa, fb = a[ka], b[kb]
            sm = difflib.SequenceMatcher(None, key(fa), key(fb), autojunk=False)
            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                if tag != 'equal': continue
                w = 100 if r == 1.0 else 1        # exact pairs outweigh near ones
                for x, y in zip(fa[i1:i2], fb[j1:j2]):
                    for p, q in zip(x[1], y[1]): gmap[p][q] += w
                    for p, q in zip(x[2], y[2]): jmap[p][q] += w
                    if x[3] and y[3]: fmap[x[3]][y[3]] += w
        return gmap, jmap, fmap
    gmap, jmap, fmap = align()
    gmap, jmap, fmap = align()
    # a short function (under 4 instructions) pairs by its text alone, which is ambiguous: what
    # Original calls in its place decides
    for ka, c in fmap.items():
        if ka in a and len(a[ka]) < 4 and ka in pairs and c.most_common(1)[0][0] != pairs[ka][0]: del pairs[ka]
    taken = {v[0] for v in pairs.values()}
    for ka, c in fmap.items():
        kb = c.most_common(1)[0][0]
        if ka in pairs or kb in taken or ka not in a or kb not in b: continue
        pairs[ka] = (kb, round(difflib.SequenceMatcher(None, key(a[ka]), key(b[kb]), autojunk=False).ratio(), 3) or 0.001)
        taken.add(kb)
    gmap, jmap, fmap = align()
    out = {'functions': {k: {'original': v[0], 'similarity': round(v[1], 3)} for k, v in sorted(pairs.items())},
           'globals': {}, 'jt': {}, 'conflicts': []}
    # one Original global per Catacombs one: the best-supported claims first; first, what the
    # instructions do not show (globals reached through an address register; checked by hand)
    for p, q in MANUAL.items(): out['globals'][p] = q
    used = set(MANUAL.values())
    for n, p, q in sorted(((n, p, q) for p, c in gmap.items() for q, n in c.items()), reverse=True):
        if p in out['globals'] or q in used: continue
        out['globals'][p] = q; used.add(q)
    for p, c in sorted(gmap.items()):
        if len(c) > 1: out['conflicts'].append({'catacombs': f'G_{p:04X}', 'original': {f'G_{x:04X}': m for x, m in c.items()}})
    # jump-table slots: a slot's function, paired, gives Original's slot for it (the aligned
    # instructions only where the function is not paired)
    def slots(d):
        t = {}
        for l in open(os.path.join(d, 'jumptable.txt')):
            w = l.split()
            if len(w) == 4 and w[0].isdigit(): t[int(w[1]) + 2] = w[3]
        return t
    ja, jb = slots(sys.argv[1]), slots(sys.argv[2])
    jb_of = {f: o for o, f in jb.items()}
    for p, c in sorted(jmap.items()): out['jt'][p] = c.most_common(1)[0][0]
    for o, f in ja.items():
        q = jb_of.get(pairs.get(f, (None,))[0])
        if q is not None: out['jt'][o] = q
    if '--header' in sys.argv:
        header(a, out, pairs, {k: c.most_common(1)[0][0] for k, c in fmap.items()})
    else:
        print(json.dumps(out, indent=1))
    sys.stderr.write(f"{len(pairs)} function pairs ({sum(1 for v in pairs.values() if v[1] == 1.0)} exact), "
                     f"{len(out['globals'])} globals mapped, {len(out['conflicts'])} conflicts, {len(out['jt'])} JT slots\n")

main()
