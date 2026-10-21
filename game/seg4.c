/* CODE segment 4: functions reimplemented so far. Each matches the original instruction for
 * instruction in effect (see build/disasm/seg4.s); verified by substitution in the harness. */
#include "mz.h"

/* fn_S4_36F2: 1 if Random() is non-negative, else 0. */
int16_t fn_S4_36F2(void) {
    uint16_t r = (uint16_t)mac_Random();
    return r < 0x8000 ? 1 : 0;
}

/* fn_S4_3704: shuffles a list { uint16 count; uint32 item[count] } with count/2 random swaps.
 * Indices are Random() taken as unsigned, modulo count (divu.w remainder). */
void fn_S4_3704(uint32_t list) {
    uint16_t count = r16(list);
    uint16_t swaps = count >> 1;
    if (count < 2) return;
    for (uint16_t i = 0; i < swaps; i++) {
        uint16_t a = (uint16_t)((uint16_t)mac_Random() % count);
        uint16_t b = (uint16_t)((uint16_t)mac_Random() % count);
        uint32_t tmp = r32(list + 2 + 4u * a);
        w32(list + 2 + 4u * a, r32(list + 2 + 4u * b));
        w32(list + 2 + 4u * b, tmp);
    }
}

/* fn_S4_3794: random ordering of the values 0..3 (eight random pair swaps), packed two bits each:
 * result = d[3] << 6 | d[2] << 4 | d[1] << 2 | d[0]. */
int16_t fn_S4_3794(void) {
    int16_t d[4];
    for (int16_t i = 0; i < 4; i++) d[i] = i;
    for (int16_t i = 0; i < 8; i++) {
        int16_t a = (int16_t)(mac_Random() & 3);
        int16_t b = (int16_t)(mac_Random() & 3);
        int16_t t = d[a]; d[a] = d[b]; d[b] = t;
    }
    return (int16_t)((d[3] << 6) + (d[2] << 4) + (d[1] << 2) + d[0]);
}

/* fn_S4_332C: with probability 1/3 (Random < 0x5555 unsigned), turns *dir one step left or right
 * at random and wraps it to 0..3. */
void fn_S4_332C(uint32_t dir) {
    if ((uint16_t)mac_Random() >= 0x5555) return;
    if ((uint16_t)mac_Random() < 0x8000) w16(dir, (uint16_t)(r16(dir) + 1));
    else w16(dir, (uint16_t)(r16(dir) - 1));
    w16(dir, (uint16_t)(r16(dir) & 3));
}

/* fn_S4_382C: entry `index` (0..3) of a direction list packed by fn_S4_3794.
 * For an index outside 0..3 the original returns an uninitialised local; that case is never reached. */
int16_t fn_S4_382C(int16_t packed, int16_t index) {
    switch (index) {
    case 0: return (int16_t)(packed & 3);
    case 1: return (int16_t)((packed >> 2) & 3);
    case 2: return (int16_t)((packed >> 4) & 3);
    case 3: return (int16_t)((packed >> 6) & 3);
    }
    return 0;   /* ORIGINAL: uninitialised */
}

/* fn_S4_3896: sets the number of sound channels (G_2A8A) to n if 0..4, else 0. */
void fn_S4_3896(int16_t n) {
    sg16(0x2A8A, (uint16_t)(n < 0 || n > 4 ? 0 : n));
}

/* fn_S4_38B8: clears G_2A86 and the 128-entry long array at G_2A7E. */
void fn_S4_38B8(void) {
    sg32(0x2A86, 0);
    for (int16_t i = 0; i < 128; i++) w32(GADDR(0x2A7E) + 4u * (uint32_t)i, 0);
}

/* ---------- register-interface routines (game/mzasm.h) ---------- */
#include "mzasm.h"
#include "fns.h"
int16_t fn_S2_3532_value(void);

/* fn_S4_141A (JT 18): G_3A46 = D0.w. */
void fn_S4_141A(Regs *r) { sg16(0x3A46, (uint16_t)r->d[0]); cc_nz_w(r, (uint16_t)r->d[0]); }

/* fn_S4_44DE (JT 73): D0.l = -1 if G_248A is set, else 0. */
void fn_S4_44DE(Regs *r) { r->d[0] = g8(0x248A) ? 0xFFFFFFFFu : 0; cc_nz_l(r, r->d[0]); }

/* fn_S4_3886: D0 = 0 (long) if G_2A86 is 0, else D0.w = G_2A88. */
void fn_S4_3886(Regs *r) {
    if (g32(0x2A86) == 0) { r->d[0] = 0; cc_nz_l(r, 0); }
    else { set_w(&r->d[0], g16(0x2A88)); cc_nz_w(r, (uint16_t)r->d[0]); }
}

/* fn_S4_434E (JT 71): G_248A = 0, G_2488 = 0, G_2489 = 1. */
void fn_S4_434E(Regs *r) { sg8(0x248A, 0); sg16(0x2488, 0); sg8(0x2489, 1); cc_nz_b(r, 1); }

/* fn_S4_1E76: G_36FC = G_1DC4 + $40 (the second $40-byte record), G_36F8 = 1, G_225E = 0. D0 = that address. */
void fn_S4_1E76(Regs *r) {
    uint32_t base = g32(0x1DC4), v = base + 0x40;
    int carry = v < base;
    r->d[0] = v;
    sg32(0x36FC, v); sg16(0x36F8, 1); sg16(0x225E, 0);
    r->ccr = (uint8_t)((carry ? CCR_X : 0) | CCR_Z);   /* X from the add, the rest from clr.w */
}

/* ---------- compiled C ---------- */

/* fn_S4_1E44: resets the 120 $40-byte records at G_1DC4: word +0 = 0, word +2 = -1. */
void fn_S4_1E44(void) {
    uint32_t p = g32(0x1DC4);
    for (int16_t i = 0; i < 0x78; i++, p += 0x40) { w16(p, 0); w16(p + 2, 0xFFFF); }
}

void fn_S2_3480(Regs *r);
void fn_S2_33A4(Regs *r);

/* fn_S4_1EAE: (re)places object n (a $40-byte record in G_1DC4, active when its word +2 is not
 * negative): refreshes its derived fields (fn_S2_3480); if its word +$E is set (1 means "use +$30"),
 * marks that cell's word +$E in G_1DCC; then adds the object to the list of its cell +$30. */
void fn_S4_1EAE(int16_t n) {
    uint16_t off = (uint16_t)((uint16_t)n << 6);
    uint32_t cells = g32(0x1DCC), rec = g32(0x1DC4) + (uint32_t)(int32_t)(int16_t)off;
    if (r16(rec + 2) & 0x8000) return;
    Regs r = {{0}, {0}, 0};
    r.d[0] = off; mz_seg(2), fn_S2_3480(&r);
    uint16_t m = r16(rec + 0xE);
    if (m) {
        if (m == 1) w16(rec + 0xE, r16(rec + 0x30));
        w16(cells + (uint32_t)(int32_t)(int16_t)r16(rec + 0xE) + 0xE, 1);
    }
    uint16_t cell = r16(rec + 0x30);
    if (cell) { r.d[0] = off; r.d[1] = cell; mz_seg(2), fn_S2_33A4(&r); }
}

/* fn_S4_3B9A (JT 66): unlocks the handle G_2A86 (if any) and the 128 handles of G_2A7E[], clearing
 * G_2A82 and the 128 longs of G_287E[] (their dereferenced pointers). */
void fn_S4_3B9A(void) {
    if (g32(0x2A86)) mac_HUnlock(g32(0x2A86));
    sg32(0x2A82, 0);
    for (int16_t i = 0; i < 0x80; i++) {
        uint32_t h = r32(GADDR(0x2A7E) + 4u * (uint32_t)i);
        if (h) mac_HUnlock(h);
        w32(GADDR(0x287E) + 4u * (uint32_t)i, 0);
    }
}

/* fn_S4_2DF6: fills the 32x32 byte map in handle h with 1 (locked while writing). */
void fn_S4_2DF6(uint32_t h) {
    mac_HLock(h);
    uint32_t p = deref(h);
    for (int16_t x = 0; x < 0x20; x++)
        for (int16_t y = 0; y < 0x20; y++) w8(p + (uint32_t)(int32_t)y * 32u + (uint32_t)(int32_t)x, 1);
    mac_HUnlock(h);
}

/* fn_S4_2E6C: clears cell (x, y) of the 32x32 byte map in handle h and appends it to list
 * { uint16 count; { byte pad[2]; byte x, y; uint16 0 } [count] } (entries 4 bytes apart, as written). */
void fn_S4_2E6C(uint32_t h, uint32_t list, int16_t x, int16_t y) {
    mac_HLock(h);
    uint32_t p = deref(h);
    int16_t n = (int16_t)r16(list);
    uint32_t e = list + (uint32_t)((int32_t)n << 2);
    w8(e + 2, (uint8_t)x); w8(e + 3, (uint8_t)y); w16(e + 4, 0);
    w16(list, (uint16_t)(n + 1));
    w8(p + (uint32_t)((int32_t)y << 5) + (uint32_t)(int32_t)x, 0);
    mac_HUnlock(h);
}

/* fn_S4_2E4C: empties list and adds cell (x, y) to it (fn_S4_2E6C). */
void fn_S4_2E4C(uint32_t h, uint32_t list, int16_t x, int16_t y) {
    w16(list, 0);
    fn_S4_2E6C(h, list, x, y);
}

/* fn_S4_1C88: builds the 32x32 cell table G_1DCC ($20 bytes per cell) from the byte map in handle
 * h (argument on the stack): +0 = map byte, +$E = 1 if its bit 0 is set, +$1C/+$1E = the cell centre
 * (8 + 16 * column, 8 + 16 * row), everything else 0. Uses D4/D5 without saving them, so it is
 * written at register level: D4.w = 8 and D5.w = $200 afterwards, A1 past the table, D1.l = 0. */
