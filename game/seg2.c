/* CODE segment 2: functions reimplemented so far (see build/disasm/seg2.s). */
#include "mzasm.h"
#include "fns.h"
#include <string.h>

void fn_S2_39F8(Regs *r);

/* fn_S2_3502: fills the 1024-entry random table at G_1CA0 and resets its read index G_14A0. */
void fn_S2_3502(void) {
    for (int16_t i = 0; i < 0x400; i++) w16(GADDR(0x1CA0) + 2u * (uint32_t)i, (uint16_t)mac_Random());
    sg16(0x14A0, 0);
}

/* ---------- hand-written assembly routines (register interfaces, see game/mzasm.h) ---------- */

/* fn_S2_3532 (JT 83): next value from the 1024-entry random table G_1CA0 (index G_14A0 wraps);
 * D0 = 1 while G_21FC is set. No frame; assembly callers see A0 = the table and D1.w = the new index,
 * so the register version is the implementation and fn_S2_3532_value is the C-facing form. */
void fn_S2_3532(Regs *r) {
    if (mz_original) {                                   /* Original (fn_S2_2B1C): D0.w = 1 or Random() */
        set_w(&r->d[0], g8(0x21FC) ? 1 : (uint16_t)mac_Random());
        cc_nz_w(r, (uint16_t)r->d[0]);
        return;
    }
    if (g8(0x21FC)) { r->d[0] = 1; cc_nz_l(r, 1); return; }
    r->a[0] = GADDR(0x1CA0);
    set_w(&r->d[1], g16(0x14A0));
    lsl_w(r, &r->d[1], 1);
    set_w(&r->d[0], r16(r->a[0] + (uint32_t)(int32_t)(int16_t)r->d[1]));
    uint16_t i = g16(0x14A0), n = (uint16_t)(i + 1);
    set_w(&r->d[1], n);
    r->ccr = (uint8_t)(n == 0 ? CCR_X : 0);             /* addq.w carry */
    if (n >= 0x400) r->d[1] = 0;                         /* moveq #0 */
    sg16(0x14A0, (uint16_t)r->d[1]);
    cc_nz_w(r, (uint16_t)r->d[1]);
}
int16_t fn_S2_3532_value(void) { Regs r = {{0}, {0}, 0}; fn_S2_3532(&r); return (int16_t)r.d[0]; }

/* fn_S2_33A4 (JT 78): adds a byte to a zero-terminated list of at most 10 entries.
 * In:  D0.w = value << 6 (the byte stored is D0.w >> 6), D1.w = offset of the list in the G_1DCC block
 *      (the list's bytes start one past that offset).
 * Out: D0.l = 0 and the value appended (new terminator after it), or -1 if all 10 entries are in use;
 *      D1.l = offset zero-extended; A0 = address of the entry before the slot used (or of the 9th);
 *      D2.b = the last byte read; CCR from the final moveq, X from the shift. */
void fn_S2_33A4(Regs *r) {
    lsr_w(r, &r->d[0], 6);
    r->d[1] &= 0xFFFF;
    r->a[0] = g32(0x1DCC) + r->d[1];
    for (int i = 0; i < 10; i++) {
        if (i) r->a[0]++;
        set_b(&r->d[2], r8(r->a[0] + 1));
        if ((uint8_t)r->d[2] == 0) {
            w8(r->a[0] + 1, (uint8_t)r->d[0]);
            w8(r->a[0] + 2, 0);
            r->d[0] = 0; cc_nz_l(r, 0);
            return;
        }
    }
    r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]);
}

/* fn_S2_3410 (JT 79): removes a byte from the same kind of list (first match among the first 10
 * entries), moving the rest down including the terminator.
 * In:  D0.w = value << 6, D1.w = list offset in the G_1DCC block.
 * Out: D0.l = 0 if found and removed, -1 if not; D1.l zero-extended; A0 = address where the
 *      terminator was copied (found) or of the 9th entry (not found); CCR from the final moveq. */
void fn_S2_3410(Regs *r) {
    lsr_w(r, &r->d[0], 6);
    r->d[1] &= 0xFFFF;
    r->a[0] = g32(0x1DCC) + r->d[1];
    for (int i = 0; i < 10; i++) {
        if (i) r->a[0]++;
        cc_cmp_b(r, r8(r->a[0] + 1), (uint8_t)r->d[0]);
        if (r->ccr & CCR_Z) {
            for (;;) {   /* move.b 2(a0),1(a0) until the terminator has moved */
                uint8_t b = r8(r->a[0] + 2);
                w8(r->a[0] + 1, b);
                if (!b) break;
                r->a[0]++;
            }
            r->d[0] = 0; cc_nz_l(r, 0);
            return;
        }
    }
    r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]);
}

/* fn_S2_3480 (JT 80): refreshes the derived fields of one record in the G_1DC4 block.
 * In:  D0.w = record offset.
 * Fields written: +$30 = ((hi word of +8) & $1F0) << 6 | ((hi word of +4) & $1F0) << 1 (16-bit);
 *      +$3A/+$38/+$2E/+$34/+$32/+$36/+$3C from seven word tables indexed by the word at +2;
 *      +$26 = -1.
 * Out: A0 = record address, A1 = the last table (G_0366), D0.w = the +$30 value (upper word = low word
 *      of the long at +8), D1.w = the +4 part, D2.w = index * 2; CCR: N from the final move.w #-1,
 *      X from the index doubling. */
void fn_S2_3480(Regs *r) {
    static const uint16_t table[7] = {0x065A, 0x05DC, 0x055E, 0x04E0, 0x0462, 0x03E4, 0x0366};
    static const uint8_t  field[7] = {0x3A, 0x38, 0x2E, 0x34, 0x32, 0x36, 0x3C};
    r->d[0] &= 0xFFFF;
    r->a[0] = g32(0x1DC4) + r->d[0];
    uint32_t v = r32(r->a[0] + 4);
    r->d[0] = (v << 16) | (v >> 16);                                   /* swap */
    set_w(&r->d[0], (uint16_t)(((uint16_t)r->d[0] & 0x1F0) << 1));
    set_w(&r->d[1], (uint16_t)r->d[0]);
    v = r32(r->a[0] + 8);
    r->d[0] = (v << 16) | (v >> 16);
    set_w(&r->d[0], (uint16_t)((((uint16_t)r->d[0] & 0x1F0) << 6) | (uint16_t)r->d[1]));
    w16(r->a[0] + 0x30, (uint16_t)r->d[0]);
    set_w(&r->d[2], r16(r->a[0] + 2));
    lsl_w(r, &r->d[2], 1);
    for (int k = 0; k < 7; k++) {
        r->a[1] = GADDR(table[k]);
        w16(r->a[0] + field[k], r16(r->a[1] + (uint32_t)(int32_t)(int16_t)r->d[2]));
    }
    w16(r->a[0] + 0x26, 0xFFFF);
    cc_nz_w(r, 0xFFFF);
}

/* fn_S2_3500 (JT 81): empty (a lone rts). Mazeworld Original calls fo_S2_2AD2 in its place. */
void fo_S2_2AD2(Regs *r);
void fn_S2_3500(Regs *r) { if (mz_original) fo_S2_2AD2(r); }

/* fn_S2_39F8 (JT 90): sets ten flags: G_2104 G_2100 G_20FF G_20FA G_20F4 G_20EE G_20ED G_20E2 G_20DE G_20DD = 1. */
void fn_S2_39F8(Regs *r) {
    static const uint16_t flag[10] = {0x2104, 0x2100, 0x20FF, 0x20FA, 0x20F4, 0x20EE, 0x20ED, 0x20E2, 0x20DE, 0x20DD};
    static const uint16_t orig[6] = {0x56F0, 0x56EC, 0x56EB, 0x56E8, 0x56E4, 0x56E3};   /* Original's six (fn_S2_2FB4) */
    if (mz_original) for (int i = 0; i < 6; i++) sog8(orig[i], 1);
    else for (int i = 0; i < 10; i++) sg8(flag[i], 1);
    cc_nz_b(r, 1);
}

/* ---------- compiled C ---------- */

/* fn_S2_0252 (JT 106): length of a zero-terminated string. */
int16_t fn_S2_0252(uint32_t s) {
    int16_t n = 0;
    while (r8(s)) { n++; s++; }
    return n;
}

/* fn_S2_0332 (JT 107): copies the Pascal string src (length byte and text) to dst. */
void fn_S2_0332(uint32_t src, uint32_t dst) {
    int16_t len = r8(src);
    for (int16_t i = 0; i <= len; i++) w8(dst++, r8(src++));
}

/* fn_S2_39B8 (JT 89): sets the ten flags (fn_S2_39F8) and resets: G_2102 = 0; G_20FE, G_20FC, G_20F8,
 * G_20F6, G_20F2, G_20F0 = $8E; G_20EC = 1; G_20EA, G_20E6 (longs) = 0; G_20DC = 0. */
void fn_S2_39B8(Regs *r) {
    static const uint16_t w8e[6] = {0x20FE, 0x20FC, 0x20F8, 0x20F6, 0x20F2, 0x20F0};
    fn_S2_39F8(r);
    sg16(0x2102, 0);
    for (int i = 0; i < 6; i++) sg16(w8e[i], 0x8E);
    sg16(0x20EC, 1); sg32(0x20EA, 0); sg32(0x20E6, 0); sg16(0x20DC, 0);
    cc_nz_w(r, 0);
}

/* fn_S2_4DC4 (JT 86): merges this frame's span lists into the previous frame's, row by row.
 * The two row-list buffers G_1DF0 and G_1DEC are swapped first; then for each of G_2150 rows
 * ($CC bytes apart; with G_2392 = 2 or 3 every second or third row) the source list (G_1DF4 + 2,
 * words, $FFFF-terminated) is merged with the old list (G_1DF0, prefixed with a 0 word) into the
 * new one (G_1DEC + 2). Words carry a position in the high ten bits and a value in the low six:
 * runs whose position >> 6 is 4 are collapsed to their last entry, keeping the old list's value at
 * that position. Transcribed label by label from the original (the labels are its addresses).
 * Mazeworld Original (fn_S2_40E6) is the same with every row merged.
 * Keeps D3-D7 and A2-A4; flags from restoring D7. */
void fn_S2_4DC4(Regs *r) {
    uint32_t save_d[8], save_a[8];
    memcpy(save_d, r->d, sizeof save_d); memcpy(save_a, r->a, sizeof save_a);
    uint32_t t = g32(0x1DF0); sg32(0x1DF0, g32(0x1DEC)); sg32(0x1DEC, t);
    uint32_t *d0 = &r->d[0], *d1 = &r->d[1], *d2 = &r->d[2];
    uint32_t a0, a1, a2, a3, a4;
    const uint16_t d5 = 0xFFC0, d6 = 0xFFFF, d7 = 0x3F;
    uint16_t d3 = g16(0x2150);
    int x = 0;
    a3 = g32(0x1DF4) + 2; a4 = g32(0x1DF0); *d2 = g32(0x1DEC) + 2;
L4E16:
    a0 = a3; a1 = a4; a2 = *d2;
    *d0 = 0; w16(a1, 0); a1 += 2;
    set_w(d0, r16(a0));
    if ((uint16_t)*d0 == d6) goto L4E7C;
    /* L4E26 */
    set_w(d1, (uint16_t)((uint16_t)*d0 >> 6));
    if ((uint16_t)*d1 != 4) goto L4E66;
    set_w(d0, r16(a0)); a0 += 2;
    w16(a2, (uint16_t)*d0);
    set_w(d1, (uint16_t)((uint16_t)*d0 | d7));
    if ((uint16_t)*d1 == (uint16_t)*d0) goto L4E4E;
L4E3C:
    set_w(d0, r16(a0)); a0 += 2;
    if ((uint16_t)*d1 < (uint16_t)*d0) a2 += 2;
    w16(a2, (uint16_t)*d0);
    set_w(d1, (uint16_t)((uint16_t)*d0 | d7));
    if ((uint16_t)*d1 != (uint16_t)*d0) goto L4E3C;
L4E4E:
    w16(a2, (uint16_t)(r16(a2) & d5));
    { uint16_t v; do { v = r16(a1); a1 += 2; } while ((uint16_t)*d1 >= v); }
    a1 -= 4;
    set_w(d0, (uint16_t)(r16(a1) & d7));
    w16(a2, (uint16_t)(r16(a2) | (uint16_t)*d0));
    a1 += 2; a2 += 2;
    set_w(d0, r16(a0));
    if ((uint16_t)*d0 == d6) goto L4E7C;
L4E66:
    set_w(d1, (uint16_t)((uint16_t)*d0 & d5));
    do { set_w(d0, r16(a1)); a1 += 2; w16(a2, (uint16_t)*d0); a2 += 2; } while ((uint16_t)*d0 < (uint16_t)*d1);
    a1 -= 2; a2 -= 4;
    set_w(d1, (uint16_t)(r16(a2) | d7));
    goto L4E3C;
L4E7C:
    do { set_w(d0, r16(a1)); a1 += 2; w16(a2, (uint16_t)*d0); a2 += 2; } while ((uint16_t)*d0 != d6);
    if (!mz_original) set_w(d0, g16(0x2392));          /* Original has no row-skipping modes: every row */
    {
        uint16_t step = mz_original ? 1 : (uint16_t)*d0 == 2 ? 2 : (uint16_t)*d0 == 3 ? 3 : 1;
        if (step == 1) { a0 = a3; a1 = a4; a2 = *d2; a3 += 0xCC; a4 += 0xCC; *d2 += 0xCC; }
        x = d3 < step;                              /* X from subq.w #step,d3 */
        d3 = (uint16_t)(d3 - step);
        if (step == 1) { if (d3) goto L4E16; }
        else if (d3 && !(d3 & 0x8000)) {
            uint32_t rowstep = 0xCCu * step;
            a0 = a3; a1 = a4; a2 = *d2; a3 += rowstep; a4 += rowstep; *d2 += rowstep;
            goto L4E16;
        }
    }
    r->a[0] = a0; r->a[1] = a1;
    (void)a2;
    for (int i = 3; i < 8; i++) r->d[i] = save_d[i];
    for (int i = 2; i < 5; i++) r->a[i] = save_a[i];
    r->ccr = (uint8_t)(x ? CCR_X : 0);
    cc_nz_l(r, save_d[7]);   /* N, Z from restoring D7; X from the last subq on the row count */
}

