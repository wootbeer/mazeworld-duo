/* CODE segment 7: functions reimplemented so far (see build/disasm/seg7.s). Segment 7 is almost all
 * hand-written assembly with register interfaces (game/mzasm.h). */
#include "mzasm.h"
#include "fns.h"

void fn_S7_008A(Regs *r);

void fn_S7_3ACA(Regs *r);

void fn_S2_3410(Regs *r);
void fn_S7_3972(Regs *r);
void fn_S7_37C4(Regs *r);
void fn_S7_3784(Regs *r);
void fn_S7_36F4(Regs *r);

void fn_S2_3480(Regs *r);
void fn_S7_374C(Regs *r);

/* fn_S7_367E: D0.w = distance of a 6-bit value from 0 around a 64-step circle:
 * v = D0 & $3F; if v >= 32 then v = 64 - v; result & $3F (so 32 stays 32). */
void fn_S7_367E(Regs *r) {
    uint16_t v = (uint16_t)r->d[0] & 0x3F;
    if (v & 0x20) { v = (uint16_t)((v ^ 0x3F) + 1); r->ccr &= (uint8_t)~CCR_X; }   /* addq: no carry, X cleared */
    v &= 0x3F;
    set_w(&r->d[0], v); cc_nz_w(r, v);
}

/* fn_S7_3D0E: moves a marker in the G_1DCC block: clears the word at +$E of the cell whose offset is
 * the word at A4+$E, sets the word at +$E of cell G_00D2 to 1, and stores G_00D2 at A4+$E.
 * Out: A0 = G_1DCC, D0.w = G_00D2. */
void fn_S7_3D0E(Regs *r) {
    r->a[0] = g32(0x1DCC);
    set_w(&r->d[0], r16(r->a[4] + 0xE));
    w16(r->a[0] + (uint32_t)(int32_t)(int16_t)r->d[0] + 0xE, 0);
    set_w(&r->d[0], g16(0x00D2));
    w16(r->a[0] + (uint32_t)(int32_t)(int16_t)r->d[0] + 0xE, 1);
    w16(r->a[4] + 0xE, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fn_S7_404A: D0.l = -1 if the word at +2 of $40-byte record D0.b (base A0) is in $17..$1A, else 0.
 * D1.w = that word. */
void fn_S7_404A(Regs *r) {
    set_w(&r->d[0], (uint16_t)r->d[0] & 0xFF);
    lsl_w(r, &r->d[0], 6);
    set_w(&r->d[1], r16(r->a[0] + (uint32_t)(int32_t)(int16_t)r->d[0] + 2));
    uint16_t v = (uint16_t)r->d[1];
    r->d[0] = (v >= 0x17 && v < 0x1B) ? 0xFFFFFFFFu : 0;
    cc_nz_l(r, r->d[0]);
}

/* fn_S7_36F4: turns the 6-bit direction at A4+$C one step towards D0.w (the shorter way round,
 * by the sign bit of the 6-bit difference). D1.w = the new direction (or 0 if already there). */
void fn_S7_36F4(Regs *r) {
    uint16_t cur = r16(r->a[4] + 0xC), diff = (uint16_t)((uint16_t)r->d[0] - cur);
    set_w(&r->d[1], diff);
    if (diff == 0) { r->ccr = CCR_Z; return; }   /* sub.w of equal values */
    uint16_t v; int x;
    if (!(diff & 0x20)) { v = (uint16_t)(cur + 1); x = cur == 0xFFFF; }
    else { v = (uint16_t)(cur - 1); x = cur == 0; }
    v &= 0x3F;
    set_w(&r->d[1], v); w16(r->a[4] + 0xC, v);
    r->ccr = (uint8_t)(x ? CCR_X : 0); cc_nz_w(r, v);
}

/* fn_S7_374C: position of the object at A4 moved by entry D0.w of the step tables at A0
 * (A0+$40: longs added to A4+4, A0: longs added to A4+8). Results in G_00EA and G_00E6, and the
 * cell offset made from their integer parts in G_00E2 (as fn_S2_3480 computes for records).
 * D0.w = the cell offset, D1.w = its row part. */
void fn_S7_374C(Regs *r) {
    set_w(&r->d[1], (uint16_t)r->d[0]);
    lsl_w(r, &r->d[1], 2);
    uint32_t idx = (uint32_t)(int32_t)(int16_t)r->d[1];
    uint32_t a = r32(r->a[4] + 4), b = r32(r->a[0] + 0x40 + idx);
    r->d[0] = a + b; sg32(0xEA, r->d[0]);
    a = r32(r->a[4] + 8); b = r32(r->a[0] + idx);
    r->d[0] = a + b; sg32(0xE6, r->d[0]);
    set_w(&r->d[0], (uint16_t)(g16(0xEA) & 0x1F0)); lsl_w(r, &r->d[0], 1);
    set_w(&r->d[1], (uint16_t)(g16(0xE6) & 0x1F0)); lsl_w(r, &r->d[1], 6);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] | (uint16_t)r->d[1]));
    sg16(0xE2, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

void fn_S2_33A4(Regs *r);
void fn_S2_3410(Regs *r);

/* fn_S7_3784: commits the move computed by fn_S7_374C for the object at A4. If the cell changes,
 * the object's id (G_00F2) is added to the new cell's list (fn_S2_33A4) and removed from the old
 * one's (fn_S2_3410); a full cell list refuses the move. D0.l = 0 if moved, -1 if refused. */
void fn_S7_3784(Regs *r) {
    set_w(&r->d[0], g16(0xE2));
    if ((uint16_t)r->d[0] != r16(r->a[4] + 0x30)) {
        set_w(&r->d[0], g16(0xF2)); set_w(&r->d[1], g16(0xE2));
        mz_seg_of(2, 0x33A4), fn_S2_33A4(r);
        if ((uint16_t)r->d[0]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
        set_w(&r->d[0], g16(0xF2)); set_w(&r->d[1], r16(r->a[4] + 0x30));
        mz_seg_of(2, 0x3410), fn_S2_3410(r);
    }
    w32(r->a[4] + 4, g32(0xEA)); w32(r->a[4] + 8, g32(0xE6)); w16(r->a[4] + 0x30, g16(0xE2));
    r->d[0] = 0; cc_nz_l(r, 0);
}

void fn_S2_3532(Regs *r);

/* fn_S7_3FA6: a local step of the routine whose frame is at A6 (A2 = cell table, D3.w = cell offset,
 * D4.w, A4 = object). If the cell's word at +$E is clear, rolls a weight (random & 3, plus 4 for the
 * object's own cell at A4+$10, else plus 8) and keeps the cell as the best so far when the weight is
 * at least the frame's -$E: stores weight, D3, D4 at -$E, -$10, -$12. */
void fn_S7_3FA6(Regs *r) {
    set_w(&r->d[0], r16(r->a[2] + sxw(r->d[3]) + 0xE));
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) return;
    int own = (uint16_t)r->d[3] == r16(r->a[4] + 0x10);
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)(((uint16_t)r->d[0] & 3) | (own ? 4 : 8)));
    uint16_t best = r16(r->a[6] - 0xE);
    cc_cmp_w(r, best, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] < best) return;
    w16(r->a[6] - 0xE, (uint16_t)r->d[0]); w16(r->a[6] - 0x10, (uint16_t)r->d[3]); w16(r->a[6] - 0x12, (uint16_t)r->d[4]);
    cc_nz_w(r, (uint16_t)r->d[4]);
}

/* fn_S7_1CC2: D0 = the byte of the 128x128 map G_1DBC at the player's position relative to the
 * object at A4 (player G_20CE/G_20D2 plus twice G_20C4/G_20C8, minus the object's +8/+4; integer
 * parts & 127), or D0.w = G_00EE when G_00EC is $30. */
void fn_S7_1CC2(Regs *r) {
    set_w(&r->d[0], g16(0xEC));
    cc_cmp_w(r, 0x30, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] == 0x30) { set_w(&r->d[0], g16(0xEE)); cc_nz_w(r, (uint16_t)r->d[0]); return; }
    r->d[1] = g32(0x20CE); r->d[2] = g32(0x20C4); lsl_l1(r, &r->d[2]);
    add_l(r, &r->d[1], r->d[2]); sub_l(r, &r->d[1], r32(r->a[4] + 8));
    r->d[1] = swap(r->d[1]);
    set_w(&r->d[1], (uint16_t)r->d[1] & 0x7F); lsl_w(r, &r->d[1], 7);
    r->d[0] = g32(0x20D2); r->d[2] = g32(0x20C8); lsl_l1(r, &r->d[2]);
    add_l(r, &r->d[0], r->d[2]); sub_l(r, &r->d[0], r32(r->a[4] + 4));
    r->d[0] = swap(r->d[0]);
    set_w(&r->d[0], (uint16_t)r->d[0] & 0x7F);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    r->a[0] = g32(0x1DBC);
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    cc_nz_b(r, (uint8_t)r->d[0]);
}

/* fn_S7_3A7A: is object D0.b (a $40-byte record in G_1DC4, not the current object G_00F2, with a
 * non-negative word at +$32) close enough? Looks up the 128x128 table G_1DB8 at the offset between
 * it and the position G_00EA/G_00E6; if that value minus both objects' +$32 is negative, D0.w = the
 * record offset, else D0.l = 0. A0 = G_1DC4, D2.w = record offset. */
