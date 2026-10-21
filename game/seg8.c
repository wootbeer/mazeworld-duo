/* CODE segment 8 (main program: init, menus, event loop, preferences): functions reimplemented so
 * far (see build/disasm/seg8.s). */
#include "mzasm.h"

void fn_S2_3500(Regs *r);
void fn_S8_139E(Regs *r);
void fn_S8_13B6(Regs *r);
void fn_S8_13D2(Regs *r);
void fn_S8_13EE(Regs *r);

/* fn_S8_0E4C: G_2102 = 0. */
void fn_S8_0E4C(Regs *r) { sg16(0x2102, 0); cc_nz_w(r, 0); }

/* fn_S8_1376 / fn_S8_138A: G_20D4 = (G_20D4 -/+ 1) & 63, then the (empty) fn_S2_3500. D0.w = new value. */
static void step_20D4(Regs *r, int delta) {
    uint16_t old = g16(0x20D4), v = (uint16_t)(old + delta);
    int x = delta < 0 ? old == 0 : old == 0xFFFF;      /* subq/addq carry */
    v &= 0x3F;
    set_w(&r->d[0], v); sg16(0x20D4, v);
    r->ccr = (uint8_t)(x ? CCR_X : 0); cc_nz_w(r, v);
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
}
void fn_S8_1376(Regs *r) { step_20D4(r, -1); }
void fn_S8_138A(Regs *r) { step_20D4(r, +1); }

/* fn_S8_0E68: every 16th step of G_20BC (low four bits zero), G_20F2 counts down to 0.
 * D0.l = G_20BC & 15. */
void fn_S8_0E68(Regs *r) {
    uint16_t m = g16(0x20BC) & 0xF;
    r->d[0] = m;
    if (m) { cc_nz_w(r, m); return; }
    uint16_t old = g16(0x20F2), v = (uint16_t)(old - 1);
    sg16(0x20F2, v);
    r->ccr = (uint8_t)(old == 0 ? CCR_X : 0);          /* subq borrow */
    cc_nz_w(r, v);
    if (v & 0x8000) { sg16(0x20F2, 0); cc_nz_w(r, 0); }
}

/* fn_S8_2A32: G_21FC = 0, G_22C6 = 0, G_21B4 = 1, G_21B3 = 0, G_21B0 = 0. */
void fn_S8_2A32(Regs *r) {
    sg8(0x21FC, 0);
    if (mz_original) sog8(0x57DE, 0);                         /* Original (fn_S8_21CC) also clears G_57DE */
    sg8(0x22C6, 0); sg8(0x21B4, 1); sg8(0x21B3, 0); sg8(0x21B0, 0);
    cc_nz_b(r, 0);
}

/* ---------- compiled C ---------- */

/* fn_S8_01F2: sets the byte flag G_4E68[index] (index is a signed byte offset). */
void fn_S8_01F2(int16_t index) { w8(GADDR(0x4E68) + (uint32_t)(int32_t)index, 1); }

/* fn_S8_4858: 1 if the byte is negative (high bit set), else 0. */
int16_t fn_S8_4858(int8_t c) { return c < 0 ? 1 : 0; }

/* fn_S8_04C8: G_20CA = cell offset of the position in G_20D2/G_20CE (integer parts & $1F0, combined
 * as in fn_S2_3480), then the word at +$C of that cell in the G_1DCC block = 1, then fn_S2_3500.
 * A0 = G_1DCC, D0.w = D1.w = the cell offset. */
void fn_S2_355E(Regs *r);
void fn_S8_04C8(Regs *r) {
    if (mz_original) mz_seg_of(2, 0x355E), fn_S2_355E(r);    /* Original (fn_S8_08B2) floods the scent first */
    uint32_t v = g32(0x20D2);
    r->d[1] = (v << 16) | (v >> 16);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] & 0x1F0)); lsl_w(r, &r->d[1], 1);
    v = g32(0x20CE);
    r->d[0] = (v << 16) | (v >> 16);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0x1F0)); lsl_w(r, &r->d[0], 6);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    sg16(0x20CA, (uint16_t)r->d[1]);
    r->a[0] = g32(0x1DCC);
    set_w(&r->d[0], g16(0x20CA));
    w16(r->a[0] + (uint32_t)(int32_t)(int16_t)r->d[0] + 0xC, 1);
    cc_nz_w(r, 1);
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
}

/* fn_S8_01CE: clears the 50 byte flags G_4E68[0..49]. */
void fn_S8_01CE(void) { for (int16_t i = 0; i < 0x32; i++) w8(GADDR(0x4E68) + (uint32_t)i, 0); }

/* fn_S8_09FC: for G_2150 records of $CC bytes starting at p: word +2 = $110, word +4 = -1.
 * (A count of 0 runs 65536 times, as the original's subq/bne loop does.) */
void fn_S8_09FC(uint32_t p) {
    uint16_t n = g16(0x2150);
    do { w16(p + 2, 0x110); w16(p + 4, 0xFFFF); p += 0xCC; } while (--n);
}