/* fn_S2_3692 (JT 85): builds the per-row span lists for the floor and ceiling from a column height
 * table (the words at arg 1, one per column, clamped to G_214E rows). Rows are $CC-byte lists
 * starting at arg 2 + G_2144: { word end offset; word span... }; spans carry the column in the
 * high ten bits and a shade from the table G_1DAC (indexed by height) in the low six. Each list is
 * then terminated with $FFFF and mirrored into the rows above (arg 2 + G_2144 - $CC downwards).
 * Uses D3-D7, A2, A3 without saving them (A4 is kept), so it is written at register level;
 * transcribed label by label (labels are the original's addresses). */
void fn_S2_3692(Regs *r) {
    uint32_t *d0 = &r->d[0], *d1 = &r->d[1], *d2 = &r->d[2], *d3 = &r->d[3], *d4 = &r->d[4],
             *d5 = &r->d[5], *d6 = &r->d[6], *d7 = &r->d[7];
    uint32_t sp = r->a[7], arg1 = r32(sp + 4), arg2 = r32(sp + 8), a4save = r->a[4];
    uint16_t lim = (uint16_t)((uint16_t)(4 + g16(0x2156)) << 6);
    uint32_t base = arg2 + g32(0x2144);
    uint32_t a0 = base, a1 = arg1, a3 = g32(0x1DAC), a4;
#define W(addr, v) w16((addr), (uint16_t)(v))
#define SX(x) sxw(x)
    set_w(d2, g16(0x214E));
    set_w(d6, 0x100); *d4 = 0xC8; set_w(d5, 4);
    set_w(d0, r16(a1)); a1 += 2;
    if (!((uint16_t)*d0 < (uint16_t)*d2)) set_w(d0, (uint16_t)*d2);
    set_w(d7, (uint16_t)((uint16_t)*d0 << 1));
    set_w(d3, (uint16_t)(r16(a3 + SX(*d7)) | (uint16_t)*d6));
    *d1 = 0;
    if ((uint16_t)*d1 < (uint16_t)*d0)
        do { W(a0, *d5); W(a0 + 2, *d3); a0 += 4 + *d4; set_w(d1, (uint16_t)((uint16_t)*d1 + 1)); } while ((uint16_t)*d1 < (uint16_t)*d0);
    a4 = a0;
    if ((uint16_t)*d1 < (uint16_t)*d2)
        do { W(a0, *d5); W(a0 + 2, *d6); a0 += 4 + *d4; set_w(d1, (uint16_t)((uint16_t)*d1 + 1)); } while ((uint16_t)*d1 < (uint16_t)*d2);
    *d4 = 0xCC;
    set_w(d3, (uint16_t)((uint16_t)*d3 + 0x40)); set_w(d6, (uint16_t)((uint16_t)*d6 + 0x40));
    set_w(d7, (uint16_t)*d0);
L371E:
    set_w(d0, r16(a1)); a1 += 2;
    if (!((uint16_t)*d0 < (uint16_t)*d2)) set_w(d0, (uint16_t)*d2);
    set_w(d5, (uint16_t)((uint16_t)*d0 << 1));
    set_w(d5, (uint16_t)(r16(a3 + SX(*d5)) | (uint16_t)*d6));
    if ((uint16_t)*d5 != (uint16_t)*d3) goto L376E;
    {
        uint16_t old = (uint16_t)*d7;
        set_w(d7, (uint16_t)(old - (uint16_t)*d0));
        if ((uint16_t)*d7 == 0) goto L375C;
        if (old >= (uint16_t)*d0) goto L374C;
    }
    do {   /* L373A */
        set_w(d5, r16(a4)); W(a4 + SX(*d5), *d3); set_w(d5, (uint16_t)((uint16_t)*d5 + 2)); W(a4, *d5);
        a4 += *d4; set_w(d7, (uint16_t)((uint16_t)*d7 + 1));
    } while ((uint16_t)*d7);
    goto L375C;
L374C:
    do {
        a4 -= *d4;
        set_w(d5, r16(a4)); W(a4 + SX(*d5), *d6); set_w(d5, (uint16_t)((uint16_t)*d5 + 2)); W(a4, *d5);
        set_w(d7, (uint16_t)((uint16_t)*d7 - 1));
    } while ((uint16_t)*d7);
L375C:
    set_w(d3, (uint16_t)((uint16_t)*d3 + 0x40)); set_w(d6, (uint16_t)((uint16_t)*d6 + 0x40));
    set_w(d7, (uint16_t)*d0);
    if ((uint16_t)*d6 != lim) goto L371E;
    goto L37A0;
L376E:
    set_w(d3, (uint16_t)*d5);
    a0 = base;
    set_w(d1, (uint16_t)*d0);
    if ((uint16_t)*d1)
        do {
            set_w(d5, r16(a0)); W(a0 + SX(*d5), *d3); set_w(d5, (uint16_t)((uint16_t)*d5 + 2)); W(a0, *d5);
            a0 += *d4; set_w(d1, (uint16_t)((uint16_t)*d1 - 1));
        } while ((uint16_t)*d1);
    a4 = a0;
    {
        uint16_t old = (uint16_t)*d7;
        set_w(d7, (uint16_t)(old - (uint16_t)*d0));
        if (old <= (uint16_t)*d0) goto L375C;   /* bls */
    }
    do {
        set_w(d5, r16(a0)); W(a0 + SX(*d5), *d6); set_w(d5, (uint16_t)((uint16_t)*d5 + 2)); W(a0, *d5);
        a0 += *d4; set_w(d7, (uint16_t)((uint16_t)*d7 - 1));
    } while ((uint16_t)*d7);
    goto L375C;
L37A0:
    a0 = base;
    set_w(d1, (uint16_t)*d2);
    *d3 = 0xFFFFFFFFu; *d4 = 0xCC;
    do { set_w(d0, r16(a0)); W(a0 + SX(*d0), *d3); a0 += *d4; set_w(d1, (uint16_t)((uint16_t)*d1 - 1)); } while ((uint16_t)*d1);
    *d4 = 0xCC;
    a0 = base;
    uint32_t a2 = a0 - 0xCC;
    set_w(d1, (uint16_t)*d2);
    do {   /* L37D6 */
        a1 = a0; a3 = a2;
        set_w(d0, r16(a0));
        do { W(a3 + SX(*d0), r16(a1 + SX(*d0))); set_w(d0, (uint16_t)((uint16_t)*d0 - 2)); } while ((uint16_t)*d0);
        a0 += *d4; a2 -= *d4;
        set_w(d1, (uint16_t)((uint16_t)*d1 - 1));
    } while ((uint16_t)*d1);
#undef W
#undef SX
    r->a[0] = a0; r->a[1] = a1; r->a[2] = a2; r->a[3] = a3; r->a[4] = a4save;
    r->ccr = CCR_Z;   /* the final subq.w #1,d1 reaching 0 */
}

/* fn_S2_03DC (JT 103): error alert 131 with the message msg (and G_2424 / G_2422 / G_2420 as ^1-^3);
 * quits if `fatal` is set. */
void fn_S2_03DC(uint32_t msg, int8_t fatal) {
    mac_ParamText(msg, GADDR(0x2424), GADDR(0x2422), GADDR(0x2420));
    mac_NoteAlert(0x83, 0);
    if (fatal) mac_ExitToShell();
}

void fn_S3_038E(int32_t num, uint32_t str);

/* fn_S2_03A0 (JT 102): error alert 128 with the message msg and the number num (as ^1; G_2428 and
 * G_2426 as ^2, ^3); quits if `fatal` is set. */
void fn_S2_03A0(uint32_t msg, int16_t num, int8_t fatal) {
    MZ_FRAME;
    uint32_t text = mz_local(256);
    mz_seg_of(3, 0x038E), fn_S3_038E((int32_t)num, text);
    mac_ParamText(msg, text, GADDR(0x2428), GADDR(0x2426));
    mac_StopAlert(0x80, 0);
    if (fatal) mac_ExitToShell();
}

/* ---------- fn_S2_1CF6: wall edges from the command stream into the row span lists ----------
 *
 * The 3D view is drawn from per-row span lists ($CC bytes per row, starting at G_1DF4 + 2): each
 * entry is a word with the screen column in the high ten bits and a colour in the low six. This
 * routine turns the scene command stream G_20A2 into those entries. Each command covers n rows and
 * appends one entry per row, with the column moving along a wall edge:
 *   flat              column constant                                 (cases 0, 6, 12)
 *   up 1, 2, 4        column grows by 1, 2 or 4 per row               (cases 9, 10, 11)
 *   down 1, 2, 4      column shrinks by 1, 2 or 4 per row             (cases 3, 2, 1)
 *   up / down 1 in 4  one column every four rows, with a start phase  (cases 7 / 5)
 *   up / down 1 in 2  one column every two rows                       (cases 8 / 4)
 * Columns are clamped to the view's left and right edges (G_20A6, G_20A4). The stream is drawn in
 * two passes from the horizon row: downwards and upwards (the order depends on G_22C9).
 *
 * The original is about 5 KB of hand-unrolled assembly: every case written out twice (one per
 * direction) with each loop unrolled in pairs and entered at the right slot when a clamp kicks in.
 * Here each pattern is one small state machine; the values written, their order and the registers
 * left behind are the original's (including its quirks: the encoded column wraps at 16 bits, and
 * the "up 1 in 4/2" patterns keep stepping past the right edge without clamping). */

typedef struct { uint32_t a1; int32_t stride; } SpanOut;
typedef struct { int16_t left, right; uint16_t L, R; } SpanClip;   /* L, R = left, right << 6 */

static inline void span_put(SpanOut *o, uint16_t v) { w16(o->a1, v); o->a1 += (uint32_t)o->stride; }

/* n copies of v, as the original's constant-fill loop (entered at its lsr: a count of 1 leaves X
 * set, 0 means 65536 pairs). Returns X. */
static int span_fill(SpanOut *o, uint16_t v, uint16_t n) {
    uint16_t pairs = n >> 1;
    if (n & 1) { span_put(o, v); if (!pairs) return 1; }
    do { span_put(o, v); span_put(o, v); } while (--pairs);
    return 0;
}

/* Number of writes of a count at an lsr entry: n, or 2 * 65536 when n is 0. */
static uint32_t span_count(uint16_t n) { return n ? n : 0x20000u; }

/* Integer slopes (s = 0 flat, 1, 2, 4), dir +1 up or -1 down. Returns X. */
static int span_slope(SpanOut *o, const SpanClip *k, uint16_t n, uint16_t c, int16_t x, int s, int dir) {
    enum { CONST, EDGE, RUN } ph;        /* EDGE: outside, waiting for x to come in (x tracked) */
    uint16_t v;
    if (x < k->left) {
        if (dir > 0 && s) { ph = EDGE; v = (uint16_t)(c | k->L); }
        else { ph = CONST; v = (uint16_t)(c | k->L); }
    } else if (x >= k->right) {
        if (dir < 0) { ph = EDGE; v = (uint16_t)(c | k->R); }
        else { ph = CONST; v = (uint16_t)(c | k->R); }
    } else { ph = s ? RUN : CONST; v = (uint16_t)(c | (uint16_t)(x << 6)); }
    if (!s) return span_fill(o, v, n);
    if (n == 1) { span_put(o, v); return 1; }
    uint16_t step = (uint16_t)(s << 6);
    for (uint32_t i = span_count(n); i; i--) {
        span_put(o, v);
        if (ph == EDGE && dir > 0) {
            x = (int16_t)(x + s);
            if (x >= k->left) {
                if (s == 1) ph = RUN;                                   /* v is already the left edge */
                else if (x >= k->right) { v = (uint16_t)((v & 0x3F) | k->R); ph = CONST; }
                else { v = (uint16_t)((v & 0x3F) | (uint16_t)(x << 6)); ph = RUN; }
            }
        } else if (ph == EDGE) {
            x = (int16_t)(x - s);
            if (x < k->right) {
                if (s >= 2 && x < k->left) { v = (uint16_t)((v & 0x3F) | k->L); ph = CONST; }
                else { v = (uint16_t)((v & 0x3F) | (uint16_t)(x << 6)); ph = RUN; }
            }
        } else if (ph == RUN && dir > 0) {
            v = (uint16_t)(v + step);
            if (v >= k->R) { v = (uint16_t)((v & 0x3F) | k->R); ph = CONST; }
        } else if (ph == RUN) {
            v = (uint16_t)(v - step);
            if (v < k->L) { v = (uint16_t)((v & 0x3F) | k->L); ph = CONST; }
        }
    }
    return 0;
}

