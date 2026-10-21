/* Register-level translation of the game's hand-written 68k assembly routines.
 *
 * These routines pass arguments and results in registers (and sometimes condition codes), and
 * their callers may rely on any register they leave behind. Until every caller is C, a translated
 * routine works on a Regs block and must leave every data/address register and the CCR exactly as
 * the original does, partial-width writes included (a move.b changes only the low byte).
 * The harness check mode (-subcheck) compares all of them after every call.
 *
 * Conventions: r->d[n] / r->a[n] are Dn / An; A7 (the stack) is not touched by these routines;
 * the CCR holds X N Z V C in its low five bits as on the 68k.
 */
#ifndef MZASM_H
#define MZASM_H
#include "mz.h"

typedef struct { uint32_t d[8], a[8]; uint8_t ccr; } Regs;
enum { CCR_C = 1, CCR_V = 2, CCR_Z = 4, CCR_N = 8, CCR_X = 16 };

/* A Toolbox or OS trap called from register-level code: registers in and out through the block
 * (the OS dispatcher's TST.W D0 included). */
void mac_trap_regs(Regs *r, uint16_t trapword);
void mz_draw_stream(Regs *r, uint32_t stream);      /* game/seg7.c: model stream G_20A2, drawn */
void mz_draw_by_angle(Regs *r, uint32_t models);

/* partial-width register writes */
static inline void set_b(uint32_t *r, uint8_t v)  { *r = (*r & 0xFFFFFF00u) | v; }
static inline void set_w(uint32_t *r, uint16_t v) { *r = (*r & 0xFFFF0000u) | v; }

/* N and Z from a result, V and C cleared, X kept: move, moveq, and/or/eor, tst, clr */
static inline void cc_nz_b(Regs *r, uint8_t v)  { r->ccr = (uint8_t)((r->ccr & CCR_X) | (v ? 0 : CCR_Z) | (v & 0x80 ? CCR_N : 0)); }
static inline void cc_nz_w(Regs *r, uint16_t v) { r->ccr = (uint8_t)((r->ccr & CCR_X) | (v ? 0 : CCR_Z) | (v & 0x8000 ? CCR_N : 0)); }
static inline void cc_nz_l(Regs *r, uint32_t v) { r->ccr = (uint8_t)((r->ccr & CCR_X) | (v ? 0 : CCR_Z) | (v & 0x80000000u ? CCR_N : 0)); }

/* cmp.b src,dst: flags of dst - src (X kept) */
static inline void cc_cmp_b(Regs *r, uint8_t src, uint8_t dst) {
    uint8_t res = (uint8_t)(dst - src);
    int v = ((src ^ dst) & (res ^ dst)) & 0x80;
    r->ccr = (uint8_t)((r->ccr & CCR_X) | (res ? 0 : CCR_Z) | (res & 0x80 ? CCR_N : 0) | (v ? CCR_V : 0) | (src > dst ? CCR_C : 0));
}

/* cmp.w src,dst: flags of dst - src (X kept) */
static inline void cc_cmp_w(Regs *r, uint16_t src, uint16_t dst) {
    uint16_t res = (uint16_t)(dst - src);
    int v = ((src ^ dst) & (res ^ dst)) & 0x8000;
    r->ccr = (uint8_t)((r->ccr & CCR_X) | (res ? 0 : CCR_Z) | (res & 0x8000 ? CCR_N : 0) | (v ? CCR_V : 0) | (src > dst ? CCR_C : 0));
}