static void build_cells(Regs *r, uint32_t h) {
    r->a[0] = h; mac_trap_regs(r, 0xA029);
    r->a[0] = deref(h);
    r->a[1] = g32(0x1DCC);
    set_w(&r->d[4], 8); set_w(&r->d[5], 8);
    for (;;) {
        set_b(&r->d[0], r8(r->a[0])); r->a[0]++;
        w8(r->a[1], (uint8_t)r->d[0]);
        r->d[1] = 0;
        w8(r->a[1] + 1, 0); w16(r->a[1] + 2, 0);
        for (uint32_t o = 4; o < 0x1C; o += 4) w32(r->a[1] + o, 0);
        if (r->d[0] & 1) w16(r->a[1] + 0xE, 1);
        w16(r->a[1] + 0x1C, (uint16_t)r->d[4]); w16(r->a[1] + 0x1E, (uint16_t)r->d[5]);
        r->a[1] += 0x20;
        set_w(&r->d[4], (uint16_t)((uint16_t)r->d[4] + 0x10));
        if ((int16_t)r->d[4] < 0x200) continue;
        set_w(&r->d[4], 8);
        set_w(&r->d[5], (uint16_t)((uint16_t)r->d[5] + 0x10));
        if ((int16_t)r->d[5] >= 0x200) break;
    }
    r->a[0] = h; mac_trap_regs(r, 0xA02A);
}
void fn_S4_1C88(Regs *r) { build_cells(r, r32(r->a[7] + 4)); }

/* fn_S4_3B10 (JT 65): locks the handle G_2A86 (pointer in G_2A82, or 0) and the 128 handles of
 * G_2A7E[] (pointers in G_287E[], 0 for none). The reverse of fn_S4_3B9A. */
void fn_S4_3B10(void) {
    if (g32(0x2A86)) { mac_HLock(g32(0x2A86)); sg32(0x2A82, deref(g32(0x2A86))); }
    else sg32(0x2A82, 0);
    for (int16_t i = 0; i < 0x80; i++) {
        uint32_t h = r32(GADDR(0x2A7E) + 4u * (uint32_t)i);
        if (h) { mac_HLock(h); w32(GADDR(0x287E) + 4u * (uint32_t)i, deref(h)); }
        else w32(GADDR(0x287E) + 4u * (uint32_t)i, 0);
    }
}

/* fn_S4_2CC8: adds edge bits to the 32x32 maze byte map in handle h (bit 0 = solid). A solid cell
 * gets $40 if the cell to its right (x+1) is open and $80 if the cell below (y+1) is open; an open
 * cell gets $50 / $A0 if that neighbour is solid. Other bits are cleared. Edges of the map count as
 * nothing. */
void fn_S4_2CC8(uint32_t h) {
    mac_HLock(h);
    uint32_t p = deref(h);
    for (int16_t y = 0; y < 0x20; y++)
        for (int16_t x = 0; x < 0x20; x++) {
            uint32_t c = p + (uint32_t)((int32_t)y << 5) + (uint32_t)(int32_t)x;
            uint8_t b = r8(c) & 1;
            int right = x + 1 < 0x20, below = y + 1 < 0x20;
            if (b) {
                if (right && !(r8(c + 1) & 1)) b |= 0x40;
                if (below && !(r8(c + 0x20) & 1)) b |= 0x80;
            } else {
                if (right && (r8(c + 1) & 1)) b |= 0x50;
                if (below && (r8(c + 0x20) & 1)) b |= 0xA0;
            }
            w8(c, b);
        }
    mac_HUnlock(h);
}

/* fn_S4_1D0A: lists the open cells of the 32x32 table G_1DCC (bit 0 of byte +0 clear) in
 * G_36F2[] as { word cell offset; word number of open neighbours } (count in G_36F6, G_36F4 = 0),
 * then shuffles the list with 2 * count random swaps (indices: unsigned Random() mod count). */
void fn_S4_1D0A(void) {
    sg16(0x36F6, 0); sg16(0x36F4, 0);
    uint32_t p = g32(0x1DCC);
    for (uint16_t off = 0; off < 0x8000; off = (uint16_t)(off + 0x20), p += 0x20) {
        if (r8(p) & 1) continue;
        int16_t open = 0;
        if (!(r8(p + 0x20) & 1)) open++;
        if (!(r8(p - 0x20) & 1)) open++;
        if (!(r8(p + 0x400) & 1)) open++;
        if (!(r8(p - 0x400) & 1)) open++;
        uint32_t e = MZ_A5 + (uint32_t)((int32_t)(int16_t)g16(0x36F6) << 2) - 0x36F2;
        w16(e, off); w16(e + 2, (uint16_t)open);
        sg16(0x36F6, (uint16_t)(g16(0x36F6) + 1));
    }
    int16_t n = (int16_t)g16(0x36F6), swaps = (int16_t)(n + n);
    if (n < 2) return;
    for (int16_t k = 0; k < swaps; k++) {
        uint16_t a = (uint16_t)((uint16_t)mac_Random() % (uint16_t)n);
        uint16_t b = (uint16_t)((uint16_t)mac_Random() % (uint16_t)n);
        uint32_t ea = GADDR(0x36F2) + 4u * a, eb = GADDR(0x36F2) + 4u * b;
        uint32_t t = r32(ea);
        w32(ea, r32(eb)); w32(eb, t);
    }
}

/* fn_S4_0112: marks what can be seen from the point (x, y) (16.16 fixed point, 16 units per cell) on the
 * 32-wide cell byte map at base: the cell itself and up to three cells straight ahead in each of the
 * four directions get bit 2 (stopping after a solid cell, bit 0), and the cells beside each of those
 * (and the four diagonals) get bit 1. */
void fn_S4_0112(int32_t x, int32_t y, uint32_t base) {
    uint16_t ix = (uint16_t)((uint32_t)x >> 16), iy = (uint16_t)((uint32_t)y >> 16);   /* integer parts */
    uint16_t off = (uint16_t)((((iy & 0xFFF0) << 1) | (ix >> 4)) << 5);
    uint32_t c = base + off;
#define OR(o, v) w8(c + (uint32_t)(int32_t)(o), (uint8_t)(r8(c + (uint32_t)(int32_t)(o)) | (v)))
    OR(0, 4); OR(-0x420, 2); OR(-0x3E0, 2); OR(0x3E0, 2); OR(0x420, 2);
    static const int32_t dir[4] = {0x400, -0x400, 0x20, -0x20}, side[4] = {0x20, 0x20, 0x400, 0x400};
    for (int k = 0; k < 4; k++)
        for (int step = 1; step <= 3; step++) {
            int32_t o = dir[k] * step;
            OR(o, 4); OR(o + side[k], 2); OR(o - side[k], 2);
            if (step < 3 && (r8(c + (uint32_t)o) & 1)) break;
        }
#undef OR
}

/* fn_S4_12D6: plots pixel (x, y) in colour c into the offscreen buffer G_3A5C (row bytes G_3A62,
 * depth G_3A48, pixel values from the long table G_3A58[c]). Depth 1 clears the bit (sets white)
 * only for c >= 3; other depths store the table value at 8, 4 (nibble), 16 or 32 bits. */
void fn_S4_12D6(int16_t x, int16_t y, int16_t c) {
    uint16_t depth = g16(0x3A48);
    uint32_t row = g32(0x3A5C) + (uint32_t)(uint16_t)y * (uint32_t)g16(0x3A62);
    uint32_t v = r32(g32(0x3A58) + (uint32_t)(int32_t)(int16_t)(uint16_t)((uint16_t)c << 2));
    uint16_t ux = (uint16_t)x;
    switch (depth) {
    case 8:    w8(row + ux, (uint8_t)v); break;
    case 0x10: w16(row + (uint16_t)(ux << 1), (uint16_t)v); break;
    case 0x20: w32(row + (uint16_t)(ux << 2), v); break;
    case 4: {
        uint32_t a = row + (uint16_t)(ux >> 1);
        if (ux & 1) w8(a, (uint8_t)((r8(a) & 0xF0) | (v & 0x0F)));
        else        w8(a, (uint8_t)((r8(a) & 0x0F) | (v & 0xF0)));
        break; }
    case 1:
        if ((uint16_t)c >= 3) { uint32_t a = row + (uint16_t)(ux >> 3); w8(a, (uint8_t)(r8(a) & (uint8_t)(0xFF7Fu >> (ux & 7)))); }
        break;
    }
}

/* fn_S4_3D64: requests sound effect `id` at priority `prio` with value `value`. Sound table G_2A82
 * (word +8 + 2*id: low byte = sound number + $80, high byte = repeat count, at most 10); channels
 * are 8-byte entries at G_265E[G_2A8A] { word sound; word priority; word repeats; word value }.
 * Picks the channel already playing this priority (its priority then counts as -1) or else the one
 * with the lowest priority, and takes it if that is below prio. Re-requesting a repeating sound on
 * its channel only raises its value. (The original keeps the repeat count in its third parameter's
 * stack slot, which the caller's argument value never reaches.) */
void fn_S4_3D64(int16_t id, int16_t prio, int16_t unused, int16_t value) {
    (void)unused;
    if (!g32(0x2A86) || !g16(0x2A8A) || id >= (int16_t)g16(0x2A88)) return;
    uint16_t w = r16(g32(0x2A82) + (uint32_t)((int32_t)id * 2) + 8);
    int16_t snd = (int16_t)(uint16_t)((w & 0xFF) + 0xFF80);
    int16_t rep = (int16_t)((w & 0xFF00) >> 8);
    if (rep > 10) rep = 10;
    if (snd < 0 || snd >= 0x80) return;
    if (!r32(GADDR(0x287E) + 4u * (uint32_t)snd)) return;
    int16_t best = -1, bestprio = 0x7FBC;
    for (int16_t ch = 0; ch < (int16_t)g16(0x2A8A); ch++) {
        int16_t p = (int16_t)r16(GADDR(0x265E) + 8u * (uint32_t)ch + 2);
        if (p == prio) { best = ch; bestprio = -1; }
        else if (p < bestprio) { best = ch; bestprio = p; }
    }
    if (best == -1 || bestprio >= prio) return;
    uint32_t e = GADDR(0x265E) + 8u * (uint32_t)best;
    if (bestprio == -1 && r16(e + 4) && rep) { if (value > (int16_t)r16(e + 6)) w16(e + 6, (uint16_t)value); }
    else w16(e + 6, (uint16_t)value);
    w16(e + 2, (uint16_t)prio); w16(e, (uint16_t)snd); w16(e + 4, (uint16_t)rep);
}

