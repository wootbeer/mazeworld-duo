/* CODE segment 7 of Mazeworld Original: its own object helpers (no Catacombs counterpart), named by
 * Original's offsets (see build/disasm-original/seg7.s). Register interfaces (game/mzasm.h). */
#include "mzasm.h"
#include "orig_names.h"
#include "fns.h"

/* fo_S7_1278 (JT 92): the word D0 into the object table G_53C8 at D1 + $12. A0 = the table. */
void fo_S7_1278(Regs *r) {
    r->a[0] = og32(OG_OBJECTS);
    w16(r->a[0] + sxw(r->d[1]) + 0x12, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fo_S7_1282 (JT 93): D0.w = the word at G_53C8 + 2 * D0 + $16, masked by D1. A0 = the table,
 * D2.w = 2 * D0. */
void fo_S7_1282(Regs *r) {
    r->a[0] = og32(OG_OBJECTS);
    set_w(&r->d[2], (uint16_t)r->d[0]);
    lsl_w(r, &r->d[2], 1);
    set_w(&r->d[0], (uint16_t)(r16(r->a[0] + sxw(r->d[2]) + 0x16) & (uint16_t)r->d[1]));
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fo_S7_155C (JT 105): the cell offset of the position G_382C / G_3828 (integer parts & $1F0,
 * combined as fn_S2_3480 does) into G_3824. D0.w = it, D1.w = its row part. */
void fo_S7_155C(Regs *r) {
    set_w(&r->d[0], (uint16_t)(og16(OG_MOVE_X) & 0x1F0)); lsl_w(r, &r->d[0], 1);
    set_w(&r->d[1], (uint16_t)(og16(OG_MOVE_Y) & 0x1F0)); lsl_w(r, &r->d[1], 6);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] | (uint16_t)r->d[1]));
    sog16(OG_MOVE_CELL, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* the index of the object at A4 in the 128 x 128 distance maps: ((G_3818 - y) & $7F) << 7 |
 * ((G_381C - x) & $7F), into D1; D0 = the x part; A0 = G_53BC */
static void dist_index(Regs *r) {
    set_w(&r->d[1], og16(OG_TARGET_Y)); sub_w(r, &r->d[1], r16(r->a[4] + OB_Y));
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] & 0x7F)); lsl_w(r, &r->d[1], 7);
    set_w(&r->d[0], og16(OG_TARGET_X)); sub_w(r, &r->d[0], r16(r->a[4] + OB_X));
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0x7F));
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    r->a[0] = og32(0x53BC);
}

/* fo_S7_14C0 (JT 100): D0.l = the distance byte of the object at A4 (G_53BC map). */
void fo_S7_14C0(Regs *r) {
    dist_index(r);
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    cc_nz_l(r, r->d[0]);
}

/* fo_S7_1488 (JT 99): D0.l = the byte of the G_53C0 map for the object at A4, or -1 where the
 * distance map (G_53BC) has 0. */
