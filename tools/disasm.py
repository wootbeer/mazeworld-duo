#!/usr/bin/env python3
"""Mazeworld Catacombs 68k disassembler.

Produces, from the original application resource fork:
  build/disasm/seg<N>.s     annotated listing per CODE segment
  build/disasm/jumptable.txt  jump-table map (index, A5 offset, seg:offset, function id)
  build/disasm/functions.json function list with boundaries, callers, callees, traps used
  build/disasm/globals.txt    A5-relative global usage (offset -> functions)

Names are neutral ids only (fn_S<seg>_<off>), never guessed.
Usage: disasm.py <app .rsrc or AppleDouble> <outdir>
"""
import sys, os, json, struct, re
from collections import defaultdict
import capstone as cs

sys.path.insert(0, os.path.dirname(__file__))
from rsrc import load_fork, parse

HERE = os.path.dirname(os.path.abspath(__file__))
TR = json.load(open(os.path.join(HERE, 'traps.json')))
TRAPS, DISP = TR['traps'], TR['dispatchers']

# OS traps (A0xx-A7xx): bits 8-10 are flags; identify by low byte.
OS_BY_NUM = {}
for k, v in TRAPS.items():
    w = int(k, 16)
    if w & 0x0800 == 0:
        OS_BY_NUM.setdefault(w & 0xFF, v['name'])

def trap_name(w):
    if w & 0x0800:  # Toolbox trap: 10-bit number, bit 10 = autopop
        k = f"{0xA800 | (w & 0x3FF):04X}"
        n = TRAPS.get(k, {}).get('name')
        return (n or f"Toolbox_{w & 0x3FF:03X}") + (",AUTOPOP" if w & 0x0400 else "")
    num = w & 0xFF
    if num in (0x46, 0x47):  # (N)Get/SetTrapAddress: bit 9 = new style, bit 10 = Toolbox table
        base = 'GetTrapAddress' if num == 0x46 else 'SetTrapAddress'
        if w & 0x0200: return ('N' + base) + (',TOOL' if w & 0x0400 else ',OS')
        return base
    n = OS_BY_NUM.get(num) or f"OS_{num:02X}"
    fl = []
    if num <= 0x18 or num in (0x44, 0x60):        # File Manager: ASYNC / HFS bits
        if w & 0x0400: fl.append('ASYNC')
        if w & 0x0200: fl.append('HFS')
    elif num in (0x1E, 0x22, 0x24, 0x27):        # NewPtr/NewHandle/...: SYS / CLEAR
        if w & 0x0400: fl.append('SYS')
        if w & 0x0200: fl.append('CLEAR')
    else:
        if w & 0x0400: fl.append('F10')
        if w & 0x0200: fl.append('F9')
    return n + (',' + ','.join(fl) if fl else '')

def dispatcher_for(w):
    name = trap_name(w).split(',')[0]
    return DISP.get(name)

EXTRA_SEL = {}  # every selector the game uses is in multiversal

def selector_name(w, hist):
    nm = trap_name(w).split(',')[0]
    d = DISP.get(nm)
    if not d or not hist: return None
    mn, ops = hist[-1]
    loc = d.get('loc', 'stack')
    val = None
    m = re.match(r'#(-?\$?[0-9a-f]+), (d0|-\(a7\))$', ops)
    if m and mn.startswith(('move', 'moveq')):
        reg = m.group(2)
        if (loc.startswith('D0') and reg == 'd0') or (loc.startswith('Stack') and reg == '-(a7)'):
            val = int(m.group(1).replace('$', '0x'), 0) & 0xFFFFFFFF
            if loc.startswith('D0<'): val &= 0xFFFFFF
            if loc in ('D0W', 'StackW'): val &= 0xFFFF
    if val is None: return None
    key = f"{val:04X}"
    return d['sel'].get(key) or EXTRA_SEL.get(nm, {}).get(key) or f"{nm}_sel_{key}"

JT_BASE = 32  # first JT entry sits at A5+32 under MPW