/* Sound effect requests: each calls fn_S4_3D64(sound, priority, -, value) with fixed numbers;
 * the value is either a constant or G_3A46 (set by fn_S4_141A). The third argument is never used. */
void fn_S4_1420(void) { fn_S4_3D64(0x00, 0x05, 1, 0x7F); }   /* JT 19 */
void fn_S4_1550(void) { fn_S4_3D64(0x0E, 0x32, 0, 0xFF); }   /* JT 33 */
void fn_S4_167E(void) { fn_S4_3D64(0x1D, 0x0A, 0, 0xFF); }
void fn_S4_16B4(void) { fn_S4_3D64(0x20, 0x0A, 0, 0xFF); }
void fn_S4_16FC(void) { fn_S4_3D64(0x24, 0x28, 0, 0xFF); }
void fn_S4_170E(void) { fn_S4_3D64(0x25, 0x08, 3, 0xFF); }   /* JT 51 */
void fn_S4_1722(void) { fn_S4_3D64(0x26, 0x1E, 0, 0xFF); }
void fn_S4_1772(void) { fn_S4_3D64(0x2A, 0x3C, 0, 0xFF); }
void fn_S4_1784(void) { fn_S4_3D64(0x2B, 0x0A, 0, 0xFF); }
void fn_S4_17A8(void) { fn_S4_3D64(0x2D, 0x3E, 0, 0xFF); }
void fn_S4_17BA(void) { fn_S4_3D64(0x2E, 0x0A, 0, 0xFF); }
void fn_S4_17CC(void) { fn_S4_3D64(0x2F, 0x0A, 0, 0xFF); }
void fn_S4_17F0(void) { fn_S4_3D64(0x31, 0x0A, 0, 0xFF); }
void fn_S4_1432(void) { fn_S4_3D64(0x01, 0x14, 0, (int16_t)g16(0x3A46)); }   /* JT 20 */
void fn_S4_14CC(void) { fn_S4_3D64(0x08, 0x1E, 0, (int16_t)g16(0x3A46)); }   /* JT 27 */
void fn_S4_15CE(void) { fn_S4_3D64(0x15, 0x0B, 0, (int16_t)g16(0x3A46)); }   /* JT 39 */
void fn_S4_1746(void) { fn_S4_3D64(0x28, 0x0C, 0, (int16_t)g16(0x3A46)); }   /* JT 54 */
void fn_S4_175C(void) { fn_S4_3D64(0x29, 0x0D, 0, (int16_t)g16(0x3A46)); }   /* JT 55 */
/* The other sound requests, same pattern (generated from the disassembly; not all are reached by
 * the recorded test games, so some are checked only when later coverage reaches them). */
void fn_S4_1448(void) { fn_S4_3D64(0x02, 0x14, 0, (int16_t)g16(0x3A46)); }
void fn_S4_145E(void) { fn_S4_3D64(0x03, 0x14, 0, (int16_t)g16(0x3A46)); }
void fn_S4_1474(void) { fn_S4_3D64(0x04, 0x14, 0, (int16_t)g16(0x3A46)); }
void fn_S4_148A(void) { fn_S4_3D64(0x05, 0x14, 0, (int16_t)g16(0x3A46)); }
void fn_S4_14A0(void) { fn_S4_3D64(0x06, 0x14, 0, (int16_t)g16(0x3A46)); }
void fn_S4_14B6(void) { fn_S4_3D64(0x07, 0x14, 0, (int16_t)g16(0x3A46)); }
void fn_S4_14E2(void) { fn_S4_3D64(0x09, 0x1E, 0, (int16_t)g16(0x3A46)); }
void fn_S4_14F8(void) { fn_S4_3D64(0x0A, 0x1E, 0, (int16_t)g16(0x3A46)); }
void fn_S4_150E(void) { fn_S4_3D64(0x0B, 0x1E, 0, (int16_t)g16(0x3A46)); }
void fn_S4_1524(void) { fn_S4_3D64(0x0C, 0x1E, 0, (int16_t)g16(0x3A46)); }
void fn_S4_153A(void) { fn_S4_3D64(0x0D, 0x1E, 0, (int16_t)g16(0x3A46)); }
void fn_S4_1562(void) { fn_S4_3D64(0x0F, 0x32, 0, 0xFF); }
void fn_S4_1574(void) { fn_S4_3D64(0x10, 0x32, 0, 0xFF); }
void fn_S4_1586(void) { fn_S4_3D64(0x11, 0x32, 0, 0xFF); }
void fn_S4_15AA(void) { fn_S4_3D64(0x13, 0x32, 0, 0xFF); }
void fn_S4_15BC(void) { fn_S4_3D64(0x14, 0x32, 0, 0xFF); }
void fn_S4_15E4(void) { fn_S4_3D64(0x16, 0x0B, 0, (int16_t)g16(0x3A46)); }
void fn_S4_15FA(void) { fn_S4_3D64(0x17, 0x0B, 0, (int16_t)g16(0x3A46)); }
void fn_S4_1610(void) { fn_S4_3D64(0x18, 0x0B, 0, (int16_t)g16(0x3A46)); }
void fn_S4_1626(void) { fn_S4_3D64(0x19, 0x0B, 0, (int16_t)g16(0x3A46)); }
void fn_S4_163C(void) { fn_S4_3D64(0x1A, 0x0B, 0, (int16_t)g16(0x3A46)); }
void fn_S4_1652(void) { fn_S4_3D64(0x1B, 0x0B, 0, (int16_t)g16(0x3A46)); }
void fn_S4_1668(void) { fn_S4_3D64(0x1C, 0x14, 0, (int16_t)g16(0x3A46)); }
void fn_S4_1690(void) { fn_S4_3D64(0x1E, 0x0A, 0, 0xFF); }
void fn_S4_16A2(void) { fn_S4_3D64(0x1F, 0x0A, 0, 0xFF); }
void fn_S4_16C6(void) { fn_S4_3D64(0x21, 0x20, 0, 0xFF); }
void fn_S4_16D8(void) { fn_S4_3D64(0x22, 0x0A, 0, 0xFF); }
void fn_S4_16EA(void) { fn_S4_3D64(0x23, 0x0A, 0, 0xFF); }
void fn_S4_1734(void) { fn_S4_3D64(0x27, 0x30, 0, 0xFF); }
void fn_S4_1796(void) { fn_S4_3D64(0x2C, 0x1E, 0, 0xFF); }
void fn_S4_17DE(void) { fn_S4_3D64(0x30, 0x0A, 0, 0xFF); }

/* fn_S4_1158: draws the nearby objects on the scanner: centred on (x, y) (16.16), it records the
 * view origin (integer parts - $50) in G_3A4C / G_3A4A, and for each of the 120 object records at
 * G_3A50 of type 0..8 whose cell (G_3A54 + its +$30) is visible (bit 2) and that lies within $42
 * units, plots a 3x3 cross: four corners in colour $E and the centre in $F (y flipped: $A0 - dy). */
void fn_S4_1158(int32_t x, int32_t y) {
    int16_t cx = (int16_t)(x >> 16), cy = (int16_t)(y >> 16);
    sg16(0x3A4C, (uint16_t)(cx - 0x50)); sg16(0x3A4A, (uint16_t)(cy - 0x50));
    const int16_t range = 0x42;
    int16_t xmin = (int16_t)(cx - range), ymin = (int16_t)(cy - range), xmax = (int16_t)(cx + range), ymax = (int16_t)(cy + range);
    uint32_t p = g32(0x3A50);
    for (int16_t i = 0; i < 0x78; i++, p += 0x40) {
        if (r16(p + 2) >= 9) continue;
        if (!(r8(g32(0x3A54) + r16(p + 0x30)) & 4)) continue;
        int16_t ox = (int16_t)r16(p + 4), oy = (int16_t)r16(p + 8);
        if (ox < xmin || ox > xmax || oy < ymin || oy > ymax) continue;
        int16_t px = (int16_t)(ox - (int16_t)g16(0x3A4C));
        int16_t py = (int16_t)(0xA0 - (int16_t)(oy - (int16_t)g16(0x3A4A)));
        fn_S4_12D6((int16_t)(px - 1), (int16_t)(py - 1), 0xE);
        fn_S4_12D6((int16_t)(px + 1), (int16_t)(py - 1), 0xE);
        fn_S4_12D6((int16_t)(px - 1), (int16_t)(py + 1), 0xE);
        fn_S4_12D6((int16_t)(px + 1), (int16_t)(py + 1), 0xE);
        fn_S4_12D6(px, py, 0xF);
    }
}

/* Scanner marks for object n (record n of G_3A50) inside the 160x160 view (origin G_3A4C /
 * G_3A4A, y flipped), types 9 and up: types $18 $22 $23 $24 $36 $3D as a plus (centre $F, arms 2),
 * type $29 as five scattered dots in $F; with `others`, every other type as a plus with centre $E
 * and arms 7. */