/* One column every `period` rows (2 or 4), starting at position `pos` of the cycle (the step comes
 * after the write at position period - 1). Returns X. */
static int span_frac(SpanOut *o, const SpanClip *k, uint16_t n, uint16_t c, int16_t x, int period, int pos, int dir) {
    enum { EDGE, RUN } ph;
    uint16_t v;
    if (dir > 0) {
        if (x < k->left) { ph = EDGE; v = (uint16_t)(c | k->L); }
        else if (x >= k->right) return span_fill(o, (uint16_t)(c | k->R), n);
        else { ph = RUN; v = (uint16_t)(c | (uint16_t)(x << 6)); }
    } else {
        if (x < k->left) return span_fill(o, (uint16_t)(c | k->L), n);
        else if (x >= k->right) { ph = EDGE; v = (uint16_t)(c | k->R); }
        else { ph = RUN; v = (uint16_t)(c | (uint16_t)(x << 6)); }
    }
    uint16_t left = n;
    for (;;) {
        span_put(o, v);
        if (pos == period - 1) {
            pos = 0;
            if (ph == EDGE && dir > 0) { x = (int16_t)(x + 1); if (x >= k->left) ph = RUN; }
            else if (ph == EDGE) { x = (int16_t)(x - 1); if (x < k->right) { v = (uint16_t)((v & 0x3F) | (uint16_t)(x << 6)); ph = RUN; } }
            else if (dir > 0) {
                v = (uint16_t)(v + 0x40);
                if (v >= k->R) { left--; return left ? span_fill(o, v, left) : 0; }   /* not clamped */
            } else {
                v = (uint16_t)(v - 0x40);
                if (v < k->L) { v = (uint16_t)((v & 0x3F) | k->L); left--; return left ? span_fill(o, v, left) : 0; }
            }
        } else pos++;
        if (!--left) return 0;
    }
}

/* Runs one command (case 0..12) for n rows from stream position *a0. Returns X. */
static int span_case(SpanOut *o, const SpanClip *k, uint32_t *a0, uint32_t a3, uint16_t n, uint16_t c, int cs) {
    uint32_t idx = (uint32_t)r8((*a0)++) << 2;
    int16_t x = (int16_t)(r16(a3 + idx) + g16(0x20A8));
    uint16_t phase = r16(a3 + idx + 2);   /* fractional cases only */
    switch (cs) {
    case 0: case 6: case 12: return span_slope(o, k, n, c, x, 0, 1);
    case 9:  return span_slope(o, k, n, c, x, 1, 1);
    case 10: return span_slope(o, k, n, c, x, 2, 1);
    case 11: return span_slope(o, k, n, c, x, 4, 1);
    case 3:  return span_slope(o, k, n, c, x, 1, -1);
    case 2:  return span_slope(o, k, n, c, x, 2, -1);
    case 1:  return span_slope(o, k, n, c, x, 4, -1);
    case 7:  return span_frac(o, k, n, c, x, 4, phase & 3, 1);
    case 8:  return span_frac(o, k, n, c, x, 2, (phase >> 1) & 1, 1);
    case 5:  return span_frac(o, k, n, c, x, 4, 3 - (phase & 3), -1);
    case 4:  return span_frac(o, k, n, c, x, 2, 1 - ((phase >> 1) & 1), -1);
    }
    return 0;   /* 13-15 do not occur (the original's table has 13 entries) */
}

void fn_S2_1CF6(Regs *r) {
    uint32_t d3 = r->d[3], d4 = r->d[4], d5 = r->d[5], d6 = r->d[6], d7 = r->d[7];
    uint32_t a2s = r->a[2], a3s = r->a[3], a4s = r->a[4];
    uint32_t d0, d1, d2, a0, a1;
    int x = 0;
    /* every row list starts empty */
    d1 = (r->d[1] & 0xFFFF0000u) | g16(0x2150);
    a0 = g32(0x1DF4) + 2; d0 = 0xFFFFFFFFu;
    do { w16(a0, 0xFFFF); a0 += 0xCC; set_w(&d1, (uint16_t)((uint16_t)d1 - 1)); } while ((uint16_t)d1);
    a1 = r->a[1]; d2 = r->d[2];
    if (!g32(0x20A2)) goto done;
    uint32_t a2 = GADDR(0x0A2A) + (uint16_t)(g16(0x209E) << 4);        /* colour table */
    a0 = g32(0x20A2);
    SpanClip k = {(int16_t)g16(0x20A6), (int16_t)g16(0x20A4), (uint16_t)(g16(0x20A6) << 6), (uint16_t)(g16(0x20A4) << 6)};
    uint32_t a4 = g32(0x1DDC) + ((uint32_t)g16(0x20AA) << 8);             /* row of each step */
    uint32_t a3 = g32(0x1DE0) + ((uint32_t)g16(0x20AA) << 10);            /* column of each step */
    d0 = ((uint32_t)g16(0x20AA) << 10 & 0xFFFFFF00u) | g8(0x22C9);
    int pass = g8(0x22C9) ? 1 : 0;   /* 0: downwards first */
    for (int done_passes = 0; done_passes < 2; done_passes++, pass ^= 1) {
        int32_t stride = pass ? -0xCC : 0xCC;
        uint32_t start = g32(0x1DF4) + g32(0x2144);
        uint32_t base = (pass ? start - 0xCC : start) + 2;
        d0 = base; d2 = 0; x = 0;                          /* the address arithmetic carries nothing */
        a1 = base;
        for (;;) {                                          /* L1D94: next column */
            uint8_t b = r8(a0++);
            d0 = (d0 & 0xFFFFFF00u) | b;
            if (!b) break;
            if (b & 0x80) { base += 2; d2 = 0; a1 = base; x = 0; }   /* next column of the lists */
            if (b & 0x40) {                                 /* start at an explicit row */
                uint8_t i = (uint8_t)(r8(a0++) << 1);
                d2 = r16(a4 + i);
                d1 = (uint32_t)(uint16_t)(d2 << 2); x = (d2 >> 14) & 1;
                d1 = r32(g32(0x1DC8) + (uint32_t)(int32_t)(int16_t)(uint16_t)d1);
                a1 = pass ? base - d1 : base + d1;
            }
            if (b & 0x20) {                                 /* skip rows (marked empty) */
                uint8_t i = (uint8_t)(r8(a0++) << 1);
                uint16_t t = r16(a4 + i), skip = (uint16_t)(t - (uint16_t)d2);
                x = t < (uint16_t)d2;
                d0 = skip;
                if (skip) {
                    d1 = a1; a1 += 2;
                    do { w16(a1, 0xFFFF); a1 += (uint32_t)stride; } while (--skip);
                    a1 = d1; d0 = 0; x = 0;
                }
            }
            for (;;) {                                      /* L1DF4: edges in this column */
                uint8_t e = r8(a0++);
                d0 = e;
                if (!e) break;
                uint16_t y = r16(a4 + (uint8_t)(e << 1));
                uint16_t n = (uint16_t)(y - (uint16_t)d2);
                x = y < (uint16_t)d2;
                set_w(&d1, n);
                if (!n) { a0 += 2; continue; }
                set_w(&d2, y);
                uint8_t cmd = r8(a0++);
                uint16_t colour = r8(a2 + ((cmd & 0xF0) >> 4));
                SpanOut o = {a1, stride};
                x = span_case(&o, &k, &a0, a3, n, colour, cmd & 0xF);
                a1 = o.a1;
                set_w(&d1, 0);
            }
        }
        d0 = (d0 & 0xFFFFFF00u) | g8(0x22C9);
    }
done:
    r->d[0] = d0; r->d[1] = d1; r->d[2] = d2; r->a[0] = a0; r->a[1] = a1;
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->d[6] = d6; r->d[7] = d7;
    r->a[2] = a2s; r->a[3] = a3s; r->a[4] = a4s;
    r->ccr = (uint8_t)(x ? CCR_X : 0);
    cc_nz_l(r, d7);   /* N, Z from restoring D7 */
}

/* Sprite projection (part of fn_S2_5810, whose LINK frame A6 these share). The view basis sits in
 * the frame: -$E/-$10 rotate an object's offset from the viewer (G_20D2, G_20CE, 16.16) into depth,
 * -$A/-$C into the sideways position. For object record D0.b of G_1DC4 ($40 bytes, entry at
 * A4 = { byte light; byte object }), visible objects get: +$26 depth (whole), +$28 screen column
 * (sideways * G_210A / depth + G_2154; a near object only if the product is in +-$40000, then
 * divided by $20), +$2A the G_1DC0 slope byte for (depth, sideways) negated on the left, +$2C the
 * size (the depth, minus the record's +$3C fade while that is negative, clamped at 0), and the
 * light byte at A4 = the G_1DB8 map value under the object plus its +$36 bias (at least 1); A4
 * then advances by 2. Objects behind the viewer return without advancing A4. A record whose +$26
 * is not negative is already projected: its depth is taken as given. */
static void sprite_project(Regs *r, int from_entry) {
    uint32_t a6 = r->a[6], a3 = r->a[3];
    uint8_t x = 0;                               /* the X flag at the exits */
    if (from_entry) {
        set_b(&r->d[0], (uint8_t)r->d[0]);
        w8(r->a[4] + 1, (uint8_t)r->d[0]);
        a3 = g32(0x1DC4) + (((uint32_t)r->d[0] & 0xFF) << 6);
        r->d[0] = ((uint32_t)r->d[0] & 0xFF) << 6;
        set_w(&r->d[0], r16(a3 + 0x26));
        if (!((uint16_t)r->d[0] & 0x8000)) {     /* already projected */
            w8(a6 - 5, (uint8_t)r->d[0]);
            w8(a6 - 6, (uint8_t)r->d[0]);
            set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] << 8));
            w16(a6 - 0x12, (uint16_t)r->d[0]);
            goto sideways;
        }
    }
    {   /* depth */
        r->d[0] = (r32(a3 + 4) - g32(0x20D2)) >> 8;
        r->d[1] = (uint32_t)((int32_t)(int16_t)r16(a6 - 0xE) * (int16_t)r->d[0]);
        r->d[0] = (r32(a3 + 8) - g32(0x20CE)) >> 8;
        r->d[2] = (uint32_t)((int32_t)(int16_t)r16(a6 - 0x10) * (int16_t)r->d[0]);
        r->d[2] += r->d[1];
        r->d[2] >>= 8;
        w16(a6 - 0x12, (uint16_t)r->d[2]);
        x = (r->d[2] >> 7) & 1;
        r->d[2] >>= 8;
        if ((uint16_t)r->d[2] & 0x8000) {        /* behind the viewer */
            r->a[3] = a3;
            r->ccr = (uint8_t)((x ? CCR_X : 0) | CCR_N);
            return;
        }
        w8(a6 - 5, (uint8_t)r->d[2]);
        w8(a6 - 6, (uint8_t)r->d[2]);
    }
sideways:
    r->d[0] = (r32(a3 + 4) - g32(0x20D2)) >> 8;
    r->d[1] = (uint32_t)((int32_t)(int16_t)r16(a6 - 0xA) * (int16_t)r->d[0]);
    r->d[0] = (r32(a3 + 8) - g32(0x20CE)) >> 8;
    r->d[2] = (uint32_t)((int32_t)(int16_t)r16(a6 - 0xC) * (int16_t)r->d[0]);
    r->d[2] += r->d[1];
    x = (r->d[2] >> 7) & 1;
    r->d[2] >>= 8;
    r->d[1] = (uint32_t)((int32_t)(int16_t)g16(0x210A) * (int16_t)r->d[2]);
    set_w(&r->d[0], r16(a6 - 0x12));
    if ((int16_t)r->d[0] < 0x100) {
        int32_t p = (int32_t)r->d[1];
        uint32_t lim = (p >= 0x40000) ? 0x40000u : (p < -0x40000) ? 0xFFFC0000u : 0;
        if (lim) {                                /* too far off to the side for a near object */
            r->a[3] = a3;
            uint32_t dst = r->d[1], res = dst - lim;
            int v = ((lim ^ dst) & (res ^ dst)) >> 31;
            r->ccr = (uint8_t)((x ? CCR_X : 0) | (res ? 0 : CCR_Z) | (res >> 31 ? CCR_N : 0) |
                               (v ? CCR_V : 0) | (lim > dst ? CCR_C : 0));
            return;
        }
        set_w(&r->d[0], 0x20);
    }
    {   /* divs.w d0, d1 (left unchanged on overflow) */
        int32_t q = (int32_t)r->d[1] / (int16_t)r->d[0], m = (int32_t)r->d[1] % (int16_t)r->d[0];
        if (q == (int16_t)q) r->d[1] = ((uint32_t)(uint16_t)m << 16) | (uint16_t)q;
    }
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] + g16(0x2154)));
    w16(a3 + 0x28, (uint16_t)r->d[1]);
    r->a[0] = g32(0x1DC0);
    r->d[2] >>= 8;
    {
        int left = ((uint16_t)r->d[2] & 0x8000) != 0;
        uint8_t s = (uint8_t)r->d[2];
        if (left) s = (uint8_t)-s;
        s = (uint8_t)(s << 1);
        set_b(&r->d[2], s);
        set_w(&r->d[1], (uint16_t)(((uint16_t)r8(a6 - 5) << 8 | s) >> 1));
        uint16_t slope = r8(r->a[0] + sxw(r->d[1]));
        r->d[0] = left ? (uint16_t)-slope : slope;
        w16(a3 + 0x2A, (uint16_t)r->d[0]);
    }
    r->d[0] = r8(a6 - 5);
    w16(a3 + 0x26, (uint16_t)r->d[0]);
    set_w(&r->d[0], r16(a3 + 0x3C));
    if ((uint16_t)r->d[0] & 0x8000) {
        uint8_t s = (uint8_t)((uint8_t)r->d[0] + r8(a6 - 6));
        set_b(&r->d[0], s);
        if (s & 0x80) r->d[0] = 0;
        w8(a6 - 6, (uint8_t)r->d[0]);
    }
    r->d[0] = r8(a6 - 6);
    w16(a3 + 0x2C, (uint16_t)r->d[0]);
    r->a[0] = g32(0x1DB8);
    map_offset(r, r16(a3 + 8), g16(0x20CE), r16(a3 + 4), g16(0x20D2));
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    {
        uint16_t b = r16(a3 + 0x36), s = (uint16_t)((uint16_t)r->d[0] + b);
        x = s < b;
        set_w(&r->d[0], s);
        if (s == 0 || (s & 0x8000)) set_w(&r->d[0], 1);
    }
    w8(r->a[4], (uint8_t)r->d[0]);
    cc_nz_b(r, (uint8_t)r->d[0]);
    r->ccr = (uint8_t)((r->ccr & ~CCR_X) | (x ? CCR_X : 0));
    r->a[4] += 2;
    r->a[3] = a3;
}

