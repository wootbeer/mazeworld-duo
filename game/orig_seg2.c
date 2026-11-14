/* CODE segment 2 of Mazeworld Original: its own routines (no Catacombs counterpart), named by
 * Original's offsets (see build/disasm-original/seg2.s). */
#include "mzasm.h"
#include "orig_names.h"

/* fo_S2_2AD2 (JT 26): after the player turns or moves (where Catacombs calls the empty fn_S2_3500):
 * G_56D6 = G_56D2 = $8000, and the two step bytes G_5830 / G_582F from the table at G_53DC:
 * row G_56DA (64 bytes each), columns G_56D8 & 15 and G_56D4 & 15:
 * G_5830 = -(t[r + x] + t[r + y + $20]), G_582F = -(t[r + x + $10] + t[r + y + $30]). */
void fo_S2_2AD2(Regs *r) {
    uint32_t t = og32(0x53DC);
    r->a[0] = t;
    set_w(&r->d[0], 0x8000);
    sog16(0x56D6, 0x8000); sog16(0x56D2, 0x8000);
    uint16_t row = (uint16_t)(og16(OG_PLAYER_HEADING) << 6);
    uint16_t x = (uint16_t)(row + (og16(OG_PLAYER_X) & 0xF)), y = (uint16_t)(row + (og16(OG_PLAYER_Y) & 0xF));
    set_w(&r->d[1], x); set_w(&r->d[2], y);
    uint8_t a = r8(t + sxw(x)), b = r8(t + sxw(y) + 0x20);
    uint8_t v = (uint8_t)(0 - a - b);
    sog8(0x5830, v);
    a = r8(t + sxw(x) + 0x10); b = r8(t + sxw(y) + 0x30);
    uint8_t m = (uint8_t)(0 - a);
    v = (uint8_t)(m - b);
    r->d[0] = v;                                             /* moveq #0 first: the upper bytes are 0 */
    sog8(0x582F, v);
    r->ccr = (uint8_t)(b > m ? CCR_X : 0);                   /* X from the last sub.b; move.b clears V, C */
    cc_nz_b(r, v);
}

/* ---------- fo_S2_41E8 (JT 31): Mazeworld Original's wall outline (Catacombs: fn_S2_4F1C) ----------
 * Clears the column table (G_5738 words at `table`) and draws into it the near faces of the walls
 * around the viewer. The visibility list G_53E0 for view direction G_56DA (a little-endian word
 * offset per direction, then 11-byte entries { le16 cell; bytes +2..+9; byte faces at +$A } up to
 * the cell value 100) gives cells of G_53D4 relative to G_56D0; for each existing cell (byte +0
 * non-zero), the face bits $80 / $40 that the cell has and the entry's mask lets through are drawn
 * as edges (fn_S2_52A2's Original, fn_S2_452A). Where Catacombs computes the ends with the sine
 * table, Original looks them up: an end is a (row, column) pair of bytes of the entry plus the
 * player's step bytes G_582F / G_5830 (fo_S2_2AD2); its screen x and depth come from the projection
 * table G_53EC (fn_S2_44BE / 44F4: row << 8 | column * 2, depth at column $FE; a negative column
 * mirrors x about G_5736). An end behind the viewer (negative row) is replaced by where the wall
 * meets the edge of the view: row 0 and a column from the side tables G_53D8 (rows of 128 bytes,
 * for G_56DA and G_56DA + 16) indexed by the entry's byte +8 / +9 less the position's fraction. */
void fn_S2_52A2(Regs *r);