/* add / sub of a source into a data register, all flags (X = C), at word or long size; returns the result */
static inline uint16_t sub_w(Regs *r, uint32_t *reg, uint16_t src) {
    uint16_t dst = (uint16_t)*reg, res = (uint16_t)(dst - src);
    int v = ((src ^ dst) & (res ^ dst)) & 0x8000, c = src > dst;
    set_w(reg, res);
    r->ccr = (uint8_t)((res ? 0 : CCR_Z) | (res & 0x8000 ? CCR_N : 0) | (v ? CCR_V : 0) | (c ? CCR_C | CCR_X : 0));
    return res;
}
static inline uint16_t add_w(Regs *r, uint32_t *reg, uint16_t src) {
    uint16_t dst = (uint16_t)*reg, res = (uint16_t)(dst + src);
    int v = (~(src ^ dst) & (res ^ dst)) & 0x8000, c = res < dst;
    set_w(reg, res);
    r->ccr = (uint8_t)((res ? 0 : CCR_Z) | (res & 0x8000 ? CCR_N : 0) | (v ? CCR_V : 0) | (c ? CCR_C | CCR_X : 0));
    return res;
}
static inline uint32_t add_l(Regs *r, uint32_t *reg, uint32_t src) {
    uint32_t dst = *reg, res = dst + src;
    int v = (~(src ^ dst) & (res ^ dst)) >> 31, c = res < dst;
    *reg = res;
    r->ccr = (uint8_t)((res ? 0 : CCR_Z) | (res >> 31 ? CCR_N : 0) | (v ? CCR_V : 0) | (c ? CCR_C | CCR_X : 0));
    return res;
}
static inline uint32_t sub_l(Regs *r, uint32_t *reg, uint32_t src) {
    uint32_t dst = *reg, res = dst - src;
    int v = ((src ^ dst) & (res ^ dst)) >> 31, c = src > dst;
    *reg = res;
    r->ccr = (uint8_t)((res ? 0 : CCR_Z) | (res >> 31 ? CCR_N : 0) | (v ? CCR_V : 0) | (c ? CCR_C | CCR_X : 0));
    return res;
}
/* lsl.l #1 */
static inline void lsl_l1(Regs *r, uint32_t *reg) {
    uint32_t v = *reg; int out = v >> 31; v <<= 1; *reg = v;
    r->ccr = (uint8_t)((v ? 0 : CCR_Z) | (v >> 31 ? CCR_N : 0) | (out ? CCR_C | CCR_X : 0));
}
static inline uint32_t swap(uint32_t v) { return (v << 16) | (v >> 16); }
/* sign-extended word index, as in d(An, Dn.w) addressing */
static inline uint32_t sxw(uint32_t reg) { return (uint32_t)(int32_t)(int16_t)reg; }

/* lsl.w / lsr.w #n on the low word of a data register (1 <= n <= 8): result, N Z, V cleared, X = C = last bit out */
static inline void lsl_w(Regs *r, uint32_t *reg, int n) {
    uint16_t v = (uint16_t)*reg; int out = (v >> (16 - n)) & 1; v = (uint16_t)(v << n); set_w(reg, v);
    r->ccr = (uint8_t)((v ? 0 : CCR_Z) | (v & 0x8000 ? CCR_N : 0) | (out ? CCR_C | CCR_X : 0));
}
static inline void lsr_w(Regs *r, uint32_t *reg, int n) {
    uint16_t v = (uint16_t)*reg; int out = (v >> (n - 1)) & 1; v = (uint16_t)(v >> n); set_w(reg, v);
    r->ccr = (uint8_t)((v ? 0 : CCR_Z) | (v & 0x8000 ? CCR_N : 0) | (out ? CCR_C | CCR_X : 0));
}
/* Offset into the 128x128 maps (G_1DBC, G_1DB8) of the point (h, v) relative to (h0, v0), using the
 * integer parts of 16.16 positions: ((v - v0) & 127) << 7 | ((h - h0) & 127), computed in D1 with
 * D0 as scratch, flags as the original's sub/andi/lsl/or sequence leaves them. */
static inline void map_offset(Regs *r, uint16_t v, uint16_t v0, uint16_t h, uint16_t h0) {
    set_w(&r->d[1], v); sub_w(r, &r->d[1], v0);
    set_w(&r->d[1], (uint16_t)r->d[1] & 0x7F); lsl_w(r, &r->d[1], 7);
    set_w(&r->d[0], h); sub_w(r, &r->d[0], h0);
    set_w(&r->d[0], (uint16_t)r->d[0] & 0x7F);
    set_w(&r->d[1], (uint16_t)((uint16_t)r->d[1] | (uint16_t)r->d[0]));
    cc_nz_w(r, (uint16_t)r->d[1]);
}

#endif
