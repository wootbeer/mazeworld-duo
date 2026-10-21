/* SANE (FP68K / Pack4) on 80-bit extended values, using SoftFloat's floatx80.
 * Operands use their 68k memory layout (big-endian; extended = 2-byte sign/exponent + 8-byte significand),
 * so the harness can pass emulated memory directly and the port can pass its own big-endian buffers. */
#include "mactb.h"
/* SoftFloat expects these base types from its host (Musashi defines them in m68kcpu.h). */
typedef signed char sint8; typedef signed short sint16; typedef signed int sint32; typedef signed long long sint64;
typedef unsigned char uint8; typedef unsigned short uint16; typedef unsigned int uint32; typedef unsigned long long uint64;
#include "softfloat/milieu.h"
#include "softfloat/softfloat.h"
#include <string.h>

static uint16_t sane_env;   /* rounding direction bits 13-14, precision bits 5-6 (only the defaults are exercised) */

static uint64_t be64(const uint8_t *p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v = v << 8 | p[i]; return v; }
static void put64(uint8_t *p, uint64_t v) { for (int i = 7; i >= 0; i--) { p[i] = (uint8_t)v; v >>= 8; } }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static void put32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static floatx80 ldx(const uint8_t *p) { floatx80 x; x.high = (bits16)(p[0] << 8 | p[1]); x.low = be64(p + 2); return x; }
static void stx(uint8_t *p, floatx80 x) { p[0] = x.high >> 8; p[1] = (uint8_t)x.high; put64(p + 2, x.low); }

int mtb_sane_operand_size(uint16_t op) {
    switch (op & 0x3800) { case 0x0000: return 10; case 0x0800: return 8; case 0x1000: return 4; case 0x2000: return 2; case 0x2800: return 4; case 0x3000: return 8; }
    return 0;
}

static floatx80 load_fmt(uint16_t op, const uint8_t *p) {
    switch (op & 0x3800) {
    case 0x0000: return ldx(p);
    case 0x0800: return float64_to_floatx80(be64(p));
    case 0x1000: return float32_to_floatx80(be32(p));
    case 0x2000: return int32_to_floatx80((int16_t)(p[0] << 8 | p[1]));
    case 0x2800: return int32_to_floatx80((int32_t)be32(p));
    default: {  /* comp: 64-bit two's complement; 0x8000000000000000 is the comp NaN */
        int64_t v = (int64_t)be64(p);
        if ((uint64_t)v == 0x8000000000000000ull) { floatx80 n = {0x7FFF, 0xC000000000000000ull}; return n; }
        return int64_to_floatx80(v); }
    }
}

static void store_fmt(uint16_t op, uint8_t *p, floatx80 x) {
    switch (op & 0x3800) {
    case 0x0000: stx(p, x); break;
    case 0x0800: put64(p, floatx80_to_float64(x)); break;
    case 0x1000: put32(p, floatx80_to_float32(x)); break;
    case 0x2000: {
        int32_t v = floatx80_to_int32(x);
        if (v < -32768 || v > 32767 || (x.high & 0x7FFF) == 0x7FFF) v = -32768;   /* invalid -> most negative */
        p[0] = (uint8_t)((uint16_t)v >> 8); p[1] = (uint8_t)v; break; }
    case 0x2800: put32(p, (uint32_t)floatx80_to_int32(x)); break;
    default: {
        int64_t v = floatx80_to_int64(x);
        if ((x.high & 0x7FFF) == 0x7FFF) v = (int64_t)0x8000000000000000ull;
        put64(p, (uint64_t)v); break; }
    }
}

static void set_rounding(void) {
    static const int8 map[4] = {float_round_nearest_even, float_round_up, float_round_down, float_round_to_zero};
    float_rounding_mode = map[(sane_env >> 13) & 3];
}

/* Number of parameter bytes FP68K pops for this opword (opword included). */
int mtb_sane_pop_bytes(uint16_t op) {
    switch (op & 0xFF) {
    case 0x12: case 0x14: case 0x16: case 0x1A: case 0x0D: case 0x0F:    /* unary on dst */
    case 0x01: case 0x03: case 0x05: case 0x07: case 0x15: case 0x17: case 0x19: case 0x1B:
        return 6;
    }
    return 10;
}

/* Executes one FP68K operation. dst/src point at operands (src unused for unary operations).
 * *ccr receives the 68k condition codes for comparisons. Returns 0, or -1 if unsupported. */
int mtb_fp68k(uint16_t op, uint8_t *dst, const uint8_t *src, uint8_t *ccr) {
    set_rounding();
    floatx80 a, b, r;
    switch (op & 0xFF) {
    case 0x00: case 0x02: case 0x04: case 0x06: case 0x0C:
        a = ldx(dst); b = load_fmt(op, src);
        r = (op & 0xFF) == 0x00 ? floatx80_add(a, b) : (op & 0xFF) == 0x02 ? floatx80_sub(a, b)
          : (op & 0xFF) == 0x04 ? floatx80_mul(a, b) : (op & 0xFF) == 0x06 ? floatx80_div(a, b) : floatx80_rem(a, b);
        stx(dst, r); return 0;
    case 0x08: case 0x0A: {   /* FOCMP / FOCPX: dst compared with src */
        a = ldx(dst); b = load_fmt(op, src);
        int un = ((a.high & 0x7FFF) == 0x7FFF && (a.low << 1) != 0) || ((b.high & 0x7FFF) == 0x7FFF && (b.low << 1) != 0);
        if (un) *ccr = 0x02;
        else if (floatx80_eq(a, b)) *ccr = 0x04;
        else if (floatx80_lt(a, b)) *ccr = 0x19;
        else *ccr = 0x00;
        return 0; }
    case 0x0E: stx(dst, load_fmt(op, src)); return 0;                     /* FOZ2X */
    case 0x10: store_fmt(op, dst, ldx(src)); return 0;                    /* FOX2Z */
    case 0x12: stx(dst, floatx80_sqrt(ldx(dst))); return 0;               /* FOSQRT */
    case 0x14: stx(dst, floatx80_round_to_int(ldx(dst))); return 0;       /* FORTI */
    case 0x16: { int8 m = float_rounding_mode; float_rounding_mode = float_round_to_zero;
                 stx(dst, floatx80_round_to_int(ldx(dst))); float_rounding_mode = m; return 0; }   /* FOTTI */
    case 0x0D: a = ldx(dst); a.high ^= 0x8000; stx(dst, a); return 0;    /* FONEG */
    case 0x0F: a = ldx(dst); a.high &= 0x7FFF; stx(dst, a); return 0;    /* FOABS */
    case 0x11: { a = ldx(dst); b = ldx(src); b.high = (bits16)((b.high & 0x7FFF) | (a.high & 0x8000)); stx((uint8_t *)src, b); return 0; } /* FOCPYSGN */
    case 0x01: sane_env = (uint16_t)(dst[0] << 8 | dst[1]); return 0;     /* FOSETENV */
    case 0x03: dst[0] = sane_env >> 8; dst[1] = (uint8_t)sane_env; return 0;   /* FOGETENV */
    case 0x17: { uint16_t old = sane_env; sane_env = 0; dst[0] = old >> 8; dst[1] = (uint8_t)old; return 0; }  /* FOPROCENTRY */
    case 0x19: sane_env = (uint16_t)(dst[0] << 8 | dst[1]); return 0;     /* FOPROCEXIT (exceptions not tracked) */
    }
    return -1;
}