/* fn_S8_521A: size class of a screen (GDHandle): 2 if smaller than 464x384, 3 if smaller than
 * 640x480, else 4. Original: 1 under 352x270, 2 under 512x384, then the same. */
int16_t fn_S8_521A(uint32_t gd) {
    uint32_t g = deref(gd);
    int16_t w = (int16_t)(r16(g + 0x28) - r16(g + 0x24)), h = (int16_t)(r16(g + 0x26) - r16(g + 0x22));
    if (mz_original) {                                     /* Original (fn_S8_46BA): from size 1 */
        if (w < 0x160 || h < 0x10E) return 1;
        if (w < 0x200 || h < 0x180) return 2;
    }
    if (w < 0x1D0 || h < 0x180) return 2;
    if (w < 0x280 || h < 0x1E0) return 3;
    return 4;
}

void fn_S2_33A4(Regs *r);
void fn_S2_3480(Regs *r);

/* fn_S8_0F1A: creates an object of type D0.w at the player's position and heading (G_20D2, G_20CE,
 * G_20D4) in the highest free record of G_1DC4 (word +2 = -1; record 0 is never used), added to the
 * player's cell G_20CA; nothing if no record is free or the cell list is full. Register interface
 * with a frame (D3/D4 saved and restored). Note the record address uses all of D4, whose upper word
 * is the caller's (adda.l): kept as in the original. */
void fn_S8_0F1A(Regs *r) {
    uint32_t d3 = r->d[3], d4 = r->d[4];
    set_w(&r->d[3], (uint16_t)r->d[0]);
    r->a[0] = g32(0x1DC4);
    set_w(&r->d[1], 0x1DC0);
    for (;;) {
        set_w(&r->d[2], r16(r->a[0] + sxw(r->d[1]) + 2));
        cc_cmp_w(r, 0xFFFF, (uint16_t)r->d[2]);
        if ((uint16_t)r->d[2] == 0xFFFF) break;
        uint16_t v = (uint16_t)r->d[1];
        sub_w(r, &r->d[1], 0x40);
        if (v == 0x40) goto out;
    }
    set_w(&r->d[4], (uint16_t)r->d[1]);
    if (mz_original) {                                        /* Original (fn_S8_14B4): a shot costs 16 of G_56E6 */
        set_w(&r->d[1], og16(0x56E6)); sub_w(r, &r->d[1], 0x10);
        if ((uint16_t)r->d[1] & 0x8000) goto out;
        sog16(0x56E6, (uint16_t)r->d[1]);
    }
    set_w(&r->d[0], (uint16_t)r->d[4]);
    set_w(&r->d[1], g16(0x20CA));
    mz_seg_of(2, 0x33A4), fn_S2_33A4(r);
    if ((uint16_t)r->d[0]) goto out;
    r->a[0] = g32(0x1DC4) + r->d[4];
    w16(r->a[0] + 2, (uint16_t)r->d[3]);
    w32(r->a[0] + 4, g32(0x20D2)); w32(r->a[0] + 8, g32(0x20CE)); w16(r->a[0] + 0xC, g16(0x20D4));
    w16(r->a[0] + 0x12, mz_original ? og16(0x56C6) : 0); w16(r->a[0] + 0x14, 0); w16(r->a[0] + 0xE, 0);
    set_w(&r->d[0], (uint16_t)r->d[4]);
    mz_seg_of(2, 0x3480), fn_S2_3480(r);
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
out:
    r->d[3] = d3; r->d[4] = d4;
    cc_nz_l(r, d4);   /* flags from restoring D4 */
}

/* fn_S8_61FE: disposes of the handle G_3C9C. */
void fn_S8_61FE(Regs *r) { r->a[0] = g32(0x3C9C); mac_trap_regs(r, 0xA023); }

/* fn_S8_6036: fills the locked table in handle G_1EDC with G_214E + 1 longs 0, $CC, 2*$CC, ...
 * (offsets of the $CC-byte records; a count of $FFFF wraps to 65536 entries, as in the original). */
void fn_S8_6036(void) {
    mac_HLock(g32(0x1EDC));
    uint32_t p = deref(g32(0x1EDC)), v = 0;
    uint16_t n = (uint16_t)(g16(0x214E) + 1);
    do { w32(p, v); p += 4; v += 0xCC; } while (--n);
    mac_HUnlock(g32(0x1EDC));
}

/* fn_S8_02F0: locks the 50 handles of G_1EBC[] and keeps their pointers in G_1DA8[] (0 for none). */
void fn_S8_02F0(void) {
    for (int16_t i = 0; i < 0x32; i++) {
        uint32_t h = r32(GADDR(0x1EBC) + 4u * (uint32_t)i);
        if (h) { mac_HLock(h); w32(GADDR(0x1DA8) + 4u * (uint32_t)i, deref(h)); }
        else w32(GADDR(0x1DA8) + 4u * (uint32_t)i, 0);
    }
}

