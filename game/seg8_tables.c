/* CODE segment 8: the renderer's lookup tables, rebuilt whenever the view's scale G_214C (a single)
 * changes (fn_S8_5884). Every table has 102 rows: row 0 for distance 1/8 of a cell, row i for
 * distance i; the values come from SANE arithmetic exactly as the original's, so they match bit for
 * bit. */
#include "mz.h"
#include "fns.h"

/* an extended in the Mac address space: sign/exponent word, 64-bit mantissa */
static uint32_t ext_new(uint16_t exp, uint32_t hi, uint32_t lo) {   /* in the caller's frame */
    uint32_t x = mz_local(10);
    w16(x, exp); w32(x + 2, hi); w32(x + 6, lo);
    return x;
}
static void ext_copy(uint32_t dst, uint32_t src) { for (int k = 0; k < 10; k += 2) w16(dst + k, r16(src + k)); }

/* scratch for operands and results (the original's frame), set up by each entry point */
static uint32_t scratch, tmp_ext;
static void scratch_init(void) { scratch = mz_local(8); tmp_ext = mz_local(10); }

/* x op= an integer constant */
static void fp_int(uint16_t op, uint32_t x, int16_t v)  { w16(scratch, (uint16_t)v); mac_FP68K((uint16_t)(FP_INT | op), x, scratch); }
static void fp_long(uint16_t op, uint32_t x, int32_t v) { w32(scratch, (uint32_t)v); mac_FP68K((uint16_t)(FP_LONG | op), x, scratch); }

/* trunc(x) as a comp, low 32 bits */
static int32_t trunc_comp_low(uint32_t x) {
    mac_FP68K(FP_TRUNC, x, 0);
    mac_FP68K(FP_COMP | FP_STORE, scratch, x);
    return (int32_t)r32(scratch + 4);
}
/* trunc(x) as an integer of the given format (FP_INT / FP_LONG) */
static int32_t trunc_to(uint16_t fmt, uint32_t x) {
    mac_FP68K(FP_TRUNC, x, 0);
    mac_FP68K((uint16_t)(fmt | FP_STORE), scratch, x);
    return fmt == FP_INT ? (int16_t)r16(scratch) : (int32_t)r32(scratch);
}

/* The 16.16 step of row `i`: base * 65536 / i, or for row 0 base * mul0 * 65536 */
static int32_t row_step(uint32_t base, int16_t i, int16_t mul0) {
    uint32_t t = tmp_ext;
    ext_copy(t, base);
    if (i == 0) { fp_int(FP_MUL, t, mul0); fp_long(FP_MUL, t, 0x10000); }
    else        { fp_long(FP_MUL, t, 0x10000); fp_int(FP_DIV, t, i); }
    return trunc_comp_low(t);
}

/* 128 words k * step (16.16, rounded to the nearest integer), the running value held at `limit`
 * once it reaches it (unsigned). Returns the final running value. */
static uint32_t ramp_row(uint32_t p, uint32_t step, uint32_t limit) {
    uint32_t acc = 0;
    for (int k = 0; k < 0x80; k++) {
        w16(p + 2u * (uint32_t)k, (uint16_t)((acc >> 16) + ((acc >> 15) & 1)));
        acc += step;
        if (acc >= limit) acc = limit;
    }
    return acc;
}

/* fn_S8_5962: table G_1EF8 (102 rows of $100 bytes): words 0..126 of row i are the ramp of
 * k * 87.1 * scale / distance, each row held at the previous row's last value (row 0 at
 * $7FFD - G_2152); word 127 is 512 * scale / distance (an alert, G_3ABE, if that would not fit). */
void fn_S8_5962(void) {
    MZ_FRAME;
    scratch_init();
    mac_HLock(g32(0x1EF8));
    uint32_t start = deref(g32(0x1EF8)), p = start;
    uint32_t x = ext_new(0x4005, 0xAE333333, 0x33333333);           /* 87.1 */
    mac_FP68K(FP_SINGLE | FP_MUL, x, GADDR(0x214C));
    uint32_t limit = (uint32_t)(int32_t)(int16_t)(0x7FFD - g16(0x2152)) << 16;
    ramp_row(p, (uint32_t)row_step(x, 0, 8), limit);
    p += 0x100;
    for (int16_t i = 1; i < 0x66; i++) {
        limit = ramp_row(p, (uint32_t)row_step(x, i, 0), limit);
        p += 0x100;
    }
    p = start + 0xFE;
    uint32_t y = ext_new(0x4008, 0x80000000, 0);                    /* 512 */
    mac_FP68K(FP_SINGLE | FP_MUL, y, GADDR(0x214C));
    uint32_t t = mz_local(10), c = mz_local(8);
    ext_copy(t, y); fp_int(FP_MUL, t, 8);
    w32(c, 0); w32(c + 4, 0x7FFD);
    if (!FP_LESS(mac_FP68K(FP_COMP | FP_CMP, t, c))) mz_seg(2), fn_S2_03A0(GADDR(0x3ABE), 0, 0);
    ext_copy(t, y); fp_int(FP_MUL, t, 8);
    w16(p, (uint16_t)trunc_to(FP_INT, t));
    p += 0x100;
    for (int16_t i = 1; i < 0x66; i++) {
        ext_copy(t, y); fp_int(FP_DIV, t, i);
        w16(p, (uint16_t)trunc_to(FP_INT, t));
        p += 0x100;
    }
    mac_HUnlock(g32(0x1EF8));
}