class Seg:
    def __init__(self, num, raw):
        self.num = num
        self.jtoff, self.jtcount = struct.unpack('>HH', raw[:4])
        self.code = raw[4:]

def main(src, out):
    os.makedirs(out, exist_ok=True)
    res = parse(load_fork(src))
    codes = {i: d for t, i, n, a, d in res if t == 'CODE'}
    c0 = codes[0]
    above, below, jtlen, jtoff = struct.unpack('>IIII', c0[:16])
    jt = []  # (index, a5off, seg, off)
    for k in range(jtlen // 8):
        e = c0[16 + 8 * k:24 + 8 * k]
        off, op, seg, trap = struct.unpack('>HHHH', e)
        assert op == 0x3F3C and trap == 0xA9F0, (k, e.hex())
        jt.append((k, JT_BASE + 8 * k, seg, off))
    segs = {n: Seg(n, d) for n, d in codes.items() if n != 0}

    md = cs.Cs(cs.CS_ARCH_M68K, cs.CS_MODE_M68K_020)
    md.detail = True

    def jt_entry_for_a5(d):
        # JSR d(A5) points at the 'move.w #seg' word, i.e. entry+2
        k, r = divmod(d - JT_BASE - 2, 8)
        if r == 0 and 0 <= k < len(jt): return jt[k]
        return None

    fn_id = lambda s, o: f"fn_S{s}_{o:04X}"
    funcs = {}  # (seg, off) -> info
    def addf(s, o, why):
        f = funcs.setdefault((s, o), {'seg': s, 'off': o, 'why': set(), 'callers': set(), 'callees': set(),
                                      'traps': set(), 'globals': set(), 'jt': []})
        f['why'].add(why); return f
    for k, a5, s, o in jt:
        addf(s, o, 'jt')['jt'].append(k)

    # ---------- per-segment recursive descent ----------
    listing = {}
    provs = {}
    gl_use = defaultdict(set)
    for n, sg in sorted(segs.items()):
        code = sg.code
        insn = {}           # off -> (size, text, comment)
        queue = [o for (s, o) in list(funcs) if s == n]
        # Compiled functions almost always start with LINK A6: heuristic seeds, decoded after proven/traced code.
        link_seeds = [m.start() for m in re.finditer(rb'\x4e\x56', code) if m.start() % 2 == 0]
        prov = {}
        cover = bytearray(len(code))   # 1 = byte belongs to a decoded instruction
        conflicts = []
        trace_f = os.path.join(os.path.dirname(out), os.path.basename(out).replace('disasm', 'trace'), f'seg{n}.pcs')
        traced = [int(x, 16) for x in open(trace_f).read().split()] if os.path.exists(trace_f) else []
        traced_set = set(traced)
        def phases():
            yield 'proven', queue
            yield 'traced', traced
            yield 'link', link_seeds
            tried = set()
            while True:
                # next even offset not yet decoded or tried
                o, nxt = 0, None
                while o < len(code):
                    if o in insn: o += insn[o][0]; continue
                    if o not in tried: nxt = o; break
                    o += 2
                if nxt is None: return
                tried.add(nxt)
                yield 'swept', [nxt]
        for phase, queue in phases():
          while queue:
            pc = queue.pop()
            hist = []
            before = set(insn)
            while 0 <= pc < len(code) and pc not in insn:
                w = struct.unpack('>H', code[pc:pc + 2])[0] if pc + 2 <= len(code) else None
                if w is None: break
                if w & 0xF000 == 0xA000:
                    nm = trap_name(w)
                    sel = selector_name(w, hist)
                    if cover[pc]: break
                    insn[pc] = (2, f"_{nm}", f"trap {w:04X}" + (f" -> {sel}" if sel else ''), ('trap', w, sel))
                    cover[pc] = cover[pc + 1] = 1
                    hist.append((f"_{nm}", ''))
                    if w == 0xA9F4:  # _ExitToShell does not return
                        break
                    pc += 2; continue
                got = list(md.disasm(code[pc:pc + 10], pc, count=1))
                if not got:
                    if phase != 'swept': insn[pc] = (2, f"dc.w ${w:04X}", 'undecodable', ('bad', w))
                    break
                i = got[0]
                if any(cover[pc:pc + i.size]):
                    if phase in ('proven', 'traced'): conflicts.append(pc)
                    break
                mn, ops = i.mnemonic, i.op_str
                cmt, kind = '', None
                tgt = None
                m = re.search(r'\$([0-9a-f]+)$', ops)
                if mn.startswith(('bra', 'bsr', 'b', 'db')) and mn not in ('btst.l', 'btst.b', 'bset.b', 'bset.l', 'bclr.b', 'bclr.l', 'bchg.b', 'bchg.l', 'bfextu', 'bfexts', 'bfins', 'bfclr', 'bfset', 'bftst', 'bkpt') and m:
                    tgt = int(m.group(1), 16)
                # A5-relative
                for d in re.findall(r'(-?\$[0-9a-f]+)\(a5\)', ops):
                    v = int(d.replace('$', '0x'), 16)
                    if mn.startswith('jsr') or mn.startswith('jmp') or mn.startswith('pea') or mn.startswith('lea'):
                        e = jt_entry_for_a5(v) if v > 0 else None
                        if e:
                            k, a5, s, o = e
                            cmt = f"-> JT[{k}] {fn_id(s, o)}"
                            kind = ('call' if mn.startswith('jsr') else 'ref', (s, o))
                            continue
                    if v < 0:
                        cmt = (cmt + ' ' if cmt else '') + f"G_{-v:04X}"
                        kind = kind or ('glob', v)
                        gl_use[v].add((n, pc))
                if mn.startswith(('jsr', 'jmp')) and '(pc)' in ops:
                    mm = re.match(r'\$([0-9a-f]+)\(pc\)', ops)
                    if mm: tgt = int(mm.group(1), 16)  # capstone resolves pc-rel to absolute
                insn[pc] = (i.size, f"{mn} {ops}".strip(), cmt, kind)
                for k in range(pc, pc + i.size): cover[k] = 1
                if tgt is not None:
                    if mn.startswith(('bsr', 'jsr')):
                        insn[pc] = (i.size, f"{mn} {ops}".strip(), (cmt + ' ' if cmt else '') + f"-> {fn_id(n, tgt)}", ('lcall', (n, tgt)))
                        addf(n, tgt, 'bsr'); queue.append(tgt)
                    else:
                        queue.append(tgt)
                hist.append((mn, ops))
                # MPW C switch: cmpi.w #N,dX ; bhi default ; lea T(pc),a1 ; add.w dX,dX ; adda.w (a1,dX.w),a1 ; jmp (a1)
                if mn.startswith('jmp') and ops == '(a1)' and len(hist) >= 6:
                    h = hist[-6:]
                    mt = re.match(r'\$([0-9a-f]+)\(pc\), a1', h[2][1])
                    mc = re.match(r'#\$?([0-9a-f]+), (d\d)', h[0][1])
                    if h[0][0].startswith('cmpi') and h[1][0].startswith(('bhi', 'bcc', 'bhs')) and mt and mc and h[3][0].startswith('add') and h[4][0].startswith('adda'):
                        tbase = int(mt.group(1), 16); cnt = int(mc.group(1), 16) + 1
                        for k in range(cnt):
                            o = tbase + 2 * k
                            dsp = struct.unpack('>h', code[o:o + 2])[0]
                            insn[o] = (2, f"dc.w {fn_id(n, tbase)[3:]}_case{k}", f"switch case {k} -> {tbase + dsp:05X}", ('case', tbase + dsp))
                            cover[o] = cover[o + 1] = 1
                            queue.append(tbase + dsp)
                # hand-written jump table: [andi.w #mask,dX ; add.w dX,dX ;] add.w T(pc,dX.w),dX ; jmp T(pc,dX.w)
                # entries are word offsets from T + 2k; the table ends at the mask or at the first word
                # that is known code (traced), whichever comes first
                mj = re.match(r'\$([0-9a-f]+)\(pc, (d\d)\.w\)$', ops) if mn.startswith('jmp') else None
                if mj and len(hist) >= 2 and hist[-2][0].startswith('add.w') and hist[-2][1].startswith(f"${mj.group(1)}(pc, {mj.group(2)}.w)"):
                    tbase = int(mj.group(1), 16); cnt = 16
                    for hm, ho in reversed(hist[-8:]):
                        mm2 = re.match(r'#\$([0-9a-f]+), ' + mj.group(2) + '$', ho)
                        if hm.startswith('andi') and mm2: cnt = int(mm2.group(1), 16) + 1; break
                    known = traced_set
                    for k in range(cnt):
                        o = tbase + 2 * k
                        if o in known or o + 2 > len(code) or cover[o]: break
                        tgt2 = tbase + 2 * k + struct.unpack('>h', code[o:o + 2])[0]
                        if not (0 <= tgt2 < len(code)) or tgt2 & 1: break
                        insn[o] = (2, f"dc.w {fn_id(n, tbase)[3:]}_case{k}", f"jump table entry {k} -> {tgt2:05X}", ('case', tgt2))
                        cover[o] = cover[o + 1] = 1
                        queue.append(tgt2)
                if mn in ('rts', 'rte', 'rtd', 'rtr') or mn.startswith(('bra', 'jmp')):
                    break
                pc += i.size
            for o in set(insn) - before: prov[o] = phase
            if phase == 'swept': break
        listing[n] = insn
        provs[n] = prov
        if conflicts: print(f"seg{n}: {len(conflicts)} proven/traced starts overlap other code: " + ' '.join(f'{c:05X}' for c in conflicts[:10]))

    # ---------- function ownership: each insn belongs to the nearest preceding function start ----------
    starts = defaultdict(list)
    for (s, o) in funcs: starts[s].append(o)
    for n in starts: starts[n].sort()
    # LINK seeds that were decoded as code but are not referenced still mark a function start
    for n, insn in listing.items():
        for pc, (sz, txt, cmt, kind) in insn.items():
            if txt.startswith('link.w a6'):
                prev = struct.unpack('>H', segs[n].code[pc - 2:pc])[0] if pc >= 2 else 0
                if pc not in starts[n]:
                    addf(n, pc, 'link'); starts[n].append(pc)
        starts[n].sort()
    import bisect
    def owner(n, pc):
        lst = starts[n]; i = bisect.bisect_right(lst, pc) - 1
        return (n, lst[i]) if i >= 0 else None
    for n, insn in listing.items():
        for pc, (sz, txt, cmt, kind) in insn.items():
            ow = owner(n, pc)
            if not ow: continue
            F = funcs[ow]
            if kind and kind[0] in ('call', 'lcall'):
                F['callees'].add(kind[1]); funcs[kind[1]]['callers'].add(ow)
            elif kind and kind[0] == 'trap':
                F['traps'].add(kind[2] or trap_name(kind[1]).split(',')[0])
            elif kind and kind[0] == 'glob':
                F['globals'].add(-kind[1])

    # ---------- write listings ----------
    executed = {}
    for n in segs:
        tf = os.path.join(os.path.dirname(out), os.path.basename(out).replace('disasm', 'trace'), f'seg{n}.pcs')
        if os.path.exists(tf): executed[n] = {int(x, 16) for x in open(tf).read().split()}
    for n, insn in sorted(listing.items()):
        code = segs[n].code
        fs = set(starts[n])
        lines = [f"; CODE {n}: {len(code)} bytes, JT first-entry offset {segs[n].jtoff}, {segs[n].jtcount} JT entries",
                 f"; Offsets are relative to segment code start (resource offset + 4).",
                 "; Column 1: ' ' proven by control flow, 't' proven by emulator trace, 'l' reached only from a LINK A6 scan, '?' unproven linear sweep",
                 "; '*' after the offset: executed in a recorded harness run", ""]
        pc = 0
        while pc < len(code):
            if pc in fs:
                F = funcs[(n, pc)]
                lines.append('')
                hdr = f"{fn_id(n, pc)}:"
                tags = []
                if F['jt']: tags.append('JT ' + ','.join(map(str, F['jt'])))
                if F['callers']: tags.append(f"{len(F['callers'])} callers")
                lines.append(f"{hdr:30} ; {'; '.join(tags)}")
            if pc in insn:
                sz, txt, cmt, kind = insn[pc]
                hx = code[pc:pc + sz].hex()
                pv = provs[n].get(pc, 'proven')
                mark = {'proven': ' ', 'traced': 't', 'link': 'l', 'swept': '?'}[pv]
                ex = '*' if pc in executed.get(n, ()) else ' '
                lines.append(f" {mark}{pc:05X}{ex} {hx:<20} {txt:<40}" + (f" ; {cmt}" if cmt else ''))
                pc += sz
            else:
                # data run until next decoded insn / function
                e = pc
                while e < len(code) and e not in insn and (e == pc or e not in fs): e += 1
                blob = code[pc:e]
                for j in range(0, len(blob), 16):
                    chunk = blob[j:j + 16]
                    asc = ''.join(chr(c) if 32 <= c < 127 else '.' for c in chunk)
                    lines.append(f"  {pc + j:05X}  dc.b {chunk.hex(' ')}   ; |{asc}|")
                pc = e
        open(os.path.join(out, f'seg{n}.s'), 'w').write('\n'.join(lines) + '\n')

    with open(os.path.join(out, 'jumptable.txt'), 'w') as f:
        f.write(f"; CODE 0: above A5 {above}, below A5 (globals) {below}, JT {len(jt)} entries\n")
        f.write("; idx  A5off  seg:off      function\n")
        for k, a5, s, o in jt:
            f.write(f"{k:5} {a5:6}  {s}:{o:04X}   {fn_id(s, o)}\n")

    fl = []
    for (s, o), F in sorted(funcs.items()):
        lst = starts[s]; i = lst.index(o)
        end = lst[i + 1] if i + 1 < len(lst) else len(segs[s].code)
        fl.append({'id': fn_id(s, o), 'seg': s, 'off': o, 'end': end, 'size': end - o,
                   'why': sorted(F['why']), 'jt': F['jt'],
                   'callers': sorted(fn_id(*c) for c in F['callers']),
                   'callees': sorted(fn_id(*c) for c in F['callees']),
                   'traps': sorted(F['traps']), 'globals': sorted(F['globals'])})
    json.dump(fl, open(os.path.join(out, 'functions.json'), 'w'), indent=1)

    with open(os.path.join(out, 'globals.txt'), 'w') as f:
        f.write(f"; A5 world: {below} bytes below A5. Globals named G_<positive offset below A5>.\n")
        for v in sorted(gl_use, reverse=True):
            users = sorted({fn_id(*owner(s, p)) for s, p in gl_use[v] if owner(s, p)})
            f.write(f"G_{-v:04X}  uses={len(gl_use[v]):3}  {' '.join(users)}\n")

    decoded = sum(sz for insn in listing.values() for sz, *_ in insn.values())
    swept = sum(listing[n][o][0] for n in provs for o, p in provs[n].items() if p == 'swept')
    trc = sum(listing[n][o][0] for n in provs for o, p in provs[n].items() if p == 'traced')
    lnk = sum(listing[n][o][0] for n in provs for o, p in provs[n].items() if p == 'link')
    total = sum(len(s.code) for s in segs.values())
    print(f"segments={len(segs)} jt={len(jt)} functions={len(fl)} code_bytes={total} decoded={decoded} ({100*decoded/total:.1f}%) of which traced={trc} link-scan={lnk} swept={swept} globals={len(gl_use)}")

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
