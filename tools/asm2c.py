#!/usr/bin/env python3
"""Translates routines of Mazeworld Original's 68000 code into C, instruction by instruction
(game/asm68.h has the operations): for long, branchy routines such as the larger creature
behaviours, where a hand translation would only restate the code. Every instruction keeps its
exact effect on registers, memory and condition codes, so the result is checked by the rig like
any other C. Code tables in the segment are read from the loaded segment (mz_seg_base). Each routine's
stack (LINK frames, pushes) is 256 bytes of C scratch (mz_local), A7 restored on return.

Usage: asm2c.py <seg> <entry hex>[=name] ... > file.c
Each entry becomes a C function `void name(Regs *r)` (default fo_S<seg>_<off>); routines it calls
with JSR that no C covers yet are translated too (static oc_S<seg>_<off>). Calls through A3 (the
behaviours' "think" pointer, set with LEA x(PC),A3) and through jump tables are resolved to the
routines and labels they can reach."""
import sys, re, os, struct
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rsrc import load_fork, parse

SEG = int(sys.argv[1])
entries = []
for a in sys.argv[2:]:
    off, _, name = a.partition('=')
    entries.append(((SEG, int(off, 16)), name or f'fo_S{SEG}_{int(off, 16):04X}'))

CODES = {i: d[4:] for t, i, nm, a, d in parse(load_fork(os.path.join(ROOT, 'build/games/original/Mazeworld v.1.0.3/Mazeworld.rsrc'))) if t == 'CODE'}
SW = lambda seg, a: struct.unpack('>h', CODES[seg][a:a + 2])[0]
INS, CASES, FALLBACK = {}, {}, {}
JT = {}
for l in open(os.path.join(ROOT, 'build/disasm-original/jumptable.txt')):
    m = re.match(r'\s*(\d+)\s+(\d+)\s+(\d+):([0-9A-F]+)', l)
    if m: JT[int(m.group(2))] = (int(m.group(3)), int(m.group(4), 16))
def decode(seg, a):
    """an instruction the disassembler did not reach (code no run and no proof covered), decoded here"""
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_M68K, capstone.CS_MODE_M68K_020)
    got = list(md.disasm(CODES[seg][a:a + 10], a, count=1))
    w = struct.unpack('>H', CODES[seg][a:a + 2])[0]
    if not got or w & 0xF000 == 0xA000: raise ValueError(f'cannot decode S{seg}:{a:05X}')
    i = got[0]; t = f'{i.mnemonic} {i.op_str}'.strip(); cm = ''
    m = re.fullmatch(r'jsr \$([0-9a-f]+)\(a5\)', t)
    if m and int(m.group(1), 16) in JT: s2, o2 = JT[int(m.group(1), 16)]; cm = f'; -> JT fn_S{s2}_{o2:04X}'
    FALLBACK[seg] += 1
    return (t, cm, i.size)
