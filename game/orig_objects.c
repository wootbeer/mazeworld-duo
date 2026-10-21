/* Mazeworld Original's objects: drawing and behaviour of its object types, spread over its
 * segments 4, 7, 9, 10 and 11 (see build/disasm-original), named by Original's offsets. */
#include "mzasm.h"

void fn_S2_3532(Regs *r);

/* the models seen by angle (as Catacombs' fn_S7_1844): one table each */
void fo_S10_0000(Regs *r) { mz_draw_by_angle(r, og32(0x53AC)); }
void fo_S10_21B4(Regs *r) { mz_draw_by_angle(r, og32(0x53A0)); }
void fo_S4_075A(Regs *r)  { mz_draw_by_angle(r, og32(0x5390)); }
void fo_S4_0C1C(Regs *r)  { mz_draw_by_angle(r, og32(0x536C)); }
void fo_S4_122E(Regs *r)  { mz_draw_by_angle(r, og32(0x538C)); }
void fo_S4_1648(Regs *r)  { mz_draw_by_angle(r, og32(0x5384)); }
void fo_S4_1DD2(Regs *r)  { mz_draw_by_angle(r, og32(0x5388)); }
void fo_S7_05DE(Regs *r)  { mz_draw_by_angle(r, og32(0x53A8)); }
void fo_S7_0680(Regs *r)  { mz_draw_by_angle(r, og32(0x53A4)); }

/* ---------- the other models: A0 = a table plus the entry it holds at a byte offset (adda.l) ---------- */
static void entry_at(Regs *r, uint32_t table, uint16_t off, uint32_t disp) {
    r->a[0] = table + r32(table + sxw(off) + disp);
    mz_draw_stream(r, r->a[0]);
}
static void entry(Regs *r, uint32_t table, uint16_t off) { entry_at(r, table, off, 0); }
/* a wrapping frame counter: A4+field + 1, back to 0 at `n` (signed or unsigned compare) */
static uint16_t next_frame(Regs *r, uint32_t field, uint16_t n, int sgn) {
    set_w(&r->d[0], r16(r->a[4] + field)); add_w(r, &r->d[0], 1);
    uint16_t v = (uint16_t)r->d[0];
    if (sgn ? (int16_t)v >= (int16_t)n : v >= n) set_w(&r->d[0], 0);
    w16(r->a[4] + field, (uint16_t)r->d[0]);
    return (uint16_t)r->d[0];
}
/* the view's centre column and the whole width as the clip (G_56B4 / G_56B2 / G_56B0) */
static void full_width(Regs *r) {
    set_w(&r->d[0], og16(0x5736)); add_w(r, &r->d[0], 4); sog16(0x56B4, (uint16_t)r->d[0]);
    sog16(0x56B2, 4);
    set_w(&r->d[0], og16(0x5738)); add_w(r, &r->d[0], 4); sog16(0x56B0, (uint16_t)r->d[0]);
}
/* (G_56DA - A4+$2A [- A4+$C]) & mask, doubled, in D0 (or D1) */
static uint16_t angle(Regs *r, uint32_t *d, int with_c, uint16_t mask) {
    set_w(d, og16(0x56DA)); sub_w(r, d, r16(r->a[4] + 0x2A));
    if (with_c) sub_w(r, d, r16(r->a[4] + 0xC));
    set_w(d, (uint16_t)((uint16_t)*d & mask)); lsl_w(r, d, 1);
    return (uint16_t)*d;
}

/* fo_S7_0000 / fo_S7_003E: a colour cycle (A4+$C over six entries of G_384C / G_3840 into G_56AA)
 * on the first model of G_537C; nothing while G_56E0 is $80 */
