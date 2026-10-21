#!/usr/bin/env python3
"""Words of a CODE segment (offsets as in build/disasm*/seg<N>.s), for tables inside code.
Usage: code_bytes.py [catacombs|original] <seg> <offset hex> <count of words>"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rsrc import load_fork, parse
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'games')
app = {'catacombs': 'catacombs/Mazeworld Catacombs 1.0.5/Mazeworld Catacombs.rsrc',
       'original': 'original/Mazeworld v.1.0.3/Mazeworld.rsrc'}[sys.argv[1]]
seg, off, n = int(sys.argv[2]), int(sys.argv[3], 16), int(sys.argv[4])
code = {i: d for t, i, nm, a, d in parse(load_fork(os.path.join(ROOT, app))) if t == 'CODE'}[seg][4:]
print(' '.join(f'{struct.unpack(">H", code[off + 2 * k:off + 2 * k + 2])[0]:04X}' for k in range(n)))
