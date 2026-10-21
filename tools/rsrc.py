"""Classic Mac resource fork parser. Accepts raw fork or AppleDouble (unar -k visible).
Usage: rsrc.py <file> [--list] [--dump OUTDIR]"""
import sys, os, struct

def load_fork(path):
    b = open(path, 'rb').read()
    if b[:4] == b'\x00\x05\x16\x07':  # AppleDouble
        n = struct.unpack('>H', b[24:26])[0]
        for i in range(n):
            eid, off, ln = struct.unpack('>III', b[26 + 12 * i:38 + 12 * i])
            if eid == 2:
                return b[off:off + ln]
        return b''
    return b

def parse(fork):
    doff, moff, dlen, mlen = struct.unpack('>IIII', fork[:16])
    m = fork[moff:moff + mlen]
    tl_off, nl_off = struct.unpack('>HH', m[24:28])
    tl = m[tl_off:]
    ntypes = struct.unpack('>H', tl[:2])[0] + 1
    res = []
    for t in range(ntypes):
        rtype, cnt, ref = struct.unpack('>4sHH', tl[2 + 8 * t:10 + 8 * t])
        for r in range(cnt + 1):
            e = tl[ref + 12 * r:ref + 12 * r + 12]
            rid, noff, attr_off = struct.unpack('>hHI', e[:8])
            attrs = attr_off >> 24; off = attr_off & 0xFFFFFF
            ln = struct.unpack('>I', fork[doff + off:doff + off + 4])[0]
            data = fork[doff + off + 4:doff + off + 4 + ln]
            name = ''
            if noff != 0xFFFF:
                nb = m[nl_off + noff:]
                name = nb[1:1 + nb[0]].decode('mac_roman')
            res.append((rtype.decode('mac_roman'), rid, name, attrs, data))
    return res

if __name__ == '__main__':
    res = parse(load_fork(sys.argv[1]))
    if '--dump' in sys.argv:
        out = sys.argv[sys.argv.index('--dump') + 1]
        for t, i, n, a, d in res:
            safe = ''.join(c if c.isalnum() else '_' for c in t)
            os.makedirs(os.path.join(out, safe), exist_ok=True)
            open(os.path.join(out, safe, f'{i}.bin'), 'wb').write(d)
    from collections import OrderedDict
    by = OrderedDict()
    for t, i, n, a, d in res:
        by.setdefault(t, []).append((i, n, len(d)))
    for t, lst in by.items():
        total = sum(x[2] for x in lst)
        print(f"{t!r:8} count={len(lst):4} bytes={total:7}  " +
              ', '.join(f"{i}{'('+n+')' if n else ''}:{s}" for i, n, s in lst[:12]) + (' ...' if len(lst) > 12 else ''))
