/* CODE segment 7 of Mazeworld Original: its own object helpers (no Catacombs counterpart), named by
 * Original's offsets (see build/disasm-original/seg7.s). Register interfaces (game/mzasm.h). */
#include "mzasm.h"

/* fo_S7_1278 (JT 92): the word D0 into the object table G_53C8 at D1 + $12. A0 = the table. */
void fo_S7_1278(Regs *r) {
    r->a[0] = og32(0x53C8);
    w16(r->a[0] + sxw(r->d[1]) + 0x12, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fo_S7_1282 (JT 93): D0.w = the word at G_53C8 + 2 * D0 + $16, masked by D1. A0 = the table,
 * D2.w = 2 * D0. */
void fo_S7_1282(Regs *r) {
    r->a[0] = og32(0x53C8);
    set_w(&r->d[2], (uint16_t)r->d[0]);
    lsl_w(r, &r->d[2], 1);
    set_w(&r->d[0], (uint16_t)(r16(r->a[0] + sxw(r->d[2]) + 0x16) & (uint16_t)r->d[1]));
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* fo_S7_155C (JT 105): the cell offset of the position G_382C / G_3828 (integer parts & $1F0,
 * combined as fn_S2_3480 does) into G_3824. D0.w = it, D1.w = its row part. */
void fo_S7_155C(Regs *r) {
    set_w(&r->d[0], (uint16_t)(og16(0x382C) & 0x1F0)); lsl_w(r, &r->d[0], 1);
    set_w(&r->d[1], (uint16_t)(og16(0x3828) & 0x1F0)); lsl_w(r, &r->d[1], 6);
    set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] | (uint16_t)r->d[1]));
    sog16(0x3824, (uint16_t)r->d[0]);
    cc_nz_w(r, (uint16_t)r->d[0]);
}

/* the index of the object at A4 in the 128 x 128 distance maps: ((G_3818 - y) & $7F) << 7 |
 * ((G_381C - x) & $7F), into D1; D0 = the x part; A0 = G_53BC */
static void dist_index(Regs *r) {
    set_w(&r->d[1], og16(0x3818)); sub_w(r, &r->d[1], r16(r->a[4] + 8));
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] & 0x7F)); lsl_w(r, &r->d[1], 7);
    set_w(&r->d[0], og16(0x381C)); sub_w(r, &r->d[0], r16(r->a[4] + 4));
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