/* fn_S8_0360: unlocks the 50 handles of G_1EBC[] and clears G_1DA8[]. */
void fn_S8_0360(void) {
    for (int16_t i = 0; i < 0x32; i++) {
        uint32_t h = r32(GADDR(0x1EBC) + 4u * (uint32_t)i);
        if (h) mac_HUnlock(h);
        w32(GADDR(0x1DA8) + 4u * (uint32_t)i, 0);
    }
}

/* fn_S8_124E: game keyboard shortcuts from an EventRecord. With Command held: G, g or . (any event)
 * sets G_2220 and G_221F; on a key-down, / 1 2 3 4 ? N O Q S (letters either case) sets G_2220,
 * G_221F and G_221E. Without Command: Escape or Return on a key-down sets G_2220 and G_221F.
 * (The original tests the characters with a compiled chain of byte subtractions; this is the set
 * it accepts, checked over all 128 values.) */
void fn_S8_124E(uint32_t ev) {
    uint8_t c = r8(ev + 5);                       /* low byte of the message: the character */
    int cmd = r8(ev + 0xE) & 1, keydown = r16(ev) == 3;
    if (cmd && (c == 'G' || c == 'g' || c == '.')) { sg8(0x2220, 1); sg8(0x221F, 1); return; }
    if (cmd) {
        if (!keydown || (c & 0x80)) return;
        switch (c) {
        case '/': case '1': case '2': case '3': case '4': case '?':
        case 'N': case 'O': case 'Q': case 'S': case 'n': case 'o': case 'q': case 's':
            sg8(0x2220, 1); sg8(0x221F, 1); sg8(0x221E, 1);
        }
        return;
    }
    if ((c == 0x1B || c == 0x0D || (mz_original && c == ' ')) && keydown) { sg8(0x2220, 1); sg8(0x221F, 1); }   /* Original: Space too */
}

/* fn_S8_15C0: D0.l = -1 if object D0.b (record in G_1DC4) is close enough to the point
 * G_4E1A/G_4E16 by the G_1DB8 map (map value < its +$2E), else 0. For types 6 7 8 $1C $26 $27 $3C,
 * 0 also when one coordinate of the point is at x.15 (low four bits $F) and the other's low four
 * bits are 7 to 9. A0 = the record. */
void fn_S8_15C0(Regs *r) {
    r->a[0] = g32(0x1DC4);
    r->d[0] = (r->d[0] & 0xFF) << 6;
    r->ccr = (uint8_t)(r->d[0] ? 0 : CCR_Z);      /* lsl.l #6: nothing shifted out, X = C = 0 */
    r->a[0] += r->d[0];
    set_w(&r->d[0], r16(r->a[0] + 2));
    uint16_t t = (uint16_t)r->d[0];
    if (t == 6 || t == 7 || t == 8 || t == 0x1C || t == 0x26 || t == 0x27 || t == 0x3C) {
        uint16_t x = g16(0x4E1A) & 0xF, y = g16(0x4E16) & 0xF, other;
        if (x == 0xF) other = y;
        else if (y == 0xF) other = x;
        else goto map;
        set_w(&r->d[0], other); cc_cmp_w(r, 7, other);
        if (other >= 7 && other < 0xA) { r->d[0] = 0; cc_nz_l(r, 0); return; }
        goto map;
    }
map:
    r->a[1] = g32(0x1DB8);
    map_offset(r, r16(r->a[0] + 8), g16(0x4E16), r16(r->a[0] + 4), g16(0x4E1A));
    r->d[0] = r8(r->a[1] + sxw(r->d[1]));
    if ((int16_t)r->d[0] < (int16_t)r16(r->a[0] + 0x2E)) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); }
    else { r->d[0] = 0; cc_nz_l(r, 0); }
}

/* fn_S8_3492: resets the player state for a new game, seeds G_0082 from the clock (low-memory Time)
 * and sets G_225A to 5 + Random() mod 6 (Random taken as unsigned). */
void fn_S8_3492(void) {
    sg8(0x21B4, 1); sg8(0x21B3, 0); sg8(0x220C, 1);
    sg32(0x2206, 0xF80000); sg32(0x2202, 0xF80000);
    sg16(0x220A, 1); sg16(0x2208, 0x30); sg8(0x21FE, 1);
    sg16(0x20FC, 0x100); sg16(0x20FE, g16(0x20FC));
    sg16(0x20F6, 0x100); sg16(0x20F8, g16(0x20F6));
    sg16(0x20EC, 1); sg32(0x20EA, 0); sg32(0x20E6, 0);
    sg16(0x20E0, 3); sg16(0x20D8, 0);
    sg32(0x0082, r32(0x020C));
    sg16(0x225A, (uint16_t)((uint16_t)mac_Random() % 6 + 5));
    sg8(0x2258, 0);
}

/* The game's resource handles and the pointer globals they are locked into (fn_S8_083A /
 * fn_S8_0946), in the original's order. */