typedef struct { int16_t x, z; } Proj;
/* fn_S2_44BE / fn_S2_44F4: the end at row `row` (0..127), column `col` (a signed byte) */
static Proj project(uint8_t row, int8_t col) {
    uint32_t t = og32(0x53EC);
    uint16_t base = (uint16_t)(row << 8);
    uint8_t c2 = (uint8_t)((col < 0 ? (uint8_t)-col : (uint8_t)col) << 1);
    int16_t x = (int16_t)r16(t + (uint32_t)(int32_t)(int16_t)(base | c2));
    Proj p;
    p.x = (int16_t)((col < 0 ? -x : x) + (int16_t)og16(0x5736));
    p.z = (int16_t)r16(t + (uint32_t)(int32_t)(int16_t)(base | 0xFE));
    return p;
}
/* the end where the wall leaves the view: row 0, the column from a side table */
static Proj side(uint32_t tab, uint8_t b, uint8_t frac) {
    uint8_t d = (uint8_t)(b - frac);
    int8_t col = (int8_t)d >= 0 ? (int8_t)r8(tab + d) : (int8_t)-(int8_t)r8(tab + (uint8_t)-d);
    return project(0, col);
}
static void edge(Regs *r, Proj a, Proj b) {
    set_w(&r->d[4], (uint16_t)a.x); set_w(&r->d[5], (uint16_t)a.z);
    set_w(&r->d[6], (uint16_t)b.x); set_w(&r->d[7], (uint16_t)b.z);
    fn_S2_52A2(r);
}
void fo_S2_41E8(uint32_t table) {
    MZ_FRAME;
    { uint32_t a = table; uint16_t n = og16(0x5738); do { w16(a, 0); a += 2; } while (--n); }
    uint32_t fr = mz_local(0x40), a6 = fr + 0x30;             /* the edge routine finds the table at A6+8 */
    w32(a6 + 8, table);
    Regs r = {{0}, {0}, 0};
    r.a[6] = a6;
    uint16_t dir = og16(OG_PLAYER_HEADING);
    uint32_t e = og32(0x53E0), i2 = (uint32_t)(uint16_t)(dir << 1);
    e += (uint32_t)(r8(e + i2 + 1) << 8 | r8(e + i2)) + 0x80;
    uint32_t row24 = og32(0x53D8) + ((uint32_t)dir << 7), row28 = og32(0x53D8) + ((uint32_t)((dir + 0x10) & 0x3F) << 7);
    uint8_t fx = (uint8_t)(og16(OG_PLAYER_X) & 0xF), fy = (uint8_t)(og16(OG_PLAYER_Y) & 0xF);
    uint8_t sy = og8(0x582F), sx = og8(0x5830);
    for (uint16_t cellw = (uint16_t)(r8(e + 1) << 8 | r8(e)); cellw != 100; e += 0xB, cellw = (uint16_t)(r8(e + 1) << 8 | r8(e))) {
        uint16_t off = (uint16_t)(cellw + og16(OG_PLAYER_CELL));
        if (off & 0x8000) continue;
        uint8_t b = r8(og32(OG_MAP) + off);
        if (!b) continue;
        uint8_t faces = (uint8_t)(((uint8_t)((r8(e + 0xA) ^ b) << 2) & b) & 0xC0);
        if (!faces) continue;
        uint8_t r7 = (uint8_t)(r8(e + 7) + sy), r3 = (uint8_t)(r8(e + 3) + sy), r5 = (uint8_t)(r8(e + 5) + sy);
        int8_t c6 = (int8_t)(r8(e + 6) + sx), c2 = (int8_t)(r8(e + 2) + sx), c4 = (int8_t)(r8(e + 4) + sx);
        Proj p1, p2;
        if (faces & 0x80) {                                   /* the face along x: ends 6/7 and 2/3 */
            if (!(r7 & 0x80)) {
                p1 = project(r7, c6);
                p2 = (r3 & 0x80) ? side(row24, r8(e + 8), fy) : project(r3, c2);
                edge(&r, p1, p2);
                if (!(faces & 0x40)) continue;
            } else {
                if (r3 & 0x80) { if (!(faces & 0x40)) continue; goto from5; }
                p2 = project(r3, c2);
                p1 = side(row24, r8(e + 8), fy);
                edge(&r, p1, p2);
                if (!(faces & 0x40)) continue;
                goto from5;
            }
        } else {                                              /* only the face along y: from end 6/7 */
            if (r7 & 0x80) goto from5;
            p1 = project(r7, c6);
        }
        /* the face along y: ends 6/7 and 4/5 */
        p2 = (r5 & 0x80) ? side(row28, r8(e + 9), fx) : project(r5, c4);
        edge(&r, p1, p2);
        continue;
    from5:                                                    /* end 6/7 behind: from where it leaves the view */
        if (r5 & 0x80) continue;
        p2 = project(r5, c4);
        p1 = side(row28, r8(e + 9), fx);
        edge(&r, p1, p2);
    }
}