void fn_S7_3A7A(Regs *r) {
    r->a[0] = g32(0x1DC4);
    set_b(&r->d[2], (uint8_t)r->d[0]);
    set_w(&r->d[2], (uint16_t)r->d[2] & 0xFF);
    lsl_w(r, &r->d[2], 6);
    uint32_t rec = r->a[0] + sxw(r->d[2]);
    cc_cmp_w(r, g16(0xF2), (uint16_t)r->d[2]);
    if ((uint16_t)r->d[2] == g16(0xF2)) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    set_w(&r->d[0], r16(rec + 0x32));
    if ((uint16_t)r->d[0] & 0x8000) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    set_w(&r->d[1], r16(rec + 8)); sub_w(r, &r->d[1], g16(0xE6));
    set_w(&r->d[1], (uint16_t)r->d[1] & 0x7F); lsl_w(r, &r->d[1], 7);
    set_w(&r->d[0], r16(rec + 4)); sub_w(r, &r->d[0], g16(0xEA));
    set_w(&r->d[0], (uint16_t)r->d[0] & 0x7F);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    r->a[1] = g32(0x1DB8);
    r->d[0] = r8(r->a[1] + sxw(r->d[1]));
    sub_w(r, &r->d[0], r16(r->a[4] + 0x32));
    sub_w(r, &r->d[0], r16(rec + 0x32));
    if (!(r->ccr & CCR_N)) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    set_w(&r->d[0], (uint16_t)r->d[2]); cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fn_S7_3CBC: a local step of the routine whose frame is at A6 (A2 = records, D3.w = record offset,
 * D4.w): the value in the 128x128 table G_1DB8 at the offset between the record's +$1C/+$1E and
 * G_00DA/G_00D6; if below the frame's -$12, stores it with D3 and D4 at -$12, -$14, -$16. */
void fn_S7_3CBC(Regs *r) {
    uint32_t rec = r->a[2] + sxw(r->d[3]);
    set_w(&r->d[1], r16(rec + 0x1E)); sub_w(r, &r->d[1], g16(0xD6));
    set_w(&r->d[1], (uint16_t)r->d[1] & 0x7F); lsl_w(r, &r->d[1], 7);
    set_w(&r->d[0], r16(rec + 0x1C)); sub_w(r, &r->d[0], g16(0xDA));
    set_w(&r->d[0], (uint16_t)r->d[0] & 0x7F);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    r->a[0] = g32(0x1DB8);
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    uint16_t best = r16(r->a[6] - 0x12);
    cc_cmp_w(r, best, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] >= best) return;
    w16(r->a[6] - 0x12, (uint16_t)r->d[0]); w16(r->a[6] - 0x14, (uint16_t)r->d[3]); w16(r->a[6] - 0x16, (uint16_t)r->d[4]);
    cc_nz_w(r, (uint16_t)r->d[4]);
}

/* fn_S7_3FF0: D0.l = -1 if any of the (up to 5) objects listed in the cell of the object at A4
 * (cell offset A4+$30, list bytes +1..+5 in G_1DCC, zero-terminated) passes fn_S7_404A, else 0.
 * A0 = G_1DC4, A1 = G_1DCC, D2.w = the cell offset. */
void fn_S7_3FF0(Regs *r) {
    r->a[0] = g32(0x1DC4); r->a[1] = g32(0x1DCC);
    set_w(&r->d[2], r16(r->a[4] + 0x30));
    for (int k = 1; k <= 5; k++) {
        set_b(&r->d[0], r8(r->a[1] + sxw(r->d[2]) + (uint32_t)k));
        if (!(uint8_t)r->d[0]) break;
        fn_S7_404A(r);
        if ((uint16_t)r->d[0]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    }
    r->d[0] = 0; cc_nz_l(r, 0);
}

/* fn_S7_390E: like fn_S7_3A7A for object D0.b, with field +$34 and also excluding object G_00DE:
 * G_00DC = the G_1DBC map byte at the offset to G_00EA/G_00E6; D0.w = record offset if the G_1DB8
 * value minus G_00E0 and the object's +$34 is negative, else D0.l = 0. */
void fn_S7_390E(Regs *r) {
    r->a[0] = g32(0x1DC4);
    set_b(&r->d[2], (uint8_t)r->d[0]);
    set_w(&r->d[2], (uint16_t)r->d[2] & 0xFF);
    lsl_w(r, &r->d[2], 6);
    uint32_t rec = r->a[0] + sxw(r->d[2]);
    uint16_t d2 = (uint16_t)r->d[2];
    cc_cmp_w(r, g16(0xF2), d2);
    if (d2 == g16(0xF2)) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    cc_cmp_w(r, g16(0xDE), d2);
    if (d2 == g16(0xDE)) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    set_w(&r->d[0], r16(rec + 0x34));
    if (!(uint16_t)r->d[0]) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    map_offset(r, r16(rec + 8), g16(0xE6), r16(rec + 4), g16(0xEA));
    r->a[1] = g32(0x1DBC); r->d[0] = r8(r->a[1] + sxw(r->d[1])); sg16(0xDC, (uint16_t)r->d[0]);
    r->a[1] = g32(0x1DB8); r->d[0] = r8(r->a[1] + sxw(r->d[1]));
    sub_w(r, &r->d[0], g16(0xE0));
    sub_w(r, &r->d[0], r16(rec + 0x34));
    if (!(r->ccr & CCR_N)) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    set_w(&r->d[0], d2); cc_nz_w(r, d2);
}

/* fn_S7_3ACA: is the point G_00EA/G_00E6 within 8 cells of the player (G_20D2/G_20CE) and, unless
 * G_20D8 >= $80, close by the G_1DB8 map? G_00DC = the G_1DBC map byte there. D0.l = -1 if the
 * G_1DB8 value minus G_00E0 is negative, else 0. */
void fn_S7_3ACA(Regs *r) {
    set_w(&r->d[0], g16(0x20D8)); cc_cmp_w(r, 0x80, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] >= 0x80) goto no;
    set_w(&r->d[0], g16(0xEA)); sub_w(r, &r->d[0], g16(0x20D2)); add_w(r, &r->d[0], 8);
    cc_cmp_w(r, 0x11, (uint16_t)r->d[0]); if ((uint16_t)r->d[0] >= 0x11) goto no;
    set_w(&r->d[0], g16(0xE6)); sub_w(r, &r->d[0], g16(0x20CE)); add_w(r, &r->d[0], 8);
    cc_cmp_w(r, 0x11, (uint16_t)r->d[0]); if ((uint16_t)r->d[0] >= 0x11) goto no;
    map_offset(r, g16(0x20CE), g16(0xE6), g16(0x20D2), g16(0xEA));
    r->a[1] = g32(0x1DBC); r->d[0] = r8(r->a[1] + sxw(r->d[1])); sg16(0xDC, (uint16_t)r->d[0]);
    r->a[1] = g32(0x1DB8); r->d[0] = r8(r->a[1] + sxw(r->d[1]));
    sub_w(r, &r->d[0], g16(0xE0));
    if (r->ccr & CCR_N) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
no:
    r->d[0] = 0; cc_nz_l(r, 0);
}

/* fn_S7_3128: the player's position relative to the object at A4: if within 63 cells both ways (and
 * G_20D8 < $80), G_00EC / G_00EE = the G_1DB8 / G_1DBC map bytes there and D0.l = 0; otherwise both
 * are -1 and D0.l = -1. */
void fn_S7_3128(Regs *r) {
    set_w(&r->d[0], g16(0x20D8)); cc_cmp_w(r, 0x80, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] >= 0x80) goto far;
    set_w(&r->d[0], g16(0x20D2)); sub_w(r, &r->d[0], r16(r->a[4] + 4)); add_w(r, &r->d[0], 0x3F);
    cc_cmp_w(r, 0x7E, (uint16_t)r->d[0]); if ((uint16_t)r->d[0] >= 0x7E) goto far;
    set_w(&r->d[0], g16(0x20CE)); sub_w(r, &r->d[0], r16(r->a[4] + 8)); add_w(r, &r->d[0], 0x3F);
    cc_cmp_w(r, 0x7E, (uint16_t)r->d[0]); if ((uint16_t)r->d[0] >= 0x7E) goto far;
    map_offset(r, g16(0x20CE), r16(r->a[4] + 8), g16(0x20D2), r16(r->a[4] + 4));
    r->a[0] = g32(0x1DB8); r->d[0] = r8(r->a[0] + sxw(r->d[1])); sg16(0xEC, (uint16_t)r->d[0]);
    r->a[0] = g32(0x1DBC); r->d[0] = r8(r->a[0] + sxw(r->d[1])); sg16(0xEE, (uint16_t)r->d[0]);
    r->d[0] = 0; cc_nz_l(r, 0);
    return;
far:
    r->d[0] = 0xFFFFFFFFu; sg16(0xEC, 0xFFFF); sg16(0xEE, 0xFFFF); cc_nz_w(r, 0xFFFF);
}

/* fn_S7_008A: two passes over the 1024 $20-byte cells of G_1DCC (a 32-wide grid). Pass 1: for each
 * cell with bit 0 of byte +$12 set, each of the four neighbours (+-1 cell, +-1 row) whose byte +0
 * has bit 0 clear gets bit 1 set in its byte +$12. Pass 2: every cell with bit 1 of byte +$12 set
 * has it replaced by bit 0. Out: A0 = G_1DCC + $8000, D2.w = 0, D0.b = the last byte changed. */
void fn_S7_008A(Regs *r) {
    static const int32_t nb[4] = {0x20, 0x400, -0x20, -0x400};
    r->a[0] = g32(0x1DCC);
    set_w(&r->d[2], 0x400);
    do {
        if (r8(r->a[0] + 0x12) & 1) {
            for (int k = 0; k < 4; k++) {
                uint32_t n = r->a[0] + (uint32_t)nb[k];
                if (!(r8(n) & 1)) { uint8_t v = (uint8_t)(r8(n + 0x12) | 2); w8(n + 0x12, v); }
            }
        }
        r->a[0] += 0x20;
        set_w(&r->d[2], (uint16_t)((uint16_t)r->d[2] - 1));
    } while ((uint16_t)r->d[2]);
    r->a[0] = g32(0x1DCC);
    set_w(&r->d[2], 0x400);
    do {
        if (r8(r->a[0] + 0x12) & 2) {
            set_b(&r->d[0], (uint8_t)((r8(r->a[0] + 0x12) & 0xFD) | 1));
            w8(r->a[0] + 0x12, (uint8_t)r->d[0]);
        }
        r->a[0] += 0x20;
        set_w(&r->d[2], (uint16_t)((uint16_t)r->d[2] - 1));
    } while ((uint16_t)r->d[2]);
    r->ccr = CCR_Z;   /* subq.w 1 -> 0: Z, no borrow (X clear) */
}

/* fn_S7_40D6: spawns an object of type D0.w with heading D1.w and value D2.w one step ahead of the
 * object at A4 (fn_S7_374C with direction D1), in the highest free record of G_1DC4, added to the
 * list of its cell. D0.l = 0 and D1.w = the record offset on success; D0.l = -1 (D1 = 0 if the cell
 * was full) otherwise. Fields: +2 type, +4/+8 position (integer parts, fractions $8000), +$C heading,
 * +$10 value, +$E 0, +$14 0, +$12 -1. Saves and restores D4 (its LINK frame is dead after return). */