static void cycling(Regs *r, uint16_t colours) {
    set_w(&r->d[0], og16(0x56E0));
    cc_cmp_w(r, 0x80, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] == 0x80) return;
    next_frame(r, 0xC, 6, 1);
    lsl_w(r, &r->d[0], 1);
    r->a[0] = OADDR(colours);
    set_w(&r->d[0], r16(r->a[0] + sxw(r->d[0])));
    sog16(0x56AA, (uint16_t)r->d[0]);
    entry(r, og32(0x537C), 0);
}
void fo_S7_0000(Regs *r) { cycling(r, 0x384C); }
void fo_S7_003E(Regs *r) { cycling(r, 0x3840); }
/* frame A4+$12 of G_5358 */
void fo_S7_03E2(Regs *r) { set_w(&r->d[0], r16(r->a[4] + 0x12)); lsl_w(r, &r->d[0], 2); entry(r, og32(0x5358), (uint16_t)r->d[0]); }
/* G_539C by the angle without A4+$C, eight steps */
void fo_S7_0504(Regs *r) { entry(r, og32(0x539C), angle(r, &r->d[1], 0, 0xE)); }
void fo_S7_080C(Regs *r) { entry(r, og32(0x5324), 0); }
/* G_533C, the frame from a 16-step sequence by A4+$3E */
void fo_S7_0820(Regs *r) {
    static const uint16_t seq[16] = {0, 2, 3, 5, 0, 2, 3, 1, 4, 5, 4, 2, 0, 4, 1, 3};
    next_frame(r, 0x3E, 0x10, 0);
    lsl_w(r, &r->d[0], 1);
    set_w(&r->d[0], seq[((uint16_t)r->d[0] >> 1) & 15]);
    lsl_w(r, &r->d[0], 2);
    entry(r, og32(0x533C), (uint16_t)r->d[0]);
}
void fo_S7_08E2(Regs *r) { entry(r, og32(0x5374), 0); }
/* G_5370, frame A4+$C cycling over six */
void fo_S7_08F6(Regs *r) { r->a[0] = og32(0x5370); next_frame(r, 0xC, 6, 0); lsl_w(r, &r->d[0], 2); entry(r, og32(0x5370), (uint16_t)r->d[0]); }
/* G_5310 by the angle, through D0 / A0 */
void fo_S7_0C40(Regs *r) { entry(r, og32(0x5310), angle(r, &r->d[0], 1, 0x3E)); }
/* the first model of G_5358 over the whole width (not while A4+$12 is negative) */
void fo_S7_0C68(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] & 0x8000) return;
    full_width(r);
    entry(r, og32(0x5358), 0);
}
/* frame A4+$C of G_5314 over the whole width */
void fo_S7_0CFC(Regs *r) { full_width(r); set_w(&r->d[0], r16(r->a[4] + 0xC)); lsl_w(r, &r->d[0], 2); entry(r, og32(0x5314), (uint16_t)r->d[0]); }
/* a flame: A4+$12 negative: frame (A4+$14 & $C) of G_5358; 0: nothing; else colour 3 + random 0..3
 * (7 for 1) in G_56B6, the whole width, a random frame of G_5354 */
void fo_S7_0920(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    cc_nz_w(r, (uint16_t)r->d[0]);
    if ((uint16_t)r->d[0] & 0x8000) {
        r->a[0] = og32(0x5358);
        set_w(&r->d[0], (uint16_t)(r16(r->a[4] + 0x14) & 0xC));
        entry(r, og32(0x5358), (uint16_t)r->d[0]);
        return;
    }
    if (!(uint16_t)r->d[0]) return;
    if ((uint16_t)r->d[0] == 1) { cc_cmp_w(r, 1, 1); set_w(&r->d[0], 7); }
    else {
        cc_cmp_w(r, 1, (uint16_t)r->d[0]);
        mz_seg_of(2, 0x3532), fn_S2_3532(r);   /* Original's fn_S2_2B1C: Random */
        set_w(&r->d[0], (uint16_t)(((uint16_t)r->d[0] & 3) + 3));
    }
    sog16(0x56B6, (uint16_t)r->d[0]);
    full_width(r);
    mz_seg_of(2, 0x3532), fn_S2_3532(r);   /* Original's fn_S2_2B1C: Random */
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3)); lsl_w(r, &r->d[0], 2);
    entry(r, og32(0x5354), (uint16_t)r->d[0]);
}
/* G_5370 counting A4+$12 down from 5 */
void fo_S4_0000(Regs *r) {
    r->a[0] = og32(0x5370);
    set_w(&r->d[0], r16(r->a[4] + 0x12)); sub_w(r, &r->d[0], 1);
    if ((uint16_t)r->d[0] & 0x8000) set_w(&r->d[0], 5);
    w16(r->a[4] + 0x12, (uint16_t)r->d[0]);
    lsl_w(r, &r->d[0], 2);
    entry(r, og32(0x5370), (uint16_t)r->d[0]);
}
void fo_S4_21AE(Regs *r) { entry(r, og32(0x5360), angle(r, &r->d[0], 1, 0x3E)); }
void fo_S4_27E2(Regs *r) { entry(r, og32(0x5364), angle(r, &r->d[0], 1, 0x3E)); }
/* G_5368 turning: A4+$C advances by 2 each frame, eight steps through D1 / A1 */
void fo_S4_2ABA(Regs *r) {
    r->a[1] = og32(0x5368);
    set_w(&r->d[1], r16(r->a[4] + 0xC)); add_w(r, &r->d[1], 2);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] & 0x3F)); w16(r->a[4] + 0xC, (uint16_t)r->d[1]);
    uint16_t off = angle(r, &r->d[1], 1, 0xE);
    r->a[1] += r32(r->a[1] + sxw(off));
    mz_draw_stream(r, r->a[1]);
}
/* fo_S10_18F0 / 1924: colour (from three by A4+$12) into G_56AA, frame A4+$C of G_5398 */
static void coloured(Regs *r, const uint16_t *colours) {
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    set_w(&r->d[0], colours[((uint16_t)r->d[0] >> 1) % 3]);
    sog16(0x56AA, (uint16_t)r->d[0]);
    set_w(&r->d[0], r16(r->a[4] + 0xC)); lsl_w(r, &r->d[0], 2);
    entry(r, og32(0x5398), (uint16_t)r->d[0]);
}
void fo_S10_18F0(Regs *r) { static const uint16_t c[3] = {0x23, 0x24, 0x0F}; coloured(r, c); }
void fo_S10_1924(Regs *r) { static const uint16_t c[3] = {0x05, 0x2B, 0x06}; coloured(r, c); }
void fo_S10_1958(Regs *r) { set_w(&r->d[0], r16(r->a[4] + 0x16)); lsl_w(r, &r->d[0], 2); entry(r, og32(0x5344), (uint16_t)r->d[0]); }
/* G_5348: frame A4+$C, or entry $50 while A4+$16 is negative; centred on the view when the
 * object is in the player's spot */