/* fn_S2_5AB2: projects object D0.b (see sprite_project). */
void fn_S2_5AB2(Regs *r) { sprite_project(r, 1); }

/* fn_S2_5A6C: adds a temporary object (projectile spark) at (D0.w, D1.w) as record number -8(A6)
 * of G_1DC4, if that is below $C8, and projects it: +2 = $14, +$38 = $14, +$3A = $11, +$36 and
 * +$3C cleared. The frame's record counter -8(A6) then advances. */
void fn_S2_5A6C(Regs *r) {
    uint32_t a6 = r->a[6];
    set_w(&r->d[2], r16(a6 - 8));
    cc_cmp_w(r, 0xC8, (uint16_t)r->d[2]);
    if ((uint16_t)r->d[2] >= 0xC8) return;
    w8(r->a[4] + 1, (uint8_t)r->d[2]);
    r->d[2] = ((uint32_t)r->d[2] & 0xFF) << 6;
    r->a[3] = g32(0x1DC4) + r->d[2];
    w16(r->a[3] + 4, (uint16_t)r->d[0]);
    w16(r->a[3] + 8, (uint16_t)r->d[1]);
    w16(r->a[3] + 2, 0x14);
    w16(r->a[3] + 0x3C, 0);
    w16(r->a[3] + 0x36, 0);
    w16(r->a[3] + 0x38, 0x14);
    w16(r->a[3] + 0x3A, 0x11);
    w16(a6 - 8, (uint16_t)(r16(a6 - 8) + 1));
    sprite_project(r, 0);
}

/* ---------- HUD panel drawing ----------
 * The panel's items sit at fixed offsets from an origin that is (G_2162, G_2160) normally and
 * (0, 0) while G_21FC is set. Each routine redraws only when its "dirty" flag is set or its value
 * changed since it was last drawn. */
static void hud_origin(int16_t *h, int16_t *v) {
    if (g8(0x21FC)) { *h = 0; *v = 0; }
    else { *h = (int16_t)g16(0x2162); *v = (int16_t)g16(0x2160); }
}
static void set_rect(uint32_t r, int16_t top, int16_t left, int16_t bottom, int16_t right) {
    w16(r, (uint16_t)top); w16(r + 2, (uint16_t)left); w16(r + 4, (uint16_t)bottom); w16(r + 6, (uint16_t)right);
}

/* fn_S2_4058 (JT 91): when G_2100 is set, draws the panel's frames: the outline of the whole panel
 * (G_2166 x G_2164) with a rule under the top row, the double frame around the box G_215E..G_215C
 * x G_215A..G_2158, and the double frame (inner one hanging from the same top edge) of the 128-pixel scanner at (G_2112, G_2110). */
void fn_S2_4058(void) {
    MZ_FRAME;
    if (!g8(0x2100)) return;
    int16_t h, v; hud_origin(&h, &v);
    uint32_t r = mz_local(8);
    set_rect(r, v, (int16_t)(h + 2), (int16_t)(v + g16(0x2164)), (int16_t)(h + g16(0x2166) - 2));
    mac_PmForeColor(2); mac_FrameRect(r);
    mac_PmForeColor(2);
    mac_MoveTo((int16_t)(h + 2), (int16_t)(v + g16(0x2110) - 2));
    mac_Line((int16_t)(g16(0x2166) - 4), 0);
    set_rect(r, (int16_t)(v + g16(0x215A) - 2), (int16_t)(h + g16(0x215E) - 2),
             (int16_t)(v + g16(0x2158) + 2), (int16_t)(h + g16(0x215C) + 2));
    mac_PmForeColor(2); mac_FrameRect(r);
    w16(r + 2, (uint16_t)(r16(r + 2) + 1)); w16(r + 6, (uint16_t)(r16(r + 6) - 1));
    w16(r, (uint16_t)(r16(r) + 1));         w16(r + 4, (uint16_t)(r16(r + 4) - 1));
    mac_PmForeColor(4); mac_FrameRect(r);
    set_rect(r, (int16_t)(v + g16(0x2110) - 1), (int16_t)(h + g16(0x2112) - 2),
             (int16_t)(v + g16(0x2110) + 0x82), (int16_t)(h + g16(0x2112) + 0x82));
    mac_PmForeColor(2); mac_FrameRect(r);
    w16(r + 2, (uint16_t)(r16(r + 2) + 1)); w16(r + 6, (uint16_t)(r16(r + 6) - 1));
    w16(r + 4, (uint16_t)(r16(r + 4) - 1));
    mac_PmForeColor(4); mac_FrameRect(r);
    sg8(0x2100, 0);
}

/* fn_S2_41A6 (JT 92): when G_20EE is set, draws the panel labels in font G_2244, size G_213E:
 * "life", "energy", "time" in a column at (G_213A, G_213C) and "level", "bonus", "score" at
 * (G_212C, G_212E), with line spacing G_2130 and G_2122. */
void fn_S2_41A6(void) {
    if (!g8(0x20EE)) return;
    int16_t h, v; hud_origin(&h, &v);
    mac_PmForeColor(0xF); mac_PmBackColor(0);
    mac_TextSize((int16_t)g16(0x213E)); mac_TextFont((int16_t)g16(0x2244));
    int16_t h1 = (int16_t)(g16(0x213A) + h), v1 = (int16_t)g16(0x213C), dv1 = (int16_t)g16(0x2130);
    mac_MoveTo(h1, (int16_t)(v1 + v));                    mac_DrawString(GADDR(0x142E));
    mac_MoveTo(h1, (int16_t)(v1 + dv1 + v));              mac_DrawString(GADDR(0x1428));
    mac_MoveTo(h1, (int16_t)(dv1 + dv1 + v1 + v));        mac_DrawString(GADDR(0x1420));
    int16_t h2 = (int16_t)(g16(0x212C) + h), v2 = (int16_t)g16(0x212E), dv2 = (int16_t)g16(0x2122);
    mac_MoveTo(h2, (int16_t)(v2 + v));                    mac_DrawString(GADDR(0x141A));
    mac_MoveTo(h2, (int16_t)(v2 + dv2 + v));              mac_DrawString(GADDR(0x1414));
    mac_MoveTo(h2, (int16_t)(dv2 + dv2 + v2 + v));        mac_DrawString(GADDR(0x140E));
    sg8(0x20EE, 0);
}

/* fn_S2_4A98 (JT 98): when G_20E2 is set or the lives count G_20E0 changed (last drawn: G_1434),
 * draws "lives" at (G_211C, G_211E) and after it eight small diamonds, the first G_20E0 of them
 * lit (outline 7, inner ring $E, centre $F) and the rest in the background colour. */
void fn_S2_4A98(void) {
    if (!g8(0x20E2) && g16(0x20E0) == g16(0x1434)) return;
    int16_t h, v; hud_origin(&h, &v);
    mac_PmForeColor(0xF); mac_PmBackColor(0);
    mac_TextSize(9); mac_TextFont((int16_t)g16(0x2244)); mac_TextMode(0);
    mac_MoveTo((int16_t)(g16(0x211C) + h), (int16_t)(g16(0x211E) + v));
    mac_DrawString(GADDR(0x1408));
    mac_Move(5, -4);
    for (int16_t i = 1; i <= 8; i++) {
        int lit = i <= (int16_t)g16(0x20E0);
        mac_PmForeColor(lit ? 7 : 0);
        mac_Line(2, -2); mac_Line(2, 2); mac_Line(-2, 2); mac_Line(-2, -2);
        mac_Move(1, 0);
        if (lit) mac_PmForeColor(0xE);
        mac_Line(1, -1); mac_Line(1, 1); mac_Line(-1, 1); mac_Line(-1, -1);
        mac_Move(1, 0);
        if (lit) mac_PmForeColor(0xF);
        mac_Line(0, 0);
        mac_Move(7, 0);
    }
    sg16(0x1434, g16(0x20E0));
    sg8(0x20E2, 0);
}

/* fn_S2_4BF8 (JT 99): when G_20DE is set or the count G_225E changed (last drawn: G_1432), draws
 * "jewels left: " and the count at (G_2118, G_211A), followed by spaces to cover a longer number. */
void fn_S2_4BF8(void) {
    MZ_FRAME;
    if (!g8(0x20DE) && g16(0x225E) == g16(0x1432)) return;
    int16_t h, v; hud_origin(&h, &v);
    uint32_t text = mz_local(256);
    mac_PmForeColor(0xF); mac_PmBackColor(0);
    mac_TextSize(9); mac_TextFont((int16_t)g16(0x2244)); mac_TextMode(0);
    mac_MoveTo((int16_t)(g16(0x2118) + h), (int16_t)(g16(0x211A) + v));
    mac_DrawString(GADDR(0x1402));
    mz_seg_of(3, 0x038E), fn_S3_038E((int32_t)(int16_t)g16(0x225E), text);
    mac_DrawString(text);
    mac_DrawString(GADDR(0x13F4));
    sg16(0x1432, g16(0x225E));
    sg8(0x20DE, 0);
}

/* fn_S2_4C86 (JT 100): the 48-pixel, 11-high bar at (G_2114, G_2116 - 9) showing G_20DC (last
 * drawn: G_1430) in colour $B on background 0. A full redraw when G_20DD is set, otherwise only the
 * part that changed. */
void fn_S2_4C86(void) {
    MZ_FRAME;
    if (!g8(0x20DD) && g16(0x20DC) == g16(0x1430)) return;
    int16_t h, v; hud_origin(&h, &v);
    uint32_t r = mz_local(8);
    int16_t top = (int16_t)(g16(0x2116) + v - 9), x0 = (int16_t)(g16(0x2114) + h);
    int16_t now = (int16_t)g16(0x20DC), was = (int16_t)g16(0x1430);
    if (g8(0x20DD)) {
        set_rect(r, top, x0, (int16_t)(top + 11), (int16_t)(x0 + now));
        mac_PmForeColor(0xB); mac_PaintRect(r);
        set_rect(r, top, (int16_t)(x0 + now), (int16_t)(top + 11), (int16_t)(x0 + 0x30));
        mac_PmForeColor(0); mac_PaintRect(r);
    } else if (now > was) {
        set_rect(r, top, (int16_t)(x0 + was), (int16_t)(top + 11), (int16_t)(x0 + now));
        mac_PmForeColor(0xB); mac_PaintRect(r);
    } else {
        set_rect(r, top, (int16_t)(x0 + now), (int16_t)(top + 11), (int16_t)(x0 + was));
        mac_PmForeColor(0); mac_PaintRect(r);
    }
    sg8(0x20DD, 0);
    sg16(0x1430, g16(0x20DC));
}

/* divs.w as the compiled code uses it: the quotient's low word, or the low word of the unchanged
 * dividend when the quotient does not fit in 16 bits */
static int16_t divs_w(int32_t num, int16_t den) {
    int32_t q = num / den;
    return (int16_t)(q == (int16_t)q ? q : num);
}

/* The life / energy / time bars: row `row` of the panel (rows G_2130 apart), from column G_2136
 * to G_2132 between G_2138 and G_2134, framed in 6 and filled in 7 to value * width / max. A full
 * redraw when the bar's dirty flag is set; otherwise, if the value changed and the filled length
 * with it, only the difference is painted (in 7 or the background 0). The globals record the last
 * drawn length and value. */
