/* Operations for C translated mechanically from 68000 code (tools/asm2c.py): each instruction's
 * result and condition codes, at byte / word / long size, exactly as the processor sets them. */
#ifndef ASM68_H
#define ASM68_H
#include "mzasm.h"

static inline uint32_t m_mask(int sz) { return sz == 1 ? 0xFFu : sz == 2 ? 0xFFFFu : 0xFFFFFFFFu; }
static inline uint32_t m_msb(int sz)  { return sz == 1 ? 0x80u : sz == 2 ? 0x8000u : 0x80000000u; }
static inline uint32_t m_sx(int sz, uint32_t v) { return sz == 1 ? (uint32_t)(int32_t)(int8_t)v : sz == 2 ? (uint32_t)(int32_t)(int16_t)v : v; }
static inline uint32_t m_rd(int sz, uint32_t a) { return sz == 1 ? r8(a) : sz == 2 ? r16(a) : r32(a); }
static inline void m_wr(int sz, uint32_t a, uint32_t v) { if (sz == 1) w8(a, (uint8_t)v); else if (sz == 2) w16(a, (uint16_t)v); else w32(a, v); }
static inline void m_set(int sz, uint32_t *reg, uint32_t v) { *reg = (*reg & ~m_mask(sz)) | (v & m_mask(sz)); }

/* move / and / or / eor / tst / clr: N Z from the result, V C cleared, X kept */
static inline uint32_t f_logic(Regs *r, int sz, uint32_t v) {
    v &= m_mask(sz);
    r->ccr = (uint8_t)((r->ccr & CCR_X) | (v ? 0 : CCR_Z) | (v & m_msb(sz) ? CCR_N : 0));
    return v;
}
/* cmp: flags of dst - src, X kept */
static inline void f_cmp(Regs *r, int sz, uint32_t src, uint32_t dst) {
    uint32_t m = m_mask(sz), s = src & m, d = dst & m, res = (d - s) & m;
    int v = (((s ^ d) & (res ^ d)) & m_msb(sz)) != 0;
    r->ccr = (uint8_t)((r->ccr & CCR_X) | (res ? 0 : CCR_Z) | (res & m_msb(sz) ? CCR_N : 0) | (v ? CCR_V : 0) | (s > d ? CCR_C : 0));
}
/* add / sub: all flags, X = C */
static inline uint32_t f_add(Regs *r, int sz, uint32_t src, uint32_t dst) {
    uint32_t m = m_mask(sz), s = src & m, d = dst & m, res = (d + s) & m;
    int v = ((~(s ^ d) & (res ^ d)) & m_msb(sz)) != 0, c = res < d;
    r->ccr = (uint8_t)((res ? 0 : CCR_Z) | (res & m_msb(sz) ? CCR_N : 0) | (v ? CCR_V : 0) | (c ? CCR_C | CCR_X : 0));
    return res;
}
static inline uint32_t f_sub(Regs *r, int sz, uint32_t src, uint32_t dst) {
    uint32_t m = m_mask(sz), s = src & m, d = dst & m, res = (d - s) & m;
    int v = (((s ^ d) & (res ^ d)) & m_msb(sz)) != 0, c = s > d;
    r->ccr = (uint8_t)((res ? 0 : CCR_Z) | (res & m_msb(sz) ? CCR_N : 0) | (v ? CCR_V : 0) | (c ? CCR_C | CCR_X : 0));
    return res;
}
/* shifts by n (1..63; 0 leaves the value, clears C and V) */
static inline uint32_t f_shift(Regs *r, int sz, uint32_t v, int n, int left, int arith) {
    uint32_t m = m_mask(sz), msb = m_msb(sz); v &= m;
    n &= 63;
    if (!n) { r->ccr = (uint8_t)((r->ccr & CCR_X) | (v ? 0 : CCR_Z) | (v & msb ? CCR_N : 0)); return v; }
    int out = 0, ov = 0;
    for (int k = 0; k < n; k++) {
        if (left) { out = (v & msb) != 0; uint32_t nv = (v << 1) & m; if (arith && ((nv ^ v) & msb)) ov = 1; v = nv; }
        else { out = v & 1; v = arith ? ((v >> 1) | (v & msb)) : (v >> 1); }
    }
    r->ccr = (uint8_t)((v ? 0 : CCR_Z) | (v & msb ? CCR_N : 0) | (ov ? CCR_V : 0) | (out ? CCR_C | CCR_X : 0));
    return v;
}
/* the condition codes' tests */
#define C_ (r->ccr & CCR_C)
#define V_ (r->ccr & CCR_V)
#define Z_ (r->ccr & CCR_Z)
#define N_ (r->ccr & CCR_N)
#define CC_HI (!C_ && !Z_)
#define CC_LS (C_ || Z_)
#define CC_CC (!C_)
#define CC_CS (C_)
#define CC_NE (!Z_)
#define CC_EQ (Z_)
#define CC_VC (!V_)
#define CC_VS (V_)
#define CC_PL (!N_)
#define CC_MI (N_)
#define CC_GE (!N_ == !V_)
#define CC_LT (!N_ != !V_)
#define CC_GT (!Z_ && !N_ == !V_)
#define CC_LE (Z_ || !N_ != !V_)

static inline void m_push(Regs *r, uint32_t v) { r->a[7] -= 4; w32(r->a[7], v); }
static inline uint32_t m_pop(Regs *r) { uint32_t v = r32(r->a[7]); r->a[7] += 4; return v; }
#endif