void fo_S9_0000(Regs *r) {
    uint32_t t = og32(0x5348);
    r->a[0] = t;
    set_w(&r->d[0], r16(r->a[4] + 0x16));
    if ((uint16_t)r->d[0] & 0x8000) r->a[0] = t + r32(t + 0x50);
    else { set_w(&r->d[0], r16(r->a[4] + 0xC)); lsl_w(r, &r->d[0], 2); r->a[0] = t + r32(t + sxw(r->d[0])); }
    sog32(0x56AE, r->a[0]);
    set_w(&r->d[0], r16(r->a[4] + 4));
    if ((uint16_t)r->d[0] == og16(0x56D8)) {
        set_w(&r->d[0], r16(r->a[4] + 8));
        if ((uint16_t)r->d[0] == og16(0x56D4)) { set_w(&r->d[0], og16(0x5736)); add_w(r, &r->d[0], 4); sog16(0x56B4, (uint16_t)r->d[0]); }
    }
    mz_draw_stream(r, r->a[0]);
}
void fo_S9_034A(Regs *r) { entry(r, og32(0x535C), angle(r, &r->d[0], 1, 0x3E)); }
/* the first model of G_5340 in colour (A4+$18: $3F..$42), then frame A4+$1A + 1 unless negative */
void fo_S9_0D2E(Regs *r) {
    static const uint16_t colour[4] = {0x3F, 0x40, 0x41, 0x42};
    set_w(&r->d[0], r16(r->a[4] + 0x18));
    sog16(0x56AA, colour[((uint16_t)r->d[0] >> 1) & 3]);
    entry(r, og32(0x5340), 0);
    set_w(&r->d[0], r16(r->a[4] + 0x1A));
    if ((uint16_t)r->d[0] & 0x8000) return;
    lsl_w(r, &r->d[0], 2);
    entry_at(r, og32(0x5340), (uint16_t)r->d[0], 4);
}
/* G_5330 by the angle in colour $4B + A4+$10 (cycling over 14) */
void fo_S9_1C6A(Regs *r) {
    next_frame(r, 0x10, 0xE, 0);
    add_w(r, &r->d[0], 0x4B); sog16(0x56AA, (uint16_t)r->d[0]);
    entry(r, og32(0x5330), angle(r, &r->d[0], 1, 0x3E));
}
/* G_532C: the first model while A4+$C is 0, else model 1 + min((A4+8 - $15B) / 2, 4) */
void fo_S9_202C(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0xC));
    if (!(uint16_t)r->d[0]) { entry(r, og32(0x532C), 0); return; }
    set_w(&r->d[0], r16(r->a[4] + 8)); sub_w(r, &r->d[0], 0x15B); lsr_w(r, &r->d[0], 1);
    if ((uint16_t)r->d[0] >= 5) set_w(&r->d[0], 4);
    lsl_w(r, &r->d[0], 2);
    entry_at(r, og32(0x532C), (uint16_t)r->d[0], 4);
}
/* fo_S10_2ABA / 2B26: the twelve parts listed in the object table's words from $1E (a high byte
 * direction, a low byte frame under $11), those facing the viewer (quarter turns 0-3 and 12-15:
 * 2ABA) or facing away (4-11: 2B26), from G_5320 in colour 4 / $8D */