static void hud_bar(int16_t row, uint32_t dirty, uint32_t value, uint32_t max, uint32_t lastlen, uint32_t lastval) {
    MZ_FRAME;
    int16_t h, v, len, width = (int16_t)(g16(0x2132) - g16(0x2136));
    uint32_t r = mz_local(8);
    if (g8(dirty)) {
        hud_origin(&h, &v);
        v = (int16_t)(v + row * (int16_t)g16(0x2130));
        set_rect(r, (int16_t)(v + g16(0x2138) - 1), (int16_t)(h + g16(0x2136) - 1),
                 (int16_t)(v + g16(0x2134) + 1), (int16_t)(h + g16(0x2132) + 1));
        mac_PmForeColor(6); mac_FrameRect(r);
        len = divs_w((int32_t)(int16_t)g16(value) * width, (int16_t)g16(max));
        if (len > width) len = width; else if (len < 0) len = 0;
        int16_t x0 = (int16_t)(h + g16(0x2136));
        set_rect(r, (int16_t)(v + g16(0x2138)), x0, (int16_t)(v + g16(0x2134)), (int16_t)(x0 + len));
        mac_PmForeColor(7); mac_PaintRect(r);
        sg8(dirty, 0);
        sg16(lastlen, (uint16_t)len);
        sg16(lastval, g16(value));
        return;
    }
    if (g16(value) == g16(lastval)) return;
    len = divs_w((int32_t)(int16_t)g16(value) * width, (int16_t)g16(max));
    if (len > width) len = width; else if (len < 0) len = 0;
    if (len != (int16_t)g16(lastlen)) {
        hud_origin(&h, &v);
        v = (int16_t)(v + row * (int16_t)g16(0x2130));
        int16_t x0 = (int16_t)(h + g16(0x2136)), was = (int16_t)g16(lastlen);
        int grow = len > was;
        set_rect(r, (int16_t)(v + g16(0x2138)), (int16_t)(x0 + (grow ? was : len)),
                 (int16_t)(v + g16(0x2134)), (int16_t)(x0 + (grow ? len : was)));
        mac_PmForeColor(grow ? 7 : 0); mac_PaintRect(r);
        sg16(lastlen, (uint16_t)len);
    }
    sg16(lastval, g16(value));
}

/* fn_S2_42A8 (JT 94): the life bar (G_20FE of G_20FC; dirty G_20FF). */
void fn_S2_42A8(void) { hud_bar(0, 0x20FF, 0x20FE, 0x20FC, 0x1448, 0x144A); }
/* fn_S2_44AA (JT 95): the energy bar (G_20F8 of G_20F6; dirty G_20FA). */
void fn_S2_44AA(void) { hud_bar(1, 0x20FA, 0x20F8, 0x20F6, 0x1444, 0x1446); }
/* fn_S2_46BC (JT 96): the time bar (G_20F2 of G_20F0; dirty G_20F4). */
void fn_S2_46BC(void) { hud_bar(2, 0x20F4, 0x20F2, 0x20F0, 0x1440, 0x1442); }

/* ---------- fn_S2_52A2: one edge of a polygon into the column table G_136E ----------
 * The line from (D4, D5) to (D6, D7) is stepped across columns 0 .. G_2156-1 (Bresenham), storing
 * for each column the edge's row (a word, capped at $7FFF) in the table G_136E. Ends off the
 * left side count as large unsigned columns: a line with both ends outside is drawn only if they
 * are on opposite sides, and the part outside is stepped through without storing. The original
 * keeps a separate copy of every loop for: forward / backward along the table, rows rising /
 * falling, with / without the $7FFF cap. Here the loops are written once with the direction as
 * a parameter, following the original's labels so that every register it leaves is reproduced,
 * including its quirks: the capped falling y-major line takes an error step before its first
 * store, and the backward clipped line stores its first value one column early. */
#define D(n)      (r->d[n])
#define Wd(n)     ((uint16_t)r->d[n])
#define SW(n, v)  set_w(&r->d[n], (uint16_t)(v))
#define NEG(v)    (((uint16_t)(v) & 0x8000) != 0)
static inline void edge_put(Regs *r, int back, uint16_t v) {
    if (back) { r->a[4] -= 2; w16(r->a[4], v); } else { w16(r->a[4], v); r->a[4] += 2; }
}
/* subq.w #1,d4 / bpl: true while the count has not run out */
static inline int edge_count(Regs *r) { SW(4, Wd(4) - 1); return !NEG(Wd(4)); }
static inline void edge_exg(Regs *r) {
    uint32_t t = D(4); D(4) = D(6); D(6) = t;
    t = D(5); D(5) = D(7); D(7) = t;
}

/* rows rising, no cap; D4 = count - 1 */
static void edge_rise(Regs *r, int back) {
    if (Wd(7) < Wd(6)) {                       /* x-major */
        SW(1, Wd(6) >> 1);
        for (;;) {
            edge_put(r, back, Wd(5));
            if (!edge_count(r)) return;
            SW(1, Wd(1) - Wd(7));
            if (NEG(Wd(1))) { SW(5, Wd(5) + 1); SW(1, Wd(1) + Wd(6)); }
        }
    }
    SW(1, Wd(7) >> 1);                         /* y-major */
    for (;;) {
        edge_put(r, back, Wd(5));
        if (!edge_count(r)) return;
        do { SW(5, Wd(5) + 1); SW(1, Wd(1) - Wd(6)); } while (!NEG(Wd(1)));
        SW(1, Wd(1) + Wd(7));
    }
}

/* rows falling, no cap */
static void edge_fall(Regs *r, int back) {
    if (Wd(7) < Wd(6)) {
        SW(1, Wd(6) >> 1);
        for (;;) {
            edge_put(r, back, Wd(5));
            if (!edge_count(r)) return;
            SW(1, Wd(1) - Wd(7));
            if (NEG(Wd(1))) { SW(1, Wd(1) + Wd(6)); SW(5, Wd(5) - 1); }
        }
    }
    SW(1, Wd(7) >> 1);
    for (;;) {
        edge_put(r, back, Wd(5));
        if (!edge_count(r)) return;
        do { SW(5, Wd(5) - 1); SW(1, Wd(1) - Wd(6)); } while (!NEG(Wd(1)));
        SW(1, Wd(1) + Wd(7));
    }
}

/* stores the cap D3 until the count runs out */
static void edge_fill(Regs *r, int back) {
    do edge_put(r, back, Wd(3)); while (edge_count(r));
}

/* rows rising, capped at D3 (once reached, the rest is D3) */
static void edge_rise_cap(Regs *r, int back) {
    if (Wd(7) < Wd(6)) {
        SW(1, Wd(6) >> 1);
        if (Wd(5) >= Wd(3)) { edge_fill(r, back); return; }
        for (;;) {
            edge_put(r, back, Wd(5));
            if (!edge_count(r)) return;
            SW(1, Wd(1) - Wd(7));
            if (NEG(Wd(1))) {
                SW(5, Wd(5) + 1);
                if (Wd(5) >= Wd(3)) { edge_fill(r, back); return; }
                SW(1, Wd(1) + Wd(6));
            }
        }
    }
    SW(1, Wd(7) >> 1);
    if (Wd(5) >= Wd(3)) { edge_fill(r, back); return; }
    for (;;) {
        edge_put(r, back, Wd(5));
        if (!edge_count(r)) return;
        do {
            SW(5, Wd(5) + 1);
            if (Wd(5) >= Wd(3)) { edge_fill(r, back); return; }
            SW(1, Wd(1) - Wd(6));
        } while (!NEG(Wd(1)));
        SW(1, Wd(1) + Wd(7));
    }
}

/* rows falling from a start that may be above the cap: D3 is stored until the row drops below it
 * (D5 is kept relative to the cap meanwhile) */
static void edge_fall_cap(Regs *r, int back) {
    if (Wd(7) < Wd(6)) {
        SW(1, Wd(6) >> 1);
        SW(5, Wd(5) - Wd(3));
        if (NEG(Wd(5))) goto x_below;
        for (;;) {
            edge_put(r, back, Wd(3));
            if (!edge_count(r)) return;
            SW(1, Wd(1) - Wd(7));
            if (NEG(Wd(1))) {
                SW(1, Wd(1) + Wd(6)); SW(5, Wd(5) - 1);
                if (NEG(Wd(5))) goto x_below;
            }
        }
    x_below:
        SW(5, Wd(5) + Wd(3));
        for (;;) {
            edge_put(r, back, Wd(5));
            if (!edge_count(r)) return;
            SW(1, Wd(1) - Wd(7));
            if (NEG(Wd(1))) { SW(1, Wd(1) + Wd(6)); SW(5, Wd(5) - 1); }
        }
    }
    SW(1, Wd(7) >> 1);
    SW(5, Wd(5) - Wd(3));
    if (NEG(Wd(5))) goto y_below;
    for (;;) {
        edge_put(r, back, Wd(3));
        if (!edge_count(r)) return;
        do {
            SW(5, Wd(5) - 1);
            if (NEG(Wd(5))) goto y_below;
            SW(1, Wd(1) - Wd(6));
        } while (!NEG(Wd(1)));
        SW(1, Wd(1) + Wd(7));
    }
y_below:                                       /* enters mid-step: one error step before storing */
    SW(5, Wd(5) + Wd(3));
    for (;;) {
        SW(1, Wd(1) - Wd(6));
        if (!NEG(Wd(1))) { SW(5, Wd(5) - 1); continue; }
        SW(1, Wd(1) + Wd(7));
        edge_put(r, back, Wd(5));
        if (!edge_count(r)) return;
        SW(5, Wd(5) - 1);
    }
}

/* both ends outside, on opposite sides: steps -D4 columns without storing, then stores across
 * the whole width (rows rising, capped) */
static void edge_cross(Regs *r, int back) {
    uint16_t width = Wd(2);
    if (Wd(7) < Wd(6)) {
        SW(1, Wd(6) >> 1);
        if (Wd(5) >= Wd(3)) goto fill_all;
        while (edge_count(r)) {
            SW(1, Wd(1) - Wd(7));
            if (NEG(Wd(1))) {
                SW(5, Wd(5) + 1);
                if (Wd(5) >= Wd(3)) goto fill_all;
                SW(1, Wd(1) + Wd(6));
            }
        }
        SW(4, width - 1);
        for (;;) {
            edge_put(r, back, Wd(5));
            if (!edge_count(r)) return;
            SW(1, Wd(1) - Wd(7));
            if (NEG(Wd(1))) {
                SW(5, Wd(5) + 1);
                if (Wd(5) >= Wd(3)) { edge_fill(r, back); return; }
                SW(1, Wd(1) + Wd(6));
            }
        }
    }
    SW(1, Wd(7) >> 1);
    if (Wd(5) >= Wd(3)) goto fill_all;
    while (edge_count(r)) {
        do {
            SW(5, Wd(5) + 1);
            if (Wd(5) >= Wd(3)) goto fill_all;
            SW(1, Wd(1) - Wd(6));
        } while (!NEG(Wd(1)));
        SW(1, Wd(1) + Wd(7));
    }
    SW(4, width - 1);
    for (;;) {
        edge_put(r, back, Wd(5));
        if (!edge_count(r)) return;
        do {
            SW(5, Wd(5) + 1);
            if (Wd(5) >= Wd(3)) { edge_fill(r, back); return; }
            SW(1, Wd(1) - Wd(6));
        } while (!NEG(Wd(1)));
        SW(1, Wd(1) + Wd(7));
    }
fill_all:
    SW(4, width - 1);
    edge_fill(r, back);
}

/* the column table: G_136E; Mazeworld Original (fn_S2_452A) takes it from its caller's argument */
static uint32_t col_table(Regs *r) { return mz_original ? r32(r->a[6] + 8) : g32(0x136E); }

/* A4 = the table + 2 * D4 (through D1, as the original) */
static void edge_start(Regs *r) {
    D(1) = (uint16_t)(Wd(4) << 1);
    r->a[4] = col_table(r) + D(1);
}

void fn_S2_52A2(Regs *r) {
    SW(2, g16(0x2156));
    SW(3, 0x7FFF);
    int cap, partial;
    if (Wd(4) >= Wd(2)) {
        if (Wd(6) >= Wd(2)) {
            SW(1, Wd(4) ^ Wd(6));
            cc_nz_w(r, Wd(1));
            if (!NEG(Wd(1))) return;           /* both off the same side */
            if (Wd(7) < Wd(5)) edge_exg(r);
            SW(7, Wd(7) - Wd(5));
            SW(6, Wd(6) - Wd(4));
            if (!NEG(Wd(6))) {                 /* from off the left to off the right */
                r->a[4] = col_table(r);
                SW(4, -Wd(4));
                edge_cross(r, 0);
            } else {                           /* from off the right to off the left */
                D(1) = (uint16_t)(Wd(2) << 1);
                r->a[4] = col_table(r) + D(1);
                SW(4, Wd(4) - Wd(2));
                SW(6, -Wd(6));
                edge_cross(r, 1);
            }
            goto done;
        }
        cap = Wd(5) >= Wd(3) || Wd(7) >= Wd(3);
        edge_exg(r);
        partial = 1;
    } else {
        partial = Wd(6) >= Wd(2);
        cap = Wd(5) >= Wd(3) || Wd(7) >= Wd(3);
    }
    if (!partial) {                            /* both ends inside: rows rising from the start */
        if (Wd(7) < Wd(5)) edge_exg(r);
        SW(7, Wd(7) - Wd(5));
        edge_start(r);
        SW(6, Wd(6) - Wd(4));
        int back = NEG(Wd(6));
        if (back) { r->a[4] += 2; SW(6, -Wd(6)); }
        SW(4, Wd(6));
        if (cap) edge_rise_cap(r, back); else edge_rise(r, back);
        goto done;
    }
    /* D4 inside, D6 outside: store from the inside end to the edge of the table */
    edge_start(r);
    SW(6, Wd(6) - Wd(4));
    int back = NEG(Wd(6));
    SW(7, Wd(7) - Wd(5));
    int fall = NEG(Wd(7));
    if (back) { r->a[4] += 2; SW(6, -Wd(6)); }
    if (fall) SW(7, -Wd(7));
    if (!back) { SW(1, Wd(2) - Wd(4) - 1); SW(4, Wd(1)); }
    if (cap) { if (fall) edge_fall_cap(r, back); else edge_rise_cap(r, back); }
    else     { if (fall) edge_fall(r, back);     else edge_rise(r, back); }
done:
    r->ccr = CCR_X | CCR_N | CCR_C;            /* the final subq.w #1,d4 from 0 */
}
#undef D
#undef Wd
#undef SW
#undef NEG