static void scanner_mark(uint8_t n, int others) {
    uint32_t rec = g32(0x3A50) + ((uint32_t)n << 6);
    int16_t px = (int16_t)(r16(rec + 4) - g16(0x3A4C));
    int16_t py = (int16_t)(0xA0 - (int16_t)(r16(rec + 8) - g16(0x3A4A)));
    if ((uint16_t)px >= 0xA0 || (uint16_t)py >= 0xA0) return;
    int16_t t = (int16_t)r16(rec + 2);
    if (t < 9) return;
    if (t == 0x18 || t == 0x22 || t == 0x23 || t == 0x24 || t == 0x36 || t == 0x3D) {
        fn_S4_12D6(px, py, 0xF);
        fn_S4_12D6((int16_t)(px - 1), py, 2); fn_S4_12D6((int16_t)(px + 1), py, 2);
        fn_S4_12D6(px, (int16_t)(py - 1), 2); fn_S4_12D6(px, (int16_t)(py + 1), 2);
    } else if (t == 0x29) {
        fn_S4_12D6((int16_t)(px - 2), (int16_t)(py - 4), 0xF);
        fn_S4_12D6((int16_t)(px + 4), (int16_t)(py - 2), 0xF);
        fn_S4_12D6((int16_t)(px - 3), py, 0xF);
        fn_S4_12D6((int16_t)(px - 1), (int16_t)(py + 2), 0xF);
        fn_S4_12D6((int16_t)(px + 2), (int16_t)(py + 4), 0xF);
    } else if (others) {
        fn_S4_12D6((int16_t)(px - 1), py, 7); fn_S4_12D6((int16_t)(px + 1), py, 7);
        fn_S4_12D6(px, (int16_t)(py - 1), 7); fn_S4_12D6(px, (int16_t)(py + 1), 7);
        fn_S4_12D6(px, py, 0xE);
    }
}
/* fn_S4_0FD0 / fn_S4_0CF2: scanner mark for object n; 0CF2 also draws the other types. */
void fn_S4_0FD0(uint8_t n) { scanner_mark(n, 0); }
void fn_S4_0CF2(uint8_t n) { scanner_mark(n, 1); }

void fn_S2_03A0(uint32_t msg, int16_t num, int8_t fatal);

/* fn_S4_1E8C: moves to the next object record (G_36FC += $40, G_36F8 += 1); 100 or more is a fatal
 * error (alert with the message G_2AB4). D0.l = $40. */
void fn_S4_1E8C(Regs *r) {
    r->d[0] = 0x40;
    uint32_t p = g32(0x36FC), np = p + 0x40;
    sg32(0x36FC, np);
    uint16_t n = (uint16_t)(g16(0x36F8) + 1);
    sg16(0x36F8, n);
    r->ccr = (uint8_t)(n == 0 ? CCR_X : 0);   /* addq.w carry */
    cc_cmp_w(r, 100, n);
    if ((int16_t)n >= 100) mz_seg(2), fn_S2_03A0(GADDR(0x2AB4), 0, 1);   /* arguments packed by the original as one long $100 */
}

void fn_S4_1E8C(Regs *r);

/* Fills the next object record G_36FC (cleared fields, the given type and 16.16 position), places
 * it (fn_S4_1EAE) and advances to the following record (fn_S4_1E8C). */
static void new_object(uint16_t type, int32_t x, int32_t y, int set18) {
    uint32_t p = g32(0x36FC);
    w16(p, 0); w16(p + 2, type);
    w32(p + 4, (uint32_t)x); w32(p + 8, (uint32_t)y);
    for (uint32_t o = 0xC; o <= 0x16; o += 2) w16(p + o, 0);
    if (set18) w16(p + 0x18, 0x14);
    fn_S4_1EAE((int16_t)g16(0x36F8));
    Regs r = {{0}, {0}, 0};
    fn_S4_1E8C(&r);
}

/* fn_S4_1F10: a new object of type 0 at the centre of cell (col, row). */
void fn_S4_1F10(int16_t col, int16_t row) {
    new_object(0, (int32_t)(((uint32_t)((int32_t)col << 4) + 8) << 16), (int32_t)(((uint32_t)((int32_t)row << 4) + 8) << 16), 0);
}

/* fn_S4_1F8E: a new object of type 3 (+$18 = $14) at the centre of the next cell in the shuffled
 * open-cell list G_36F2 (index G_36F4) that has no objects in it. */
void fn_S4_1F8E(void) {
    uint32_t cell;
    do {
        int16_t i = (int16_t)g16(0x36F4);
        int16_t off = (int16_t)r16(MZ_A5 + (uint32_t)((int32_t)i << 2) - 0x36F2);
        sg16(0x36F4, (uint16_t)(i + 1));
        cell = g32(0x1DCC) + (uint32_t)(int32_t)off;
    } while (r8(cell + 1));
    new_object(3, (int32_t)((uint32_t)(int32_t)(int16_t)r16(cell + 0x1C) << 16), (int32_t)((uint32_t)(int32_t)(int16_t)r16(cell + 0x1E) << 16), 1);
}

/* Places one object of `type` in the next dead-end cell (one open neighbour) of the shuffled
 * open-cell list (from index G_36F4) that holds no objects: +$E = 1, position = the cell centre. */
static void place_in_dead_end(int16_t type, int count_jewel) {
    int placed = 0;
    while (!placed && (int16_t)g16(0x36F4) < (int16_t)g16(0x36F6)) {
        uint32_t e = MZ_A5 + (uint32_t)((int32_t)(int16_t)g16(0x36F4) << 2) - 0x36F2;
        if (r16(e + 2) == 1) {
            uint32_t cell = g32(0x1DCC) + (uint32_t)(int32_t)(int16_t)r16(e);
            if (!r8(cell + 1)) {
                uint32_t p = g32(0x36FC);
                w16(p, 0); w16(p + 2, (uint16_t)type);
                w32(p + 4, (uint32_t)(int32_t)(int16_t)r16(cell + 0x1C) << 16);
                w32(p + 8, (uint32_t)(int32_t)(int16_t)r16(cell + 0x1E) << 16);
                w16(p + 0xC, 0); w16(p + 0xE, 1); w16(p + 0x10, 0); w16(p + 0x12, 0); w16(p + 0x14, 0);
                fn_S4_1EAE((int16_t)g16(0x36F8));
                Regs r = {{0}, {0}, 0};
                fn_S4_1E8C(&r);
                placed = 1;
                if (count_jewel) sg16(0x225E, (uint16_t)(g16(0x225E) + 1));
            }
        }
        sg16(0x36F4, (uint16_t)(g16(0x36F4) + 1));
    }
}
/* fn_S4_2044: a jewel of `type` in a dead end, counted in G_225E (jewels to find). */
void fn_S4_2044(int16_t type) { place_in_dead_end(type, 1); }
/* fn_S4_2128 / fn_S4_2208: a type 7 (bonus multiplier) / type 1 (refill) object in a dead end. */
void fn_S4_2128(void) { place_in_dead_end(7, 0); }
void fn_S4_2208(void) { place_in_dead_end(1, 0); }

/* fn_S4_0C08 / fn_S4_0EE6: scanner marks (fn_S4_0CF2 / fn_S4_0FD0) for each object listed in the
 * cell at `cell` (bytes +1..+10, zero-terminated). */
void fn_S4_0C08(uint32_t cell) {
    for (uint32_t k = 1; k <= 10; k++) { uint8_t n = r8(cell + k); if (!n) break; fn_S4_0CF2(n); }
}
void fn_S4_0EE6(uint32_t cell) {
    for (uint32_t k = 1; k <= 10; k++) { uint8_t n = r8(cell + k); if (!n) break; fn_S4_0FD0(n); }
}

/* fn_S4_25AA: places one object of `type` (heading Random() & $30, +$E = 1) at the centre of the next
 * empty cell of the shuffled open-cell list (from index G_36F4). */
void fn_S4_25AA(int16_t type) {
    int placed = 0;
    while (!placed && (int16_t)g16(0x36F4) < (int16_t)g16(0x36F6)) {
        int16_t off = (int16_t)r16(MZ_A5 + (uint32_t)((int32_t)(int16_t)g16(0x36F4) << 2) - 0x36F2);
        sg16(0x36F4, (uint16_t)(g16(0x36F4) + 1));
        uint32_t cell = g32(0x1DCC) + (uint32_t)(int32_t)off;
        if (r8(cell + 1)) continue;
        uint32_t p = g32(0x36FC);
        w16(p, 0); w16(p + 2, (uint16_t)type);
        w32(p + 4, (uint32_t)(int32_t)(int16_t)r16(cell + 0x1C) << 16);
        w32(p + 8, (uint32_t)(int32_t)(int16_t)r16(cell + 0x1E) << 16);
        w16(p + 0xC, (uint16_t)((uint16_t)mac_Random() & 0x30));
        w16(p + 0xE, 1);
        for (uint32_t o = 0x10; o <= 0x1A; o += 2) w16(p + o, 0);
        fn_S4_1EAE((int16_t)g16(0x36F8));
        Regs r = {{0}, {0}, 0};
        fn_S4_1E8C(&r);
        placed = 1;
    }
}

/* fn_S4_09EC: scanner objects around the point (x, y) (16.16): sets the view origin G_3A4C / G_3A4A
 * (integer parts - $50), marks the objects in the point's cell of map G_3A54 and in up to three open
 * cells straight ahead each way (fn_S4_0C08, stopping at a solid cell), then those in the four open
 * diagonal neighbours (fn_S4_0EE6). */
void fn_S4_09EC(int32_t x, int32_t y) {
    int16_t cx = (int16_t)(x >> 16), cy = (int16_t)(y >> 16);
    sg16(0x3A4C, (uint16_t)(cx - 0x50)); sg16(0x3A4A, (uint16_t)(cy - 0x50));
    uint16_t off = (uint16_t)((((uint16_t)cy & 0xFFF0) << 6) | (((uint16_t)cx & 0xFFF0) << 1));
    uint32_t cell = g32(0x3A54) + off;
    fn_S4_0C08(cell);
    static const int32_t dir[4] = {0x400, -0x400, 0x20, -0x20}, diag[4] = {0x420, 0x3E0, -0x3E0, -0x420};
    for (int k = 0; k < 4; k++)
        for (int step = 1; step <= 3; step++) {
            uint32_t c = cell + (uint32_t)(dir[k] * step);
            if (r8(c) & 1) break;
            fn_S4_0C08(c);
        }
    for (int k = 0; k < 4; k++) { uint32_t c = cell + (uint32_t)diag[k]; if (!(r8(c) & 1)) fn_S4_0EE6(c); }
}