void fn_S7_40D6(Regs *r) {
    uint32_t d4 = r->d[4];
    uint16_t type = (uint16_t)r->d[0], heading = (uint16_t)r->d[1], value = (uint16_t)r->d[2];
    set_w(&r->d[0], heading);
    fn_S7_374C(r);
    r->a[0] = g32(0x1DC4);
    set_w(&r->d[1], 0x1DC0);
    for (;;) {
        set_w(&r->d[2], r16(r->a[0] + sxw(r->d[1]) + 2));
        cc_cmp_w(r, 0xFFFF, (uint16_t)r->d[2]);
        if ((uint16_t)r->d[2] == 0xFFFF) break;
        uint16_t v = (uint16_t)r->d[1];
        sub_w(r, &r->d[1], 0x40);
        if (v == 0x40) { r->d[0] = 0xFFFFFFFFu; goto out; }
    }
    r->d[4] = (uint16_t)r->d[1];
    set_w(&r->d[0], (uint16_t)r->d[4]);
    set_w(&r->d[1], g16(0xE2));
    mz_seg_of(2, 0x33A4), fn_S2_33A4(r);
    if ((uint16_t)r->d[0]) { r->d[0] = 0xFFFFFFFFu; r->d[1] = 0; goto out; }
    r->a[0] = g32(0x1DC4) + r->d[4];
    w16(r->a[0] + 2, type); w16(r->a[0] + 4, g16(0xEA)); w16(r->a[0] + 8, g16(0xE6));
    w16(r->a[0] + 6, 0x8000); w16(r->a[0] + 0xA, 0x8000); w16(r->a[0] + 0xE, 0);
    w16(r->a[0] + 0xC, heading); w16(r->a[0] + 0x10, value); w16(r->a[0] + 0x14, 0); w16(r->a[0] + 0x12, 0xFFFF);
    set_w(&r->d[0], (uint16_t)r->d[4]);
    mz_seg_of(2, 0x3480), fn_S2_3480(r);
    r->d[0] = 0; set_w(&r->d[1], (uint16_t)r->d[4]);
out:
    r->d[4] = d4;
    cc_nz_l(r, d4);   /* flags from restoring D4 */
}

/* fn_S7_3858 / fn_S7_39C4: run fn_S7_390E / fn_S7_3A7A on each object listed in cell D0.w of
 * G_1DCC (bytes +1..+10, zero-terminated; a negative D0 means no cell). D0 = the first non-zero
 * result, else D0.l = 0. D1.w = the cell. A2 is saved and restored (LINK frame). */
