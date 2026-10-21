"""Minimal BinHex 4.0 decoder: writes <name>.data, <name>.rsrc, <name>.info.txt"""
import sys, os, struct

ALPHA = b"!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr"

def crc16(data, crc=0):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc

def main(src, outdir):
    txt = open(src, 'rb').read()
    start = txt.index(b'(This file must be converted with BinHex')
    start = txt.index(b':', start) + 1
    end = txt.index(b':', start)
    body = bytes(c for c in txt[start:end] if c not in b'\r\n\t ')
    lut = {c: i for i, c in enumerate(ALPHA)}
    bits = acc = 0
    out = bytearray()
    for c in body:
        acc = (acc << 6) | lut[c]; bits += 6
        if bits >= 8:
            bits -= 8; out.append((acc >> bits) & 0xFF)
    # RLE (0x90)
    dec = bytearray(); i = 0; last = 0
    while i < len(out):
        b = out[i]
        if b == 0x90:
            n = out[i + 1]; i += 2
            if n == 0: dec.append(0x90); last = 0x90
            else: dec.extend([last] * (n - 1))
            continue
        dec.append(b); last = b; i += 1
    p = 0
    nl = dec[0]; name = dec[1:1 + nl].decode('mac_roman'); p = 1 + nl + 1
    ftype, creator = dec[p:p + 4], dec[p + 4:p + 8]; p += 8
    flags, dlen, rlen = struct.unpack('>HII', dec[p:p + 10]); p += 10
    hcrc = struct.unpack('>H', dec[p:p + 2])[0]
    ok_h = crc16(dec[:p]) == hcrc; p += 2
    data = dec[p:p + dlen]; p += dlen
    dcrc = struct.unpack('>H', dec[p:p + 2])[0]; p += 2
    rsrc = dec[p:p + rlen]; p += rlen
    rcrc = struct.unpack('>H', dec[p:p + 2])[0]
    ok_d = crc16(bytes(data)) == dcrc
    ok_r = crc16(bytes(rsrc)) == rcrc
    safe = ''.join(ch if ch.isalnum() or ch in '._-' else '_' for ch in name)
    os.makedirs(outdir, exist_ok=True)
    open(os.path.join(outdir, safe + '.data'), 'wb').write(data)
    open(os.path.join(outdir, safe + '.rsrc'), 'wb').write(rsrc)
    info = (f"name={name!r} type={ftype.decode('mac_roman')} creator={creator.decode('mac_roman')} "
            f"flags={flags:#06x} data={dlen} rsrc={rlen} crc_ok header={ok_h} data={ok_d} rsrc={ok_r}")
    open(os.path.join(outdir, safe + '.info.txt'), 'w').write(info + '\n')
    print(info)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