static const uint16_t lock_pairs[19][2] = {
    {0x1F08, 0x1DF4}, {0x1F04, 0x1DF0}, {0x1F00, 0x1DEC}, {0x1EFC, 0x1DE8}, {0x1EDC, 0x1DC8},
    {0x1EC4, 0x1DB0}, {0x1EC0, 0x1DAC}, {0x1EF8, 0x1DE4}, {0x1EF4, 0x1DE0}, {0x1EF0, 0x1DDC},
    {0x1EEC, 0x1DD8}, {0x1EE8, 0x1DD4}, {0x1EE4, 0x1DD0}, {0x1EE0, 0x1DCC}, {0x1ED8, 0x1DC4},
    {0x1ED4, 0x1DC0}, {0x1ED0, 0x1DBC}, {0x1ECC, 0x1DB8}, {0x1EC8, 0x1DB4}};

/* Mazeworld Original's (fn_S8_0C28 / fn_S8_0D42): 20 handles, all but the last unlocked again */
static const uint16_t lock_pairs_original[20][2] = {
    {0x5514, 0x53FC}, {0x5510, 0x53F8}, {0x550C, 0x53F4}, {0x5508, 0x53F0}, {0x54E8, 0x53D0},
    {0x54CC, 0x53B4}, {0x54C8, 0x53B0}, {0x5504, 0x53EC}, {0x5500, 0x53E8}, {0x54FC, 0x53E4},
    {0x54F8, 0x53E0}, {0x54F4, 0x53DC}, {0x54F0, 0x53D8}, {0x54EC, 0x53D4}, {0x54E4, 0x53CC},
    {0x54E0, 0x53C8}, {0x54DC, 0x53C4}, {0x54D8, 0x53C0}, {0x54D4, 0x53BC}, {0x54D0, 0x53B8}};

/* fn_S8_083A: locks the 19 handles (Original: 20) and stores their master pointers in the pointer globals. */
void fn_S8_083A(Regs *r) {
    int n = mz_original ? 20 : 19;
    for (int i = 0; i < n; i++) {
        uint32_t h = mz_original ? OADDR(lock_pairs_original[i][0]) : GADDR(lock_pairs[i][0]);
        uint32_t d = mz_original ? OADDR(lock_pairs_original[i][1]) : GADDR(lock_pairs[i][1]);
        r->a[0] = r32(h); mac_trap_regs(r, 0xA029);
        r->a[0] = r32(h);
        uint32_t p = r32(r->a[0]); w32(d, p); cc_nz_l(r, p);
    }
}

/* fn_S8_0946: unlocks the first 18 (Original: 19) of those handles and clears their pointer globals (the 19th,
 * G_1EC8 / G_1DB4, is left locked, as in the original). */
void fn_S8_0946(Regs *r) {
    int n = mz_original ? 19 : 18;
    for (int i = 0; i < n; i++) {
        uint32_t h = mz_original ? OADDR(lock_pairs_original[i][0]) : GADDR(lock_pairs[i][0]);
        uint32_t d = mz_original ? OADDR(lock_pairs_original[i][1]) : GADDR(lock_pairs[i][1]);
        r->a[0] = r32(h); mac_trap_regs(r, 0xA02A);
        w32(d, 0); cc_nz_l(r, 0);
    }
}

/* fn_S8_1522: D0.l = -1 if any object listed in cell D0.w of G_1DCC (bytes +1..+10, zero-terminated)
 * passes fn_S8_15C0, else 0 (also for a negative cell). Keeps D3 and A2 (LINK frame). */
void fn_S8_1522(Regs *r) {
    uint32_t d3 = r->d[3], a2 = r->a[2];
    set_w(&r->d[3], (uint16_t)r->d[0]);
    if ((uint16_t)r->d[3] & 0x8000) { r->d[0] = 0; goto out; }
    r->a[2] = g32(0x1DCC);
    for (int k = 1; k <= 10; k++) {
        set_b(&r->d[0], r8(r->a[2] + sxw(r->d[3]) + (uint32_t)k));
        if (!(uint8_t)r->d[0]) { r->d[0] = 0; goto out; }
        fn_S8_15C0(r);
        if (r->d[0]) { r->d[0] = 0xFFFFFFFFu; goto out; }
    }
    r->d[0] = 0;
out:
    r->d[3] = d3; r->a[2] = a2;
    cc_nz_l(r, d3);   /* flags from restoring D3 */
}

void fn_S8_1522(Regs *r);

/* Cell offset in G_1DCC of a 16.16 position: integer parts & $1F0, x << 1 | y << 6 (computed with
 * the swap / andi / lsl sequence on a data register; X from the shift). */
static uint16_t cell_x(Regs *r, uint32_t *reg, uint32_t pos) {
    *reg = swap(pos); set_w(reg, (uint16_t)((uint16_t)*reg & 0x1F0)); lsl_w(r, reg, 1); return (uint16_t)*reg;
}
static uint16_t cell_y(Regs *r, uint32_t *reg, uint32_t pos) {
    *reg = swap(pos); set_w(reg, (uint16_t)((uint16_t)*reg & 0x1F0)); lsl_w(r, reg, 6); return (uint16_t)*reg;
}