/* fn_S4_0408: fills the 16x16 block at (x, y) of the offscreen buffer G_3A5C (row bytes G_3A62,
 * row stride G_3A60, depth G_3A48) with colour c from the long table G_3A58. Every depth writes
 * the full 16 rows with the store widths the original uses, since the bytes beside a 4-bit block
 * that starts on an odd pixel keep their other nibble. Depth 1 draws the pattern $AAAAAAAA for
 * c=2, white for c=7, $EEEEEEEE for c=10 and black otherwise, rotating the pattern one bit per
 * row. */
void fn_S4_0408(int16_t x, int16_t y, int16_t c) {
    uint32_t stride = g32(0x3A60);
    uint32_t a = g32(0x3A5C) + (uint32_t)(uint16_t)y * (uint32_t)g16(0x3A62);
    uint16_t ux = (uint16_t)x;
    uint32_t v = r32(g32(0x3A58) + (uint32_t)(int32_t)(int16_t)(uint16_t)((uint16_t)c << 2));
    switch (g16(0x3A48)) {
    case 8:
        a += ux;
        for (int row = 0; row < 16; row++, a += stride) {
            if (!(a & 1)) { w32(a, v); w32(a + 4, v); w32(a + 8, v); w32(a + 12, v); }
            else {
                w8(a, (uint8_t)v); w32(a + 1, v); w32(a + 5, v); w32(a + 9, v);
                w16(a + 13, (uint16_t)v); w8(a + 15, (uint8_t)v);
            }
        }
        break;
    case 0x10:
        a += (uint16_t)(ux << 1);
        for (int row = 0; row < 16; row++, a += stride)
            for (int i = 0; i < 8; i++) w32(a + 4u * i, v);
        break;
    case 0x20:
        a += (uint16_t)(ux << 2);
        for (int row = 0; row < 16; row++, a += stride)
            for (int i = 0; i < 16; i++) w32(a + 4u * i, v);
        break;
    case 4: {
        a += (uint16_t)(ux >> 1);
        int odd = ux & 1, wide = (ux & 2) != 0;
        for (int row = 0; row < 16; row++, a += stride) {
            if (!wide && !odd) { w32(a, v); w32(a + 4, v); continue; }
            if (odd) w8(a, (uint8_t)((r8(a) & 0xF0) | (v & 0x0F)));
            if (!wide) {                      /* x = 1 mod 4 */
                w8(a + 1, (uint8_t)v); w32(a + 2, v); w16(a + 6, (uint16_t)v);
            } else {                          /* x = 2 or 3 mod 4 */
                if (!odd) w8(a, (uint8_t)v);
                w32(a + 1, v); w16(a + 5, (uint16_t)v); w8(a + 7, (uint8_t)v);
            }
            if (odd) w8(a + 8, (uint8_t)((r8(a + 8) & 0x0F) | (v & 0xF0)));
        }
        break; }
    case 1: {
        a += (uint32_t)(uint16_t)((ux >> 4) << 1);
        uint32_t mask = 0xFFFF0000u >> (ux & 15);
        uint32_t pat;
        switch (c) {
        case 2:  pat = 0xAAAAAAAAu; break;
        case 7:  pat = 0; break;
        case 10: pat = 0xEEEEEEEEu; break;
        default: pat = 0xFFFFFFFFu; break;
        }
        for (int row = 0; row < 16; row++, a += stride) {
            w32(a, (pat & mask) | (r32(a) & ~mask));
            pat = (pat << 1) | (pat >> 31);
        }
        break; }
    }
}

/* fn_S4_06FA: draws the scanner's player marker at the centre (80, 80): a plus (centre $F, arms
 * $E) and a two-dot heading pointer in $F. The low 4 bits of `dir` pick one of 16 directions
 * within a quadrant, bits 4-5 the quadrant (the offsets rotate by 90 degrees each). */
static const int8_t heading_dots[16][4] = {   /* dx1, dy1, dx2, dy2 for quadrant 0 */
    {-4, 0,-5, 0}, {-4, 0,-5, 0}, {-4,-1,-5,-1}, {-4,-1,-5,-1},
    {-4,-2,-5,-2}, {-4,-2,-5,-3}, {-4,-3,-5,-4}, {-4,-3,-5,-4},
    {-4,-4,-5,-5}, {-3,-4,-4,-5}, {-3,-4,-4,-5}, {-2,-4,-3,-5},
    {-2,-4,-2,-5}, {-1,-4,-1,-5}, {-1,-4,-1,-5}, { 0,-4, 0,-5},
};
void fn_S4_06FA(int16_t dir) {
    const int16_t cx = 0x50, cy = 0x50;
    fn_S4_12D6(cx, cy, 0xF);
    fn_S4_12D6((int16_t)(cx - 1), cy, 0xE);
    fn_S4_12D6((int16_t)(cx + 1), cy, 0xE);
    fn_S4_12D6(cx, (int16_t)(cy - 1), 0xE);
    fn_S4_12D6(cx, (int16_t)(cy + 1), 0xE);
    const int8_t *o = heading_dots[dir & 0xF];
    for (int k = 0; k < 4; k += 2) {
        int16_t a = o[k], b = o[k + 1];
        switch (dir & 0x30) {
        case 0x00: fn_S4_12D6((int16_t)(cx + a), (int16_t)(cy - b), 0xF); break;
        case 0x10: fn_S4_12D6((int16_t)(cx - b), (int16_t)(cy - a), 0xF); break;
        case 0x20: fn_S4_12D6((int16_t)(cx - a), (int16_t)(cy + b), 0xF); break;
        case 0x30: fn_S4_12D6((int16_t)(cx + b), (int16_t)(cy + a), 0xF); break;
        }
    }
}

/* fn_S4_021E: the scanner's map: the 9x9 cells of the 32x32 map G_3A54 (cell records $20 bytes
 * apart in x, $400 in y) around the point (x, y) (16.16), one 16x16 block each (fn_S4_0408),
 * scrolled by the point's position within its cell and with y running upwards. Cell colours:
 * a cell with neither bit 1 nor bit 2 set, or outside the map, 2; with bit 0 (solid) 7; else with
 * bit 0 of its byte +$12 set $A; else with bit 2 set 0; otherwise 2. */
void fn_S4_021E(int32_t x, int32_t y) {
    int16_t ox = (int16_t)((int16_t)(x >> 16) - 0x40), oy = (int16_t)((int16_t)(y >> 16) - 0x40);
    int16_t cx0 = (int16_t)(ox >> 4), cy0 = (int16_t)(oy >> 4);
    int16_t fx = (int16_t)(0x10 - (ox & 0xF)), fy = (int16_t)((oy & 0xF) + 0x81);
    for (int16_t j = 0; j < 9; j++)
        for (int16_t i = 0; i < 9; i++) {
            int16_t cx = (int16_t)(cx0 + i), cy = (int16_t)(cy0 + j), c = 2;
            if (cx >= 0 && cx < 0x20 && cy >= 0 && cy < 0x20) {
                uint32_t cell = g32(0x3A54) + (uint32_t)(int32_t)(int16_t)(uint16_t)(cx << 5)
                              + (uint32_t)(int32_t)(int16_t)(uint16_t)(cy << 10);
                uint8_t b = r8(cell);
                if (!(b & 6))              c = 2;
                else if (b & 1)            c = 7;
                else if (r8(cell + 0x12) & 1) c = 0xA;
                else if (b & 4)            c = 0;
                else                       c = 2;
            }
            fn_S4_0408((int16_t)((uint16_t)(i << 4) + fx), (int16_t)(fy - (int16_t)(uint16_t)(j << 4)), c);
        }
}

/* fn_S4_22E8: places a pair of type-8 objects, 5 pixels either side of the centre in x, in the
 * next cell of the shuffled open-cell list (from index G_36F4) whose entry type is 2, that holds
 * no objects and whose neighbours at +$20 and -$20 are both solid. A second test meant for the
 * other axis (pair split in y) repeats the same neighbours and so never succeeds. */
void fn_S4_22E8(void) {
    int placed = 0;
    while (!placed && (int16_t)g16(0x36F4) < (int16_t)g16(0x36F6)) {
        uint32_t e = MZ_A5 + (uint32_t)((int32_t)(int16_t)g16(0x36F4) << 2) - 0x36F2;
        if (r16(e + 2) == 2) {
            uint32_t cell = g32(0x1DCC) + (uint32_t)(int32_t)(int16_t)r16(e);
            if (!r8(cell + 1)) {
                int32_t cx = (int16_t)r16(cell + 0x1C), cy = (int16_t)r16(cell + 0x1E);
                if ((r8(cell + 0x20) & 1) && (r8(cell - 0x20) & 1)) {
                    new_object(8, (int32_t)((uint32_t)(cx - 5) << 16), (int32_t)((uint32_t)cy << 16), 0);
                    new_object(8, (int32_t)((uint32_t)(cx + 5) << 16), (int32_t)((uint32_t)cy << 16), 0);
                    placed = 1;
                } else if ((r8(cell + 0x20) & 1) && (r8(cell - 0x20) & 1)) {
                    new_object(8, (int32_t)((uint32_t)cx << 16), (int32_t)((uint32_t)(cy - 5) << 16), 0);
                    new_object(8, (int32_t)((uint32_t)cx << 16), (int32_t)((uint32_t)(cy + 5) << 16), 0);
                    placed = 1;
                }
            }
        }
        sg16(0x36F4, (uint16_t)(g16(0x36F4) + 1));
    }
}

/* fn_S4_3016: can a block of the room map (handle h: 32 columns per row, one byte per cell,
 * non-zero = still solid, 0 = dug) take a 3-wide, 2-deep step from cell (x, y) in direction dir (0 = +x, 1 = +y,
 * 2 = -x, 3 = -y)? The step must stay within the limit set by `size` (x or y + 1 <= size + 15
 * going up, - 1 >= 15 - size going down) and all six cells must still be solid. Other directions: true
 * (negative) or false (above 3). */