static void scan_cell(Regs *r, void (*test)(Regs *)) {
    uint32_t a2 = r->a[2];
    set_w(&r->d[1], (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[1]);
    if ((uint16_t)r->d[0] & 0x8000) goto none;
    r->d[0] &= 0xFFFF;
    r->a[2] = g32(0x1DCC) + r->d[0];
    for (int k = 1; k <= 10; k++) {
        set_b(&r->d[0], r8(r->a[2] + (uint32_t)k));
        if (!(uint8_t)r->d[0]) goto none;
        test(r);
        cc_nz_w(r, (uint16_t)r->d[0]);
        if ((uint16_t)r->d[0]) goto out;
    }
none:
    r->d[0] = 0; cc_nz_l(r, 0);
out:
    r->a[2] = a2;
}
void fn_S7_3858(Regs *r) { scan_cell(r, fn_S7_390E); }
void fn_S7_39C4(Regs *r) { scan_cell(r, fn_S7_3A7A); }

/* fn_S7_3EE2: chooses the next cell for the object at A4 heading for the cell A4+$E (in G_1DCC).
 * While the object is not yet at that cell's centre (words +$1C/+$1E vs the object's +4/+8), D0.w
 * is the direction to it: $20 / $00 when the cell's x is greater / less, else $30 / $10 by y. Once there, the
 * cell's marker +$E is cleared and the four neighbours are weighed by fn_S7_3FA6 (north $10, west 0,
 * east $20, south $30); the best becomes the new target (the old one goes to A4+$10) and is marked,
 * or the current cell is re-marked if none is free. D0.w = the chosen direction (-1 if none).
 * Keeps D3, D4, A2 (LINK frame; fn_S7_3FA6 works on its locals -$E, -$10, -$12). */
void fn_S7_3EE2(Regs *r) {
    uint32_t d3 = r->d[3], d4 = r->d[4], a2 = r->a[2], a6 = r->a[6];
    uint32_t fp = r->a[7] - 4;                  /* A6 after LINK */
    set_w(&r->d[3], r16(r->a[4] + 0xE));
    r->a[2] = g32(0x1DCC);
    uint32_t cell = r->a[2] + sxw(r->d[3]);
    set_w(&r->d[0], r16(cell + 0x1C));
    sub_w(r, &r->d[0], r16(r->a[4] + 4));
    if ((uint16_t)r->d[0]) {
        if ((uint16_t)r->d[0] & 0x8000) { set_w(&r->d[0], 0); cc_nz_w(r, 0); }
        else { set_w(&r->d[0], 0x20); cc_nz_w(r, 0x20); }
        goto out;
    }
    set_w(&r->d[0], r16(cell + 0x1E));
    sub_w(r, &r->d[0], r16(r->a[4] + 8));
    if ((uint16_t)r->d[0]) {
        uint16_t dir = ((uint16_t)r->d[0] & 0x8000) ? 0x10 : 0x30;
        set_w(&r->d[0], dir); cc_nz_w(r, dir);
        goto out;
    }
    w16(cell + 0xE, 0);
    w16(fp - 0xE, 0); w16(fp - 0x10, 0xFFFF); w16(fp - 0x12, 0xFFFF);
    r->a[6] = fp;
    static const int16_t step[4] = {-0x400, 0x3E0, 0x40, 0x3E0};
    static const uint16_t dir[4] = {0x10, 0x00, 0x20, 0x30};
    for (int k = 0; k < 4; k++) {
        if (step[k] < 0) sub_w(r, &r->d[3], 0x400); else add_w(r, &r->d[3], (uint16_t)step[k]);
        set_w(&r->d[4], dir[k]);
        fn_S7_3FA6(r);
    }
    set_w(&r->d[0], r16(fp - 0x10));
    if ((uint16_t)r->d[0] & 0x8000) {
        set_w(&r->d[0], r16(r->a[4] + 0xE));
    } else {
        w16(r->a[4] + 0x10, r16(r->a[4] + 0xE));
        w16(r->a[4] + 0xE, r16(fp - 0x10));
        set_w(&r->d[0], r16(fp - 0x10));
    }
    w16(r->a[2] + sxw(r->d[0]) + 0xE, 1);
    set_w(&r->d[0], r16(fp - 0x12));
out:
    r->d[3] = d3; r->d[4] = d4; r->a[2] = a2; r->a[6] = a6;
    cc_nz_l(r, d4);   /* flags from restoring D4 */
}

/* fn_S7_3972 / fn_S7_37C4: run fn_S7_39C4 / fn_S7_3858 on the cell G_00E2 and its neighbours (3972:
 * itself, south, north, east, west; 37C4: also the four diagonals), stopping at the first non-zero
 * result, which is returned in D0 (else D0.w = 0). */
static void scan_around(Regs *r, void (*scan)(Regs *), const uint16_t *off, int n) {
    for (int k = 0; k < n; k++) {
        set_w(&r->d[0], g16(0xE2));
        if (k) add_w(r, &r->d[0], off[k]);
        scan(r);
        cc_nz_w(r, (uint16_t)r->d[0]);
        if ((uint16_t)r->d[0]) return;
    }
    set_w(&r->d[0], 0); cc_nz_w(r, 0);
}
void fn_S7_3972(Regs *r) {
    static const uint16_t off[5] = {0, 0x400, 0xFC00, 0x20, 0xFFE0};
    scan_around(r, fn_S7_39C4, off, 5);
}
void fn_S7_37C4(Regs *r) {
    static const uint16_t off[9] = {0, 0x400, 0xFC00, 0x20, 0xFFE0, 0xFBE0, 0x3E0, 0xFC20, 0x420};
    scan_around(r, fn_S7_3858, off, 9);
}

/* fn_S7_1C9A: turns the object at A4 towards D0.w (fn_S7_36F4); once it faces that way, steps it
 * along the step table G_0F2A and commits the move unless an object is in the way (fn_S7_3972).
 * D0.l = 0 while still turning, -1 if blocked, else fn_S7_3784's result. */
void fn_S7_1C9A(Regs *r) {
    fn_S7_36F4(r);
    cc_cmp_w(r, r16(r->a[4] + 0xC), (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] != r16(r->a[4] + 0xC)) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    r->a[0] = GADDR(0x0F2A);
    fn_S7_374C(r);
    fn_S7_3972(r);
    if ((uint16_t)r->d[0]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    fn_S7_3784(r);
}

/* fn_S7_0E46: moves a projectile (the object at A4) two steps along its heading with the step table
 * G_12EA. Its +$14 counts: 0 = just fired (set to 1, no move), negative = spent. It is removed
 * (taken out of its cell's list, +2 = -1) when a move is refused or it reaches a solid cell; if it
 * meets an object (fn_S7_37C4, D0 = that object's record offset), that object's +$14 = G_00F2 and
 * the projectile's +$14 = -1. G_00E0 = 1, G_00DE = 0 for the checks. Keeps A2. */
void fn_S7_0E46(Regs *r) {
    uint32_t a2 = r->a[2];
    r->a[2] = g32(0x1DCC);
    r->d[0] = 0;
    sg16(0xE0, mz_original ? 0 : 1); sg16(0xDE, 0);           /* Original (fn_S7_03FE): 0, 0 and four steps */
    set_w(&r->d[0], r16(r->a[4] + 0x14));
    if (!(uint16_t)r->d[0]) { w16(r->a[4] + 0x14, 1); cc_nz_w(r, 1); goto out; }
    if ((uint16_t)r->d[0] & 0x8000) goto remove;
    for (int step = 0; step < (mz_original ? 4 : 2); step++) {
        set_w(&r->d[0], r16(r->a[4] + 0xC));
        r->a[0] = GADDR(0x12EA);
        fn_S7_374C(r);
        fn_S7_3784(r);
        if ((uint16_t)r->d[0]) goto remove;
        set_w(&r->d[0], r16(r->a[4] + 0x30));
        set_b(&r->d[0], r8(r->a[2] + sxw(r->d[0])));
        if (r->d[0] & 1) { r->ccr &= (uint8_t)~CCR_Z; goto remove; }
        fn_S7_37C4(r);
        cc_nz_w(r, (uint16_t)r->d[0]);
        if ((uint16_t)r->d[0]) goto hit;
    }
    goto out;
remove:
    set_w(&r->d[0], g16(0xF2)); set_w(&r->d[1], r16(r->a[4] + 0x30));
    mz_seg_of(2, 0x3410), fn_S2_3410(r);
    w16(r->a[4] + 2, 0xFFFF); cc_nz_w(r, 0xFFFF);
    goto out;
hit:
    r->a[0] = g32(0x1DC4);
    w16(r->a[0] + sxw(r->d[0]) + 0x14, g16(0xF2));
    w16(r->a[4] + 0x14, 0xFFFF); cc_nz_w(r, 0xFFFF);
out:
    r->a[2] = a2;
}

void fn_S4_1550(void);

/* fn_S7_1E20: moves a monster's shot (A4) along its heading (step table G_12EA). D0.l = -1 when it
 * ends: it hit a wall (+$12 = 0), the player (G_00E0 = 4 for the test, fn_S7_3ACA: +$12 = 9 and
 * sound $0E) or another object (fn_S7_37C4 with G_00E0 = 1 and the shooter A4+$10 excluded:
 * +$12 = 0). */
void fn_S7_1E20(Regs *r) {
    r->a[0] = GADDR(0x12EA);
    set_w(&r->d[0], r16(r->a[4] + 0xC));
    fn_S7_374C(r);
    fn_S7_3784(r);
    set_w(&r->d[1], r16(r->a[4] + 0x30));
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + sxw(r->d[1])) & 1) { w16(r->a[4] + 0x12, 0); r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    sg16(0xE0, 4);
    fn_S7_3ACA(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) { w16(r->a[4] + 0x12, 9); mz_seg_of(4, 0x1550), fn_S4_1550(); r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    sg16(0xE0, 1); sg16(0xDE, r16(r->a[4] + 0x10));
    fn_S7_37C4(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) { w16(r->a[4] + 0x12, 0); r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); }
}

void fn_S4_1420(void);
void fn_S4_1722(void);
void fn_S4_170E(void);

/* fn_S7_0014: per-frame update of the object at A4 (sound 0 first). While its +$16 and G_20F2 are
 * both 0 it is triggered once: G_225C = 1, +$16 = 1, sound $26, and its cell gets bit 0 of +$12.
 * Once triggered (G_225C set): every 32 frames at phase 8 the marked area spreads (fn_S7_008A); if
 * the player's cell G_20CA is marked (and G_20D8 < $80), sound $25 and G_20FE counts down to 0. */
void fn_S7_0014(Regs *r) {
    mz_seg_of(4, 0x1420), fn_S4_1420();
    set_w(&r->d[0], r16(r->a[4] + 0x16));
    if (!(uint16_t)r->d[0]) {
        set_w(&r->d[0], g16(0x20F2));
        if (!(uint16_t)r->d[0]) {
            sg8(0x225C, 1); w16(r->a[4] + 0x16, 1);
            mz_seg_of(4, 0x1722), fn_S4_1722();
            set_w(&r->d[0], r16(r->a[4] + 0x30));
            r->a[0] = g32(0x1DCC);
            uint32_t c = r->a[0] + sxw(r->d[0]) + 0x12; uint8_t v = (uint8_t)(r8(c) | 1);
            w8(c, v); cc_nz_b(r, v);
            return;
        }
    }
    set_b(&r->d[0], g8(0x225C));
    if (!(uint8_t)r->d[0]) { cc_nz_b(r, 0); return; }
    set_w(&r->d[0], (uint16_t)(g16(0x20BC) & 0x1F));
    if ((uint16_t)r->d[0] == 8) fn_S7_008A(r);
    set_w(&r->d[0], g16(0x20D8));
    cc_cmp_w(r, 0x80, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] >= 0x80) return;
    set_w(&r->d[0], g16(0x20CA));
    r->a[0] = g32(0x1DCC);
    if (!(r8(r->a[0] + sxw(r->d[0]) + 0x12) & 1)) { r->ccr |= CCR_Z; return; }
    mz_seg_of(4, 0x170E), fn_S4_170E();
    set_w(&r->d[0], (uint16_t)(g16(0x20FE) - 1));
    if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
    sg16(0x20FE, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

void fn_S7_1E20(Regs *r);

/* fn_S7_1DB2: per-frame update of a monster shot / effect at A4 by its +$12 state: positive counts
 * down while it sits at the player's position (moved there with fn_S7_3784, +$26 = 0, G_20FE -= 3
 * down to 0); zero removes it (out of its cell's list, +2 = -1); negative: its animation frame +$16
 * advances every other frame (0..15) and it flies on (fn_S7_1E20). */
/* A shot's tick (types $22, $23, $24, $36, $3D) by its +$12 state: positive counts down while it
 * sits at the player's position (moved there with fn_S7_3784, +$26 = 0, life G_20FE drained by
 * `drain` down to 0); zero removes it (out of its cell's list, +2 = -1); negative: (with `anim`, its
 * animation frame +$16 advances every other frame, 0..15) it flies on. */
static void shot_tick(Regs *r, int anim, void (*fly)(Regs *), uint16_t drain) {
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    uint16_t s = (uint16_t)r->d[0];
    if (s & 0x8000) {
        if (anim) {
            set_w(&r->d[0], r16(r->a[4] + 0x16));
            set_w(&r->d[1], (uint16_t)(g16(0x20BC) & 1));
            if ((uint16_t)r->d[1]) add_w(r, &r->d[0], 1);
            if ((uint16_t)r->d[0] >= 0x10) set_w(&r->d[0], 0);
            w16(r->a[4] + 0x16, (uint16_t)r->d[0]);
        }
        fly(r);
        return;
    }
    if (!s) {
        set_w(&r->d[0], g16(0xF2)); set_w(&r->d[1], r16(r->a[4] + 0x30));
        mz_seg_of(2, 0x3410), fn_S2_3410(r);
        w16(r->a[4] + 2, 0xFFFF); cc_nz_w(r, 0xFFFF);
        return;
    }
    sub_w(r, &r->d[0], 1);
    w16(r->a[4] + 0x12, (uint16_t)r->d[0]);
    sg32(0xEA, g32(0x20D2)); sg32(0xE6, g32(0x20CE)); sg16(0xE2, g16(0x20CA));
    fn_S7_3784(r);
    w16(r->a[4] + 0x26, 0);
    set_w(&r->d[0], g16(0x20FE));
    sub_w(r, &r->d[0], drain);
    if ((uint16_t)r->d[0] & 0x8000) set_w(&r->d[0], 0);
    sg16(0x20FE, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}
void fn_S7_1DB2(Regs *r) { shot_tick(r, 1, fn_S7_1E20, 3); }

/* A shot's flight step, as fn_S7_1E20 with: the step table, the hit-the-player state and sound,
 * and G_00E0 for the player test. */
static void shot_step(Regs *r, uint32_t steps, uint16_t hit_state, uint16_t near, void (*sound)(void)) {
    r->a[0] = GADDR(steps);
    set_w(&r->d[0], r16(r->a[4] + 0xC));
    fn_S7_374C(r);
    fn_S7_3784(r);
    set_w(&r->d[1], r16(r->a[4] + 0x30));
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + sxw(r->d[1])) & 1) { w16(r->a[4] + 0x12, 0); r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    sg16(0xE0, near);
    fn_S7_3ACA(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) { w16(r->a[4] + 0x12, hit_state); mz_seg(4), sound(); r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    sg16(0xE0, 1); sg16(0xDE, r16(r->a[4] + 0x10));
    fn_S7_37C4(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) { w16(r->a[4] + 0x12, 0); r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); }
}
/* fn_S7_1EF8 / fn_S7_1F5A: a type $23 shot's two half steps (G_0CAA; hitting the player: +$12 = $B) */
void fn_S7_1EF8(Regs *r) { shot_step(r, 0x0CAA, 0xB, 4, fn_S4_1550); }
void fn_S7_1F5A(Regs *r) { shot_step(r, 0x0CAA, 0xB, 4, fn_S4_1550); }
static void fly_23(Regs *r) {
    fn_S7_1EF8(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) return;
    fn_S7_1F5A(r);
}
/* fn_S7_2030: a type $24 shot homes in: turns towards the player when it can see them (fn_S7_32DC,
 * fn_S7_36F4), then a half step (hitting the player within 3: +$12 = 9, sound fn_S4_15AA). */
void fn_S7_2030(Regs *r) {
    fn_S7_32DC(r);
    set_w(&r->d[0], g16(0xEE));
    if (!((uint16_t)r->d[0] & 0x8000)) fn_S7_36F4(r);
    shot_step(r, 0x0CAA, 9, 3, fn_S4_15AA);
}
static void fly_24(Regs *r) {
    w16(r->a[4] + 0x14, 4);
    fn_S7_2030(r);
}
/* fn_S7_1E82 / fn_S7_1FBC: types $23 and $24, the other monster shots, as fn_S7_1DB2 with their own
 * flight: $23 flies in two half steps, $24 homes in (and keeps +$14 = 4). */
void fn_S7_1E82(Regs *r) { shot_tick(r, 1, fly_23, 3); }
void fn_S7_1FBC(Regs *r) { shot_tick(r, 1, fly_24, 3); }

/* fn_S7_2B9C / fn_S7_2BFE: a type $36 shot's steps (G_0F2A, then G_0CAA; the player within 3:
 * +$12 = 3, sound fn_S4_1562) */
void fn_S7_2B9C(Regs *r) { shot_step(r, 0x0F2A, 3, 3, fn_S4_1562); }
void fn_S7_2BFE(Regs *r) { shot_step(r, 0x0CAA, 3, 3, fn_S4_1562); }
static void fly_36(Regs *r) {
    w16(r->a[4] + 0x14, 0);
    fn_S7_2B9C(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) return;
    fn_S7_2BFE(r);
}
/* fn_S7_2CBA: a type $3D shot's step (G_12EA; the player within 4: +$12 = 4, sound fn_S4_1586) */
void fn_S7_2CBA(Regs *r) { shot_step(r, 0x12EA, 4, 4, fn_S4_1586); }
static void fly_3d(Regs *r) {
    fn_S7_2CBA(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) return;
    fn_S7_2CBA(r);
}
/* fn_S7_2B3E / fn_S7_2C60: shot types $36 (no animation, two steps) and $3D (two full steps,
 * draining 4). */
void fn_S7_2B3E(Regs *r) { shot_tick(r, 0, fly_36, 3); }
void fn_S7_2C60(Regs *r) { shot_tick(r, 0, fly_3d, 4); }

/* fn_S7_2620: type $29, the bruisers' shot: two steps (G_12EA, G_0DEA) a tick; it vanishes in a
 * wall; within 8 pixels of the player it drains 2 life (sound fn_S4_15BC); an object it reaches
 * (fn_S7_37C4 within 8, not its shooter) is hit (that object's +$14 = this one). */
void fn_S7_2620(Regs *r) {
    static const uint16_t steps[2] = {0x12EA, 0x0DEA};
    for (int k = 0; k < 2; k++) {
        r->a[0] = GADDR(steps[k]);
        set_w(&r->d[0], r16(r->a[4] + 0xC));
        fn_S7_374C(r);
        fn_S7_3784(r);
    }
    set_w(&r->d[1], r16(r->a[4] + 0x30));
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + sxw(r->d[1])) & 1) {
        set_w(&r->d[0], g16(0xF2)); set_w(&r->d[1], r16(r->a[4] + 0x30));
        mz_seg_of(2, 0x3410), fn_S2_3410(r);
        w16(r->a[4] + 2, 0xFFFF); cc_nz_w(r, 0xFFFF);
        return;
    }
    fn_S7_3D2A(r);
    if ((uint16_t)r->d[0] & 0x8000) {
        mz_seg_of(4, 0x15BC), fn_S4_15BC();
        set_w(&r->d[0], g16(0x20FE)); sub_w(r, &r->d[0], 2);
        if ((uint16_t)r->d[0] & 0x8000) set_w(&r->d[0], 0);
        sg16(0x20FE, (uint16_t)r->d[0]);
    }
    sg16(0xE0, 8); sg16(0xDE, r16(r->a[4] + 0x10));
    fn_S7_37C4(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if (!(uint16_t)r->d[0]) return;
    r->a[0] = g32(0x1DC4);
    w16(r->a[0] + sxw(r->d[0]) + 0x14, g16(0xF2));
    cc_nz_w(r, g16(0xF2));
}

/* fn_S7_2A70: the gunner's shot store +$18 fills by one a tick, up to $69. */
void fn_S7_2A70(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0x18)); add_w(r, &r->d[0], 1);
    if ((uint16_t)r->d[0] >= 0x69) set_w(&r->d[0], 0x69);
    w16(r->a[4] + 0x18, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fn_S7_2A86: the direction-map byte (G_1DBC) from the object at A4 to where the player will be
 * (position plus twice the movement G_20C8 / G_20C4), or G_00EE when G_00EC is $30. */
void fn_S7_2A86(Regs *r) {
    set_w(&r->d[0], g16(0xEC));
    if ((uint16_t)r->d[0] == 0x30) { set_w(&r->d[0], g16(0xEE)); cc_nz_w(r, (uint16_t)r->d[0]); return; }
    r->d[1] = g32(0x20CE); r->d[2] = g32(0x20C4); lsl_l1(r, &r->d[2]);
    add_l(r, &r->d[1], r->d[2]); sub_l(r, &r->d[1], r32(r->a[4] + 8));
    r->d[1] = swap(r->d[1]);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] & 0x7F)); lsl_w(r, &r->d[1], 7);
    r->d[0] = g32(0x20D2); r->d[2] = g32(0x20C8); lsl_l1(r, &r->d[2]);
    add_l(r, &r->d[0], r->d[2]); sub_l(r, &r->d[0], r32(r->a[4] + 4));
    r->d[0] = swap(r->d[0]);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0x7F));
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    r->a[0] = g32(0x1DBC);
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    cc_nz_b(r, (uint8_t)r->d[0]);
}