/* ---------- fn_S2_510E: a wall segment onto the screen ----------
 * Works on its caller's frame (A6): the segment's two endpoints relative to the viewer, A at
 * (-$30, -$32) and B at (-$34, -$36), and the view basis: depth axis (-$2C, -$2E), sideways axis
 * (-$28, -$2A). It rotates both points into view space (depth to -$38 / -$3A, sideways to -$3C /
 * -$3E, 8.8 fixed point), clips the segment at depth 0 when it crosses behind the viewer (both
 * behind: nothing), projects each end to a screen column (sideways * G_210A / depth + G_2154) and
 * row (G_2108 / depth; near ends with depth below $100 use a fixed divisor of $20, or $100 when
 * the column would be out of range), and hands the edge to fn_S2_52A2. Only the X flag is
 * tracked in between, the one flag the routine's exits can leave unchanged. */
static void xflag(Regs *r, int x) { r->ccr = (uint8_t)((r->ccr & ~CCR_X) | (x ? CCR_X : 0)); }
static uint32_t asr_l(Regs *r, uint32_t v, int n) { xflag(r, ((int32_t)v >> (n - 1)) & 1); return (uint32_t)((int32_t)v >> n); }
static void divs_reg(uint32_t *dn, uint16_t src) {      /* divs.w src,Dn: Dn unchanged on overflow */
    int32_t q = (int32_t)*dn / (int16_t)src, m = (int32_t)*dn % (int16_t)src;
    if (q == (int16_t)q) *dn = ((uint32_t)(uint16_t)m << 16) | (uint16_t)q;
}
static void seg_project(Regs *r, uint32_t fx, uint32_t fz, int out_h, int out_v) {
    uint32_t a6 = r->a[6];
    set_w(&r->d[2], r16(a6 - fx));
    set_w(&r->d[1], g16(0x210A));
    r->d[1] = (uint32_t)((int32_t)(int16_t)r->d[1] * (int16_t)r->d[2]);
    set_w(&r->d[0], r16(a6 - fz));
    if ((int16_t)r->d[0] >= 0x100) {
        divs_reg(&r->d[1], (uint16_t)r->d[0]);
        add_w(r, &r->d[1], g16(0x2154));
        set_w(&r->d[out_h], (uint16_t)r->d[1]);
        r->d[1] = g32(0x2108);
        divs_reg(&r->d[1], (uint16_t)r->d[0]);
    } else {
        r->d[1] = asr_l(r, r->d[1], 5);
        int far = (int32_t)r->d[1] >= 0x3FFE || (int32_t)r->d[1] < -0x3FFE;
        if (far) r->d[1] = asr_l(r, r->d[1], 3);
        add_w(r, &r->d[1], g16(0x2154));
        set_w(&r->d[out_h], (uint16_t)r->d[1]);
        r->d[1] = asr_l(r, g32(0x2108), far ? 8 : 5);
    }
    set_w(&r->d[out_v], (uint16_t)r->d[1]);
}
void fn_S2_510E(Regs *r) {
    uint32_t a6 = r->a[6];
    static const uint8_t prod[4][5] = {          /* a, b, c, d, out: out = (a*b + c*d) >> 8 */
        {0x30, 0x2C, 0x32, 0x2E, 0x38}, {0x34, 0x2C, 0x36, 0x2E, 0x3A},
        {0x30, 0x28, 0x32, 0x2A, 0x3C}, {0x34, 0x28, 0x36, 0x2A, 0x3E},
    };
    for (int k = 0; k < 4; k++) {
        set_w(&r->d[0], r16(a6 - prod[k][0])); set_w(&r->d[1], r16(a6 - prod[k][1]));
        r->d[1] = (uint32_t)((int32_t)(int16_t)r->d[1] * (int16_t)r->d[0]);
        set_w(&r->d[0], r16(a6 - prod[k][2])); set_w(&r->d[2], r16(a6 - prod[k][3]));
        r->d[2] = (uint32_t)((int32_t)(int16_t)r->d[2] * (int16_t)r->d[0]);
        add_l(r, &r->d[2], r->d[1]);
        xflag(r, (r->d[2] >> 7) & 1);
        r->d[2] >>= 8;
        w16(a6 - prod[k][4], (uint16_t)r->d[2]);
    }
    int behind0 = (r16(a6 - 0x38) & 0x8000) != 0;
    set_w(&r->d[0], r16(a6 - 0x3A));           /* both branches load the far depth next */
    int behind1 = (r16(a6 - 0x3A) & 0x8000) != 0;
    if (behind0 && behind1) { cc_nz_w(r, (uint16_t)r->d[0]); return; }
    if (behind0 || behind1) {                  /* clip at depth 0: x0 + (x1 - x0) * -z0 / (z1 - z0) */
        set_w(&r->d[0], r16(a6 - 0x3E)); sub_w(r, &r->d[0], r16(a6 - 0x3C));
        set_w(&r->d[1], r16(a6 - 0x38));
        xflag(r, (uint16_t)r->d[1] != 0); set_w(&r->d[1], (uint16_t)-(uint16_t)r->d[1]);
        r->d[1] = (uint32_t)((int32_t)(int16_t)r->d[1] * (int16_t)r->d[0]);
        set_w(&r->d[0], r16(a6 - 0x3A)); sub_w(r, &r->d[0], r16(a6 - 0x38));
        divs_reg(&r->d[1], (uint16_t)r->d[0]);
        add_w(r, &r->d[1], r16(a6 - 0x3C));
        if (behind0) { w16(a6 - 0x3C, (uint16_t)r->d[1]); w16(a6 - 0x38, 0); }
        else         { w16(a6 - 0x3E, (uint16_t)r->d[1]); w16(a6 - 0x3A, 0); }
    }
    seg_project(r, 0x3C, 0x38, 4, 5);
    seg_project(r, 0x3E, 0x3A, 6, 7);
    fn_S2_52A2(r);
}

/* fn_S2_4F1C (JT 87): the wall outline for the frame. Clears the column table (G_2156 words at
 * `table`, which becomes G_136E) and draws into it the near faces of the walls around the viewer:
 * the visibility list G_1DD8 for view direction G_20D4 (a little-endian word offset per direction,
 * then 11-byte entries { le16 cell; ...; byte faces at +$A } up to the cell value 100) gives cells
 * of G_1DCC relative to G_20CA; for each existing cell (byte +0 non-zero), the face bits $80 / $40
 * that the cell has and the entry's mask lets through (cell & ((cell ^ mask) << 2)) are drawn as
 * segments (fn_S2_510E): $80 the face at y + $800 running along x, $40 the face at x + $800
 * running along y, each end pushed $40 further from the viewer's axis. Coordinates are the cell
 * centre (+$1C, +$1E) relative to the viewer (G_20D2, G_20CE) in 8.8; the view basis comes from the
 * sine table G_13EE. */
static int16_t wall_pad(int16_t v) { return (int16_t)(v > 0 ? v + 0x40 : v - 0x40); }
void fn_S2_4F1C(uint32_t table) {
    MZ_FRAME;
    sg32(0x136E, table);
    { uint32_t a = table; uint16_t n = g16(0x2156); do { w16(a, 0); a += 2; } while (--n); }
    uint32_t fr = mz_local(0x40), a6 = fr + 0x40;              /* frame for fn_S2_510E */
    uint16_t dir = g16(0x20D4);
    uint32_t sine = GADDR(0x13EE);
    uint16_t c = r16(sine + (uint32_t)(int32_t)(int16_t)(uint16_t)(((dir + 0x20) & 0x3F) << 1));
    w16(a6 - 0x28, c); w16(a6 - 0x2E, c);
    w16(a6 - 0x2A, r16(sine + (uint32_t)(int32_t)(int16_t)(uint16_t)(((dir + 0x10) & 0x3F) << 1)));
    w16(a6 - 0x2C, r16(sine + (uint32_t)(int32_t)(int16_t)(uint16_t)(((dir + 0x30) & 0x3F) << 1)));
    uint32_t e = g32(0x1DD8);
    uint32_t i2 = (uint32_t)(int32_t)(int16_t)(uint16_t)(dir << 1);
    e += (uint32_t)(r8(e + i2 + 1) << 8 | r8(e + i2)) + 0x80;
    Regs r = {{0}, {0}, 0};
    r.a[6] = a6;
    for (uint16_t cellw = (uint16_t)(r8(e + 1) << 8 | r8(e)); cellw != 100; e += 0xB, cellw = (uint16_t)(r8(e + 1) << 8 | r8(e))) {
        uint16_t off = (uint16_t)(cellw + g16(0x20CA));
        if (off & 0x8000) continue;
        uint32_t cell = g32(0x1DCC) + off;
        uint8_t b = r8(cell);
        if (!b) continue;
        uint8_t faces = (uint8_t)(((uint8_t)((r8(e + 0xA) ^ b) << 2) & b) & 0xC0);
        if (!faces) continue;
        int16_t cx = (int16_t)((uint16_t)(r16(cell + 0x1C) << 8) - (uint16_t)(g32(0x20D2) >> 8));
        int16_t cy = (int16_t)((uint16_t)(r16(cell + 0x1E) << 8) - (uint16_t)(g32(0x20CE) >> 8));
        if (faces & 0x80) {
            int16_t x0 = (int16_t)(cx - 0x800), y = wall_pad((int16_t)(cy + 0x800));
            w16(a6 - 0x30, (uint16_t)wall_pad(x0));
            w16(a6 - 0x34, (uint16_t)wall_pad((int16_t)(x0 + 0x1000)));
            w16(a6 - 0x32, (uint16_t)y); w16(a6 - 0x36, (uint16_t)y);
            fn_S2_510E(&r);
        }
        if (faces & 0x40) {
            int16_t x = wall_pad((int16_t)(cx + 0x800)), y0 = (int16_t)(cy - 0x800);
            w16(a6 - 0x30, (uint16_t)x); w16(a6 - 0x34, (uint16_t)x);
            w16(a6 - 0x32, (uint16_t)wall_pad(y0));
            w16(a6 - 0x36, (uint16_t)wall_pad((int16_t)(y0 + 0x1000)));
            fn_S2_510E(&r);
        }
    }
}

/* ---------- the scent trail: a breadth-first distance flood over the cells of G_1DCC ----------
 * fn_S2_355E (JT 84) clears the previous flood (fn_S2_366A) and floods afresh (fn_S2_3568) from
 * the cell G_209A for up to G_209C (at most $C8) cells: each reached open cell (bit 0 of its first
 * byte clear) gets +$10 = the seed's value minus its distance (the seed has $FFFF), and is queued
 * in G_2098. G_22AC = the seed, G_22AE = the number of cells reached (0 / 0 with no flood). */
void fn_S2_366A(Regs *r) {
    r->a[1] = GADDR(0x2098);
    r->a[0] = g32(0x1DCC);
    set_w(&r->d[2], g16(0x22AE));
    if ((uint16_t)r->d[2]) {
        lsl_w(r, &r->d[2], 1);
        sub_w(r, &r->d[2], 2);
        do {
            set_w(&r->d[0], r16(r->a[1] + sxw(r->d[2])));
            w16(r->a[0] + sxw(r->d[0]) + 0x10, 0);
            sub_w(r, &r->d[2], 2);
        } while (!((uint16_t)r->d[2] & 0x8000));
    }
    sg16(0x22AE, 0);
    sg16(0x22AC, 0);
    cc_nz_w(r, 0);
}
/* fn_S2_363E: queues cell D0.w (A2 = queue, D3.w = its byte length, limit D5.w) with value D6.w */
void fn_S2_363E(Regs *r) {
    cc_cmp_w(r, (uint16_t)r->d[5], (uint16_t)r->d[3]);
    if ((uint16_t)r->d[3] >= (uint16_t)r->d[5]) return;
    w16(r->a[2] + sxw(r->d[3]), (uint16_t)r->d[0]);
    add_w(r, &r->d[3], 2);
    w16(r->a[1] + sxw(r->d[0]) + 0x10, (uint16_t)r->d[6]);
    cc_nz_w(r, (uint16_t)r->d[6]);
}
void fn_S2_3568(Regs *r) {
    uint32_t d3 = r->d[3], d4 = r->d[4], d5 = r->d[5], d6 = r->d[6], d7 = r->d[7], a2 = r->a[2];
    set_w(&r->d[0], g16(0x209C));
    if (!(uint16_t)r->d[0]) {
        sg16(0x22AC, 0); sg16(0x22AE, 0);
    } else {
        if ((uint16_t)r->d[0] >= 0xC8) set_w(&r->d[0], 0xC8);
        lsl_w(r, &r->d[0], 1);
        set_w(&r->d[5], (uint16_t)r->d[0]);
        set_w(&r->d[3], 0); set_w(&r->d[4], 0);
        r->a[1] = g32(0x1DCC);
        r->a[2] = GADDR(0x2098);
        set_w(&r->d[6], 0xFFFF);
        set_w(&r->d[0], g16(0x209A));
        fn_S2_363E(r);
        static const uint16_t step[4] = {0x20, 0xFFE0, 0x400, 0xFC00};
        while ((uint16_t)r->d[4] < (uint16_t)r->d[3]) {
            set_w(&r->d[7], r16(r->a[2] + sxw(r->d[4])));
            add_w(r, &r->d[4], 2);
            set_w(&r->d[6], r16(r->a[1] + sxw(r->d[7]) + 0x10));
            sub_w(r, &r->d[6], 1);
            for (int k = 0; k < 4; k++) {
                set_w(&r->d[0], (uint16_t)r->d[7]);
                if (step[k] & 0x8000) sub_w(r, &r->d[0], (uint16_t)-step[k]); else add_w(r, &r->d[0], step[k]);
                if (r8(r->a[1] + sxw(r->d[0])) & 1) continue;
                set_w(&r->d[1], r16(r->a[1] + sxw(r->d[0]) + 0x10));
                if ((uint16_t)r->d[1]) continue;
                fn_S2_363E(r);
            }
        }
        sg16(0x22AC, g16(0x209A));
        set_w(&r->d[0], (uint16_t)r->d[3]);
        lsr_w(r, &r->d[0], 1);
        sg16(0x22AE, (uint16_t)r->d[0]);
    }
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->d[6] = d6; r->d[7] = d7; r->a[2] = a2;
    cc_nz_l(r, d7);   /* flags from restoring D7 */
}
void fn_S2_355E(Regs *r) { fn_S2_366A(r); fn_S2_3568(r); }