uint8_t fn_S4_3016(uint32_t h, int16_t size, int16_t x, int16_t y, int16_t dir) {
    mac_HLock(h);
    uint32_t base = r32(h);
    uint8_t ok = 1;
    if (dir >= 0 && dir <= 3) {
        /* along: the step's axis (+1 / -1); the six cells are at depth 1 and 2 along it, -1..+1 across */
        int along = (dir < 2) ? 1 : -1, alongx = !(dir & 1);
        int16_t pos = alongx ? x : y;
        if (along > 0 ? (int16_t)(size + 15) < (int16_t)(pos + 1) : (int16_t)(15 - size) > (int16_t)(pos - 1)) ok = 0;
        for (int depth = 1; ok && depth <= 2; depth++)
            for (int across = -1; ok && across <= 1; across++) {
                int16_t cx = alongx ? (int16_t)(x + along * depth) : (int16_t)(x + across);
                int16_t cy = alongx ? (int16_t)(y + across) : (int16_t)(y + along * depth);
                if (!r8(base + (uint32_t)((int32_t)cy << 5) + (uint32_t)(int32_t)(cx - x) + (uint32_t)(int32_t)x)) ok = 0;
            }
    } else if (dir > 3) ok = 0;
    mac_HUnlock(h);
    return ok;
}

/* fn_S4_2ED4: looks for a place to grow the room map h by one step (fn_S4_3016, with `size`):
 * goes through the candidate cells in `list` (word count, then 4-byte entries { byte x; byte y;
 * word state }, state $FFFF = used up), trying each one's four directions in a random order
 * (fn_S4_3794 / fn_S4_382C). The first that fits sets *pdir and *px, *py to the cell one step
 * that way; a candidate with no direction left is marked used up. Returns whether one was found. */
uint8_t fn_S4_2ED4(uint32_t h, uint32_t list, int16_t size, uint32_t px, uint32_t py, uint32_t pdir) {
    uint8_t found = 0;
    for (int16_t i = 0; i < (int16_t)r16(list) && !found; i++) {
        uint32_t e = list + (uint32_t)((int32_t)i << 2);
        if (r16(e + 4) == 0xFFFF) continue;
        int16_t order = fn_S4_3794();
        for (int16_t k = 0; k < 4 && !found; k++) {
            if (!fn_S4_3016(h, size, r8(e + 2), r8(e + 3), fn_S4_382C(order, k))) continue;
            found = 1;
            w16(pdir, (uint16_t)fn_S4_382C(order, k));
            w16(px, r8(e + 2));
            w16(py, r8(e + 3));
            switch ((int16_t)r16(pdir)) {
            case 0: w16(px, (uint16_t)(r16(px) + 1)); break;
            case 1: w16(py, (uint16_t)(r16(py) + 1)); break;
            case 2: w16(px, (uint16_t)(r16(px) - 1)); break;
            case 3: w16(py, (uint16_t)(r16(py) - 1)); break;
            }
        }
        if (!found) w16(e + 4, 0xFFFF);
    }
    return found;
}

/* fn_S4_335E: steps the room-map digger at (*px, *py) one cell in direction dir (0 = +x, 1 = +y,
 * 2 = -x, 3 = -y; see fn_S4_3016) and says whether it may keep going: not past the `size` limit,
 * and only if the three cells across the step are still solid. Then, if the cell two steps ahead
 * is already dug, a coin toss (fn_S4_36F2) decides; otherwise both its side neighbours must be
 * solid too.
 * (For any other dir the original returns an uninitialised local; it is never called that way.) */
uint8_t fn_S4_335E(uint32_t h, int16_t size, uint32_t px, uint32_t py, int16_t dir) {
    mac_HLock(h);
    uint32_t base = r32(h);
    int16_t x = (int16_t)r16(px), y = (int16_t)r16(py);
    uint8_t ok = 0;
    if (dir >= 0 && dir <= 3) {
        int along = (dir < 2) ? 1 : -1, alongx = !(dir & 1);
        uint32_t p = alongx ? px : py;
        w16(p, (uint16_t)(r16(p) + along));
        int16_t pos = alongx ? x : y;
#define CELL(depth, across) r8(base + (uint32_t)((int32_t)(int16_t)(alongx ? y + (across) : y + along * (depth)) << 5) \
                                    + (uint32_t)(int32_t)(int16_t)((alongx ? x + along * (depth) : x + (across)) - x) + (uint32_t)(int32_t)x)
        if (along > 0 ? (int16_t)(size + 15) < (int16_t)(pos + 1) : (int16_t)(15 - size) > (int16_t)(pos - 1)) ok = 0;
        else if (!CELL(1, -1) || !CELL(1, 0) || !CELL(1, 1)) ok = 0;
        else if (!CELL(2, 0)) ok = (uint8_t)fn_S4_36F2();
        else ok = CELL(2, -1) && CELL(2, 1);
#undef CELL
    }
    mac_HUnlock(h);
    return ok;
}

/* fn_S4_2C1C: digs the 32x32 room map in handle h (fn_S4_2DF6 fills it solid) as a maze of
 * corridors starting from cell (x, y): repeatedly picks a dug cell that can grow
 * (fn_S4_2ED4) and digs from it in a wandering line (fn_S4_2E6C each cell, fn_S4_332C turning
 * now and then) while fn_S4_335E allows, reshuffling the candidate list (fn_S4_3704) after each
 * line; done when no cell can grow. */
void fn_S4_2C1C(uint32_t h, int16_t size, int16_t x, int16_t y) {
    MZ_FRAME;
    uint32_t loc = mz_local(0x1008);
    uint32_t list = loc + 6, px = loc + 4, py = loc + 2, pdir = loc;   /* -$1002, -$1004, -$1006, -$1008 */
    fn_S4_2DF6(h);
    fn_S4_2E4C(h, list, x, y);
    for (;;) {
        if (!fn_S4_2ED4(h, list, size, px, py, pdir)) break;
        do {
            fn_S4_2E6C(h, list, (int16_t)r16(px), (int16_t)r16(py));
            fn_S4_332C(pdir);
        } while (fn_S4_335E(h, size, px, py, (int16_t)r16(pdir)));
        fn_S4_3704(list);
    }
}

/* fn_S4_1802 (JT 57): the jewel pick-up sound: one of four (or, with more than $2F sounds loaded
 * (fn_S4_3886), one of eight) picked at random (fn_S2_3532). */
void fn_S4_1802(void) {
    static void (*const four[4])(void) = {fn_S4_167E, fn_S4_1690, fn_S4_16A2, fn_S4_16B4};
    static void (*const eight[8])(void) = {fn_S4_167E, fn_S4_1690, fn_S4_16A2, fn_S4_16B4,
                                           fn_S4_17BA, fn_S4_17CC, fn_S4_17DE, fn_S4_17F0};
    Regs r = {{0}, {0}, 0};
    fn_S4_3886(&r);
    int many = (int16_t)r.d[0] > 0x2F;
    mz_seg(2);
    int16_t v = fn_S2_3532_value();
    if (many) eight[v & 7](); else four[v & 3]();
}

/* fn_S4_4340 (JT 70): resets the level-end sequence (G_248A, G_2488, G_2489). */
void fn_S4_4340(void) { sg8(0x248A, 0); sg16(0x2488, 0); sg8(0x2489, 0); }

/* fn_S4_435E (JT 72): one step of the end-of-level sequence, state G_2488 (timer G_2486):
 *  0: if the level was won (G_2489) wait $E steps (state 1), else go to the life-lost branch (4);
 *  1: wait, then 2;  2: a life lost (G_20E0 - 1), the bonus G_20EA halved, sound, wait $C (3);
 *  3: wait, then 7;  4: sound, bonus + 1000 * level (G_20EC), then 5;
 *  5: the bonus multiplier: x2 when all jewels were found (G_225E = 0), x2 when G_225B is set;
 *     G_2484 = the target bonus; 6 if there is a multiplier, else 7;
 *  6: count the bonus up 1000 a step (with sound) to the target, then 7;
 *  7: move the bonus to the score G_20E6 500 a step (with sound), then 8;  8: done (G_248A). */
void fn_S4_435E(void) {
    switch (g16(0x2488)) {
    case 0:
        if (g8(0x2489)) { sg16(0x2486, 0xE); sg16(0x2488, (uint16_t)(g16(0x2488) + 1)); }
        else sg16(0x2488, (uint16_t)(g16(0x2488) + 4));
        break;
    case 1:
        if ((int16_t)g16(0x2486) < 0) sg16(0x2488, (uint16_t)(g16(0x2488) + 1));
        else sg16(0x2486, (uint16_t)(g16(0x2486) - 1));
        break;
    case 2:
        sg16(0x20E0, (uint16_t)(g16(0x20E0) - 1));
        sg32(0x20EA, (uint32_t)((int32_t)g32(0x20EA) / 2));
        fn_S4_17A8();
        sg16(0x2486, 0xC);
        sg16(0x2488, (uint16_t)(g16(0x2488) + 1));
        break;
    case 3:
        if ((int16_t)g16(0x2486) < 0) sg16(0x2488, (uint16_t)(g16(0x2488) + 4));
        else sg16(0x2486, (uint16_t)(g16(0x2486) - 1));
        break;
    case 4:
        fn_S4_1796();
        sg32(0x20EA, g32(0x20EA) + (uint32_t)(int32_t)(int16_t)(uint16_t)(0x3E8 * (int32_t)(int16_t)g16(0x20EC)));
        sg16(0x2488, (uint16_t)(g16(0x2488) + 1));
        break;
    case 5: {
        int16_t m = 1;
        if (!g16(0x225E)) m = (int16_t)(m << 1);
        if (g8(0x225B)) m = (int16_t)(m << 1);
        sg32(0x2484, (uint32_t)(int32_t)m * g32(0x20EA));
        sg16(0x2488, (uint16_t)(g16(0x2488) + (m > 1 ? 1 : 2)));
        break; }
    case 6:
        if (g32(0x2484) <= g32(0x20EA)) { sg16(0x2488, (uint16_t)(g16(0x2488) + 1)); break; }
        if (g32(0x20EA) + 0x3E8 >= g32(0x2484)) sg32(0x20EA, g32(0x2484));
        else sg32(0x20EA, g32(0x20EA) + 0x3E8);
        fn_S4_1796();
        break;
    case 7:
        if ((int32_t)g32(0x20EA) <= 0) { sg16(0x2488, (uint16_t)(g16(0x2488) + 1)); break; }
        if ((int32_t)g32(0x20EA) >= 0x1F4) { uint32_t t = g32(0x20E6) + 0x1F4; sg32(0x20EA, g32(0x20EA) - 0x1F4); sg32(0x20E6, t); }
        else { uint32_t t = g32(0x20E6) + g32(0x20EA); sg32(0x20EA, 0); sg32(0x20E6, t); }
        fn_S4_1784();
        break;
    case 8:
        sg8(0x248A, 1);
        break;
    }
}