/* ---------- drawing one model: G_20A2 = its command stream, then edges (fn_S2_1CF6) and the
 * merge into the span lists (fn_S2_4DC4). The streams are tables of self-relative offsets. ---------- */
void fn_S2_1CF6(Regs *r);
void fn_S2_4DC4(Regs *r);

void mz_draw_stream(Regs *r, uint32_t stream) {
    sg32(0x20A2, stream);
    mz_seg_of(2, 0x1CF6), fn_S2_1CF6(r);
    mz_seg_of(2, 0x4DC4), fn_S2_4DC4(r);
}

/* the model of table `models` (32 self-relative offsets) for the object at A4 as the player sees
 * it: entry (G_20D4 - A4+$2A - A4+$C) & $3E, by way of A1 (fn_S7_1844 and its kin; Mazeworld
 * Original's are in game/orig_seg*.c) */
void mz_draw_by_angle(Regs *r, uint32_t models) {
    r->a[1] = models;
    set_w(&r->d[1], g16(0x20D4));
    sub_w(r, &r->d[1], r16(r->a[4] + 0x2A));
    sub_w(r, &r->d[1], r16(r->a[4] + 0xC));
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] & 0x3E));
    lsl_w(r, &r->d[1], 1);
    r->a[1] += r32(r->a[1] + sxw(r->d[1]));
    mz_draw_stream(r, r->a[1]);
}

/* fn_S7_0000: model G_1D20 (its first entry). */
void fn_S7_0000(Regs *r) {
    r->a[0] = g32(0x1D20); r->a[0] += r32(r->a[0]);
    mz_draw_stream(r, r->a[0]);
}

/* fn_S7_0E2A: model G_1D54, frame A4+$12. */
void fn_S7_0E2A(Regs *r) {
    r->a[0] = g32(0x1D54);
    set_w(&r->d[0], r16(r->a[4] + 0x12)); lsl_w(r, &r->d[0], 2);
    r->a[0] += r32(r->a[0] + sxw(r->d[0]));
    mz_draw_stream(r, r->a[0]);
}

/* fn_S7_0618: model G_1D94, frame = heading A4+$C. */
void fn_S7_0618(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0xC)); lsl_w(r, &r->d[0], 2);
    r->a[0] = g32(0x1D94);
    r->a[0] += r32(r->a[0] + sxw(r->d[0]));
    mz_draw_stream(r, r->a[0]);
}

/* fn_S7_1844: model G_1D88, frame by the angle between the player's heading G_20D4 and the object
 * (heading A4+$C plus A4+$2A), in 32 steps. */
void fn_S7_1844(Regs *r) { mz_draw_by_angle(r, g32(0x1D88)); }

/* fn_S7_017E: model G_1D3C (entry 2) in the colour set G_0156[(A4+$16 >> 1) & 7]. */
void fn_S7_017E(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0x16));
    lsr_w(r, &r->d[0], 1);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 7));
    lsl_w(r, &r->d[0], 1);
    r->a[0] = GADDR(0x0156);
    set_w(&r->d[0], r16(r->a[0] + sxw(r->d[0])));
    sg16(0x209E, (uint16_t)r->d[0]);
    r->a[0] = g32(0x1D3C); r->a[0] += r32(r->a[0] + 8);
    mz_draw_stream(r, r->a[0]);
}

/* fn_S7_020E: unless G_20D8 is $80, model G_1D78 in the colour set G_0146[heading A4+$C]. */
void fn_S7_020E(Regs *r) {
    set_w(&r->d[0], g16(0x20D8));
    cc_cmp_w(r, 0x80, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] == 0x80) return;
    set_w(&r->d[0], r16(r->a[4] + 0xC)); lsl_w(r, &r->d[0], 1);
    r->a[0] = GADDR(0x0146);
    set_w(&r->d[0], r16(r->a[0] + sxw(r->d[0])));
    sg16(0x209E, (uint16_t)r->d[0]);
    r->a[0] = g32(0x1D78); r->a[0] += r32(r->a[0]);
    mz_draw_stream(r, r->a[0]);
}

/* Sets the view-relative drawing window for full-screen effects: G_20A8 = G_2154 + 4 (horizontal
 * offset), G_20A6 = 4, G_20A4 = G_2156 + 4 (left and right edges). D0.w = the last value. */
static void effect_window(Regs *r) {
    set_w(&r->d[0], g16(0x2154)); add_w(r, &r->d[0], 4); sg16(0x20A8, (uint16_t)r->d[0]);
    sg16(0x20A6, 4);
    set_w(&r->d[0], g16(0x2156)); add_w(r, &r->d[0], 4); sg16(0x20A4, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fn_S7_1D0E: an effect object at A4 by its +$12: positive draws a random frame (G_1D50) with a
 * random colour set (G_20AA) over the whole view; negative draws frame A4+$16 (0..15) of G_1D38
 * through a fixed frame order. */
void fn_S7_1D0E(Regs *r) {
    static const uint16_t order[16] = {0, 2, 3, 5, 0, 2, 3, 1, 4, 5, 4, 2, 0, 4, 1, 3};   /* table in the code at S7:1D8C */
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] & 0x8000) {
        set_w(&r->d[0], r16(r->a[4] + 0x16));
        cc_cmp_w(r, 0x10, (uint16_t)r->d[0]);
        if ((uint16_t)r->d[0] >= 0x10) set_w(&r->d[0], 0);
        lsl_w(r, &r->d[0], 1);
        set_w(&r->d[0], order[(uint16_t)r->d[0] >> 1]);
        lsl_w(r, &r->d[0], 2);
        r->a[0] = g32(0x1D38); r->a[0] += r32(r->a[0] + sxw(r->d[0]));
        mz_draw_stream(r, r->a[0]);
        return;
    }
    if (!(uint16_t)r->d[0]) return;
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3));
    sg16(0x20AA, (uint16_t)r->d[0]);
    effect_window(r);
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3));
    lsl_w(r, &r->d[0], 2);
    r->a[0] = g32(0x1D50); r->a[0] += r32(r->a[0] + sxw(r->d[0]));
    mz_draw_stream(r, r->a[0]);
}

/* G_1DBC map value at the corner (x | $F or x & ~$F, y likewise) of the object's cell, relative to
 * the object (A4+4 / A4+8 integer parts), minus G_00EE. D1 = the map offset, A1 = G_1DBC. */
