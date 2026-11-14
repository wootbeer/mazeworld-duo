"""Explores a behaviour's states with the autopilot (harness/auto.c): from a start (goal lines),
tries each choice (an ability at a record, or waiting) after each sequence found so far, keeping the
sequences that reach states not yet reached; prints the best sequences and the states reached.
Usage: explore.py <prefix script> <start goals> <seg> <handler hex> <rec> [rounds]
(The output in build/ap/ex/: one rig run per sequence tried.)"""
import sys, os, re, subprocess, glob, struct
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from rsrc import load_fork, parse
prefix, start, seg, off, rec = sys.argv[1], open(sys.argv[2]).read(), int(sys.argv[3]), int(sys.argv[4], 16), int(sys.argv[5])
rounds = int(sys.argv[6]) if len(sys.argv) > 6 else 6
code = {i: d[4:] for t, i, nm, a, d in parse(load_fork(os.path.join(ROOT, 'build/games/original/Mazeworld v.1.0.3/Mazeworld.rsrc'))) if t == 'CODE'}[seg]
S = lambda a: struct.unpack('>h', code[a:a + 2])[0]
# the think table: the handler's lea xxx(pc),a3 then move d0,d7; add; add tbl(pc,d0); jmp
txt = open(os.path.join(ROOT, f'build/disasm-original/seg{seg}.s')).read()
m = re.search(r'%05X\s+\w+\s+lea\.l \$([0-9a-f]+)\(pc\), a3' % off, txt) or re.search(r'%05X[\s\S]{0,400}?lea\.l \$([0-9a-f]+)\(pc\), a3' % off, txt)
disp = int(m.group(1), 16); tbl = disp + 0xC
states = []
for s in range(200):
    a = tbl + 2 * s
    if a + 2 > len(code) or tbl + 2 * s >= min([tbl + 2 * k + S(tbl + 2 * k) for k in range(max(1, s))]): break
    states.append(a + S(a))
# the act table: the handler's jsr xxx(pc,d0.w) after it; its entries tell the states apart (thinks are shared)
m = re.search(r'%05X[\s\S]{0,1200}?jsr \$([0-9a-f]+)\(pc, d0\.w\)' % off, txt)
act = int(m.group(1), 16)
states = [act + 2 * s + S(act + 2 * s) for s in range(len(states))]
CHOICES = ['f1', 'f2', 'f3', 'f4', 'w', 'W']
def goals(seq):
    out = [start]
    for c in seq:
        out.append(f'face {rec}\nfire {c[1]}\nwait 140\n' if c[0] == 'f' else 'wait 300\n' if c == 'w' else 'wait 1200\n')
    return ''.join(out)
def run(seq, tag):
    d = os.path.join(ROOT, 'build/ap/ex', tag)
    g = d + '.goals'; open(g, 'w').write(goals(seq))
    ticks = 1600 + sum(1250 if c == 'W' else 320 for c in seq)
    subprocess.run([os.path.join(ROOT, 'harness/mzharness'), '-o', d, '-script', prefix, '-auto', g, '-ticks', str(ticks),
                    os.path.join(ROOT, 'original/Mazeworld_v.1.0.3.sit')], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    hit = {int(l.split()[0], 16) for l in open(f'{d}/trace/seg{seg}.pcs') if l.strip()}
    return {s for s, a in enumerate(states) if a in hit}
best = {(): run((), 'base')}
seen = set(best[()])
print('states', len(states), 'base reaches', sorted(seen), flush=True)
frontier = [()]
for rnd in range(rounds):
    new = []
    for seq in frontier:
        for c in CHOICES:
            s2 = seq + (c,)
            got = run(s2, 'r%d_%s' % (rnd, '_'.join(s2)))
            if got - seen:
                print('  +', ' '.join(s2), '->', sorted(got - seen), flush=True)
                seen |= got; new.append(s2)
    if not new: break
    frontier = new[:12]
print('reached', len(seen), 'of', len(states), sorted(seen))