/* Places a monster of `type` in the next cell of the shuffled open-cell list (from G_36F4, which
 * advances past every cell tried) that holds no objects, facing a random way (Random & $30),
 * +$E = 1 and its state fields cleared. Type $28 starts with $C8 shots (+$18) and health $1F4
 * (+$20). */
static void place_monster(int16_t type) {
    int placed = 0;
    while (!placed && (int16_t)g16(0x36F4) < (int16_t)g16(0x36F6)) {
        uint32_t e = MZ_A5 + (uint32_t)((int32_t)(int16_t)g16(0x36F4) << 2) - 0x36F2;
        int16_t off = (int16_t)r16(e);
        sg16(0x36F4, (uint16_t)(g16(0x36F4) + 1));
        uint32_t cell = g32(0x1DCC) + (uint32_t)(int32_t)off;
        if (r8(cell + 1)) continue;
        uint32_t p = g32(0x36FC);
        w16(p, 0); w16(p + 2, (uint16_t)type);
        w32(p + 4, (uint32_t)(int32_t)(int16_t)r16(cell + 0x1C) << 16);
        w32(p + 8, (uint32_t)(int32_t)(int16_t)r16(cell + 0x1E) << 16);
        w16(p + 0xC, (uint16_t)(mac_Random() & 0x30));
        w16(p + 0xE, 1);
        for (uint32_t f = 0x10; f <= 0x1A; f += 2) w16(p + f, 0);
        if (type == 0x28) { w16(p + 0x18, 0xC8); w16(p + 0x20, 0x1F4); }
        fn_S4_1EAE((int16_t)g16(0x36F8));
        Regs r = {{0}, {0}, 0};
        fn_S4_1E8C(&r);
        placed = 1;
    }
}
void fn_S4_2694(void) { place_monster(0x09); }
void fn_S4_277E(void) { place_monster(0x2D); }
void fn_S4_2868(void) { place_monster(0x28); }
void fn_S4_295E(void) { place_monster(0x1E); }
void fn_S4_2A48(void) { place_monster(0x1F); }
void fn_S4_2B32(void) { place_monster(0x12); }

void fn_S2_03A0(uint32_t msg, int16_t num, int8_t fatal);

/* fn_S4_1C38: makes the room map for a level: a 32x32 scratch map (1 KB handle; out of memory is
 * fatal, message G_2AEA) dug as a maze from (x, y) with the given room size (fn_S4_2C1C), turned
 * into the cell grid (fn_S4_2CC8, fn_S4_1C88). */
void fn_S4_1C38(int16_t size, int16_t x, int16_t y) {
    uint32_t h = mac_NewHandle(0x400);
    if (!h) mz_seg(2), fn_S2_03A0(GADDR(0x2AEA), 0, 1);   /* the long $100 pushed: num 0, fatal byte 1 */
    fn_S4_2C1C(h, size, x, y);
    fn_S4_2CC8(h);
    Regs r = {{0}, {0}, 0};
    build_cells(&r, h);                 /* (the original's D4/D5 are left changed by it) */
    mac_DisposeHandle(h);
}

/* fn_S4_1898 (JT 58): sets up level G_220A (its parameters: entry min(level, 15) - 1 of the $38-byte
 * table at G_3A44): the time limit (G_20F0 / G_20F2), the maze from the corner cell (15, 15) where
 * the player starts (G_2206 / G_2202), the cell grid and object records (fn_S4_1C38 ... 1E76), the
 * start marker, exit and pair objects, refills with the level's two chances, the bonus multiplier
 * on its level G_225A unless taken (G_2258), then the jewels (two {type, min, max} entries at
 * G_3A18) and monsters (six entries at G_3A3C), each in a random number min..max. */
void fn_S4_1898(void) {
    int16_t lvl = (int16_t)(g16(0x220A) - 1);
    if (lvl >= 15) lvl = 14;
    uint32_t t = MZ_A5 + (uint32_t)(int32_t)(0x38 * lvl);
    sg16(0x20F0, r16(t - 0x3A42));
    sg16(0x20F2, g16(0x20F0));
    sg8(0x20F4, 1);
    sg16(0x20DA, 0);
    sg8(0x225C, 0); sg8(0x225B, 0);
    sg16(0x2256, 0);
    int16_t size = (int16_t)r16(t - 0x3A44);
    int16_t x = 0xF, y = 0xF;                       /* corner 4 of the original's (constant) choice */
    sg32(0x2206, (uint32_t)(((int32_t)x << 4) + 8) << 16);
    sg32(0x2202, (uint32_t)(((int32_t)y << 4) + 8) << 16);
    fn_S4_1C38(size, x, y);
    fn_S4_1D0A();
    fn_S4_1E44();
    { Regs r = {{0}, {0}, 0}; fn_S4_1E76(&r); }
    fn_S4_1F10(x, y);
    fn_S4_1F8E();
    fn_S4_22E8();
    sg16(0x36F4, 0);
    if ((uint16_t)mac_Random() < r16(t - 0x3A40)) fn_S4_2208();
    if ((uint16_t)mac_Random() < r16(t - 0x3A3E)) fn_S4_2208();
    if (g16(0x220A) == g16(0x225A) && !g8(0x2258)) fn_S4_2128();
    for (int16_t k = 0; k < 2; k++) {
        uint32_t e = GADDR(0x3A18) + (uint32_t)(int32_t)(0x38 * lvl) + (uint32_t)(6 * k);
        int16_t type = (int16_t)r16(e), lo = (int16_t)r16(e + 2), hi = (int16_t)r16(e + 4);
        uint16_t rnd = (uint16_t)mac_Random();
        int16_t n = (int16_t)(rnd % (uint16_t)(hi - lo + 1) + lo);
        for (int16_t i = 0; i < n; i++)
            if (type == 2 || type == 4 || type == 5 || type == 6) fn_S4_2044(type);
    }
    sg16(0x36F4, 0);
    for (int16_t k = 0; k < 6; k++) {
        uint32_t e = GADDR(0x3A3C) + (uint32_t)(int32_t)(0x38 * lvl) + (uint32_t)(6 * k);
        int16_t type = (int16_t)r16(e), lo = (int16_t)r16(e + 2), hi = (int16_t)r16(e + 4);
        uint16_t rnd = (uint16_t)mac_Random();
        int16_t n = (int16_t)(rnd % (uint16_t)(hi - lo + 1) + lo);
        for (int16_t i = 0; i < n; i++) {
            switch (type) {
            case 0x09: fn_S4_2694(); break;
            case 0x12: fn_S4_2B32(); break;
            case 0x1E: fn_S4_295E(); break;
            case 0x1F: fn_S4_2A48(); break;
            case 0x20: case 0x21: fn_S4_25AA(type); break;
            case 0x28: fn_S4_2868(); break;
            case 0x2D: fn_S4_277E(); break;
            default: break;
            }
        }
    }
}

/* ---------- sounds ---------- */

/* fn_S4_38E6 (JT 62): whether the sound map is loaded (G_2A86) */
int16_t fn_S4_38E6(void) { return g32(0x2A86) ? 1 : 0; }

/* fn_S4_3AB2 (JT 64): disposes of the sound map G_2A86 and the 128 sounds G_2A7E[]. */
void fn_S4_3AB2(void) {
    if (g32(0x2A86)) { mac_DisposeHandle(g32(0x2A86)); sg32(0x2A86, 0); }
    for (int16_t i = 0; i < 0x80; i++) {
        uint32_t slot = GADDR(0x2A7E) + 4u * (uint32_t)i;
        if (r32(slot)) { mac_DisposeHandle(r32(slot)); w32(slot, 0); }
    }
}

/* fn_S4_38F4 (JT 63): loads the sounds from resource file `file`: the map 'SMap' 128 (kept detached
 * in G_2A86; G_2A88 = its byte size / 2 - 4 entries, from byte 8 on), then every 'snd ' resource it
 * names (low byte of each entry, 128..255; 0 = none) into G_2A7E[id - 128] unless already there.
 * Returns 0, a Resource Manager error (1 for a missing resource) after an error alert, or 2 for a
 * bad map. The current resource file is put back. */
int16_t fn_S4_38F4(int16_t file) {
    int16_t saved = mac_CurResFile();
    mac_UseResFile(file);
    uint32_t h = mac_GetResource(0x534D6170, 0x80);          /* 'SMap' */
    int16_t err = mac_ResError();
    if (err || !h) {
        mac_UseResFile(saved);
        sg32(0x2A86, 0);
        mz_seg(2), fn_S2_03A0(GADDR(0x263E), err, 0);
        return err ? err : 1;
    }
    mac_DetachResource(h);
    sg32(0x2A86, h);
    int16_t n = (int16_t)((int16_t)(uint16_t)(mz_seg(3), fn_S3_020E(g32(0x2A86))) >> 1);
    n = (int16_t)(n - 4);
    sg16(0x2A88, (uint16_t)n);
    if (n <= 0) {
        mac_UseResFile(saved);
        fn_S4_3AB2();
        mz_seg(2), fn_S2_03A0(GADDR(g16(0x2A88) ? 0x25CE : 0x2600), 0, 0);
        return 2;
    }
    mac_HLock(g32(0x2A86));
    uint32_t map = deref(g32(0x2A86));
    for (int16_t i = 0; i < (int16_t)g16(0x2A88); i++) {
        int16_t id = (int16_t)(r16(map + 2u * (uint32_t)(int32_t)i + 8) & 0xFF);
        if (!id) continue;
        if (id < 0x80) {
            mz_seg(2), fn_S2_03A0(GADDR(0x258A), i, 0);
            mac_UseResFile(saved);
            fn_S4_3AB2();
            return 2;
        }
        uint32_t slot = GADDR(0x2A7E) + 4u * (uint32_t)(id - 0x80);
        if (r32(slot)) continue;
        uint32_t s = mac_GetResource(0x736E6420, id);         /* 'snd ' */
        err = mac_ResError();
        if (err || !s) {
            mac_UseResFile(saved);
            fn_S4_3AB2();
            mz_seg(2), fn_S2_03A0(GADDR(0x25A8), id, 0);
            return err ? err : 1;
        }
        mac_DetachResource(s);
        w32(slot, s);
    }
    mac_HUnlock(g32(0x2A86));
    mac_UseResFile(saved);
    return 0;
}