void fo_S7_1488(Regs *r) {
    dist_index(r);
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    if (!r->d[0]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    r->a[0] = og32(0x53C0);
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    cc_nz_l(r, r->d[0]);
}

/* fo_S7_1292 (JT 94): the word at G_53C8 + 2 * D0 + $16 gets the bits D1; D0.w = the new word. A0 = the
 * table, D2.w = 2 * D0. */
void fo_S7_1292(Regs *r) {
    r->a[0] = og32(OG_OBJECTS);
    set_w(&r->d[2], (uint16_t)r->d[0]);
    lsl_w(r, &r->d[2], 1);
    set_w(&r->d[0], (uint16_t)(r16(r->a[0] + sxw(r->d[2]) + 0x16) | (uint16_t)r->d[1]));
    w16(r->a[0] + sxw(r->d[2]) + 0x16, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fo_S7_1E54 (JT 116): a step along the step table at A0 (fn_S7_1540) and, unless something is in
 * the way (fn_S7_1766), taken (fn_S7_1578). */
void fo_S7_1E54(Regs *r) {
    mz_seg_of(7, 0x374C), fn_S7_374C(r);
    mz_seg_of(7, 0x3972), fn_S7_3972(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if (!(uint16_t)r->d[0]) mz_seg_of(7, 0x3784), fn_S7_3784(r);
}
/* fo_S7_1FF8 (JT 120) / fo_S7_201C (JT 121): towards direction D0 (none if negative): turned
 * (fn_S7_1516) and, facing it, a step (step table G_4F30), 1FF8 checking the way first. */
static void turn_and_step(Regs *r, int check) {
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] & 0x8000) return;
    mz_seg_of(7, 0x3722), fn_S7_3722(r);
    cc_cmp_w(r, r16(r->a[4] + OB_HEADING), (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] != r16(r->a[4] + OB_HEADING)) return;
    r->a[0] = OADDR(0x4F30);
    mz_seg_of(7, 0x374C), fn_S7_374C(r);
    if (check) {
        mz_seg_of(7, 0x3972), fn_S7_3972(r);
        cc_nz_w(r, (uint16_t)r->d[0]);
        if ((uint16_t)r->d[0]) return;
    }
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
}
void fo_S7_1FF8(Regs *r) { turn_and_step(r, 1); }
void fo_S7_201C(Regs *r) { turn_and_step(r, 0); }
/* fo_S7_226E (JT 124): D0 = -1 when the player is in the object's cell or a neighbouring one (the
 * map's +$C word, set in the player's cell), else 0. A0 = its cell in the map G_53D4. */
void fo_S7_226E(Regs *r) {
    static const int16_t near[5] = {0, -0x400, -0x20, 0x400, 0x20};
    r->a[0] = og32(OG_MAP) + r16(r->a[4] + OB_CELL);
    r->d[0] = 0;
    for (int k = 0; k < 5; k++)
        if (r16(r->a[0] + (uint32_t)(int32_t)near[k] + 0xC)) { r->d[0] = 0xFFFFFFFFu; break; }
    cc_nz_l(r, r->d[0]);
}
/* fo_S7_2038 (JT 122): D0.w = the area (1-20, level 5's parts of the map) of the point D1 = x,
 * D2 = y: rows below y $80, $100, $180 and the rest, cut by x at $80, $100, $180 and further. */
void fo_S7_2038(Regs *r) {
    uint16_t x = (uint16_t)r->d[1], y = (uint16_t)r->d[2], a;
    if (y < 0x80) a = x < 0x80 ? 1 : x < 0x100 ? 2 : x < 0x180 ? 3 : x < 0x1A0 ? (y < 0x40 ? 4 : 7)
                    : y < 0x20 ? 5 : y < 0x40 ? 6 : 8;
    else if (y < 0x100) a = x < 0x80 ? 9 : x < 0x100 ? 10 : x < 0x180 ? 11 : x < 0x1C0 ? (y < 0xB0 ? 12 : 13) : 14;
    else if (y < 0x180) a = x < 0x80 ? 15 : x < 0x100 ? 16 : x < 0x180 ? 17 : y < 0x130 ? 18 : 19;
    else a = 20;
    set_w(&r->d[0], a);
    cc_nz_w(r, a);
}
/* fo_S7_2132 (JT 123): D0.w = the heading for the next step on the way by the map's cells to cell
 * G_3814: towards the centre of the object's cell (+$E) until there ($10 / $30 / 0 / $20 as it
 * lies north / south / west / east); there -2 if it is G_3814, else the free neighbour (map +$E
 * clear) nearest G_3814 by the half-scale distance map G_53BC, the last cell (+$10) only if nothing
 * else, claimed in the map (+$10 = the cell left, +$E = the new one); -1 if none. */
void fo_S7_2132(Regs *r) {
    uint32_t o = r->a[4], map = og32(OG_MAP);
    uint16_t c = r16(o + 0xE);
    int16_t dx = (int16_t)(r16(map + sxw(c) + 0x1C) - r16(o + OB_X)), dy;
    if (dx) { set_w(&r->d[0], dx < 0 ? 0 : 0x20); return; }
    dy = (int16_t)(r16(map + sxw(c) + 0x1E) - r16(o + OB_Y));
    if (dy) { set_w(&r->d[0], dy < 0 ? 0x10 : 0x30); return; }
    uint16_t to = og16(0x3814);
    if (c == to) { r->d[0] = 0xFFFFFFFEu; return; }
    sog16(OG_TARGET_X, r16(map + sxw(to) + 0x1C)); sog16(OG_TARGET_Y, r16(map + sxw(to) + 0x1E));
    w16(map + sxw(c) + 0xE, 0);
    static const int16_t step[4] = {-0x400, -0x20, 0x20, 0x400};
    static const uint16_t head[4] = {0x10, 0, 0x20, 0x30};
    uint16_t best = 0xFFFF, bcell = 0xFFFF, bdir = 0xFFFF;
    for (int k = 0; k < 4; k++) {                           /* fn_S7_2210 */
        uint16_t n = (uint16_t)(c + step[k]);
        if (r16(map + sxw(n) + 0xE)) continue;
        uint16_t y = (uint16_t)((((uint16_t)(r16(map + sxw(n) + 0x1E) - og16(OG_TARGET_Y)) >> 1) & 0x7F) << 7);
        uint16_t x = (uint16_t)(((uint16_t)(r16(map + sxw(n) + 0x1C) - og16(OG_TARGET_X)) >> 1) & 0x7F);
        uint16_t w = (uint16_t)(r8(og32(0x53BC) + sxw((uint16_t)(y | x))) | (n == r16(o + 0x10) ? 0x100 : 0));
        if (w < best) { best = w; bcell = n; bdir = head[k]; }
    }
    if (!(bcell & 0x8000)) {
        w16(o + 0x10, r16(o + 0xE));
        w16(o + 0xE, bcell);
        w16(map + sxw(bcell) + 0xE, 1);
    } else w16(map + sxw(r16(o + 0xE)) + 0xE, 1);
    set_w(&r->d[0], bdir);
}