static void corner_height(Regs *r, int fx, int fy) {
    uint16_t ox = r16(r->a[4] + 4), oy = r16(r->a[4] + 8);
    set_w(&r->d[1], (uint16_t)(oy & 0xFFF0)); if (fy) add_w(r, &r->d[1], 0xF);
    sub_w(r, &r->d[1], oy);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] & 0x7F)); lsl_w(r, &r->d[1], 7);
    set_w(&r->d[0], (uint16_t)(ox & 0xFFF0)); if (fx) add_w(r, &r->d[0], 0xF);
    sub_w(r, &r->d[0], ox);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0x7F));
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    r->a[1] = g32(0x1DBC);
    r->d[0] = r8(r->a[1] + sxw(r->d[1]));
    sub_w(r, &r->d[0], g16(0xEE));
}

/* fn_S7_32DC: can the object at A4 see the player? (G_20D8 < $80, within 63 cells; G_00EC / G_00EE =
 * the G_1DB8 / G_1DBC map bytes between them.) Yes (D0.l = 0) if the player marker (+$C of a cell
 * in G_1DCC) is in the object's cell, in one of the next three open cells in a straight line each
 * way, or in a diagonal neighbour whose corner passes the height test (bit 5 of the corner value
 * minus G_00EE; the rule depends on whether the side cell between them is solid). Otherwise
 * D0.l = -1 and G_00EC = G_00EE = -1. */
void fn_S7_32DC(Regs *r) {
    set_w(&r->d[0], g16(0x20D8)); cc_cmp_w(r, 0x80, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] >= 0x80) goto no;
    set_w(&r->d[0], g16(0x20D2)); sub_w(r, &r->d[0], r16(r->a[4] + 4)); add_w(r, &r->d[0], 0x3F);
    cc_cmp_w(r, 0x7E, (uint16_t)r->d[0]); if ((uint16_t)r->d[0] >= 0x7E) goto no;
    set_w(&r->d[0], g16(0x20CE)); sub_w(r, &r->d[0], r16(r->a[4] + 8)); add_w(r, &r->d[0], 0x3F);
    cc_cmp_w(r, 0x7E, (uint16_t)r->d[0]); if ((uint16_t)r->d[0] >= 0x7E) goto no;
    map_offset(r, g16(0x20CE), r16(r->a[4] + 8), g16(0x20D2), r16(r->a[4] + 4));
    r->a[0] = g32(0x1DB8); r->d[0] = r8(r->a[0] + sxw(r->d[1])); sg16(0xEC, (uint16_t)r->d[0]);
    r->a[0] = g32(0x1DBC); r->d[0] = r8(r->a[0] + sxw(r->d[1])); sg16(0xEE, (uint16_t)r->d[0]);
    r->d[0] = r16(r->a[4] + 0x30);
    r->a[0] = g32(0x1DCC) + r->d[0];
    set_w(&r->d[0], r16(r->a[0] + 0xC));
    if ((uint16_t)r->d[0]) goto yes;
    static const int32_t dir[4] = {-0x20, 0x20, 0x400, -0x400};
    for (int k = 0; k < 4; k++)
        for (int step = 1; step <= 3; step++) {
            uint32_t c = r->a[0] + (uint32_t)(dir[k] * step);
            if (r8(c) & 1) break;
            set_w(&r->d[0], r16(c + 0xC));
            if ((uint16_t)r->d[0]) goto yes;
        }
    /* diagonals: +$420, +$3E0, -$3E0, -$420; side cell +$400 or -$400; corner and rule */
    static const struct { int32_t cell, side; int fx, fy, open_rule_p; } diag[4] = {
        {0x420, 0x400, 1, 1, 1}, {0x3E0, 0x400, 0, 1, 0}, {-0x3E0, -0x400, 1, 0, 0}, {-0x420, -0x400, 0, 0, 1}};
    for (int k = 0; k < 4; k++) {
        set_w(&r->d[0], r16(r->a[0] + (uint32_t)diag[k].cell + 0xC));
        if (!(uint16_t)r->d[0]) continue;
        int side_solid = r8(r->a[0] + (uint32_t)diag[k].side) & 1;
        corner_height(r, diag[k].fx, diag[k].fy);
        int rule_p = side_solid ? !diag[k].open_rule_p : diag[k].open_rule_p;
        if (rule_p) { if (r->d[0] & 0x20) goto yes; goto no; }
        if (!(uint16_t)r->d[0]) goto no;
        if (!(r->d[0] & 0x20)) goto yes;
        goto no;
    }
no:
    r->d[0] = 0xFFFFFFFFu; sg16(0xEE, 0xFFFF); sg16(0xEC, 0xFFFF); cc_nz_w(r, 0xFFFF);
    return;
yes:
    r->d[0] = 0; cc_nz_l(r, 0);
}

/* fn_S7_3CB6: fn_S7_3CBC for the record at D3.w, if its marker +$E is clear. */
void fn_S7_3CB6(Regs *r) {
    set_w(&r->d[0], r16(r->a[2] + sxw(r->d[3]) + 0xE));
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) return;
    fn_S7_3CBC(r);
}

/* fn_S7_372C: fn_S7_36F4 on even G_20BC ticks only. */
void fn_S7_372C(Regs *r) {
    set_w(&r->d[1], g16(0x20BC) & 1);
    cc_nz_w(r, (uint16_t)r->d[1]);
    if (!((uint16_t)r->d[1])) fn_S7_36F4(r);
}

/* fn_S7_3B36: the direction for the object at A4 to head from its target cell A4+$E (G_1DCC
 * record, also put in G_00D2) towards the point G_00DA / G_00D6:
 *  - not yet at the cell's centre on x (the cell's +$1C vs the object's +4): westward 0 or eastward
 *    $20; but if the next cell that way is free (marker +$E clear) and the goal's 16-pixel column
 *    lies beyond it, that cell becomes the target (old one to A4+$10, G_00D2 = new) and the
 *    direction turns ($20 / 0);
 *  - else on y likewise ($10 north / $30 south, with $30 / $10 when advancing);
 *  - at the centre: the free neighbour (N, W, E, S) with the lowest distance-map value (G_1DB8,
 *    fn_S7_3CBC; the cell itself counts with direction -1) wins, or -1 if none.
 * D0.w and G_00D0 = the direction. Keeps D3-D5/A2 (LINK frame for fn_S7_3CBC). */
void fn_S7_3B36(Regs *r) {
    uint32_t d3 = r->d[3], d4 = r->d[4], d5 = r->d[5], a2 = r->a[2], a6 = r->a[6];
    uint32_t fp = r->a[7] - 4;
    r->a[6] = fp;
    set_w(&r->d[5], r16(r->a[4] + 0xE));
    sg16(0xD2, (uint16_t)r->d[5]);
    r->a[2] = g32(0x1DCC);
    uint32_t cell = r->a[2] + sxw(r->d[5]);
    set_w(&r->d[0], r16(cell + 0x1C));
    sub_w(r, &r->d[0], r16(r->a[4] + 4));
    int along_x = (uint16_t)r->d[0] != 0, neg = ((uint16_t)r->d[0] & 0x8000) != 0;
    if (!along_x) {
        set_w(&r->d[0], r16(cell + 0x1E));
        sub_w(r, &r->d[0], r16(r->a[4] + 8));
        if (!(uint16_t)r->d[0]) goto centre;
        neg = ((uint16_t)r->d[0] & 0x8000) != 0;
    }
    {
        /* one case per direction: next-cell step, goal coordinate, cell coordinate, results */
        uint16_t step = along_x ? 0x20 : 0x400;
        uint16_t goal = g16(along_x ? 0xDA : 0xD6) & 0xFFF0, here = r16(cell + (along_x ? 0x1C : 0x1E)) & 0xFFF0;
        set_w(&r->d[3], (uint16_t)r->d[5]);
        if (neg) add_w(r, &r->d[3], step); else sub_w(r, &r->d[3], step);
        set_w(&r->d[0], r16(r->a[2] + sxw(r->d[3]) + 0xE));
        uint16_t keep = along_x ? (neg ? 0 : 0x20) : (neg ? 0x10 : 0x30);   /* not advancing */
        uint16_t turn = along_x ? (neg ? 0x20 : 0) : (neg ? 0x30 : 0x10);   /* advancing */
        if ((uint16_t)r->d[0]) { set_w(&r->d[0], keep); goto out; }
        /* neg: goal - here < 0 ... computed as (here - goal) for neg, (goal - here) otherwise */
        if (neg) { set_w(&r->d[1], goal); set_w(&r->d[0], here); sub_w(r, &r->d[0], goal); }
        else     { set_w(&r->d[1], here); set_w(&r->d[0], goal); sub_w(r, &r->d[0], here); }
        if (!((uint16_t)r->d[0] & 0x8000)) { set_w(&r->d[0], keep); goto out; }
        w16(r->a[4] + 0x10, (uint16_t)r->d[5]);
        sg16(0xD2, (uint16_t)r->d[3]);
        set_w(&r->d[0], turn);
        goto out;
    }
centre:
    r->d[0] = 0xFFFFFFFF;
    w16(fp - 0x12, 0xFFFF); w16(fp - 0x14, 0xFFFF); w16(fp - 0x16, 0xFFFF);
    set_w(&r->d[3], (uint16_t)r->d[5]);
    set_w(&r->d[4], 0xFFFF);
    fn_S7_3CBC(r);
    static const uint16_t steps[4] = {0xFC00, 0x3E0, 0x40, 0x3E0}, dirs[4] = {0x10, 0, 0x20, 0x30};
    for (int k = 0; k < 4; k++) {
        if (k == 0) sub_w(r, &r->d[3], 0x400); else add_w(r, &r->d[3], steps[k]);
        set_w(&r->d[4], dirs[k]);
        fn_S7_3CB6(r);
    }
    set_w(&r->d[0], r16(fp - 0x16));
    if (!((uint16_t)r->d[0] & 0x8000)) {
        w16(r->a[4] + 0x10, (uint16_t)r->d[5]);
        sg16(0xD2, r16(fp - 0x14));
        set_w(&r->d[0], r16(fp - 0x16));
    }
out:
    sg16(0xD0, (uint16_t)r->d[0]);
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->a[2] = a2; r->a[6] = a6;
    cc_nz_l(r, d5);   /* flags from restoring D5 */
}