/* Sound channels G_2A8A (at most 4): CHAN(i) = { long SndChannelPtr; word priority of the sound
 * playing, 0 = idle }. Requests made this tick (fn_S4_3D64), one per channel: REQ(i) = { word sound
 * index into the map; word priority (0 = none); word play count (0 = once, n = n times); word
 * amplitude }. */
#define CHAN(i) (GADDR(0x267E) + 8u * (uint32_t)(int32_t)(i))
#define REQ(i)  (GADDR(0x265E) + 8u * (uint32_t)(int32_t)(i))

/* fn_S4_42E6 (JT 59): the channels' callBack routine (Pascal, at interrupt time): the channel
 * (param1 of the callBackCmd) has finished its sound, so it is idle again. param2 carries the A5
 * world, which the port does not need. */
void fn_S4_42E6(uint32_t chan, uint32_t cmd) {
    int16_t i = (int16_t)r16(cmd + 2);
    if (i < (int16_t)g16(0x2A8A) && r32(CHAN(i))) w16(CHAN(i) + 4, 0);
}

/* fn_S4_3BF8 (JT 67): opens the G_2A8A sound channels (sampled sound, mono, callBack fn_S4_42E6),
 * with an error alert for each that fails (G_2542 when the hardware is missing / busy), and clears
 * the requests. Nothing without a sound map. */
void fn_S4_3BF8(void) {
    if (!g32(0x2A86) || !g16(0x2A8A)) return;
    for (int16_t i = 0; i < (int16_t)g16(0x2A8A); i++) {
        w32(CHAN(i), 0);
        int16_t err = mac_SndNewChannel(CHAN(i), 5, 0x80, MZ_A5 + 0x1FA);   /* sampledSynth, initMono */
        if (err) {
            w32(CHAN(i), 0);
            mz_seg(2), fn_S2_03A0(GADDR(err == -200 || err == -201 ? 0x2542 : 0x24D4), err, 0);
        }
        w16(CHAN(i) + 4, 0);
    }
    for (int16_t i = 0; i < (int16_t)g16(0x2A8A); i++) { w16(REQ(i) + 2, 0); w16(REQ(i), 0); }
}

/* fn_S4_3CE8 (JT 68): closes the sound channels (quietly), alerting G_24C4 on an error. */
void fn_S4_3CE8(void) {
    if (!g32(0x2A86) || !g16(0x2A8A)) return;
    for (int16_t i = 0; i < (int16_t)g16(0x2A8A); i++) {
        if (!r32(CHAN(i))) continue;
        int16_t err = mac_SndDisposeChannel(r32(CHAN(i)), 1);
        if (err) mz_seg(2), fn_S2_03A0(GADDR(0x24C4), err, 0);
        w32(CHAN(i), 0);
    }
}

static void swap_req(int16_t i, int16_t j) {
    uint32_t a = r32(REQ(i)), b = r32(REQ(i) + 4);
    w32(REQ(i), r32(REQ(j))); w32(REQ(i) + 4, r32(REQ(j) + 4));
    w32(REQ(j), a); w32(REQ(j) + 4, b);
}

/* fn_S4_3F02 (JT 69): starts this tick's sound requests, highest priority first. Each takes the
 * channel already playing a sound of the same priority (the last such), otherwise the one playing
 * the lowest priority, if that is below its own. A busy channel is silenced (quietCmd) and, unless
 * the request repeats onto a sound of its own priority, flushed; then: ampCmd, the sound's sampled
 * sound header (bufferCmd, play count times) and a callBackCmd that frees the channel. Every SndDo
 * error but the last brings up an alert. The requests are cleared. */
void fn_S4_3F02(void) {
    MZ_FRAME;
    int16_t n = (int16_t)g16(0x2A8A);
    if (!g32(0x2A86) || !n) return;
    if (n == 2) {
        if ((int16_t)r16(REQ(0) + 2) < (int16_t)r16(REQ(1) + 2)) swap_req(0, 1);
    } else if (n != 1) {                                   /* bubble sort, highest priority first */
        for (int sorted = 0; !sorted; ) {
            sorted = 1;
            for (int16_t i = 0; i < n - 1; i++)
                if ((int16_t)r16(REQ(i) + 2) < (int16_t)r16(REQ(i + 1) + 2)) { swap_req(i, (int16_t)(i + 1)); sorted = 0; }
        }
    }
    uint32_t cmd = mz_local(8);                           /* SndCommand { cmd; param1; long param2 } */
    for (int16_t i = 0; i < (int16_t)g16(0x2A8A); i++) {
        int16_t pri = (int16_t)r16(REQ(i) + 2);
        if (!pri) continue;
        int16_t best = -1, best_pri = 0x7FBC;
        for (int16_t j = 0; j < (int16_t)g16(0x2A8A); j++) {
            int16_t p = (int16_t)r16(CHAN(j) + 4);
            if (p == pri) { best = j; best_pri = -1; }
            else if (p < best_pri) { best = j; best_pri = p; }
        }
        if (best == -1 || best_pri >= pri) continue;
        int16_t id = (int16_t)r16(REQ(i));
        uint32_t snd = r32(GADDR(0x287E) + 4u * (uint32_t)(int32_t)id);
        uint32_t header = snd + (uint32_t)(int32_t)(int16_t)(r16(snd + 0xA) << 3) + 0xC;
        uint32_t chan = r32(CHAN(best));
        if (!chan) continue;
        int16_t err;
        if (r16(CHAN(best) + 4)) {
            w16(cmd, 4);                                   /* quietCmd */
            if ((err = mac_SndDoImmediate(chan, cmd))) mz_seg(2), fn_S2_03A0(GADDR(0x24B2), err, 0);
            if (!r16(REQ(i) + 4) || best_pri != -1) {
                w16(cmd, 3);                               /* flushCmd */
                if ((err = mac_SndDoImmediate(chan, cmd))) mz_seg(2), fn_S2_03A0(GADDR(0x24A8), err, 0);
            }
        }
        w16(cmd, 0x2B); w16(cmd + 2, r16(REQ(i) + 6));     /* ampCmd */
        if ((err = mac_SndDoImmediate(chan, cmd))) mz_seg(2), fn_S2_03A0(GADDR(0x249E), err, 0);
        w16(CHAN(best) + 4, r16(REQ(i) + 2));
        int16_t times = r16(REQ(i) + 4) ? (int16_t)(r16(REQ(i) + 4) - 1) : 0;
        for (int16_t k = 0; k <= times; k++) {
            w16(cmd, 0x51); w16(cmd + 2, 0); w32(cmd + 4, header);   /* bufferCmd */
            if ((err = mac_SndDoCommand(chan, cmd, 1))) mz_seg(2), fn_S2_03A0(GADDR(0x2496), err, 0);
        }
        w16(cmd, 0x0D); w16(cmd + 2, (uint16_t)best); w32(cmd + 4, MZ_A5);   /* callBackCmd */
        mac_SndDoCommand(chan, cmd, 1);
    }
    for (int16_t i = 0; i < (int16_t)g16(0x2A8A); i++) { w16(REQ(i) + 2, 0); w16(REQ(i), 0); }
}

/* fn_S4_0000 (JT 74): draws the scanner / automap for the player at (x, y) into GWorld gw: the
 * maze (fn_S4_0112 with `cells`), then, with the GWorld's pixels locked and MMU 32-bit mode on, the
 * pixel-level layers (fn_S4_021E, 09EC, 1158) and the direction marker (fn_S4_06FA, G_20D4). The
 * drawing routines take their targets from G_3A54 (`cells`), G_3A50 (`other`), G_3A58 (the fill
 * patterns G_1CE0), G_3A5C / G_3A62 / G_3A60 (pixels, row bytes) and G_3A48 (depth). */
void fn_S4_0000(int32_t x, int32_t y, uint32_t other, uint32_t cells, uint32_t gw) {
    fn_S4_0112(x, y, cells);
    mz_seg(2), fn_S2_0000(gw, 0);
    sg32(0x3A66, mac_GetGWorldPixMap(gw));
    uint32_t dev = mac_GetGWorldDevice(gw);
    sg16(0x3A48, r16(deref(r32(deref(dev) + 0x16)) + 0x20));
    if (!mac_LockPixels(g32(0x3A66))) return;
    uint8_t mmu = mac_SwapMMUMode(1);
    mz_seg(3);
    sg32(0x3A54, fn_S3_03A0(cells));
    sg32(0x3A50, fn_S3_03A0(other));
    sg32(0x3A58, fn_S3_03A0(GADDR(0x1CE0)));
    sg32(0x3A5C, mac_GetPixBaseAddr(g32(0x3A66)));
    uint16_t rowbytes = r16(deref(g32(0x3A66)) + 4) & 0x3FFF;
    sg16(0x3A62, rowbytes);
    sg32(0x3A60, (uint32_t)(int32_t)(int16_t)rowbytes);
    fn_S4_021E(x, y);
    fn_S4_09EC(x, y);
    fn_S4_1158(x, y);
    fn_S4_06FA((int16_t)g16(0x20D4));
    mac_SwapMMUMode(mmu);
    mac_UnlockPixels(g32(0x3A66));
}
