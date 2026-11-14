#!/usr/bin/env python3
"""Generate mactb/font_data.c: an original bitmap font ("Mzw Sans") standing in for Geneva.

The 9-point strike is drawn by hand below. Larger strikes are derived by fixed rules:
  12 pt: stretch straight strokes (duplicate rows 1 and 5; widen 5- and 4-wide glyphs by columns that continue horizontal strokes only)
  14 pt: the 12 pt glyphs with one extra pixel of spacing and taller line metrics
  18 pt: Scale2x of the 9 pt strike
Each glyph: 9 rows = 7 cap-height rows + 2 descender rows. '#' ink, '.' blank. Width = row length;
advance = width + 1. Metrics (ascent/descent/leading) are chosen to match Geneva's line spacing closely.
FIDELITY: the 9 pt strike's ASCII is Geneva 9's (see geneva9() below); the rest is original, with
line metrics aimed at Geneva's.
"""
import os

G = {}
def g(ch, *rows):
    rows = list(rows)
    while len(rows) < 9: rows.append('.' * len(rows[0]))
    w = len(rows[0]); assert all(len(r) == w for r in rows), ch
    G[ch] = rows

B5, B4, B3, B2, B1 = '.....', '....', '...', '..', '.'

# ---- capitals (rows 0-6) ----
g('A', '.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#')
g('B', '####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.')
g('C', '.###.', '#...#', '#....', '#....', '#....', '#...#', '.###.')
g('D', '####.', '#...#', '#...#', '#...#', '#...#', '#...#', '####.')
g('E', '#####', '#....', '#....', '####.', '#....', '#....', '#####')
g('F', '#####', '#....', '#....', '####.', '#....', '#....', '#....')
g('G', '.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.####')
g('H', '#...#', '#...#', '#...#', '#####', '#...#', '#...#', '#...#')
g('I', '###', '.#.', '.#.', '.#.', '.#.', '.#.', '###')
g('J', '....#', '....#', '....#', '....#', '....#', '#...#', '.###.')
g('K', '#...#', '#..#.', '#.#..', '##...', '#.#..', '#..#.', '#...#')
g('L', '#....', '#....', '#....', '#....', '#....', '#....', '#####')
g('M', '#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#')
g('N', '#...#', '##..#', '##..#', '#.#.#', '#..##', '#..##', '#...#')
g('O', '.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.')
g('P', '####.', '#...#', '#...#', '####.', '#....', '#....', '#....')
g('Q', '.###.', '#...#', '#...#', '#...#', '#.#.#', '#..#.', '.##.#')
g('R', '####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#')
g('S', '.###.', '#...#', '#....', '.###.', '....#', '#...#', '.###.')
g('T', '#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..')
g('U', '#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.')
g('V', '#...#', '#...#', '#...#', '#...#', '.#.#.', '.#.#.', '..#..')
g('W', '#...#', '#...#', '#...#', '#.#.#', '#.#.#', '##.##', '#...#')
g('X', '#...#', '#...#', '.#.#.', '..#..', '.#.#.', '#...#', '#...#')
g('Y', '#...#', '#...#', '.#.#.', '..#..', '..#..', '..#..', '..#..')
g('Z', '#####', '....#', '...#.', '..#..', '.#...', '#....', '#####')
# ---- digits ----
g('0', '.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.')
g('1', '..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.')
g('2', '.###.', '#...#', '....#', '...#.', '..#..', '.#...', '#####')
g('3', '#####', '...#.', '..#..', '...#.', '....#', '#...#', '.###.')
g('4', '...#.', '..##.', '.#.#.', '#..#.', '#####', '...#.', '...#.')
g('5', '#####', '#....', '####.', '....#', '....#', '#...#', '.###.')
g('6', '..##.', '.#...', '#....', '####.', '#...#', '#...#', '.###.')
g('7', '#####', '....#', '...#.', '..#..', '.#...', '.#...', '.#...')
g('8', '.###.', '#...#', '#...#', '.###.', '#...#', '#...#', '.###.')
g('9', '.###.', '#...#', '#...#', '.####', '....#', '...#.', '.##..')
# ---- lower case (x-height rows 2-6) ----
g('a', B4, B4, '.##.', '...#', '.###', '#..#', '.###')
g('b', '#...', '#...', '###.', '#..#', '#..#', '#..#', '###.')
g('c', B4, B4, '.###', '#...', '#...', '#...', '.###')
g('d', '...#', '...#', '.###', '#..#', '#..#', '#..#', '.###')
g('e', B4, B4, '.##.', '#..#', '####', '#...', '.###')
g('f', '.##', '#..', '###', '#..', '#..', '#..', '#..')
g('g', B4, B4, '.###', '#..#', '#..#', '#..#', '.###', '...#', '.##.')
g('h', '#...', '#...', '###.', '#..#', '#..#', '#..#', '#..#')
g('i', '#', '.', '#', '#', '#', '#', '#')
g('j', '..#', '...', '..#', '..#', '..#', '..#', '..#', '..#', '##.')
g('k', '#...', '#...', '#..#', '#.#.', '##..', '#.#.', '#..#')
g('l', '#', '#', '#', '#', '#', '#', '#')
g('m', B5, B5, '####.', '#.#.#', '#.#.#', '#.#.#', '#.#.#')
g('n', B4, B4, '###.', '#..#', '#..#', '#..#', '#..#')
g('o', B4, B4, '.##.', '#..#', '#..#', '#..#', '.##.')
g('p', B4, B4, '###.', '#..#', '#..#', '#..#', '###.', '#...', '#...')
g('q', B4, B4, '.###', '#..#', '#..#', '#..#', '.###', '...#', '...#')
g('r', B3, B3, '#.#', '##.', '#..', '#..', '#..')
g('s', B4, B4, '.###', '#...', '.##.', '...#', '###.')
g('t', B3, '.#.', '###', '.#.', '.#.', '.#.', '..#')
g('u', B4, B4, '#..#', '#..#', '#..#', '#..#', '.###')
g('v', B5, B5, '#...#', '#...#', '.#.#.', '.#.#.', '..#..')
g('w', B5, B5, '#...#', '#...#', '#.#.#', '#.#.#', '.#.#.')
g('x', B5, B5, '#...#', '.#.#.', '..#..', '.#.#.', '#...#')
g('y', B4, B4, '#..#', '#..#', '#..#', '#..#', '.###', '...#', '.##.')
g('z', B4, B4, '####', '...#', '.##.', '#...', '####')
# ---- punctuation ----
g(' ', B2, B2, B2, B2, B2, B2, B2)
g('!', '#', '#', '#', '#', '#', '.', '#')
g('"', '#.#', '#.#', B3, B3, B3, B3, B3)
g('#', B5, '.#.#.', '#####', '.#.#.', '#####', '.#.#.', B5)
g('$', '..#..', '.####', '#.#..', '.###.', '..#.#', '####.', '..#..')
g('%', '##..#', '##.#.', '...#.', '..#..', '.#...', '.#.##', '#..##')
g('&', '.##..', '#..#.', '#.#..', '.#...', '#.#.#', '#..#.', '.##.#')
g("'", '#', '#', B1, B1, B1, B1, B1)
g('(', '.#', '#.', '#.', '#.', '#.', '#.', '.#')
g(')', '#.', '.#', '.#', '.#', '.#', '.#', '#.')
g('*', B5, '..#..', '#.#.#', '.###.', '#.#.#', '..#..', B5)
g('+', B5, '..#..', '..#..', '#####', '..#..', '..#..', B5)
g(',', B2, B2, B2, B2, B2, '.#', '.#', '#.', B2)
g('-', B4, B4, B4, '####', B4, B4, B4)
g('.', B1, B1, B1, B1, B1, B1, '#')
g('/', '....#', '...#.', '...#.', '..#..', '.#...', '.#...', '#....')
g(':', B1, B1, '#', B1, B1, B1, '#')
g(';', B2, B2, '.#', B2, B2, '.#', '.#', '#.', B2)
g('<', '...#', '..#.', '.#..', '#...', '.#..', '..#.', '...#')
g('=', B4, B4, '####', B4, '####', B4, B4)
g('>', '#...', '.#..', '..#.', '...#', '..#.', '.#..', '#...')
g('?', '.###.', '#...#', '....#', '...#.', '..#..', B5, '..#..')
g('@', '.###.', '#...#', '#.###', '#.#.#', '#.###', '#....', '.####')
g('[', '##', '#.', '#.', '#.', '#.', '#.', '##')
g('\\', '#....', '.#...', '.#...', '..#..', '...#.', '...#.', '....#')
g(']', '##', '.#', '.#', '.#', '.#', '.#', '##')
g('^', '..#..', '.#.#.', '#...#', B5, B5, B5, B5)
g('_', B5, B5, B5, B5, B5, B5, B5, B5, '#####')
g('`', '#.', '.#', B2, B2, B2, B2, B2)
g('{', '..#', '.#.', '.#.', '#..', '.#.', '.#.', '..#')
g('|', '#', '#', '#', '#', '#', '#', '#', '#', '#')
g('}', '#..', '.#.', '.#.', '..#', '.#.', '.#.', '#..')
g('~', B4, B4, '.#.#', '#.#.', B4, B4, B4)
# Mac Roman extras that appear in the game's text
g('\xa9', '.###.', '#...#', '#.#.#', '##..#', '#.#.#', '#...#', '.###.')   # (c)
g('\xa5', B3, B3, '.#.', '###', '.#.', B3, B3)                               # bullet
g('\xd2', '#.#', '#.#', B3, B3, B3, B3, B3)                                  # curly quotes -> straight
g('\xd3', '#.#', '#.#', B3, B3, B3, B3, B3)
g('\xd4', '#', '#', B1, B1, B1, B1, B1)
g('\xd5', '#', '#', B1, B1, B1, B1, B1)
g('\xc9', B5, B5, B5, B5, B5, B5, '#.#.#')                                   # ellipsis
g('\xd0', B4, B4, B4, '####', B4, B4, B4)                                    # en dash
g('\xd1', B5 + '.', B5 + '.', B5 + '.', '######', B5 + '.', B5 + '.', B5 + '.')  # em dash