/* fn_S8_140A: moves the player one step in direction D0.w (step table G_12EA), sliding along walls:
 * x and y are tried separately into G_4E1A / G_4E16 (a solid cell, bit 0, refuses that axis and
 * marks the move as blocked). If an object is in the target cell (fn_S8_1522) nothing moves.
 * Otherwise the player position G_20D2 / G_20CE is updated; within the same cell the bytes D1.b and
 * D2.b go to G_22B0 / G_22AF, else the cell marker (+$C) moves to the new cell and G_20CA follows.
 * Keeps D5 (LINK frame). */
static void move_original(Regs *r);
void fn_S8_140A(Regs *r) {
    if (mz_original) { move_original(r); return; }
    uint32_t d5save = r->d[5];
    uint8_t in1 = (uint8_t)r->d[1], in2 = (uint8_t)r->d[2];
    lsl_w(r, &r->d[0], 2);
    r->a[0] = GADDR(0x12EA);
    sg32(0x4E1A, g32(0x20D2)); sg32(0x4E16, g32(0x20CE));
    int blocked = 0;
    r->d[1] = r32(r->a[0] + sxw(r->d[0]) + 0x40); add_l(r, &r->d[1], g32(0x20D2));
    cell_x(r, &r->d[5], r->d[1]);
    cell_y(r, &r->d[2], g32(0x4E16));
    set_w(&r->d[5], (uint16_t)((uint16_t)r->d[5] | (uint16_t)r->d[2]));
    r->a[1] = g32(0x1DCC);
    set_b(&r->d[2], r8(r->a[1] + sxw(r->d[5])));
    if (!(r->d[2] & 1)) sg32(0x4E1A, r->d[1]); else blocked = 1;
    r->d[1] = r32(r->a[0] + sxw(r->d[0])); add_l(r, &r->d[1], g32(0x20CE));
    cell_x(r, &r->d[5], g32(0x4E1A));
    r->d[2] = r->d[1]; cell_y(r, &r->d[2], r->d[1]);
    set_w(&r->d[5], (uint16_t)((uint16_t)r->d[5] | (uint16_t)r->d[2]));
    r->a[1] = g32(0x1DCC);
    set_b(&r->d[2], r8(r->a[1] + sxw(r->d[5])));
    if (!(r->d[2] & 1)) sg32(0x4E16, r->d[1]); else blocked = 1;
    cell_x(r, &r->d[5], g32(0x4E1A));
    cell_y(r, &r->d[0], g32(0x4E16));
    set_w(&r->d[5], (uint16_t)((uint16_t)r->d[5] | (uint16_t)r->d[0]));
    set_w(&r->d[0], (uint16_t)r->d[5]);
    fn_S8_1522(r);
    set_w(&r->d[1], (uint16_t)r->d[0]);
    if ((uint16_t)r->d[1]) goto out;
    sg32(0x20D2, g32(0x4E1A)); sg32(0x20CE, g32(0x4E16));
    if ((uint16_t)r->d[5] == g16(0x20CA)) {
        sg8(0x22B0, in1); sg8(0x22AF, in2);
        set_b(&r->d[0], (uint8_t)blocked);
        if (blocked) mz_seg_of(2, 0x3500), fn_S2_3500(r);
        goto out;
    }
    set_w(&r->d[0], g16(0x20CA));
    r->a[0] = g32(0x1DCC);
    w16(r->a[0] + sxw(r->d[0]) + 0xC, 0);
    w16(r->a[0] + sxw(r->d[5]) + 0xC, 1);
    sg16(0x20CA, (uint16_t)r->d[5]);
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
out:
    r->d[5] = d5save;
    cc_nz_l(r, d5save);   /* flags from restoring D5 */
}

/* Mazeworld Original's step (fn_S8_19C6): no sliding: x and y move together, into G_4E1A / G_4E16,
 * unless the target cell is solid or holds an object (fn_S8_1522). Otherwise as Catacombs' (without
 * the blocked case). */
static void move_original(Regs *r) {
    uint32_t d5save = r->d[5];
    uint8_t in1 = (uint8_t)r->d[1], in2 = (uint8_t)r->d[2];
    lsl_w(r, &r->d[0], 2);
    r->a[0] = GADDR(0x12EA);
    r->d[1] = r32(r->a[0] + sxw(r->d[0]) + 0x40); add_l(r, &r->d[1], g32(0x20D2)); sg32(0x4E1A, r->d[1]);
    r->d[1] = r32(r->a[0] + sxw(r->d[0]));        add_l(r, &r->d[1], g32(0x20CE)); sg32(0x4E16, r->d[1]);
    cell_x(r, &r->d[5], g32(0x4E1A));
    cell_y(r, &r->d[0], g32(0x4E16));
    set_w(&r->d[5], (uint16_t)((uint16_t)r->d[5] | (uint16_t)r->d[0]));
    r->a[0] = g32(0x1DCC);
    set_b(&r->d[0], r8(r->a[0] + sxw(r->d[5])));
    if (r->d[0] & 1) goto out;
    set_w(&r->d[0], (uint16_t)r->d[5]);
    fn_S8_1522(r);
    set_w(&r->d[1], (uint16_t)r->d[0]);
    if ((uint16_t)r->d[1]) goto out;
    sg32(0x20D2, g32(0x4E1A)); sg32(0x20CE, g32(0x4E16));
    if ((uint16_t)r->d[5] == g16(0x20CA)) { sg8(0x22B0, in1); sg8(0x22AF, in2); goto out; }
    set_w(&r->d[0], g16(0x20CA));
    r->a[0] = g32(0x1DCC);
    w16(r->a[0] + sxw(r->d[0]) + 0xC, 0);
    w16(r->a[0] + sxw(r->d[5]) + 0xC, 1);
    sg16(0x20CA, (uint16_t)r->d[5]);
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
out:
    r->d[5] = d5save;
    cc_nz_l(r, d5save);
}

