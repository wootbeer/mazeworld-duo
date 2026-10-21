#!/usr/bin/env python3
"""Shows how a Catacombs function differs from its Mazeworld Original pair (build/orig_map.json).
Usage: diff_pair.py fn_S2_1CF6"""
import sys, re, json, difflib
def body(d, name):
    seg = re.match(r'fn_S(\d+)_', name).group(1)
    out, on = [], False
    for l in open(f'{d}/seg{seg}.s'):
        if re.match(r'fn_S\d+_[0-9A-F]+:', l): on = l.startswith(name + ':'); continue
        m = re.match(r'\s+[0-9A-F]{5}\*?\s+[0-9a-f]+\s+(.*)$', l)
        if on and m and not m.group(1).startswith('dc.'): out.append(m.group(1).rstrip())
    return out
f = sys.argv[1]
o = json.load(open('build/orig_map.json'))['functions'][f]['original']
norm = lambda x: re.sub(r'\$[0-9a-f]+|G_[0-9A-F]+|fn_S\d+_[0-9A-F]+|JT\[\d+\]', '#', x)
a, b = body('build/disasm', f), body('build/disasm-original', o)
print(f'{f} -> {o}')
sm = difflib.SequenceMatcher(None, [norm(x) for x in a], [norm(x) for x in b], autojunk=False)
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == 'equal': continue
    for x in a[i1:i2]: print('  - ' + x)
    for x in b[j1:j2]: print('  + ' + x)
    print('  ---')