MACROMAN = {'\xa9': 0xA9, '\xa5': 0xA5, '\xd2': 0xD2, '\xd3': 0xD3, '\xd4': 0xD4, '\xd5': 0xD5, '\xc9': 0xC9, '\xd0': 0xD0, '\xd1': 0xD1}

def code_of(ch): return MACROMAN.get(ch, ord(ch))

def stretch12(rows):
    w = len(rows[0])
    # inserted columns only continue horizontal strokes (ink on both sides), so diagonals stay thin
    J = lambda a, b: '#' if a == '#' and b == '#' else '.'
    def cols(r):
        if w == 5: return r[0] + r[1] + J(r[1], r[2]) + r[2] + J(r[2], r[3]) + r[3] + r[4]
        if w == 4: return r[0] + r[1] + J(r[1], r[2]) + r[2] + r[3]
        return r
    out = []
    for i, r in enumerate(rows[:7]):
        out.append(cols(r))
        if i in (1, 5): out.append(cols(r))
    for r in rows[7:]: out.append(cols(r))
    out.append(cols(rows[8]))       # third descender row
    return out                      # 9 cap rows + 3 descender rows

def scale2x(rows):
    h, w = len(rows), len(rows[0])
    P = lambda y, x: rows[y][x] if 0 <= y < h and 0 <= x < w else '.'
    out = [['.'] * (2 * w) for _ in range(2 * h)]
    for y in range(h):
        for x in range(w):
            E = P(y, x); B = P(y - 1, x); D = P(y, x - 1); F = P(y, x + 1); H = P(y + 1, x)
            if B != H and D != F:
                e0 = D if D == B else E; e1 = F if B == F else E; e2 = D if D == H else E; e3 = F if H == F else E
            else:
                e0 = e1 = e2 = e3 = E
            out[2 * y][2 * x], out[2 * y][2 * x + 1], out[2 * y + 1][2 * x], out[2 * y + 1][2 * x + 1] = e0, e1, e2, e3
    return [''.join(r) for r in out]