static void parts(Regs *r, uint16_t colour, int away) {
    sog16(0x56AA, colour);
    r->a[2] = og32(0x53C8) + 0x1E;
    for (r->d[3] = 0; (uint16_t)r->d[3] < 0x18; set_w(&r->d[3], (uint16_t)(r->d[3] + 2))) {
        uint16_t w = r16(r->a[2] + sxw(r->d[3]));
        set_w(&r->d[0], (uint16_t)((w >> 8) << 2));
        set_w(&r->d[2], og16(0x56DA)); sub_w(r, &r->d[2], r16(r->a[4] + 0x2A)); sub_w(r, &r->d[2], (uint16_t)r->d[0]);
        lsr_w(r, &r->d[2], 2);
        set_w(&r->d[2], (uint16_t)((uint16_t)r->d[2] & 0xF));
        int facing = (uint16_t)r->d[2] < 4 || (uint16_t)r->d[2] >= 0xC;
        if (facing == away) continue;
        set_w(&r->d[0], (uint16_t)((uint16_t)r->d[2] << 4)); set_w(&r->d[2], (uint16_t)((uint16_t)r->d[2] + (uint16_t)r->d[0]));
        set_w(&r->d[0], (uint16_t)(w & 0xFF));
        if ((uint16_t)r->d[0] >= 0x11) continue;
        set_w(&r->d[2], (uint16_t)((uint16_t)r->d[2] + (uint16_t)r->d[0]));
        lsl_w(r, &r->d[2], 2);
        r->a[0] = og32(0x5320); r->a[0] += r32(r->a[0] + sxw(r->d[2]));
        mz_draw_stream(r, r->a[0]);
    }
    r->ccr = CCR_Z;                                           /* addq: no carry; cmpi #$18 equal */
}
void fo_S10_2ABA(Regs *r) { parts(r, 4, 0); }
void fo_S10_2B26(Regs *r) { parts(r, 0x8D, 1); }
/* G_5350 by the angle (no A4+$2A), colour $2D one time in 16 (always while A4+$20 is negative),
 * the whole height when A4+$1A is set */
void fo_S10_2C2C(Regs *r) {
    set_w(&r->d[0], r16(r->a[4] + 0x20));
    int lit = (uint16_t)r->d[0] & 0x8000;
    if (!lit) { mz_seg_of(2, 0x3532), fn_S2_3532(r); set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0xF)); lit = !(uint16_t)r->d[0]; }
    if (lit) sog16(0x56AA, 0x2D);
    set_w(&r->d[0], og16(0x56DA)); sub_w(r, &r->d[0], r16(r->a[4] + 0xC));
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0x3E)); lsl_w(r, &r->d[0], 1);
    uint32_t t = og32(0x5350);
    r->a[0] = t + r32(t + sxw(r->d[0]));
    sog32(0x56AE, r->a[0]);
    set_w(&r->d[0], r16(r->a[4] + 0x1A));
    if ((uint16_t)r->d[0]) { sog16(0x56B2, 4); set_w(&r->d[0], og16(0x5738)); add_w(r, &r->d[0], 4); sog16(0x56B0, (uint16_t)r->d[0]); }
    mz_draw_stream(r, r->a[0]);
}
/* G_5318 by the angle (sixteen), colour $4A while A4+$12 is positive; while it is negative, entry
 * $40 too, in a colour cycling through $27..$2A by A4+$16 */
void fo_S11_0000(Regs *r) {
    static const uint16_t colour[5] = {0x10, 0x27, 0x28, 0x29, 0x2A};
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    if ((int16_t)r->d[0] > 0) sog16(0x56AA, 0x4A);
    entry(r, og32(0x5318), angle(r, &r->d[0], 1, 0x1E));
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    if (!((uint16_t)r->d[0] & 0x8000)) return;
    set_w(&r->d[0], r16(r->a[4] + 0x16));
    sog16(0x56AA, colour[((uint16_t)r->d[0] >> 1) % 5]);
    add_w(r, &r->d[0], 2);
    if ((uint16_t)r->d[0] >= 9) set_w(&r->d[0], 2);
    w16(r->a[4] + 0x16, (uint16_t)r->d[0]);
    entry_at(r, og32(0x5318), 0, 0x40);
}
/* the first model of G_531C, then (while A4+$12) a frame A4+$C moved on by 2 + random 0..3 */
void fo_S11_007A(Regs *r) {
    entry(r, og32(0x531C), 0);
    set_w(&r->d[0], r16(r->a[4] + 0x12));
    if (!(uint16_t)r->d[0]) return;
    mz_seg_of(2, 0x3532), fn_S2_3532(r);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3)); add_w(r, &r->d[0], r16(r->a[4] + 0xC)); add_w(r, &r->d[0], 2);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0xF)); w16(r->a[4] + 0xC, (uint16_t)r->d[0]);
    lsl_w(r, &r->d[0], 2);
    entry_at(r, og32(0x531C), (uint16_t)r->d[0], 4);
}
/* the first model of G_5378; while A4+$1A is positive, G_56B6 grows by its half and quarter (the
 * part's distance) and the object A4 + A4+$18 (unless bit 14 of its +$18) is drawn by its type
 * ($1E, $20, $23, $2D) in its colour +$3A; while negative, frame 1 + (A4+$C cycling over five) */