# instructions the disassembler took as data (a table's guessed end overlaps them)
PATCH = {(10, 0x115A): ('jsr $322(a5)', '; -> JT[96] fn_S7_12BE', 4)}
def seg_tables(seg):
    if seg in INS: return
    ins, cases = {}, {}
    for l in open(os.path.join(ROOT, f'build/disasm-original/seg{seg}.s')):
        m = re.match(r'\s*[ ?l]?([0-9A-F]{5})\*?\s+([0-9a-f]+)\s+(.*?)\s*(;.*)?$', l)
        if not m: continue
        if m.group(3).startswith('dc.'):
            mm = re.match(r'dc\.w S\d+_([0-9A-F]+)_case\d+ ; .*-> ([0-9A-F]+)', m.group(3) + ' ' + (m.group(4) or ''))
            if mm: cases.setdefault(int(mm.group(1), 16), []).append(int(mm.group(2), 16))
            continue
        ins[int(m.group(1), 16)] = (m.group(3).strip(), m.group(4) or '', len(m.group(2)) // 2)
    for (sg, a), v in PATCH.items():
        if sg == seg: ins[a] = v
    INS[seg] = ins
    FALLBACK[seg] = 0
    CASES[seg] = {t: [x for x in v if x in ins] for t, v in cases.items()}   # (a table's length is the disassembler's guess)

# routines C already covers: Original's own (subs.txt orig) and shared ones that run for Original
cfun = {}
subs = {}
for l in open(os.path.join(ROOT, 'game/subs.txt')):
    w = l.split('#')[0].split()
    if len(w) < 4 or not w[0].isdigit() or w[2] == 'internal': continue
    subs[(int(w[0]), int(w[1], 16))] = (w[2], w[3].split('(')[0] if w[2] != 'regs' else w[3], 'orig' in w)
for (s, o), (kind, name, orig) in subs.items():
    if orig: cfun[(s, o)] = (kind, name, None)
for l in open(os.path.join(ROOT, 'game/orig_map.h')):
    m = re.match(r'\s*\{(\d+), 0x([0-9A-F]+), (\d+), 0x([0-9A-F]+), 1\}', l)
    if not m: continue
    cs, co, os_, oo = int(m.group(1)), int(m.group(2), 16), int(m.group(3)), int(m.group(4), 16)
    if (cs, co) in subs and not subs[(cs, co)][2]:
        cfun.setdefault((os_, oo), (subs[(cs, co)][0], subs[(cs, co)][1], (cs, co)))

REG = {f'd{i}': f'r->d[{i}]' for i in range(8)}; REG.update({f'a{i}': f'r->a[{i}]' for i in range(8)}); REG['sp'] = 'r->a[7]'
SZ = {'b': 1, 'w': 2, 'l': 4}
COND = {'hi', 'ls', 'cc', 'cs', 'ne', 'eq', 'vc', 'vs', 'pl', 'mi', 'ge', 'lt', 'gt', 'le'}

def num(s):
    neg = s.startswith('-'); s = s.lstrip('-')
    v = int(s[1:], 16) if s.startswith('$') else int(s)
    return -v if neg else v

class EA:
    """an operand: kind 'reg' (C lvalue), 'imm' (value), 'mem' (address expression)"""
    def __init__(self, kind, x, reg=None, post=0, pre=0):
        self.kind, self.x, self.reg, self.post, self.pre = kind, x, reg, post, pre

def ea(op, sz):
    op = op.strip()
    if op in REG: return EA('reg', REG[op], reg=op)
    if op.startswith('#'): return EA('imm', num(op[1:]) & 0xFFFFFFFF)
    m = re.fullmatch(r'(-?\$[0-9a-f]+)\(a5\)', op)
    if m and num(m.group(1)) < 0: return EA('mem', f'OADDR(0x{-num(m.group(1)):X})')
    m = re.fullmatch(r'(-?\$[0-9a-f]+)?\((a[0-7]|sp)\)', op)
    if m:
        d = num(m.group(1)) if m.group(1) else 0
        return EA('mem', f'({REG[m.group(2)]} + 0x{d & 0xFFFFFFFF:X}u)' if d else REG[m.group(2)])
    m = re.fullmatch(r'\((a[0-7]|sp)\)\+', op)
    if m: return EA('mem', REG[m.group(1)], reg=m.group(1), post=sz)
    m = re.fullmatch(r'-\((a[0-7]|sp)\)', op)
    if m: return EA('mem', REG[m.group(1)], reg=m.group(1), pre=sz)
    m = re.fullmatch(r'(-?\$[0-9a-f]+)?\((a[0-7]|pc), ([ad][0-7])\.([wl])\)', op)
    if m:
        d = num(m.group(1)) if m.group(1) else 0
        idx = f'sxw({REG[m.group(3)]})' if m.group(4) == 'w' else REG[m.group(3)]
        base = 'cs' if m.group(2) == 'pc' else REG[m.group(2)]
        return EA('mem', f'({base} + 0x{d & 0xFFFFFFFF:X}u + {idx})')
    m = re.fullmatch(r'\$([0-9a-f]+)\(pc\)', op)
    if m: return EA('mem', f'(cs + 0x{int(m.group(1), 16):X}u)')
    m = re.fullmatch(r'\$([0-9a-f]+)\.w', op)
    if m: return EA('mem', f'0x{int(m.group(1), 16):X}u')
    raise ValueError(f'operand {op!r}')

def operands(s):
    parts, depth, cur = [], 0, ''
    for ch in s:
        if ch == '(': depth += 1
        if ch == ')': depth -= 1
        if ch == ',' and depth == 0: parts.append(cur); cur = ''
        else: cur += ch
    parts.append(cur)
    return [p.strip() for p in parts]

queue = list(entries); done = {}; a3_targets = set(); rel_targets = set(); out_fns = []
def fname(k): return next((n for e, n in entries if e == k), f'oc_S{k[0]}_{k[1]:04X}')
def want(k):
    if k not in done and all(k != q[0] for q in queue): queue.append((k, fname(k)))

def call_of(k):
    """C for a call of the routine k = (seg, offset)"""
    if k in cfun:
        kind, name, cat = cfun[k]
        if kind != 'regs': raise ValueError(f'call of C-convention {name}')
        return (f'mz_seg_of({cat[0]}, 0x{cat[1]:04X}), ' if cat else f'mz_seg({k[0]}), ') + f'{name}(r)'
    want(k)
    return f'{fname(k)}(r)'

class Ctx: pass
def translate(k, name):
    seg, entry = k; seg_tables(seg); ins, cases = INS[seg], CASES[seg]
    seen = set(); work = [entry]; labels = set()
    while work:
        a = work.pop()
        while a not in seen:
            if a not in ins: ins[a] = decode(seg, a)
            seen.add(a)
            t = ins[a][0]; op = t.split()[0]
            m = re.match(r'(b[a-z]{2}|bra)\.[bw] \$([0-9a-f]+)$', t) or re.match(r'(db[a-z]{1,2})\.w [ad]\d, \$([0-9a-f]+)$', t)
            if m:
                labels.add(int(m.group(2), 16)); work.append(int(m.group(2), 16))
                if op.startswith('bra'): break
            mm = re.match(r'jmp \$([0-9a-f]+)\(pc, d0\.w\)', t)
            if mm:
                for tg in cases.get(int(mm.group(1), 16), []): labels.add(tg); work.append(tg)
                break
            if op in ('rts', 'jmp'): break
            a += ins[a][2]
    order = sorted(seen)
    body = []
    if order[0] != entry: body.append(f'    goto L{entry:05X};'); labels.add(entry)   # the entry is not the lowest address
    c = Ctx(); c.seg, c.ins, c.cases, c.think = seg, ins, cases, None
    for b in order[:12]:
        mm = re.fullmatch(r'lea\.l \$([0-9a-f]+)\(pc\), a3', ins[b][0])
        if mm: c.think = int(mm.group(1), 16); a3_targets.add((seg, c.think)); break
    for i, a in enumerate(order):
        t, cm, n = ins[a]
        if a in labels: body.append(f'L{a:05X}:')
        try: body.extend('    ' + x for x in insn(c, a, t, cm))
        except ValueError as e: raise ValueError(f'S{seg}:{a:05X} {t}: {e}')
        term = re.match(r'(rts|bra|jmp)', t)
        if not term and (i + 1 == len(order) or order[i + 1] != a + n):
            body.append(f'    goto L{a + n:05X};')
            if a + n not in labels: raise ValueError(f'S{seg}:{a:05X}: falls through to {a + n:05X}, not translated')
    out_fns.append((k, name, body))

def read(e, sz):
    if e.kind == 'imm': return f'0x{e.x & 0xFFFFFFFF:X}u'
    if e.kind == 'reg': return e.x
    return f'm_rd({sz}, {e.x})'
def write(e, sz, v):
    if e.kind == 'reg':
        if e.reg[0] == 'a': return [f'{e.x} = {v};']
        return [f'm_set({sz}, &{e.x}, {v});'] if sz < 4 else [f'{e.x} = {v};']
    out = [f'm_wr({sz}, {e.x}, {v});']
    if e.post: out.append(f'{e.x} += {e.post};')
    return out
def pre(e):
    return [f'{e.x} -= {e.pre};'] if e.pre else []

def rel_table(c, tbl, at):
    """the routines a JSR tbl(PC, D0.W) reaches: one per state of the think routine last put in A3"""
    think = None
    for b in sorted((x for x in c.ins if x < at), reverse=True)[:40]:
        mm = re.fullmatch(r'lea\.l \$([0-9a-f]+)\(pc\), a3', c.ins[b][0])
        if mm: think = int(mm.group(1), 16); break
    if think is None: raise ValueError('call table without a think routine')
    a3_targets.add((c.seg, think))
    tt = int(re.search(r'\$([0-9a-f]+)\(pc', c.ins[think + 4][0]).group(1), 16)
    return [tbl + 2 * i + SW(c.seg, tbl + 2 * i) for i in range(len(c.cases[tt]))]

def insn(c, a, t, cm):
    m = re.match(r'([a-z]+)(?:\.([bwl]))?\s*(.*)$', t)
    op, s, args = m.group(1), m.group(2), m.group(3)
    sz = SZ.get(s, 4)
    ops = operands(args) if args else []
    if op == 'rts': return ['r->a[7] = sp0; return;']
    if op == 'nop': return []
    if op == 'bra': return [f'goto L{int(args[1:], 16):05X};']
    if op[0] == 'b' and op[1:] in COND: return [f'if (CC_{op[1:].upper()}) goto L{int(args[1:], 16):05X};']
    if op in ('dbra', 'dbf'):
        rg, tg = ops; return [f'set_w(&{REG[rg]}, (uint16_t)({REG[rg]} - 1)); if ((uint16_t){REG[rg]} != 0xFFFF) goto L{int(tg[1:], 16):05X};']
    if op in ('jsr', 'jmp'):
        tail = op == 'jmp'
        wrap = lambda x: ([f'{x}; r->a[7] = sp0; return;'] if tail else [f'{x};'])
        mm = re.fullmatch(r'\$([0-9a-f]+)\(pc\)', args)
        if mm: return wrap(call_of((c.seg, int(mm.group(1), 16))))
        mm = re.fullmatch(r'\$([0-9a-f]+)\(a5\)', args)
        if mm:
            jt = re.search(r'fn_S(\d+)_([0-9A-F]+)', cm)
            return wrap(call_of((int(jt.group(1)), int(jt.group(2), 16))))
        if args == '(a3)': return wrap('oc_call_a3(r)')
        mm = re.fullmatch(r'\$([0-9a-f]+)\(pc, d0\.w\)', args)
        if mm:
            tbl = int(mm.group(1), 16)
            if tail:   # after "add.w tbl(pc, d0.w), d0", d0 holds the target's offset from tbl
                tg = c.cases.get(tbl)
                if not tg: raise ValueError('jump table without cases')
                return [' '.join([f'switch ((uint16_t)(0x{tbl:X}u + (uint16_t)r->d[0])) {{'] + [f'case 0x{x:X}: goto L{x:05X};' for x in sorted(set(tg))]
                                 + [f'default: hx_fault("jump table at S{c.seg}:%05X: index %04X", 0x{a:X}u, (unsigned)(uint16_t)r->d[0]); r->a[7] = sp0; return; }}'])]
            for x in rel_table(c, tbl, a): rel_targets.add((c.seg, x))
            return wrap(f'oc_call_rel(r, {c.seg}, (uint16_t)(0x{tbl:X}u + (uint16_t)r->d[0]))')
        raise ValueError('jump')
    if op == 'link':
        rg, d = ops; v = num(d[1:]) & 0xFFFF; v = v - 0x10000 if v & 0x8000 else v
        return [f'm_push(r, {REG[rg]}); {REG[rg]} = r->a[7]; r->a[7] += 0x{v & 0xFFFFFFFF:X}u;']
    if op == 'unlk': return [f'r->a[7] = {REG[ops[0]]}; {REG[ops[0]]} = m_pop(r);']
    if op == 'lea':
        src, dst = ops
        return [f'{REG[dst]} = {ea(src, 4).x};']
    if op == 'pea': return [f'm_push(r, {ea(ops[0], 4).x});']
    if op == 'moveq':
        v = num(ops[0][1:]) & 0xFF; v = v - 256 if v > 127 else v
        return [f'{REG[ops[1]]} = f_logic(r, 4, 0x{v & 0xFFFFFFFF:X}u);']
    if op in ('move', 'movea'):
        src, dst = ops; es, ed = ea(src, sz), ea(dst, sz)
        out = pre(es)
        v = read(es, sz)
        if es.post: out.append(f'uint32_t t{a:X} = {v}; {es.x} += {es.post};'); v = f't{a:X}'
        out += pre(ed)
        if ed.kind == 'reg' and ed.reg[0] == 'a': return out + [f'{ed.x} = {"sxw(" + v + ")" if sz == 2 else v};']
        return ['{'] + out + write(ed, sz, f'f_logic(r, {sz}, {v})') + ['}'] if es.post else out + write(ed, sz, f'f_logic(r, {sz}, {v})')
    if op == 'clr': e = ea(ops[0], sz); return pre(e) + write(e, sz, f'f_logic(r, {sz}, 0)')
    if op == 'tst': e = ea(ops[0], sz); return pre(e) + [f'f_logic(r, {sz}, {read(e, sz)});'] + ([f'{e.x} += {e.post};'] if e.post else [])
    if op in ('cmp', 'cmpi', 'cmpa'):
        src, dst = ops; es, ed = ea(src, sz), ea(dst, sz)
        if ed.post or es.pre or ed.pre: raise ValueError('cmp with -(An) / (An)+ destination')
        v = read(es, sz)
        if es.post: return [f'{{ uint32_t s = {v}; {es.x} += {es.post}; f_cmp(r, {sz}, s, {read(ed, sz)}); }}']
        if op == 'cmpa' and sz == 2: return [f'f_cmp(r, 4, sxw({v}), {read(ed, 4)});']
        return [f'f_cmp(r, {sz}, {v}, {read(ed, sz)});']
    if op in ('add', 'addq', 'addi', 'sub', 'subq', 'subi', 'adda', 'suba'):
        src, dst = ops; es, ed = ea(src, sz), ea(dst, sz)
        if es.post or es.pre or ed.post or ed.pre: raise ValueError('add with (An)+')
        plus = op.startswith('add')
        if ed.kind == 'reg' and ed.reg[0] == 'a':
            v = read(es, sz)
            if sz == 2: v = f'0x{(es.x - 0x10000 if es.x & 0x8000 else es.x) & 0xFFFFFFFF:X}u' if es.kind == 'imm' else f'sxw({v})'
            return [f'{ed.x} {"+" if plus else "-"}= {v};']
        f = 'f_add' if plus else 'f_sub'
        if ed.kind == 'mem': return [f'{{ uint32_t ad = {ed.x}; m_wr({sz}, ad, {f}(r, {sz}, {read(es, sz)}, m_rd({sz}, ad))); }}']
        return write(ed, sz, f'{f}(r, {sz}, {read(es, sz)}, {read(ed, sz)})')
    if op in ('and', 'andi', 'or', 'ori', 'eor', 'eori'):
        src, dst = ops; es, ed = ea(src, sz), ea(dst, sz)
        if es.post or es.pre or ed.post or ed.pre: raise ValueError('logic with (An)+')
        o = {'a': '&', 'o': '|', 'e': '^'}[op[0]]
        if ed.kind == 'mem': return [f'{{ uint32_t ad = {ed.x}; m_wr({sz}, ad, f_logic(r, {sz}, m_rd({sz}, ad) {o} {read(es, sz)})); }}']
        return write(ed, sz, f'f_logic(r, {sz}, {read(ed, sz)} {o} {read(es, sz)})')
    if op == 'neg':
        e = ea(ops[0], sz)
        if e.kind != 'reg': raise ValueError('neg mem')
        return write(e, sz, f'f_sub(r, {sz}, {read(e, sz)}, 0)')
    if op == 'ext':
        rg = REG[ops[0]]
        return [f'm_set(2, &{rg}, f_logic(r, 2, (uint32_t)(int32_t)(int8_t){rg}));'] if sz == 2 else [f'{rg} = f_logic(r, 4, sxw({rg}));']
    if op == 'swap': rg = REG[ops[0]]; return [f'{rg} = f_logic(r, 4, swap({rg}));']
    if op in ('lsl', 'lsr', 'asl', 'asr'):
        if len(ops) == 1: raise ValueError('memory shift')
        cnt, dst = ops; rg = REG[dst]
        n = f'{num(cnt[1:])}' if cnt.startswith('#') else f'(int)({REG[cnt]} & 63)'
        return [f'm_set({sz}, &{rg}, f_shift(r, {sz}, {rg}, {n}, {1 if op[2] == "l" else 0}, {1 if op[0] == "a" else 0}));']
    if op in ('btst', 'bset', 'bclr', 'bchg'):
        bit, dst = ops; ed = ea(dst, 1)
        w = 1 if ed.kind == 'mem' else 4
        b = f'({num(bit[1:])} & {8 * w - 1})' if bit.startswith('#') else f'({REG[bit]} & {8 * w - 1})'
        v = read(ed, w)
        out = [f'r->ccr = (uint8_t)((r->ccr & ~CCR_Z) | ((({v}) >> {b}) & 1 ? 0 : CCR_Z));']
        if op != 'btst':
            out += write(ed, w, {'bset': f'{v} | (1u << {b})', 'bclr': f'{v} & ~(1u << {b})', 'bchg': f'{v} ^ (1u << {b})'}[op])
        return out
    if op[0] == 's' and op[1:] in COND | {'t', 'f'} and s == 'b':
        e = ea(ops[0], 1); cond = {'t': '1', 'f': '0'}.get(op[1:], f'CC_{op[1:].upper()}')
        return pre(e) + write(e, 1, f'({cond}) ? 0xFFu : 0u')
    if op in ('muls', 'mulu'):
        src, dst = ops; rg = REG[dst]; v = read(ea(src, 2), 2)
        x = f'(uint32_t)((int32_t)(int16_t){v} * (int32_t)(int16_t){rg})' if op == 'muls' else f'(uint32_t)((uint16_t){v} * (uint32_t)(uint16_t){rg})'
        return [f'{rg} = f_logic(r, 4, {x});']
    if op == 'exg':
        x, y = REG[ops[0]], REG[ops[1]]; return [f'{{ uint32_t t = {x}; {x} = {y}; {y} = t; }}']
    if op == 'not':
        e = ea(ops[0], sz)
        if e.kind != 'reg': raise ValueError('not mem')
        return write(e, sz, f'f_logic(r, {sz}, ~{read(e, sz)})')
    raise ValueError(f'instruction {op}')

while True:
    while queue:
        k, name = queue.pop(0)
        if k in done: continue
        done[k] = name
        translate(k, name)
    more = [x for x in sorted(a3_targets | rel_targets) if x not in done]
    if not more: break
    for x in more: want(x)

is_entry = lambda k: any(k == e for e, _ in entries)
print(f'/* Generated by tools/asm2c.py {" ".join(sys.argv[1:])} from build/disasm-original: do not edit. */')
print('#include "asm68.h"\n#include "fns.h"\nvoid hx_fault(const char *fmt, ...);')
for k, name, _ in out_fns: print(f'{"" if is_entry(k) else "static "}void {name}(Regs *r);')
print('static void oc_call_a3(Regs *r);\nstatic void oc_call_rel(Regs *r, int seg, uint16_t at);')
for k, name, body in out_fns:
    print(f'\n/* S{k[0]}:{k[1]:05X} */\n{"" if is_entry(k) else "static "}void {name}(Regs *r) {{\n    uint32_t cs = mz_seg_base({k[0]}); (void)cs;')
    print('    MZ_FRAME;\n    uint32_t sp0 = r->a[7]; r->a[7] = mz_local(0x100) + 0x100;   /* its stack: frames and pushes */')
    print('\n'.join(body)); print('}')
print('\nstatic void oc_call_a3(Regs *r) {')
for (sg, x) in sorted(a3_targets): print(f'    if (r->a[3] == mz_seg_base({sg}) + 0x{x:X}u) {{ {fname((sg, x))}(r); return; }}')
print('    hx_fault("call through A3 to %06X", (unsigned)r->a[3]);\n}')
print('static void oc_call_rel(Regs *r, int seg, uint16_t at) {')
for (sg, x) in sorted(rel_targets): print(f'    if (seg == {sg} && at == 0x{x:X}) {{ {fname((sg, x))}(r); return; }}')
print('    hx_fault("call through a table to S%d:%05X", seg, (unsigned)at);\n}')