/* ---------- Toolbox setup and windows (compiled C) ---------- */

/* fn_S8_25A8: the standard Toolbox initialisation (QuickDraw globals at A5 - 4). */
void fn_S8_25A8(void) {
    mac_InitGraf(GADDR(0x0004)); mac_InitFonts(); mac_InitWindows(); mac_InitMenus();
    mac_TEInit(); mac_InitDialogs(0); mac_InitCursor();
}

/* fn_S8_26A2: installs menu bar 128 and adds the desk accessories to menu 128. */
void fn_S8_26A2(void) {
    mac_SetMenuBar(mac_GetNewMBar(0x80));
    mac_AppendResMenu(mac_GetMenuHandle(0x80), 0x44525652);   /* 'DRVR' */
    mac_DrawMenuBar();
}

/* fn_S8_460C: closes the game window G_222E, restoring the menu bar height (low memory MBarHeight)
 * from G_2226, and redraws the menu bar. */
void fn_S8_460C(void) {
    w16(0x0BAA, g16(0x2226));
    mac_CloseWindow(g32(0x222E));
    sg32(0x222E, 0);
    mac_DrawMenuBar();
}

/* fn_S8_46EE: disposes of the chain of windows starting at G_222A, each window's refCon being the
 * next one. */
void fn_S8_46EE(void) {
    uint32_t w = g32(0x222A);
    while (w) { uint32_t next = mac_GetWRefCon(w); mac_DisposeWindow(w); w = next; }
}

void fn_S2_39F8(Regs *r);

/* fn_S8_11F0: update event for window w. For the game window G_222E: paints it with palette entry 0,
 * resets the span records (fn_S8_09FC on G_1DE8) and flags everything for redrawing (fn_S2_39F8). */
void fn_S8_11F0(uint32_t w) {
    if (w != g32(0x222E)) { mac_BeginUpdate(w); mac_EndUpdate(w); return; }
    uint32_t gw = g32(0x222E);
    mac_BeginUpdate(gw); mac_EndUpdate(gw);
    mac_SetPort(gw);
    mac_PmForeColor(0);
    mac_PaintRect(gw + 0x10);   /* the original paints a copy of portRect */
    fn_S8_09FC(g32(0x1DE8));
    Regs r = {{0}, {0}, 0};
    mz_seg_of(2, 0x39F8), fn_S2_39F8(&r);
    mac_ShowCursor(); mac_HideCursor();
}

void fn_S4_16FC(void);
void fn_S4_1772(void);

/* fn_S8_0F0C: creates an object of type $18 at the player (fn_S8_0F1A) and plays sound $24. */
void fn_S8_0F0C(Regs *r) { set_w(&r->d[0], 0x18); fn_S8_0F1A(r); mz_seg_of(4, 0x16FC), fn_S4_16FC(); }

/* fn_S8_0E52: when G_20FE reaches 0: G_20F8 = 0, G_20D8 = 7 and sound $2A. */
void fn_S8_0E52(Regs *r) {
    cc_nz_w(r, g16(0x20FE));
    if (g16(0x20FE)) return;
    sg16(0x20F8, 0); sg16(0x20D8, 7);
    mz_seg_of(4, 0x1772), fn_S4_1772();
}

/* fn_S8_139E / 13B6 / 13D2 / 13EE: player moves (fn_S8_140A) backwards / forwards / sideways, as
 * direction G_20D4 + 0, $20, $10, -$10 (& 63), with G_22B0 / G_22AF stepped by one. */
static void player_step(Regs *r, uint16_t turn, int dx, int dy) {
    set_w(&r->d[0], g16(0x20D4));
    if (turn) set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] + turn));
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0x3F));
    set_b(&r->d[1], g8(0x22B0)); set_b(&r->d[2], g8(0x22AF));
    if (dx) set_b(&r->d[1], (uint8_t)((uint8_t)r->d[1] + dx));
    if (dy) set_b(&r->d[2], (uint8_t)((uint8_t)r->d[2] + dy));
    fn_S8_140A(r);
}
void fn_S8_139E(Regs *r) { player_step(r, 0, 0, -1); }
void fn_S8_13B6(Regs *r) { player_step(r, 0x20, 0, +1); }
void fn_S8_13D2(Regs *r) { player_step(r, 0x10, +1, 0); }
void fn_S8_13EE(Regs *r) { player_step(r, (uint16_t)-0x10, -1, 0); }

