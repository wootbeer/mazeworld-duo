#!/usr/bin/env python3
"""Shows how a Catacombs function differs from its Mazeworld Original pair (build/orig_map.json),
compared as tools/map_builds.py compares them (what only moved -- globals, jump-table slots, code
addresses -- counts as equal). Usage: diff_pair.py fn_S2_1CF6 [fn_S2_12C8]"""
import sys, os, json, difflib
HERE = os.path.dirname(os.path.abspath(__file__))
f = sys.argv[1]
g = sys.argv[2] if len(sys.argv) > 2 else json.load(open('build/orig_map.json'))['functions'][f]['original']
sys.argv = ['map_builds.py', 'build/disasm', 'build/disasm-original']
exec(open(os.path.join(HERE, 'map_builds.py')).read().replace('\nmain()\n', '\n'))
a, b = load('build/disasm')[f], load('build/disasm-original')[g]
print(f'{f} -> {g}')
sm = difflib.SequenceMatcher(None, [x[0] for x in a], [x[0] for x in b], autojunk=False)
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == 'equal': continue
    for x in a[i1:i2]: print('  - ' + x[4])
    for x in b[j1:j2]: print('  + ' + x[4])
    print('  ---')