/* fn_S7_0108: type 1's model: the frame (+$C, 0..15) moves on by 2 + a random 0..3 (fn_S2_3532)
 * each frame; model G_1D18 (entry 1 + frame). */
void fn_S7_0108(Regs *r) {
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3));
    add_w(r, &r->d[0], r16(r->a[4] + 0xC));
    add_w(r, &r->d[0], 2);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0xF));
    w16(r->a[4] + 0xC, (uint16_t)r->d[0]);
    lsl_w(r, &r->d[0], 2);
    r->a[0] = g32(0x1D18);
    r->a[0] += r32(r->a[0] + sxw(r->d[0]) + 4);
    mz_draw_stream(r, r->a[0]);
}

/* The spinning jewels: model G_1D3C entry `entry` in the colour set table[(+$16 >> 1) & 7]. */
static void jewel_model(Regs *r, uint32_t table, uint32_t entry) {
    set_w(&r->d[0], r16(r->a[4] + 0x16));
    lsr_w(r, &r->d[0], 1);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 7));
    lsl_w(r, &r->d[0], 1);
    r->a[0] = GADDR(table);
    set_w(&r->d[0], r16(r->a[0] + sxw(r->d[0])));
    sg16(0x209E, (uint16_t)r->d[0]);
    r->a[0] = g32(0x1D3C); r->a[0] += r32(r->a[0] + entry);
    mz_draw_stream(r, r->a[0]);
}
/* fn_S7_0394 / 0424 / 04B4: the jewels of types 4, 5, 6 (entries 3, 4, 5; colours G_013A, G_012A,
 * G_011A) */
void fn_S7_0394(Regs *r) { jewel_model(r, 0x013A, 0xC); }
void fn_S7_0424(Regs *r) { jewel_model(r, 0x012A, 0x10); }
void fn_S7_04B4(Regs *r) { jewel_model(r, 0x011A, 0x14); }

/* fn_S7_0544: type 7, two parts of model G_1D3C, in colours $35 and $36, frame +$C / 2 picking the
 * entries from the tables G_010A and G_00FE. */
void fn_S7_0544(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0xC));
    if ((uint16_t)r->d[0] >= 0xC) r->d[0] = 0;      /* a range check whose result is not used */
    set_w(&r->d[0], r16(r->a[4] + 0xC));
    static const uint16_t colour[2] = {0x35, 0x36}, table[2] = {0x010A, 0x00FE};
    for (int k = 0; k < 2; k++) {
        sg16(0x209E, colour[k]);
        set_w(&r->d[0], r16(r->a[4] + 0xC));
        lsr_w(r, &r->d[0], 1); lsl_w(r, &r->d[0], 1);
        r->a[0] = GADDR(table[k]);
        set_w(&r->d[0], r16(r->a[0] + sxw(r->d[0])));
        r->a[0] = g32(0x1D3C);
        r->a[0] += r32(r->a[0] + sxw(r->d[0]));
        mz_draw_stream(r, r->a[0]);
    }
}

/* fn_S7_0704: type 9's model G_1D9C by the angle to the player, as fn_S7_1844. */
void fn_S7_0704(Regs *r) { mz_draw_by_angle(r, g32(0x1D9C)); }

/* fn_S7_319E: as fn_S7_32DC without the diagonal test: can the object at A4 see the player along a
 * straight line? (G_20D8 < $80, within 63 cells; D0.l = 0 and G_00EC / G_00EE = the G_1DB8 / G_1DBC
 * map bytes between them if the player marker is in its cell or within three open cells in a
 * straight line; otherwise D0.l = -1 and both are -1.) */
void fn_S7_319E(Regs *r) {
    set_w(&r->d[0], g16(0x20D8)); cc_cmp_w(r, 0x80, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] >= 0x80) goto no;
    set_w(&r->d[0], g16(0x20D2)); sub_w(r, &r->d[0], r16(r->a[4] + 4)); add_w(r, &r->d[0], 0x3F);
    if ((uint16_t)r->d[0] >= 0x7E) goto no;
    set_w(&r->d[0], g16(0x20CE)); sub_w(r, &r->d[0], r16(r->a[4] + 8)); add_w(r, &r->d[0], 0x3F);
    if ((uint16_t)r->d[0] >= 0x7E) goto no;
    r->d[0] = r16(r->a[4] + 0x30);
    r->a[0] = g32(0x1DCC) + r->d[0];
    set_w(&r->d[0], r16(r->a[0] + 0xC));
    if ((uint16_t)r->d[0]) goto yes;
    static const int32_t dir[4] = {-0x20, 0x20, 0x400, -0x400};
    for (int k = 0; k < 4; k++)
        for (int step = 1; step <= 3; step++) {
            uint32_t c = r->a[0] + (uint32_t)(dir[k] * step);
            if (r8(c) & 1) break;
            set_w(&r->d[0], r16(c + 0xC));
            if ((uint16_t)r->d[0]) goto yes;
        }
no:
    r->d[0] = 0xFFFFFFFFu; sg16(0xEE, 0xFFFF); sg16(0xEC, 0xFFFF); cc_nz_w(r, 0xFFFF);
    return;
yes:
    map_offset(r, g16(0x20CE), r16(r->a[4] + 8), g16(0x20D2), r16(r->a[4] + 4));
    r->a[0] = g32(0x1DB8); r->d[0] = r8(r->a[0] + sxw(r->d[1])); sg16(0xEC, (uint16_t)r->d[0]);
    r->a[0] = g32(0x1DBC); r->d[0] = r8(r->a[0] + sxw(r->d[1])); sg16(0xEE, (uint16_t)r->d[0]);
    r->d[0] = 0; cc_nz_l(r, 0);
}

/* fn_S7_3D2A: D0.l = -1 if the player is within 8 pixels of the object at A4 both ways (and
 * G_20D8 < $80), else 0. */
void fn_S7_3D2A(Regs *r) {
    if (!mz_original) {                                   /* (Original, fn_S7_1B1E, has no G_20D8 test) */
        set_w(&r->d[0], g16(0x20D8));
        if ((uint16_t)r->d[0] >= 0x80) goto far;
    }
    set_w(&r->d[0], g16(0x20D2)); sub_w(r, &r->d[0], r16(r->a[4] + 4)); add_w(r, &r->d[0], 8);
    if ((uint16_t)r->d[0] >= 0x11) goto far;
    set_w(&r->d[0], g16(0x20CE)); sub_w(r, &r->d[0], r16(r->a[4] + 8)); add_w(r, &r->d[0], 8);
    if ((uint16_t)r->d[0] >= (mz_original ? 0x10 : 0x11)) goto far;   /* Original: under 8 below */
    r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]);
    return;
far:
    r->d[0] = 0; cc_nz_l(r, 0);
}

/* fn_S7_0A52 / fn_S7_0A76: moves the player's x (G_20D2) / y (G_20CE) integer part one step towards
 * the object's (A4+4 / A4+8); once equal, the object takes the player's exact position. */
static void pull_axis(Regs *r, uint32_t g, uint32_t field) {
    set_w(&r->d[0], g16(g));
    uint16_t obj = r16(r->a[4] + field);
    cc_cmp_w(r, obj, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] == obj) {
        if (!mz_original) { w32(r->a[4] + field, g32(g)); cc_nz_l(r, g32(g)); }   /* Original (fn_S10_25EA): the object stays */
        return;
    }
    if ((uint16_t)r->d[0] < obj) add_w(r, &r->d[0], 1); else sub_w(r, &r->d[0], 1);
    sg16(g, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}
void fn_S7_0A52(Regs *r) { pull_axis(r, 0x20D2, 4); }
void fn_S7_0A76(Regs *r) { pull_axis(r, 0x20CE, 8); }

/* fn_S7_09EC: the grabber's pull: the player moves up to 4 steps towards the object on each axis,
 * the player marker (+$C of a G_1DCC cell) follows the player's new cell G_20CA, and life G_20FE
 * drops by 3 (not below 0). */