void fn_S8_1376(Regs *r);
void fn_S8_138A(Regs *r);

/* fn_S8_0E80: applies the accumulated movement input: G_2176 turns (two units per step, fn_S8_1376
 * / fn_S8_138A), G_2178 moves backwards / forwards (fn_S8_139E / fn_S8_13B6), G_217A sideways
 * (fn_S8_13EE / fn_S8_13D2), keeping the remainders, which are then added to G_217E / G_2180 /
 * G_2182. */
void fn_S8_0E80(void) {
    Regs r = {{0}, {0}, 0};
    struct { uint16_t g; void (*neg)(Regs *); void (*pos)(Regs *); } axis[3] = {
        {0x2176, fn_S8_1376, fn_S8_138A}, {0x2178, fn_S8_139E, fn_S8_13B6}, {0x217A, fn_S8_13EE, fn_S8_13D2}};
    for (int k = 0; k < 3; k++) {
        while ((int16_t)g16(axis[k].g) >= 2)  { axis[k].neg(&r); sg16(axis[k].g, (uint16_t)(g16(axis[k].g) - 2)); }
        while ((int16_t)g16(axis[k].g) <= -2) { axis[k].pos(&r); sg16(axis[k].g, (uint16_t)(g16(axis[k].g) + 2)); }
    }
    sg16(0x2182, (uint16_t)(g16(0x2182) + g16(0x217A)));
    sg16(0x2180, (uint16_t)(g16(0x2180) + g16(0x2178)));
    sg16(0x217E, (uint16_t)(g16(0x217E) + g16(0x2176)));
}

void fn_S2_0332(uint32_t src, uint32_t dst);

/* fn_S8_27CC: default preferences record in handle h: header fields (+0 = 0, +2..+9 = 0, +$A..$C = 1,
 * +$D = 0, +$E = 2), 8 spaces at +$10 and a 0 at +$18, and the high-score table at +$118: ten
 * $46-byte entries { Str63 name; word level at +$40; long score at +$42 }, all "G_4824" (copied via
 * a 64-byte local buffer, as in the original) with level and score 0, then the six built-in scores. */
void fn_S8_27CC(uint32_t h) {
    MZ_FRAME;
    static const struct { uint16_t name; uint16_t level; uint32_t score; } builtin[6] = {
        {0x42CA, 9, 0x53ADE}, {0x42BE, 10, 0x469E2}, {0x42AE, 8, 0x1E74E},
        {0x42A4, 5, 0xD8CC}, {0x4298, 3, 0x7A12}, {0x428A, 2, 0x1B58}};
    uint32_t buf = mz_local(0x40);
    for (uint32_t k = 0; k < 0x40; k += 4) w32(buf + k, r32(GADDR(0x4824) + k));
    mac_HLock(h);
    uint32_t p = deref(h);
    w16(p, 0);
    w8(p + 0xA, 1); w8(p + 0xB, 1); w8(p + 0xC, 1); w8(p + 0xD, 0);
    w16(p + 0xE, 2);
    w16(p + 4, 0); w16(p + 8, 0); w16(p + 2, 0); w16(p + 6, 0);
    for (int16_t i = 0; i < 10; i++) {
        uint32_t e = p + (uint32_t)(0x46 * i);
        mz_seg_of(2, 0x0332), fn_S2_0332(buf, e + 0x118);
        w16(e + 0x158, 0); w32(e + 0x15A, 0);
    }
    for (int i = 0; i < 6; i++) {
        uint32_t e = p + (uint32_t)(0x46 * i);
        mz_seg_of(2, 0x0332), fn_S2_0332(GADDR(builtin[i].name), e + 0x118);
        w16(e + 0x158, builtin[i].level); w32(e + 0x15A, builtin[i].score);
    }
    for (int16_t i = 0; i < 8; i++) w8(p + 0x10 + (uint32_t)i, 0x20);
    w8(p + 0x18, 0);
    mac_HUnlock(h);
}

void fn_S2_1CF6(Regs *r);
void fn_S2_4DC4(Regs *r);
void fn_S4_434E(Regs *r);

/* Draws full-view effect frame `entry` of model list `model` (self-relative offsets) in colour set
 * `set` with palette G_209E = `pal`. */
static void effect_frame(Regs *r, uint16_t pal, uint32_t model, uint32_t entry_off, uint16_t set) {
    sg16(0x209E, pal);
    r->a[0] = g32(model); r->a[0] += r32(r->a[0] + entry_off);
    sg32(0x20A2, r->a[0]);
    set_w(&r->d[0], g16(0x2154)); add_w(r, &r->d[0], 4); sg16(0x20A8, (uint16_t)r->d[0]);
    sg16(0x20A6, 4);
    set_w(&r->d[0], g16(0x2156)); add_w(r, &r->d[0], 4); sg16(0x20A4, (uint16_t)r->d[0]);
    sg16(0x20AA, set);
    mz_seg_of(2, 0x1CF6), fn_S2_1CF6(r);
    mz_seg_of(2, 0x4DC4), fn_S2_4DC4(r);
}