/* fn_S2_0376 (JT 75): one tick of every object (fn_S7_2E9A) in G_1DC4 from record $77 down to 1,
 * skipping those flagged gone (+2 negative); each one's +$26 is reset to -1 first. */
void fn_S2_0376(Regs *r) {
    r->a[2] = g32(0x1DC4);
    set_w(&r->d[3], 0x1DC0);
    do {
        set_w(&r->d[0], r16(r->a[2] + sxw(r->d[3]) + 2));
        if (!((uint16_t)r->d[0] & 0x8000)) {
            w16(r->a[2] + sxw(r->d[3]) + 0x26, 0xFFFF);
            set_w(&r->d[0], (uint16_t)r->d[3]);
            mz_seg_of(7, 0x2E9A), fn_S7_2E9A(r);
        }
        sub_w(r, &r->d[3], 0x40);
    } while ((uint16_t)r->d[3]);
}

/* Mazeworld Original's projection of object D0.b (fn_S2_4C94, part of its fn_S2_4A9A whose frame
 * A6 it shares): the object's row (-5(A6)) and depth (-6(A6)) from the cell entry at A1 (bytes +2
 * column, +3 row) plus the table at D7 by the position's fractions (+4 / +8 & 15) and the player's
 * step bytes; behind the viewer (negative row) it is skipped. Screen x (+$28, mirrored for a
 * negative column) from the projection table G_53EC, height (+$2A) from G_53C4, row into +$26;
 * an object sunk in the floor (+$3C negative) adds it to the depth; the depth (+$2C). The list
 * entry at A4 gets the object and its distance byte (G_53BC map + A4+$36, at least 1), and A4
 * moves on. A record whose +$26 is not negative is already projected. An object with a radius
 * (+$3C positive) keeps its depth cell off walls: the original tests the cell at the word in low
 * memory $30 (a bug: it meant the object's +$30) plus or minus a row / a column. */
void fo_S7_0D32(Regs *r);
static void orig_project(Regs *r) {
    uint32_t a6 = r->a[6];
    w8(r->a[4] + 1, (uint8_t)r->d[0]);
    r->d[0] &= 0xFF; r->d[0] <<= 6;
    r->a[3] = og32(0x53C8) + r->d[0];
    uint32_t o = r->a[3];
    set_w(&r->d[0], r16(o + 0x26));
    uint8_t row, depth, b;
    if (!((uint16_t)r->d[0] & 0x8000)) {                    /* already projected */
        w8(a6 - 5, (uint8_t)r->d[0]); w8(a6 - 6, (uint8_t)r->d[0]);
        set_w(&r->d[0], (uint16_t)(r16(o + 4) & 0xF)); set_w(&r->d[1], (uint16_t)(r16(o + 8) & 0xF));
        r->a[0] = r->d[7];
        goto column;
    }
    set_w(&r->d[0], r16(o + 0x3C));
    if ((uint16_t)r->d[0] && !((uint16_t)r->d[0] & 0x8000)) {   /* a radius: the depth cell kept off walls */
        uint32_t map = og32(0x53D4);
        uint16_t rad = r16(o + 0x3C), bug = r16(0x30);
        r->a[0] = map;
        set_w(&r->d[0], (uint16_t)(r16(o + 4) & 0xF));
        if ((uint16_t)r->d[0] < rad) { set_w(&r->d[2], (uint16_t)(bug - 0x20)); if (r8(map + sxw(r->d[2])) & 1) set_w(&r->d[0], rad); }
        else { set_w(&r->d[2], (uint16_t)(0x10 - rad)); if ((uint16_t)r->d[0] >= (uint16_t)r->d[2]) { set_w(&r->d[2], (uint16_t)(bug + 0x20)); if (r8(map + sxw(r->d[2])) & 1) set_w(&r->d[0], (uint16_t)(0x10 - rad)); } }
        set_w(&r->d[1], (uint16_t)(r16(o + 8) & 0xF));
        if ((uint16_t)r->d[1] < rad) { set_w(&r->d[2], (uint16_t)(bug - 0x400)); if (r8(map + sxw(r->d[2])) & 1) set_w(&r->d[1], rad); }
        else { set_w(&r->d[2], (uint16_t)(0x10 - rad)); if ((uint16_t)r->d[1] >= (uint16_t)r->d[2]) { set_w(&r->d[2], (uint16_t)(bug + 0x400)); if (r8(map + sxw(r->d[2])) & 1) set_w(&r->d[1], (uint16_t)(0x10 - rad)); } }
        r->a[0] = r->d[7];
        b = (uint8_t)(r8(r->a[1] + 3) + r8(r->a[0] + 0x10 + sxw(r->d[0])) + r8(r->a[0] + 0x30 + sxw(r->d[1])) + og8(0x582F));
        if ((int8_t)b < 0) r->d[2] = 0; else set_b(&r->d[2], b);   /* moveq #0 when negative */
        w8(a6 - 6, (uint8_t)r->d[2]);
        set_w(&r->d[0], (uint16_t)(r16(o + 4) & 0xF)); set_w(&r->d[1], (uint16_t)(r16(o + 8) & 0xF));
        r->a[0] = r->d[7];
        b = (uint8_t)(r8(r->a[1] + 3) + r8(r->a[0] + 0x10 + sxw(r->d[0])) + r8(r->a[0] + 0x30 + sxw(r->d[1])) + og8(0x582F));
        set_b(&r->d[2], b);
        if ((int8_t)b < 0) return;
        w8(a6 - 5, b);
        goto column;
    }
    set_w(&r->d[0], (uint16_t)(r16(o + 4) & 0xF)); set_w(&r->d[1], (uint16_t)(r16(o + 8) & 0xF));
    r->a[0] = r->d[7];
    b = (uint8_t)(r8(r->a[1] + 3) + r8(r->a[0] + 0x10 + sxw(r->d[0])) + r8(r->a[0] + 0x30 + sxw(r->d[1])) + og8(0x582F));
    set_b(&r->d[2], b);
    if ((int8_t)b < 0) return;                               /* behind the viewer */
    w8(a6 - 5, b); w8(a6 - 6, b);
column:
    b = (uint8_t)(r8(r->a[1] + 2) + r8(r->a[0] + sxw(r->d[0])) + r8(r->a[0] + 0x20 + sxw(r->d[1])) + og8(0x5830));
    row = r8(a6 - 5);
    {
        int neg = (int8_t)b < 0;
        uint8_t c2 = (uint8_t)((neg ? (uint8_t)-b : b) << 1);
        r->a[0] = og32(0x53EC);
        set_w(&r->d[1], (uint16_t)(row << 8 | c2));
        int16_t x = (int16_t)r16(r->a[0] + sxw(r->d[1]));
        set_w(&r->d[0], (uint16_t)((neg ? -x : x) + (int16_t)og16(0x5736)));
        w16(o + 0x28, (uint16_t)r->d[0]);
        r->a[0] = og32(0x53C4);
        set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] >> 1));
        r->d[0] = r8(r->a[0] + sxw(r->d[1]));
        if (neg) set_w(&r->d[0], (uint16_t)-(int16_t)(uint16_t)r->d[0]);
        w16(o + 0x2A, (uint16_t)r->d[0]);
        set_b(&r->d[2], c2);
    }
    r->d[0] = row; w16(o + 0x26, (uint16_t)r->d[0]);
    set_w(&r->d[0], r16(o + 0x3C));
    if ((uint16_t)r->d[0] & 0x8000) {
        depth = (uint8_t)((uint8_t)r->d[0] + r8(a6 - 6));
        if ((int8_t)depth < 0) depth = 0;
        w8(a6 - 6, depth);
    }
    r->d[0] = r8(a6 - 6); w16(o + 0x2C, (uint16_t)r->d[0]);
    r->a[0] = og32(0x53BC);
    set_w(&r->d[1], (uint16_t)(((uint16_t)(r16(o + 8) - og16(0x56D4)) & 0x7F) << 7));
    set_w(&r->d[0], (uint16_t)((uint16_t)(r16(o + 4) - og16(0x56D8)) & 0x7F));
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    r->d[0] = r8(r->a[0] + sxw(r->d[1]));
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] + r16(o + 0x36)));
    if ((int16_t)(uint16_t)r->d[0] <= 0) set_w(&r->d[0], 1);
    w8(r->a[4], (uint8_t)r->d[0]);
    r->a[4] += 2;
}

/* ---------- fn_S2_5810 (JT 88): the objects in view ----------
 * Collects the objects in the cells visible from the player's cell (G_20CA) looking in direction
 * G_20D4: the visibility list G_1DB4 for that direction (little-endian cell offsets, 100 ends it)
 * names the cells; each cell of G_1DCC lists up to ten object numbers (+1..+$A, 0 ends) and has bit
 * 0 of +$12 when a spark must be added at its centre (fn_S2_5A6C, records from $78 up). Each is
 * projected (fn_S2_5AB2) into the list G_02E8 of { distance, object } words, which is then sorted
 * far to near (bubble sort on the distance byte) and drawn in that order (fn_S7_2D1C) clipped
 * to the columns where the wall table G_1DB0 lets it show: an object of screen height d7 (from
 * G_1DE4 by its size +$2C) shows in the columns whose wall value is at most d7, between its
 * centre column (+$28) +- half its width (G_1DE4 by depth +$26 and its kind +$38).
 * The original keeps the view basis and spark counter in its frame for fn_S2_5AB2 / fn_S2_5A6C. */