/* One row of G_1EF4: 256 entries of { word integer part, word quarter (0..3) } for k * step in
 * 16.16, k = 0..127 forwards and k = -1..-128 from the end backwards. */
static void quarter_row(uint32_t p, uint32_t step) {
    uint32_t acc = 0;
    for (int k = 0; k < 0x80; k++, acc += step) {
        w16(p + 4u * (uint32_t)k, (uint16_t)(acc >> 16));
        w16(p + 4u * (uint32_t)k + 2, (uint16_t)((acc >> 14) & 3));
    }
    acc = 0;
    for (int k = 0; k < 0x80; k++) {
        acc -= step;
        uint32_t e = p + 0x400 - 4u * (uint32_t)(k + 1);
        w16(e, (uint16_t)(acc >> 16));
        w16(e + 2, (uint16_t)((acc >> 14) & 3));
    }
}

/* fn_S8_5C80: table G_1EF4 (102 rows of $400 bytes) for k * 8 * scale / distance. */
void fn_S8_5C80(void) {
    MZ_FRAME;
    scratch_init();
    mac_HLock(g32(0x1EF4));
    uint32_t p = deref(g32(0x1EF4));
    uint32_t x = ext_new(0x4002, 0x80000000, 0);                    /* 8 */
    mac_FP68K(FP_SINGLE | FP_MUL, x, GADDR(0x214C));
    quarter_row(p, (uint32_t)row_step(x, 0, 4));
    p += 0x400;
    for (int16_t i = 1; i < 0x66; i++) {
        quarter_row(p, (uint32_t)row_step(x, i, 0));
        p += 0x400;
    }
    mac_HUnlock(g32(0x1EF4));
}

/* fn_S8_5E7A: table G_1EF0 (102 rows of $100 bytes): the ramp of k * 4 * scale / distance, held at
 * G_214E. */
void fn_S8_5E7A(void) {
    MZ_FRAME;
    scratch_init();
    mac_HLock(g32(0x1EF0));
    uint32_t p = deref(g32(0x1EF0));
    uint32_t x = ext_new(0x4001, 0x80000000, 0);                    /* 4 */
    mac_FP68K(FP_SINGLE | FP_MUL, x, GADDR(0x214C));
    uint32_t limit = (uint32_t)(int32_t)(int16_t)g16(0x214E) << 16;
    ramp_row(p, (uint32_t)row_step(x, 0, 4), limit);
    p += 0x100;
    for (int16_t i = 1; i < 0x66; i++) {
        ramp_row(p, (uint32_t)row_step(x, i, 0), limit);
        p += 0x100;
    }
    mac_HUnlock(g32(0x1EF0));
}

/* fn_S8_5884: rebuilds the tables for scale G_214C, then G_210A = trunc(87.1 * scale) and
 * G_2108 = trunc(512 * scale) << 8 (both through a single). */
void fn_S8_5884(void) {
    MZ_FRAME;
    fn_S8_5962();
    fn_S8_5C80();
    fn_S8_5E7A();
    fn_S8_6036();
    scratch_init();
    uint32_t x = ext_new(0x4005, 0xAE333333, 0x33333333), s = mz_local(4), t = mz_local(10);
    mac_FP68K(FP_SINGLE | FP_MUL, x, GADDR(0x214C));
    mac_FP68K(FP_SINGLE | FP_STORE, s, x);
    mac_FP68K(FP_SINGLE | FP_LOAD, t, s);
    sg16(0x210A, (uint16_t)trunc_to(FP_INT, t));
    uint32_t y = mz_local(10);
    w16(s, 0x200); mac_FP68K(FP_INT | FP_LOAD, y, s);                 /* 512 */
    mac_FP68K(FP_SINGLE | FP_MUL, y, GADDR(0x214C));
    mac_FP68K(FP_SINGLE | FP_STORE, s, y);
    mac_FP68K(FP_SINGLE | FP_LOAD, t, s);
    sg32(0x2108, (uint32_t)trunc_to(FP_LONG, t) << 8);
}