# strike: size -> (ascent, descent, leading, extra spacing, glyph function, top padding rows)
STRIKES = {
    9:  (10, 2, 0, 1, lambda r: r, 3),
    12: (12, 3, 0, 1, stretch12, 3),
    14: (14, 3, 1, 2, stretch12, 5),
    18: (18, 4, 0, 2, scale2x, 4),
}

# The 9-point strike's ASCII glyphs come from Kelsey Higham's "Geneva 9" (FontStruct, CC BY 3.0,
# third_party/geneva9/geneva_9.ttf), a pixel-for-pixel copy of the Mac's Geneva 9: 8 rows above the
# baseline, 2 below, each glyph its advance less one wide. The space keeps its own width here.
GENEVA9 = os.path.join(os.path.dirname(__file__), '..', 'third_party', 'geneva9', 'geneva_9.ttf')
def geneva9():
    from PIL import Image, ImageDraw, ImageFont
    f = ImageFont.truetype(GENEVA9, 16)                     # 64 font units a pixel
    out = {}
    for c in range(33, 127):
        ch = chr(c); adv = int(f.getlength(ch))
        if adv < 2: continue
        im = Image.new('1', (adv + 4, 16), 0); d = ImageDraw.Draw(im); d.fontmode = '1'
        d.text((0, 0), ch, font=f, fill=1)                  # baseline at row 8
        px = im.load()
        out[ch] = [''.join('#' if px[x, y] else '.' for x in range(adv - 1)) for y in range(0, 10)]
    return out