void fo_S4_03F8(Regs *r) {
    entry(r, og32(0x5378), 0);
    set_w(&r->d[0], r16(r->a[4] + 0x1A));
    if (!(uint16_t)r->d[0]) return;
    if ((uint16_t)r->d[0] & 0x8000) {
        next_frame(r, 0xC, 5, 1);
        add_w(r, &r->d[0], 1); lsl_w(r, &r->d[0], 2);
        entry(r, og32(0x5378), (uint16_t)r->d[0]);
        return;
    }
    set_w(&r->d[1], og16(0x56B6)); lsr_w(r, &r->d[1], 1);
    uint32_t v = og16(0x56B6); add_w(r, &v, (uint16_t)r->d[1]); sog16(0x56B6, (uint16_t)v);
    lsr_w(r, &r->d[1], 1);
    v = og16(0x56B6); add_w(r, &v, (uint16_t)r->d[1]); sog16(0x56B6, (uint16_t)v);
    r->a[1] = r->a[4] + r16(r->a[4] + 0x18);
    r->d[0] = r16(r->a[1] + 0x18);
    if (r->d[0] & 0x4000) return;
    sog16(0x56AA, r16(r->a[1] + 0x3A));
    set_w(&r->d[0], r16(r->a[1] + 2));
    switch ((uint16_t)r->d[0]) {
    case 0x1E: fo_S4_075A(r); break;
    case 0x20: fo_S4_122E(r); break;
    case 0x23: fo_S4_1648(r); break;
    case 0x2D: fo_S4_1DD2(r); break;
    }
}
/* by A4+$18: bit 4: the whole width, model $40 in colour A4+$1A (from 2), then model $C0 in colour
 * $2F, or (A4+$1A under 2) model $40 + 4 x (A4+$1A ^ 1) in colours 7 / $2F; else bit 0: model by
 * the angle (colour $30 with bit 1), and bit 2: model $80 + the angle (colour $2F, $31 with bit 3) */
void fo_S10_3EF8(Regs *r) {
    uint32_t t = og32(0x5394);
    set_w(&r->d[0], r16(r->a[4] + 0x18));
    if (r->d[0] & 0x10) {
        full_width(r);
        set_w(&r->d[0], r16(r->a[4] + 0x1A));
        if ((uint16_t)r->d[0] >= 2) {
            sog16(0x56B6, (uint16_t)r->d[0]);
            entry_at(r, t, 0, 0x40);
            sog16(0x56AA, 0x2F);
            entry_at(r, t, 0, 0xC0);
        } else {
                    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] ^ 1)); lsl_w(r, &r->d[0], 2);
            add_w(r, &r->d[0], 0x100);
            r->a[0] = t + r32(t + sxw(r->d[0]));
            sog32(0x56AE, r->a[0]);
            sog16(0x56B6, 7); sog16(0x56AA, 0x2F);
            mz_draw_stream(r, r->a[0]);
        }
        return;
    }
    if (r->d[0] & 1) {
        if (r->d[0] & 2) sog16(0x56AA, 0x30);
        entry(r, t, angle(r, &r->d[0], 1, 0x3E));
    }
    set_w(&r->d[0], r16(r->a[4] + 0x18));
    if (!(r->d[0] & 4)) return;
    sog16(0x56AA, (r->d[0] & 8) ? 0x31 : 0x2F);
    angle(r, &r->d[0], 1, 0x3E);
    add_w(r, &r->d[0], 0x80);
    entry(r, t, (uint16_t)r->d[0]);
}
/* G_5314 by the angle; colour: A4+$18 under 4 changes at random to another of $A3..$A6, from 4
 * on it picks from a table */