void fn_S7_09EC(Regs *r) {
    const int steps = mz_original ? 3 : 4;                  /* Original (fn_S10_258C): 3 steps, life - 2 */
    for (int k = 0; k < steps; k++) fn_S7_0A52(r);
    for (int k = 0; k < steps; k++) fn_S7_0A76(r);
    set_w(&r->d[0], g16(0x20D2) & 0x1F0); lsl_w(r, &r->d[0], 1);
    set_w(&r->d[1], g16(0x20CE) & 0x1F0); lsl_w(r, &r->d[1], 6);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    if ((uint16_t)r->d[1] != g16(0x20CA)) {
        r->a[0] = g32(0x1DCC);
        set_w(&r->d[2], g16(0x20CA));
        w16(r->a[0] + sxw(r->d[2]) + 0xC, 0);
        w16(r->a[0] + sxw(r->d[1]) + 0xC, 1);
        sg16(0x20CA, (uint16_t)r->d[1]);
    }
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
    set_w(&r->d[0], g16(0x20FE)); sub_w(r, &r->d[0], mz_original ? 2 : 3);
    if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
    sg16(0x20FE, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fn_S7_0A9C: spins the object at A4 two steps (+$C, of 64; the way by +$1A) and, unless D0.w is
 * negative, moves it in direction D0.w (fn_S7_374C with the step tables G_12EA). D0.l = 0 if it
 * moved, -1 if not. */
void fn_S7_0A9C(Regs *r) {
    set_w(&r->d[1], r16(r->a[4] + 0x1A));
    if (!(uint16_t)r->d[1]) {
        set_w(&r->d[1], r16(r->a[4] + 0xC)); add_w(r, &r->d[1], 2);
        if ((uint16_t)r->d[1] >= 0x40) r->d[1] = 0;
    } else {
        set_w(&r->d[1], r16(r->a[4] + 0xC)); sub_w(r, &r->d[1], 2);
        if ((uint16_t)r->d[1] >= 0x40) set_w(&r->d[1], 0x3F);
    }
    w16(r->a[4] + 0xC, (uint16_t)r->d[1]);
    set_w(&r->d[1], (uint16_t)r->d[0]);
    if ((uint16_t)r->d[1] & 0x8000) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    r->a[0] = GADDR(0x12EA);
    fn_S7_374C(r);
    r->d[0] = 0; cc_nz_l(r, 0);
}

/* fn_S7_3722: fn_S7_36F4 twice (turns two steps). */
void fn_S7_3722(Regs *r) { fn_S7_36F4(r); fn_S7_36F4(r); }

/* fn_S7_0DE2: turns the object at A4 towards D0.w (fn_S7_36F4); once facing that way, steps it
 * (fn_S7_374C, step tables G_0F2A) and commits the move unless an object is in the way
 * (fn_S7_3972, fn_S7_3784). D0.l = 0 while turning, -1 if blocked, else fn_S7_3784's result. */
void fn_S7_0DE2(Regs *r) {
    fn_S7_36F4(r);
    cc_cmp_w(r, r16(r->a[4] + 0xC), (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] != r16(r->a[4] + 0xC)) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    r->a[0] = GADDR(0x0F2A);
    fn_S7_374C(r);
    fn_S7_3972(r);
    if ((uint16_t)r->d[0]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    fn_S7_3784(r);
}

/* fn_S7_0E0A: type $14's model: a random one of the eight frames of G_1D50 (fn_S2_3532). */
void fn_S7_0E0A(Regs *r) {
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 7));
    lsl_w(r, &r->d[0], 2);
    r->a[0] = g32(0x1D50);
    r->a[0] += r32(r->a[0] + sxw(r->d[0]));
    mz_draw_stream(r, r->a[0]);
}

/* fn_S7_0ADE: type $12's model G_1D58 by the angle to the player, as fn_S7_1844. */
void fn_S7_0ADE(Regs *r) { mz_draw_by_angle(r, g32(0x1D58)); }

/* fn_S7_406A: steps the object at A4 by entry D0.w of the step tables at A0 (fn_S7_374C) and
 * commits the move (fn_S7_3784) unless an object is in the way (fn_S7_3972). */
void fn_S7_406A(Regs *r) {
    fn_S7_374C(r);
    fn_S7_3972(r);
    cc_nz_w(r, (uint16_t)r->d[0]);
    if (!(uint16_t)r->d[0]) fn_S7_3784(r);
}

/* fn_S7_3EAC: a local step of fn_S7_3D5C (frame at A6): if the record at D3.w (A2) is free, and
 * its scent (+$10) is at least the best so far (-$12), it becomes the best (-$12, -$14 = D3,
 * -$16 = D4). */
void fn_S7_3EAC(Regs *r) {
    uint32_t rec = r->a[2] + sxw(r->d[3]), fp = r->a[6];
    set_w(&r->d[0], r16(rec + 0xE));
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0]) return;
    set_w(&r->d[0], r16(rec + 0x10));
    cc_cmp_w(r, r16(fp - 0x12), (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] < r16(fp - 0x12)) return;
    w16(fp - 0x12, (uint16_t)r->d[0]);
    w16(fp - 0x14, (uint16_t)r->d[3]);
    w16(fp - 0x16, (uint16_t)r->d[4]);
    cc_nz_w(r, (uint16_t)r->d[4]);
}

/* fn_S7_3D5C: as fn_S7_3B36, but following the scent trail (cell +$10, see fn_S2_355E) instead of
 * the distance map: on the way to the target cell's centre, D0.w is the direction to it, unless
 * the next cell that way is free and has a stronger scent: then that cell becomes the target and
 * D0.w = -1. At the centre: the free neighbour with the strongest scent at least the cell's own
 * (N, W, E, S; later ones win ties), or -1. G_00D0 = D0.w. Keeps D3-D5/A2. */
void fn_S7_3D5C(Regs *r) {
    uint32_t d3 = r->d[3], d4 = r->d[4], d5 = r->d[5], a2 = r->a[2], a6 = r->a[6];
    uint32_t fp = r->a[7] - 4;
    r->a[6] = fp;
    set_w(&r->d[5], r16(r->a[4] + 0xE));
    sg16(0xD2, (uint16_t)r->d[5]);
    r->a[2] = g32(0x1DCC);
    uint32_t cell = r->a[2] + sxw(r->d[5]);
    set_w(&r->d[0], r16(cell + 0x1C));
    sub_w(r, &r->d[0], r16(r->a[4] + 4));
    int along_x = (uint16_t)r->d[0] != 0, neg = ((uint16_t)r->d[0] & 0x8000) != 0;
    if (!along_x) {
        set_w(&r->d[0], r16(cell + 0x1E));
        sub_w(r, &r->d[0], r16(r->a[4] + 8));
        if (!(uint16_t)r->d[0]) goto centre;
        neg = ((uint16_t)r->d[0] & 0x8000) != 0;
    }
    {
        uint16_t step = along_x ? 0x20 : 0x400;
        uint16_t keep = along_x ? (neg ? 0 : 0x20) : (neg ? 0x10 : 0x30);
        set_w(&r->d[3], (uint16_t)r->d[5]);
        if (neg) add_w(r, &r->d[3], step); else sub_w(r, &r->d[3], step);
        set_w(&r->d[0], r16(r->a[2] + sxw(r->d[3]) + 0xE));
        if ((uint16_t)r->d[0]) { set_w(&r->d[0], keep); goto out; }
        set_w(&r->d[0], r16(cell + 0x10));
        uint16_t next = r16(r->a[2] + sxw(r->d[3]) + 0x10);
        cc_cmp_w(r, next, (uint16_t)r->d[0]);
        if ((uint16_t)r->d[0] >= next) { set_w(&r->d[0], keep); goto out; }
        w16(r->a[4] + 0x10, (uint16_t)r->d[5]);
        sg16(0xD2, (uint16_t)r->d[3]);
        set_w(&r->d[0], 0xFFFF);
        goto out;
    }
centre:
    w16(fp - 0x12, r16(cell + 0x10));
    w16(fp - 0x14, 0xFFFF); w16(fp - 0x16, 0xFFFF);
    set_w(&r->d[3], (uint16_t)r->d[5]);
    static const uint16_t dirs[4] = {0x10, 0, 0x20, 0x30};
    for (int k = 0; k < 4; k++) {
        if (k == 0) sub_w(r, &r->d[3], 0x400); else add_w(r, &r->d[3], k == 2 ? 0x40 : 0x3E0);
        set_w(&r->d[4], dirs[k]);
        fn_S7_3EAC(r);
    }
    set_w(&r->d[0], r16(fp - 0x16));
    if (!((uint16_t)r->d[0] & 0x8000)) {
        w16(r->a[4] + 0x10, (uint16_t)r->d[5]);
        sg16(0xD2, r16(fp - 0x14));
        set_w(&r->d[0], r16(fp - 0x16));
    }
out:
    sg16(0xD0, (uint16_t)r->d[0]);
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->a[2] = a2; r->a[6] = a6;
    cc_nz_l(r, d5);   /* flags from restoring D5 */
}

/* fn_S7_0EF0: types $1E/$1F model G_1D8C by the angle to the player, as fn_S7_1844. */
void fn_S7_0EF0(Regs *r) { mz_draw_by_angle(r, g32(0x1D8C)); }

/* fn_S7_20A0: type $28's model G_1D80 by the angle to the player, as fn_S7_1844. */
void fn_S7_20A0(Regs *r) { mz_draw_by_angle(r, g32(0x1D80)); }

/* fn_S7_3694: the direction-map byte (G_1DBC) from the object at A4 to the point G_00DA / G_00D6,
 * or D0.l = -1 where the distance map (G_1DB8) has nothing (0) there. */
void fn_S7_3694(Regs *r) {
    map_offset(r, g16(0xD6), r16(r->a[4] + 8), g16(0xDA), r16(r->a[4] + 4));
    r->a[0] = g32(0x1DB8);
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    cc_nz_l(r, r->d[0]);
    if (!r->d[0]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    r->a[0] = g32(0x1DBC);
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    cc_nz_l(r, r->d[0]);
}

/* fn_S7_2590: as fn_S7_0DE2 with a two-notch turn (fn_S7_3722) and the step table A0. */
void fn_S7_2590(Regs *r) {
    uint32_t steps = r->a[0];
    fn_S7_3722(r);
    cc_cmp_w(r, r16(r->a[4] + 0xC), (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] != r16(r->a[4] + 0xC)) { r->d[0] = 0; cc_nz_l(r, 0); return; }
    r->a[0] = steps;
    fn_S7_374C(r);
    fn_S7_3972(r);
    if ((uint16_t)r->d[0]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    fn_S7_3784(r);
}

/* fn_S7_2600: type $29's model: a random one of the eight frames of G_1D50 (as fn_S7_0E0A). */
void fn_S7_2600(Regs *r) {
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 7));
    lsl_w(r, &r->d[0], 2);
    r->a[0] = g32(0x1D50);
    r->a[0] += r32(r->a[0] + sxw(r->d[0]));
    mz_draw_stream(r, r->a[0]);
}

/* fn_S7_269C: type $2D's model G_1D84 by the angle to the player, as fn_S7_1844. */
void fn_S7_269C(Regs *r) { mz_draw_by_angle(r, g32(0x1D84)); }

/* fn_S7_2AD2: shots $36 / $3D: flying, model G_1D54 frame +$14 & $C; sitting on the player
 * (+$12 > 0), a full-view flash of a random G_1D50 frame (window as effect_window, G_20AA random
 * 0..3). */
void fn_S7_2AD2(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    if ((uint16_t)r->d[0] & 0x8000) {
        r->a[0] = g32(0x1D54);
        set_w(&r->d[0], r16(r->a[4] + 0x14));
        set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0xC));
        r->a[0] += r32(r->a[0] + sxw(r->d[0]));
        mz_draw_stream(r, r->a[0]);
        return;
    }
    if (!(uint16_t)r->d[0]) { cc_nz_w(r, 0); return; }
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3));
    sg16(0x20AA, (uint16_t)r->d[0]);
    set_w(&r->d[0], g16(0x2154)); add_w(r, &r->d[0], 4); sg16(0x20A8, (uint16_t)r->d[0]);
    sg16(0x20A6, 4);
    set_w(&r->d[0], g16(0x2156)); add_w(r, &r->d[0], 4); sg16(0x20A4, (uint16_t)r->d[0]);
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3));
    lsl_w(r, &r->d[0], 2);
    r->a[0] = g32(0x1D50);
    r->a[0] += r32(r->a[0] + sxw(r->d[0]));
    mz_draw_stream(r, r->a[0]);
}