def main():
    out = ['/* Generated by tools/fontgen.py; do not edit by hand. The 9 pt ASCII glyphs are from "Geneva 9" by\n * Kelsey Higham (FontStruct), CC BY 3.0, http://creativecommons.org/licenses/by/3.0/; the rest are original. */',
           '#include "font_data.h"', '']
    for size, (asc, desc, lead, sp, fn, pad) in STRIKES.items():
        bits_rows = []; glyphs = {}
        real = geneva9() if size == 9 else {}
        for ch, rows in G.items():
            if ch in real:                                  # the real shapes, already 10 rows
                w = len(real[ch][0])
                glyphs[code_of(ch)] = (w, ['.' * w] * (asc - 8) + real[ch][:8 + desc]); continue
            gr = fn(rows)
            full = ['.' * len(gr[0])] * pad + gr
            full = full[:asc + desc] + ['.' * len(gr[0])] * max(0, asc + desc - len(full))
            glyphs[code_of(ch)] = (len(gr[0]), full)
        out.append(f'static const uint32_t rows{size}[] = {{')
        offs = {}
        k = 0
        for c in sorted(glyphs):
            w, full = glyphs[c]
            offs[c] = k
            vals = [sum(1 << (31 - x) for x, p in enumerate(r) if p == '#') for r in full]
            out.append('  ' + ', '.join(f'0x{v:08X}u' for v in vals) + f',  /* {c:#04x} */')
            k += len(full)
        out.append('};')
        out.append(f'static const MtbGlyph glyphs{size}[256] = {{')
        for c in sorted(glyphs):
            w, _ = glyphs[c]
            out.append(f'  [{c}] = {{{w}, {w + sp}, {offs[c]}}},')
        out.append('};')
        out.append(f'const MtbFont mtb_font{size} = {{{size}, {asc}, {desc}, {lead}, glyphs{size}, rows{size}}};')
        out.append('')
    path = os.path.join(os.path.dirname(__file__), '..', 'mactb', 'font_data.c')
    open(path, 'w').write('\n'.join(out) + '\n')
    print('wrote', path, 'glyphs:', len(G), 'sizes:', list(STRIKES))

if __name__ == '__main__':
    main()