/* ---------- Mazeworld Original's panel ---------- */
/* the panel's origin: the window offset G_5744 / G_5742, or 0, 0 in the other window (G_57D0) */
static void panel_origin(int16_t *h, int16_t *v) {
    if (og8(0x57D0)) { *h = 0; *v = 0; }
    else { *h = (int16_t)og16(0x5744); *v = (int16_t)og16(0x5742); }
}
static void rect(uint32_t r, int16_t top, int16_t left, int16_t bottom, int16_t right) {
    w16(r, (uint16_t)top); w16(r + 2, (uint16_t)left); w16(r + 4, (uint16_t)bottom); w16(r + 6, (uint16_t)right);
}
static void bump(uint32_t r, int top) {                    /* inset: left + 1, right - 1 (and top + 1) */
    if (top) w16(r, (uint16_t)(r16(r) + 1));
    w16(r + 2, (uint16_t)(r16(r + 2) + 1)); w16(r + 6, (uint16_t)(r16(r + 6) - 1));
}

/* fo_S2_35FC (JT 35): when G_56EC is set, the panel's frames: around the view (G_5748 x G_5746
 * from the panel's origin, outer edge one pixel out) and a double frame around the box
 * G_5740..G_573E x G_573C..G_573A (colours 2 and 3). */
void fo_S2_35FC(void) {
    MZ_FRAME;
    if (!og8(0x56EC)) return;
    int16_t h, v; panel_origin(&h, &v);
    uint32_t r = mz_local(8);
    mac_PmForeColor(2);
    rect(r, (int16_t)(v - 1), (int16_t)(h - 1), (int16_t)(v + og16(0x5746) + 1), (int16_t)(h + og16(0x5748) + 1));
    mac_FrameRect(r);
    rect(r, (int16_t)(v + og16(0x573C) - 2), (int16_t)(h + og16(0x5740) - 2), (int16_t)(v + og16(0x573A) + 2), (int16_t)(h + og16(0x573E) + 2));
    mac_FrameRect(r);
    mac_PmForeColor(3);
    bump(r, 1); w16(r + 4, (uint16_t)(r16(r + 4) - 1));
    mac_FrameRect(r);
    sog8(0x56EC, 0);
}

/* fo_S2_36C2 / fo_S2_3A0E (JT 37 / 38): the two upright gauges, value G_56EA / G_56E6 out of $8E,
 * G_571C high (last drawn G_5250 / G_524E; redrawn whole when G_56EB / G_56E8 is set), G_571E wide:
 * the first G_5722 in from the panel's left, the second as far in from the right (G_5748). A gauge
 * is a column of colour 5 in frames of colours 2 and 4 (its top two pixels); a change paints the
 * difference (growing: the new column; shrinking: colour 0) and puts the top frame on again. */
static int16_t gauge_len(uint16_t value) {
    int16_t full = (int16_t)og16(0x571C);
    int16_t len = (int16_t)((int32_t)(int16_t)value * full / 0x8E);   /* fn_S1_014E: signed divide */
    return len > full ? full : len < 0 ? 0 : len;
}
static void gauge(uint16_t dirty, uint16_t value, uint16_t last, int right) {
    MZ_FRAME;
    uint32_t r = mz_local(8);
    int16_t h, v, left, rgt;
    if (!og8(dirty) && og16(value) == og16(last)) return;
    panel_origin(&h, &v);
    if (right) { rgt = (int16_t)(h + og16(0x5748) - og16(0x5722)); left = (int16_t)(rgt - og16(0x571E)); }
    else { left = (int16_t)(h + og16(0x5722)); rgt = (int16_t)(left + og16(0x571E)); }
    int16_t base = (int16_t)(v + og16(0x5720) + og16(0x571C)), now = gauge_len(og16(value));
    if (og8(dirty)) {
        rect(r, (int16_t)(base - now), left, base, rgt);
        if ((int16_t)r16(r) < base) { mac_PmForeColor(2); mac_FrameRect(r); }
        bump(r, 1);
        if ((int16_t)r16(r) < base) { mac_PmForeColor(4); mac_FrameRect(r); }
        bump(r, 1);
        if ((int16_t)r16(r) < base) { mac_PmForeColor(5); mac_PaintRect(r); }
        sog8(dirty, 0);
        sog16(last, og16(value));
        return;
    }
    int16_t was = gauge_len(og16(last));
    if (now > was) {
        rect(r, now - 2 > 0 ? (int16_t)(base - now + 2) : base, left, was - 2 > 0 ? (int16_t)(base - was + 2) : base, rgt);
        mac_PmForeColor(2); mac_FrameRect(r); bump(r, 0);
        mac_PmForeColor(4); mac_FrameRect(r); bump(r, 0);
        mac_PmForeColor(5); mac_PaintRect(r);
    } else {
        rect(r, (int16_t)(base - was), left, (int16_t)(base - now), rgt);
        mac_PmForeColor(0); mac_PaintRect(r);
    }
    int16_t top = (int16_t)(base - now), bottom = (int16_t)(top + 2);
    if (bottom > base) bottom = base;
    rect(r, top, left, bottom, rgt);
    mac_PmForeColor(2); mac_FrameRect(r);
    bump(r, 1);
    if ((int16_t)r16(r) < bottom) { mac_PmForeColor(4); mac_FrameRect(r); }
    sog16(last, og16(value));
}
void fo_S2_36C2(void) { gauge(0x56EB, 0x56EA, 0x5250, 0); }
void fo_S2_3A0E(void) { gauge(0x56E8, 0x56E6, 0x524E, 1); }