/* fn_S8_0CFE: the full-view effect sequence driven by G_20D8: 1..6 counts down showing frames
 * 0..5 of G_1D7C; 7..18 counts up showing frame (n - 7) / 2, and at 19 resets the sound state
 * (fn_S4_434E) and goes to $84; $80 shows model G_1D78 in palette G_20D6; $84 shows frame 5. */
void fn_S8_0CFE(Regs *r) {
    int16_t t = (int16_t)g16(0x20D8);
    cc_nz_w(r, (uint16_t)t);
    if (!t) return;
    cc_cmp_w(r, 7, (uint16_t)t);
    if (t < 7) {
        set_w(&r->d[0], (uint16_t)t); sub_w(r, &r->d[0], 1); lsl_w(r, &r->d[0], 2);
        effect_frame(r, 10, 0x1D7C, sxw(r->d[0]), 7);
        uint16_t v = g16(0x20D8), n = (uint16_t)(v - 1);
        sg16(0x20D8, n);
        r->ccr = (uint8_t)((v == 0 ? CCR_X | CCR_C : 0) | (n ? 0 : CCR_Z) | (n & 0x8000 ? CCR_N : 0) | (v == 0x8000 ? CCR_V : 0));
        return;
    }
    cc_cmp_w(r, 0x13, (uint16_t)t);
    if (t < 0x13) {
        set_w(&r->d[0], (uint16_t)t); sub_w(r, &r->d[0], 7); lsr_w(r, &r->d[0], 1); lsl_w(r, &r->d[0], 2);
        effect_frame(r, 10, 0x1D7C, sxw(r->d[0]), 7);
        set_w(&r->d[0], g16(0x20D8)); add_w(r, &r->d[0], 1);
        cc_cmp_w(r, 0x13, (uint16_t)r->d[0]);
        if ((int16_t)r->d[0] >= 0x13) { mz_seg_of(4, 0x434E), fn_S4_434E(r); sg16(0x20D8, 0x84); cc_nz_w(r, 0x84); }
        else { sg16(0x20D8, (uint16_t)r->d[0]); cc_nz_w(r, (uint16_t)r->d[0]); }
        return;
    }
    cc_cmp_w(r, 0x80, (uint16_t)t);
    if (t == 0x80) {
        sg16(0x209E, g16(0x20D6));
        r->a[0] = g32(0x1D78); r->a[0] += r32(r->a[0]);
        sg32(0x20A2, r->a[0]);
        set_w(&r->d[0], g16(0x2154)); add_w(r, &r->d[0], 4); sg16(0x20A8, (uint16_t)r->d[0]);
        sg16(0x20A6, 4);
        set_w(&r->d[0], g16(0x2156)); add_w(r, &r->d[0], 4); sg16(0x20A4, (uint16_t)r->d[0]);
        sg16(0x20AA, 1);
        mz_seg_of(2, 0x1CF6), fn_S2_1CF6(r);
        mz_seg_of(2, 0x4DC4), fn_S2_4DC4(r);
        return;
    }
    cc_cmp_w(r, 0x84, (uint16_t)t);
    if (t == 0x84) effect_frame(r, 10, 0x1D7C, 0x14, 7);
}

/* fn_S8_0468: puts object D0.w (64-byte records G_1DC4) back into the map: unless it is gone (+2
 * negative) it is entered in its cell's list (fn_S2_3480); a blocking object (+$E: 1 means at
 * its cell +$30) marks that cell (+$E = 1); one with a cell (+$30) is added there (fn_S2_33A4).
 * Unused in Catacombs; Mazeworld Original (fn_S8_0852) runs it when a level is entered. Keeps D3,
 * A2, A3 (LINK frame). */
void fn_S8_0468(Regs *r) {
    uint32_t d3 = r->d[3], a2 = r->a[2], a3 = r->a[3];
    set_w(&r->d[3], (uint16_t)r->d[0]); lsl_w(r, &r->d[3], 6);
    r->a[3] = g32(0x1DCC); r->a[2] = g32(0x1DC4);
    uint32_t o = r->a[2] + sxw(r->d[3]);
    set_w(&r->d[0], r16(o + 2));
    if (!((uint16_t)r->d[0] & 0x8000)) {
        r->d[0] = r->d[3];
        mz_seg_of(2, 0x3480), fn_S2_3480(r);
        set_w(&r->d[0], r16(o + 0xE));
        if ((uint16_t)r->d[0]) {
            if ((uint16_t)r->d[0] == 1) w16(o + 0xE, r16(o + 0x30));
            set_w(&r->d[1], r16(o + 0xE));
            w16(r->a[3] + sxw(r->d[1]) + 0xE, 1);
        }
        set_w(&r->d[1], r16(o + 0x30));
        if ((uint16_t)r->d[1]) { set_w(&r->d[0], (uint16_t)r->d[3]); mz_seg_of(2, 0x33A4), fn_S2_33A4(r); }
    }
    r->d[3] = d3; r->a[2] = a2; r->a[3] = a3;
    cc_nz_l(r, d3);
}