void fo_S11_0124(Regs *r) {
    static const uint16_t colour[9] = {0xA4, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAC, 0xAD};
    set_w(&r->d[0], r16(r->a[4] + 0x18));
    if ((uint16_t)r->d[0] < 4) {
        mz_seg_of(2, 0x3532), fn_S2_3532(r);
        set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3));
        if ((uint16_t)r->d[0] == r16(r->a[4] + 0x18)) { add_w(r, &r->d[0], 1); set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 3)); }
        w16(r->a[4] + 0x18, (uint16_t)r->d[0]);
        add_w(r, &r->d[0], 0xA3); sog16(0x56AA, (uint16_t)r->d[0]);
    } else {
        sub_w(r, &r->d[0], 4); lsl_w(r, &r->d[0], 1);
        sog16(0x56AA, colour[((uint16_t)r->d[0] >> 1) % 9]);
    }
    entry(r, og32(0x5314), angle(r, &r->d[0], 1, 0x3E));
}

/* fo_S9_15BE / fo_S9_1962: G_5338 by the angle (turned half round; 1962 a further quarter) and the
 * pose A4+$C, through a table of model offsets (the two keep their tables in their code) */
static const uint16_t pose_15BE[160] = {
    0x0008, 0x0008, 0x0008, 0x000C, 0x000C, 0x000C, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008,
    0x001C, 0x001C, 0x001C, 0x0020, 0x0020, 0x0020, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024,
    0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0018, 0x0018, 0x0018, 0x001C, 0x001C,
    0x0030, 0x0030, 0x0030, 0x0034, 0x0034, 0x0034, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038,
    0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x002C, 0x002C, 0x002C, 0x0030, 0x0030,
    0x0044, 0x0044, 0x0044, 0x0048, 0x0048, 0x0048, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C,
    0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x0040, 0x0040, 0x0040, 0x0044, 0x0044,
    0x0058, 0x0058, 0x0058, 0x005C, 0x005C, 0x005C, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060,
    0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0054, 0x0054, 0x0054, 0x0058, 0x0058,
};
static const uint16_t pose_1962[160] = {
    0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000C, 0x000C, 0x000C, 0x0008, 0x0008,
    0x001C, 0x001C, 0x001C, 0x0018, 0x0018, 0x0018, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014,
    0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0024, 0x0020, 0x0020, 0x0020, 0x001C, 0x001C,
    0x0030, 0x0030, 0x0030, 0x002C, 0x002C, 0x002C, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028,
    0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0038, 0x0034, 0x0034, 0x0034, 0x0030, 0x0030,
    0x0044, 0x0044, 0x0044, 0x0040, 0x0040, 0x0040, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C, 0x003C,
    0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x004C, 0x0048, 0x0048, 0x0048, 0x0044, 0x0044,
    0x0058, 0x0058, 0x0058, 0x0054, 0x0054, 0x0054, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050,
    0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x005C, 0x005C, 0x005C, 0x0058, 0x0058,
};
static void posed(Regs *r, const uint16_t *pose, uint16_t extra) {
    set_w(&r->d[0], og16(0x56DA)); sub_w(r, &r->d[0], r16(r->a[4] + 0x2A));
    if (extra) sub_w(r, &r->d[0], extra);
    set_w(&r->d[0], (uint16_t)(((uint16_t)r->d[0] & 0x3E) ^ 0x20));
    set_w(&r->d[1], r16(r->a[4] + 0xC)); lsl_w(r, &r->d[1], 6);
    add_w(r, &r->d[0], (uint16_t)r->d[1]);
    set_w(&r->d[0], pose[((uint16_t)r->d[0] >> 1) % 160]);
    entry(r, og32(0x5338), (uint16_t)r->d[0]);
}
void fo_S9_15BE(Regs *r) { posed(r, pose_15BE, 0); }
void fo_S9_1962(Regs *r) { posed(r, pose_1962, 0x30); }

/* fo_S9_1D90 / fo_S11_02CC: G_5328 by the angle in colour [(A4+$18 & ~1) + A4+$1A], then model $80
 * in colour [A4+$18 & ~1] (two colour tables each, words 8.. and 0..) */