void fn_S2_5810(Regs *r) {
    MZ_FRAME;
    uint32_t a4_saved = r->a[4], a6_saved = r->a[6];
    uint32_t fp = mz_local(0x12) + 0x12;
    r->a[6] = fp;
    if (!mz_original) w16(fp - 8, 0x78);
    w32(fp - 4, r->a[4]);
    r->a[2] = g32(0x1DCC);
    r->d[7] = g32(0x1DD4);
    r->d[0] = 0; set_w(&r->d[0], g16(0x20D4)); lsl_w(r, &r->d[0], 6);
    add_l(r, &r->d[7], r->d[0]);
    static const uint16_t quarter[3] = {0x20, 0x10, 0x30};
    if (!mz_original) r->a[0] = GADDR(0x136A);
    for (int k = 0; k < (mz_original ? 0 : 3); k++) {          /* (Original has no view basis here) */
        set_w(&r->d[0], g16(0x20D4)); add_w(r, &r->d[0], quarter[k]);
        set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] & 0x3F)); lsl_w(r, &r->d[0], 1);
        uint16_t v = r16(r->a[0] + sxw(r->d[0]));
        if (k == 0) { set_w(&r->d[0], v); w16(fp - 0xA, v); w16(fp - 0x10, v); }
        else w16(fp - (k == 1 ? 0xC : 0xE), v);
    }
    r->a[4] = GADDR(0x02E8);
    r->a[1] = g32(0x1DB4);
    set_w(&r->d[0], g16(0x20D4)); lsl_w(r, &r->d[0], 1);
    r->d[1] = 0;
    set_b(&r->d[1], r8(r->a[1] + sxw(r->d[0]) + 1)); lsl_w(r, &r->d[1], 8);
    set_b(&r->d[1], r8(r->a[1] + sxw(r->d[0])));
    r->a[1] += r->d[1];
    r->a[1] += 0x80;
    set_b(&r->d[6], r8(r->a[1] + 1)); lsl_w(r, &r->d[6], 8); set_b(&r->d[6], r8(r->a[1]));
    do {
        add_w(r, &r->d[6], g16(0x20CA));
        if (!((uint16_t)r->d[6] & 0x8000)) {
            for (int k = 1; k <= 10; k++) {
                set_b(&r->d[0], r8(r->a[2] + sxw(r->d[6]) + (uint32_t)k));
                if (!(uint8_t)r->d[0]) break;
                if (mz_original) orig_project(r); else fn_S2_5AB2(r);
            }
        }
        if (!mz_original && (r8(r->a[2] + sxw(r->d[6]) + 0x12) & 1)) {
            set_w(&r->d[0], r16(r->a[2] + sxw(r->d[6]) + 0x1C));
            set_w(&r->d[1], r16(r->a[2] + sxw(r->d[6]) + 0x1E));
            fn_S2_5A6C(r);
        }
        r->a[1] += 4;
        set_b(&r->d[6], r8(r->a[1] + 1)); lsl_w(r, &r->d[6], 8); set_b(&r->d[6], r8(r->a[1]));
    } while ((uint16_t)r->d[6] != 0x64);
    w16(r->a[4], 0); r->a[4] += 2;
    /* far to near */
    r->a[4] = GADDR(0x02E8);
    r->d[1] = r->a[4];
    if (r16(r->a[4]) && r16(r->a[4] + 2)) {
        do {
            r->a[4] = r->d[1];
            r->d[2] = 0;
            set_b(&r->d[0], r8(r->a[4] + 2));
            do {
                if ((uint8_t)r->d[0] > r8(r->a[4])) {
                    uint16_t t = r16(r->a[4]);
                    set_w(&r->d[0], t);
                    w16(r->a[4], r16(r->a[4] + 2)); w16(r->a[4] + 2, t);
                    r->d[2] = 1;
                }
                r->a[4] += 2;
                set_b(&r->d[0], r8(r->a[4] + 2));
            } while ((uint8_t)r->d[0]);
        } while ((uint16_t)r->d[2]);
    }
    /* draw */
    r->a[4] = GADDR(0x02E8);
    r->a[2] = g32(0x1DE4);
    set_w(&r->d[0], r16(r->a[4])); r->a[4] += 2;
    uint16_t width = 0;
    while ((uint16_t)r->d[0]) {
        r->a[3] = g32(0x1DC4);
        r->d[0] = ((uint32_t)r->d[0] & 0xFF) << 6;
        r->d[3] = r->d[0];
        r->a[3] += r->d[0];
        uint32_t o = r->a[3];
        sg16(0x20A8, r16(o + 0x28)); sg16(0x20AA, r16(o + 0x26)); sg16(0x209E, r16(o + 0x3A));
        set_w(&r->d[1], (uint16_t)(r16(o + 0x2C) << 8)); set_b(&r->d[1], 0xFE);
        set_w(&r->d[7], r16(r->a[2] + sxw(r->d[1])));
        set_w(&r->d[1], (uint16_t)(r16(o + 0x26) << 8)); set_b(&r->d[1], (uint8_t)r16(o + 0x38));
        set_w(&r->d[6], r16(r->a[2] + sxw(r->d[1])));
        width = g16(0x2156);
        r->d[0] = 0; set_w(&r->d[0], (uint16_t)r->d[6]);
        add_w(r, &r->d[0], g16(0x20A8));
        if ((uint16_t)r->d[0] & 0x8000) goto next;
        if ((uint16_t)r->d[0] >= width) set_w(&r->d[0], width);
        r->a[1] = g32(0x1DB0);
        lsl_w(r, &r->d[0], 1);
        r->a[1] += r->d[0];
        set_w(&r->d[0], g16(0x20A8)); sub_w(r, &r->d[0], (uint16_t)r->d[6]);
        if ((uint16_t)r->d[0] & 0x8000) set_w(&r->d[0], 0);
        if ((uint16_t)r->d[0] >= width) goto next;
        r->a[0] = g32(0x1DB0);
        lsl_w(r, &r->d[0], 1);
        r->a[0] += r->d[0];
        {
            uint16_t h = (uint16_t)r->d[7];
#define RIGHT_VISIBLE() (r->a[1] -= 2, h >= r16(r->a[1]))
#define LEFT_VISIBLE()  (r->a[0] += 2, h >= r16(r->a[0] - 2))
            if (RIGHT_VISIBLE()) {
                if (!LEFT_VISIBLE()) while (!LEFT_VISIBLE()) ;
            } else if (LEFT_VISIBLE()) {
                while (!RIGHT_VISIBLE()) ;
            } else {
                for (;;) {
                    if (r->a[0] >= r->a[1]) goto next;
                    if (LEFT_VISIBLE()) break;
                }
                while (!RIGHT_VISIBLE()) ;
            }
#undef RIGHT_VISIBLE
#undef LEFT_VISIBLE
        }
        r->a[1] += 2;
        r->a[0] -= 2;
        r->d[0] = (r->a[0] - g32(0x1DB0)) >> 1;
        if ((uint16_t)r->d[0] > width) goto next;
        r->d[1] = (r->a[1] - g32(0x1DB0)) >> 1;
        if ((uint16_t)r->d[1] > width) goto next;
        if ((uint16_t)r->d[0] >= (uint16_t)r->d[1]) goto next;
        add_w(r, &r->d[0], 4); sg16(0x20A6, (uint16_t)r->d[0]);
        add_w(r, &r->d[1], 4); sg16(0x20A4, (uint16_t)r->d[1]);
        sg16(0x20A8, (uint16_t)(g16(0x20A8) + 4));
        set_w(&r->d[0], r16(o + 2));
        r->a[0] = o;
        r->d[1] = r->d[3];
        if (mz_original) mz_seg(7), fo_S7_0D32(r);
        else mz_seg_of(7, 0x2D1C), fn_S7_2D1C(r);
    next:
        set_w(&r->d[0], r16(r->a[4])); r->a[4] += 2;
    }
    r->a[4] = r32(fp - 4);
    r->a[6] = a6_saved;
    (void)a4_saved;
    cc_nz_w(r, 0);
}

/* fn_S2_3A36 (JT 93): rebuilds the shading ramp G_1DAC when G_2104 is set or the shading choice
 * G_2102 differs from the one it was last built for (G_144C). The choice picks a start and an end
 * value (single-precision; out-of-range choices start at -100); the table gets $E and then
 * G_214E + 1 entries running linearly from start to end in 16.16 fixed point, each the integer
 * part clamped to 2..15 (anything below 2 becomes 0). The arithmetic goes through SANE exactly as
 * the original's, so rounding matches bit for bit. */
static const uint32_t shade_start[40] = {
    0x40000000, 0x3FF33333, 0x3FE66666, 0x3FD9999A, 0x3FCF5C29, 0x3FB33333, 0x3FA00000, 0x3F666666,
    0x3F23D70A, 0x3E99999A, 0x00000000, 0xBECCCCCD, 0xBF47AE14, 0xBFA00000, 0xBFE00000, 0xC0100000,
    0xC0370A3D, 0xC06CCCCD, 0xC09570A4, 0xC0B00000, 0xC0E00000, 0xC10CCCCD, 0xC1280000, 0xC1600000,
    0xC182A3D7, 0xC1B00000, 0xC1E00000, 0xC2360000, 0xC27C0000, 0xC27E0000, 0xC2800000, 0xC2810000,
    0xC2820000, 0xC2830000, 0xC2840000, 0xC2850000, 0xC2860000, 0xC2870000, 0xC2880000, 0xC2890000,
};

/* trunc(single at f * 65536) as a 16.16 long, via extended x */
static int32_t shade_fixed(uint32_t x, uint32_t tmp) {
    w32(tmp, 0x10000); mac_FP68K(FP_LONG | FP_MUL, x, tmp);
    mac_FP68K(FP_TRUNC, x, 0);
    mac_FP68K(FP_LONG | FP_STORE, tmp, x);
    return (int32_t)r32(tmp);
}

void fn_S2_3A36(void) {
    MZ_FRAME;
    if (!g8(0x2104) && g16(0x2102) == g16(0x144C)) return;
    uint32_t x = mz_local(10), start = mz_local(4), end = mz_local(4), tmp = mz_local(4);
    w32(end, 0x40E051EC);                                    /* 7.01 */
    uint16_t sel = g16(0x2102);
    if (sel < 40) {
        w32(start, shade_start[sel]);
        if (sel >= 29) {                                     /* end = 7.01 - (sel - 28) / 2 */
            uint16_t halves = (uint16_t)(sel - 28);
            mac_FP68K(FP_LOAD | FP_SINGLE, x, end);
            if (halves & 1) {                                /* a single-precision n.5 constant */
                static const uint32_t half[6] = { 0x3F000000, 0x3FC00000, 0x40200000, 0x40600000, 0x40900000, 0x40B00000 };
                w32(tmp, half[halves / 2]); mac_FP68K(FP_SINGLE | FP_SUB, x, tmp);
            } else {                                         /* an integer constant */
                w16(tmp, (uint16_t)(halves / 2)); mac_FP68K(FP_INT | FP_SUB, x, tmp);
            }
            mac_FP68K(FP_SINGLE | FP_STORE, end, x);
        }
    } else switch (sel) {
    case 40: w32(start, 0x3F800000); w32(end, 0x40066666); break;   /* 1 .. 2.1 */
    case 41: w32(start, 0x3F800000); w32(end, 0x4001EB85); break;   /* 1 .. 2.03 */
    case 42: w32(start, 0x3F800000); w32(end, 0x3F800000); break;   /* 1 .. 1 */
    case 43: w32(start, 0x41700000); w32(end, 0x41700000); break;   /* 15 .. 15 */
    case 44: case 45: w32(start, 0x40000000); break;                /* 2 .. 7.01 */
    default: w32(start, 0xC2C80000); break;                         /* -100 .. 7.01 */
    }
    uint32_t p = g32(0x1DAC);
    w16(p, 0xE); p += 2;
    mac_FP68K(FP_LOAD | FP_SINGLE, x, start);
    int32_t acc = shade_fixed(x, tmp);
    int16_t n = (int16_t)g16(0x214E);
    mac_FP68K(FP_LOAD | FP_SINGLE, x, end);
    mac_FP68K(FP_SINGLE | FP_SUB, x, start);
    w16(tmp, (uint16_t)(n - 1)); mac_FP68K(FP_INT | FP_DIV, x, tmp);
    int32_t step = shade_fixed(x, tmp);
    for (int16_t i = 1; i < (int16_t)(n + 1); i++) {
        int16_t v = (int16_t)(acc >> 16);
        if (v < 2) v = 0; else if (v > 15) v = 15;
        w16(p, (uint16_t)v); p += 2;
        acc = (int32_t)((uint32_t)acc + (uint32_t)step);
    }
    sg8(0x2104, 0);
    sg16(0x144C, g16(0x2102));
}

/* fn_S2_01FA (JT 105): a 16-entry palette (pmTolerant, tolerance 0) from 'clut' id, or 0 with an
 * error alert (G_2480) if the colour table is missing (G_2454 if the palette cannot be made). */
uint32_t fn_S2_01FA(int16_t id) {
    uint32_t ct = mac_GetCTable(id);
    if (!ct) { fn_S2_03A0(GADDR(0x2480), 0, 0); return 0; }
    uint32_t pal = mac_NewPalette(0x10, ct, 2, 0);
    if (!pal) fn_S2_03A0(GADDR(0x2454), 0, 0);
    mac_DisposeCTable(ct);
    return pal;
}

/* fn_S2_3872 (JT 101): disposes of the GWorld *pgw, if any, and clears it. */
void fn_S2_3872(uint32_t pgw) {
    if (r32(pgw)) mac_DisposeGWorld(r32(pgw));
    w32(pgw, 0);
}

/* fn_S2_48D2 (JT 97): the level, bonus and score numbers (G_20EC, G_20EA, G_20E6), each redrawn when
 * it changed since last drawn (G_143E, G_143C, G_1438) or everything is to be redrawn (G_20ED):
 * centred in boxes G_212A..G_2126 (top, bottom) by G_2128..G_2124 (left, right), one below the
 * other G_2122 apart, in the panel at G_2160 / G_2162 (0, 0 for the other window, G_21FC), in
 * colour 15 on 0, font G_2244, size G_2120. */
static void panel_number(int32_t value, int16_t k, int16_t h, int16_t v) {
    MZ_FRAME;
    uint32_t str = mz_local(0x100), box = mz_local(8);
    mz_seg_of(3, 0x038E), fn_S3_038E(value, str);
    int16_t gap = (int16_t)(k * (int16_t)g16(0x2122));
    w16(box + 2, (uint16_t)(h + g16(0x2128)));
    w16(box + 6, (uint16_t)(h + g16(0x2124)));
    w16(box, (uint16_t)(v + g16(0x212A) + gap));
    w16(box + 4, (uint16_t)(v + g16(0x2126) + gap));
    mac_PmForeColor(15); mac_PmBackColor(0);
    mac_TextSize((int16_t)g16(0x2120)); mac_TextFont((int16_t)g16(0x2244)); mac_TextMode(0);
    mac_TETextBox(str + 1, r8(str), box, 1);
}
void fn_S2_48D2(void) {
    int16_t h = 0, v = 0;
    if (!g8(0x21FC)) { h = (int16_t)g16(0x2162); v = (int16_t)g16(0x2160); }
    if (g8(0x20ED) || g16(0x20EC) != g16(0x143E)) {
        panel_number((int16_t)g16(0x20EC), 0, h, v);
        sg16(0x143E, g16(0x20EC));
    }
    if (g8(0x20ED) || g32(0x20EA) != g32(0x143C)) {
        panel_number((int32_t)g32(0x20EA), 1, h, v);
        sg32(0x143C, g32(0x20EA));
    }
    if (g8(0x20ED) || g32(0x20E6) != g32(0x1438)) {
        panel_number((int32_t)g32(0x20E6), 2, h, v);
        sg32(0x1438, g32(0x20E6));
    }
    sg8(0x20ED, 0);
}