/* fo_S2_3F44: one of the four three-letter labels, drawn a letter at a time down a column: 1 "crt"
 * and 3 "aff" in column G_570E, 2 "dst" and 4 "neg" in column G_570C, at their rows */
static void label(int16_t which) {
    static const struct { uint16_t col; uint16_t row[3]; char s[4]; } l[4] = {
        {0x570E, {0x570A, 0x5708, 0x5706}, "crt"}, {0x570C, {0x5704, 0x5702, 0x5700}, "dst"},
        {0x570E, {0x56FE, 0x56FC, 0x56FA}, "aff"}, {0x570C, {0x56F8, 0x56F6, 0x56F4}, "neg"}};
    if (which < 1 || which > 4) return;
    int16_t h, v; panel_origin(&h, &v);
    /* the rows are closer than the letters are tall, and each letter (srcCopy) rubs out the bottom
     * of the one above; the port's option draws them srcOr, whole */
    if (mz_full_labels) mac_TextMode(1);
    for (int k = 0; k < 3; k++) {
        mac_MoveTo((int16_t)(og16(l[which - 1].col) + h), (int16_t)(og16(l[which - 1].row[k]) + v));
        mac_DrawChar((int16_t)l[which - 1].s[k]);
    }
    if (mz_full_labels) mac_TextMode(0);
}
/* fo_S2_3E04 (JT 40): the attitude G_56E2 (last drawn G_524A; all when G_56E3): its label is
 * rubbed out (colour 0) and the new one drawn in its colour; 0 and 5 have none */
void fo_S2_3E04(void) {
    static const int16_t which[10] = {0, 1, 2, 3, 4, 0, 1, 2, 3, 4};
    static const int16_t colour[10] = {0, 0xE, 0xA, 8, 6, 0, 0xF, 0xB, 9, 7};
    if (!og8(0x56E3) && og16(0x56E2) == og16(0x524A)) return;
    mac_TextFont((int16_t)og16(0x5816));
    mac_TextSize((int16_t)og16(0x5710));
    mac_PmForeColor(0);
    uint16_t was = og16(0x524A), now = og16(0x56E2);
    if (was <= 9) label(which[was]);
    if (now <= 9 && which[now]) { mac_PmForeColor(colour[now]); label(which[now]); }
    sog8(0x56E3, 0);
    sog16(0x524A, og16(0x56E2));
}

/* fo_S2_3D6E (JT 39): when G_56E4 is set, the message box G_5718..G_5714 x G_5716..G_5712 (from
 * the panel's origin) gets text G_56BE (fo_S6_0004), white on black, in font G_5816 size G_571A. */
void fo_S6_0004(Regs *r);
void fo_S2_3D6E(void) {
    MZ_FRAME;
    if (!og8(0x56E4)) return;
    int16_t h, v; panel_origin(&h, &v);
    uint32_t box = mz_local(8);
    rect(box, (int16_t)(og16(0x5718) + v), (int16_t)(og16(0x5716) + h), (int16_t)(og16(0x5714) + v), (int16_t)(og16(0x5712) + h));
    Regs r = {{0}, {0}, 0};
    set_w(&r.d[0], og16(0x56BE));
    mz_seg(6), fo_S6_0004(&r);
    mac_PmForeColor(0xF); mac_PmBackColor(0);
    mac_TextSize((int16_t)og16(0x571A)); mac_TextFont((int16_t)og16(0x5816));
    mac_TETextBox(r.a[0], (int32_t)r.d[0], box, 0);
    sog8(0x56E4, 0);
}