static const uint16_t colours_1D90[40] = {
    0x005E, 0x005F, 0x005E, 0x005D, 0x005C, 0x005B, 0x005C, 0x005D, 0x0059, 0x0060, 0x0061, 0x0062, 0x0063, 0x0062, 0x0061, 0x0060,
    0x0059, 0x0064, 0x0065, 0x0066, 0x0067, 0x0066, 0x0065, 0x0064, 0x0059, 0x0068, 0x0069, 0x006A, 0x006B, 0x006A, 0x0069, 0x0068,
    0x0059, 0x006C, 0x006D, 0x006E, 0x006F, 0x006E, 0x006D, 0x006C,
};
static const uint16_t colours_02CC[40] = {
    0x0088, 0x0089, 0x0088, 0x0087, 0x0086, 0x0085, 0x0086, 0x0087, 0x0059, 0x0060, 0x0061, 0x0062, 0x0063, 0x0062, 0x0061, 0x0060,
    0x0059, 0x0064, 0x0065, 0x0066, 0x0067, 0x0066, 0x0065, 0x0064, 0x0059, 0x0068, 0x0069, 0x006A, 0x006B, 0x006A, 0x0069, 0x0068,
    0x0059, 0x006C, 0x006D, 0x006E, 0x006F, 0x006E, 0x006D, 0x006C,
};
static void two_part(Regs *r, const uint16_t *colours) {
    set_w(&r->d[0], (uint16_t)(r16(r->a[4] + 0x18) & 0xFFFE)); add_w(r, &r->d[0], r16(r->a[4] + 0x1A));
    sog16(0x56AA, colours[8 + (((uint16_t)r->d[0] >> 1) % 32)]);
    entry(r, og32(0x5328), angle(r, &r->d[0], 1, 0x3E));
    set_w(&r->d[0], (uint16_t)(r16(r->a[4] + 0x18) & 0xFFFE));
    sog16(0x56AA, colours[((uint16_t)r->d[0] >> 1) % 40]);
    entry_at(r, og32(0x5328), 0, 0x80);
}
void fo_S9_1D90(Regs *r) { two_part(r, colours_1D90); }
void fo_S11_02CC(Regs *r) { two_part(r, colours_02CC); }

/* ---------- fo_S7_0D32 (JT 87): draws object A0 (G_3834 = D1.w, its record offset) by type D0.w
 * (Catacombs: fn_S7_2D1C). The case routine gets D0 = 2 x type + its jump-table word. Keeps
 * D3-D7/A2-A4. ---------- */
void fo_S10_0000(Regs *r);
void fo_S10_18F0(Regs *r);
void fo_S10_1924(Regs *r);
void fo_S10_1958(Regs *r);
void fo_S10_21B4(Regs *r);
void fo_S10_2ABA(Regs *r);
void fo_S10_2B26(Regs *r);
void fo_S10_2C2C(Regs *r);
void fo_S10_3EF8(Regs *r);
void fo_S11_0000(Regs *r);
void fo_S11_007A(Regs *r);
void fo_S11_0124(Regs *r);
void fo_S11_02CC(Regs *r);
void fo_S4_0000(Regs *r);
void fo_S4_03F8(Regs *r);
void fo_S4_075A(Regs *r);
void fo_S4_0C1C(Regs *r);
void fo_S4_122E(Regs *r);
void fo_S4_1648(Regs *r);
void fo_S4_1DD2(Regs *r);
void fo_S4_21AE(Regs *r);
void fo_S4_27E2(Regs *r);
void fo_S4_2ABA(Regs *r);
void fo_S7_0000(Regs *r);
void fo_S7_003E(Regs *r);
void fo_S7_03E2(Regs *r);
void fo_S7_0504(Regs *r);
void fo_S7_05DE(Regs *r);
void fo_S7_0680(Regs *r);
void fo_S7_080C(Regs *r);
void fo_S7_0820(Regs *r);
void fo_S7_08E2(Regs *r);
void fo_S7_08F6(Regs *r);
void fo_S7_0920(Regs *r);
void fo_S7_0C40(Regs *r);
void fo_S7_0C68(Regs *r);
void fo_S7_0CFC(Regs *r);
void fo_S9_0000(Regs *r);
void fo_S9_034A(Regs *r);
void fo_S9_0D2E(Regs *r);
void fo_S9_15BE(Regs *r);
void fo_S9_1962(Regs *r);
void fo_S9_1C6A(Regs *r);
void fo_S9_1D90(Regs *r);
void fo_S9_202C(Regs *r);
static const struct { uint16_t d0; void (*fn)(Regs *); } draw_case[63] = {
    {0x0082, fo_S7_0504}, {0x008A, fo_S7_05DE}, {0x0092, fo_S7_0680},
    {0x009A, fo_S10_0000}, {0x009A, fo_S10_0000}, {0x009A, fo_S10_0000},
    {0x00A2, fo_S10_18F0}, {0x00AA, fo_S10_1924}, {0x00AA, fo_S10_1924},
    {0x00B2, fo_S10_21B4}, {0x00BA, fo_S10_2ABA}, {0x00C2, fo_S7_0000},
    {0x00CA, fo_S7_08E2}, {0x00D2, fo_S4_0000}, {0x00DA, fo_S7_08F6},
    {0x00E2, fo_S4_03F8}, {0x00EA, fo_S7_003E}, {0x00F2, fo_S9_0000},
    {0x00FA, fo_S9_034A}, {0x0102, fo_S7_0820}, {0x010A, fo_S10_2C2C},
    {0x0112, fo_S11_0000}, {0x011A, fo_S11_007A}, {0x0122, fo_S7_03E2},
    {0x0122, fo_S7_03E2}, {0x0122, fo_S7_03E2}, {0x0122, fo_S7_03E2},
    {0x012A, fo_S10_1958}, {0x00A2, fo_S10_18F0}, {0x0132, fo_S10_3EF8},
    {0x013A, fo_S4_075A}, {0x0142, fo_S4_0C1C}, {0x014A, fo_S4_122E},
    {0x007E, 0}, {0x0152, fo_S7_0920}, {0x015A, fo_S4_1648},
    {0x0162, fo_S9_0D2E}, {0x016A, fo_S7_080C}, {0x00A2, fo_S10_18F0},
    {0x00A2, fo_S10_18F0}, {0x0172, fo_S9_15BE}, {0x017A, fo_S9_1962},
    {0x0152, fo_S7_0920}, {0x0182, fo_S9_1C6A}, {0x0182, fo_S9_1C6A},
    {0x018A, fo_S4_1DD2}, {0x0192, fo_S4_21AE}, {0x019A, fo_S7_0C40},
    {0x01A2, fo_S4_27E2}, {0x01AA, fo_S4_2ABA}, {0x01B2, fo_S9_1D90},
    {0x007E, 0}, {0x01BA, fo_S9_202C}, {0x0132, fo_S10_3EF8},
    {0x0152, fo_S7_0920}, {0x01C2, fo_S7_0C68}, {0x01CA, fo_S11_0124},
    {0x01D2, fo_S11_02CC}, {0x007E, 0}, {0x01DA, fo_S7_0CFC},
    {0x00AA, fo_S10_1924}, {0x0152, fo_S7_0920}, {0x01E2, fo_S10_2B26},
};
void fo_S7_0D32(Regs *r) {
    uint32_t d3 = r->d[3], d4 = r->d[4], d5 = r->d[5], d6 = r->d[6], d7 = r->d[7];
    uint32_t a2 = r->a[2], a3 = r->a[3], a4 = r->a[4];
    r->a[4] = r->a[0];
    sog16(0x3834, (uint16_t)r->d[1]);
    uint16_t type = (uint16_t)r->d[0];
    add_w(r, &r->d[0], (uint16_t)r->d[0]);
    if (type < 63) {
        add_w(r, &r->d[0], (uint16_t)(draw_case[type].d0 - 2 * type));
        if (draw_case[type].fn) draw_case[type].fn(r);
    }
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->d[6] = d6; r->d[7] = d7;
    r->a[2] = a2; r->a[3] = a3; r->a[4] = a4;
    cc_nz_l(r, d7);
}

/* ---------- Mazeworld Original's segment 6: the texts, kept in the code segment after these two
 * entry points as 12-byte records from offset $42 { word id; ...; word at +10: the text's offset
 * from there } ending with id $FFFF, then the C strings ---------- */
static void find_text(Regs *r) {
    r->a[0] = mz_seg_base(6) + 0x42;
    for (;;) {
        set_w(&r->d[1], r16(r->a[0]));
        if ((uint16_t)r->d[1] == 0xFFFF || (uint16_t)r->d[1] == (uint16_t)r->d[0]) break;
        r->a[0] += 0xC;
    }
}
/* fo_S6_0000 (JT 128): A0 = the record of text D0.w (or the end record). */
void fo_S6_0000(Regs *r) {
    r->d[2] = 0;                                              /* moveq */
    find_text(r);
    cc_nz_w(r, 0);                                            /* tst.w d2 */
}
/* fo_S6_0004 (JT 129): A0 = text D0.w (the end record's for an unknown one), D0.l = its length. */
void fo_S6_0004(Regs *r) {
    r->d[2] = 1;
    find_text(r);
    r->a[0] += 0xA;
    r->d[1] = r16(r->a[0]);
    r->a[0] += r->d[1];
    r->a[1] = r->a[0];
    r->d[0] = 0;
    for (;;) { uint8_t c = r8(r->a[1]++); set_b(&r->d[1], c); if (!c) break; r->d[0]++; r->ccr &= (uint8_t)~CCR_X; }   /* addq: no carry */
    cc_nz_b(r, 0);
}
