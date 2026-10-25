/* Mazeworld Original's creatures (the object behaviours its dispatcher fn_S7_0F78 calls by type, in
 * segments 4, 7, 9, 10 and 11), named by Original's offsets. As in Catacombs (game/seg7_objects.c)
 * each is a state machine over the record at A4 (64 bytes in G_53C8: +2 type, +$12 event, +$14 hit
 * by, +$16 state, +$18 timer, +$3A picture, ...): "think" code per state (the original's JMP (A3),
 * which also means "switch to state D0 and think again"), "act" code per state, and per-state
 * lists for the reactions below. Here the tables are switches. Their register results are not
 * compared (the dispatcher restores what its caller keeps). */
#include "mzasm.h"
#include "fns.h"
#include <stddef.h>

typedef void (*think_fn)(Regs *);

/* fn_S7_122E: a hit (+$14 = the hitter's record offset, also in G_3832; 0 otherwise): the hitter's
 * type looked up in `list` { type, new state } ... $FFFF switches to that state (think); the hit is
 * consumed either way */
static void o_hit(Regs *r, const uint16_t *list, think_fn think) {
    sog16(0x3832, 0);
    uint16_t by = r16(r->a[4] + 0x14);
    if (!by) return;
    sog16(0x3832, by);
    uint16_t type = r16(og32(0x53C8) + by + 2) & 0x7FFF;
    for (; list && list[0] != 0xFFFF; list += 2)
        if (list[0] == type) { set_w(&r->d[0], list[1]); think(r); break; }
    w16(r->a[4] + 0x14, 0);
}
/* fn_S7_11FC: an event (+$12): looked up in `list` { event, new state } ..., in increasing order
 * (ending $FFFF), switches to that state; the event is consumed either way */
__attribute__((unused)) static void o_event(Regs *r, const uint16_t *list, think_fn think) {
    uint16_t ev = r16(r->a[4] + 0x12);
    if (!ev) return;
    for (; list; list += 2) {
        if (ev == list[0]) { set_w(&r->d[0], list[1]); think(r); break; }
        if (ev < list[0]) break;
    }
    w16(r->a[4] + 0x12, 0);
}
/* fn_S7_11D4: the player's shot (G_56B8, of type G_56C0) against the state's { type, new state }:
 * a match switches state */
__attribute__((unused)) static void o_shot(Regs *r, const uint16_t *e, think_fn think) {
    if (!og16(0x56B8) || !(e[0] & 0x7FFF)) return;
    if ((e[0] & 0x7FFF) != (og16(0x56C0) & 0x7FFF)) return;
    set_w(&r->d[0], e[1]); think(r);
}
static void set_pic(Regs *r, uint16_t p) { w16(r->a[4] + 0x3A, p); }
static void call_d0d1(Regs *r, void (*f)(Regs *), uint16_t d0, uint16_t d1) { set_w(&r->d[0], d0); set_w(&r->d[1], d1); f(r); }

/* ---------- type 0: the lever (fn_S7_0528) ----------
 * States: 0 up, 1 down, 2-3 moving (4 ticks, then up). Hits by types $17-$1A switch it. */
static void t0_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        switch ((uint16_t)r->d[7]) {
        case 0: set_pic(r, 0); return;
        case 1: set_pic(r, 0x1A); return;
        case 2: w16(r->a[4] + 0x18, 4); set_w(&r->d[0], 3); continue;
        case 3: {
            uint16_t t = (uint16_t)(r16(r->a[4] + 0x18) - 1);
            w16(r->a[4] + 0x18, t);
            if (!(t & 0x8000)) return;
            set_w(&r->d[0], 0); continue;
        }
        default: return;
        }
    }
}
void fo_S7_0528(Regs *r) {
    static const uint16_t l0[] = {0x18, 2, 0x19, 1, 0xFFFF}, l1[] = {0x17, 0, 0x18, 2, 0x1A, 0, 0xFFFF},
                          l2[] = {0xFFFF}, l3[] = {0x17, 0, 0x18, 2, 0x19, 1, 0x1A, 0, 0xFFFF};
    static const uint16_t *const lists[4] = {l0, l1, l2, l3};
    set_w(&r->d[7], r16(r->a[4] + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t0_think(r);
    o_hit(r, (uint16_t)r->d[7] < 4 ? lists[(uint16_t)r->d[7]] : NULL, t0_think);
    if ((uint16_t)r->d[7] == 3) set_pic(r, 0x1B);
    w16(r->a[4] + 0x16, (uint16_t)r->d[7]);
}

/* ---------- type 1 (fn_S7_0606) ----------
 * A hit by a type $18 object: in states 0-12 sets bit $20 of word 0 of the level's flags (the
 * first time also three map words, $101 at $80, $C0 and $100) and becomes state -1 (picture $1C);
 * from state 13 on it goes back 13 states. */
void fo_S7_0606(Regs *r) {
    set_w(&r->d[2], r16(r->a[4] + 0x14));
    if ((uint16_t)r->d[2]) {
        r->a[0] = og32(0x53C8);
        set_w(&r->d[0], r16(r->a[0] + sxw(r->d[2]) + 2));
        uint16_t s = r16(r->a[4] + 0x16);
        if ((uint16_t)r->d[0] == 0x18 && !(s & 0x8000)) {
            if ((int16_t)s >= 0xD) w16(r->a[4] + 0x16, (uint16_t)(s - 0xD));
            else {
                call_d0d1(r, fo_S7_1282, 0, 0x20);
                if ((uint16_t)r->d[0]) {
                    call_d0d1(r, fo_S7_1278, 0x101, 0x80);
                    call_d0d1(r, fo_S7_1278, 0x101, 0xC0);
                    call_d0d1(r, fo_S7_1278, 0x101, 0x100);
                }
                call_d0d1(r, fo_S7_1292, 0, 0x20);
                r->d[0] = 0xFFFFFFFFu;
                w16(r->a[4] + 0x16, 0xFFFF);
            }
        }
        w16(r->a[4] + 0x14, 0);
    }
    set_pic(r, (r16(r->a[4] + 0x16) & 0x8000) ? 0x1C : 0);
}

/* ---------- type 2: the exit (fn_S7_06A8) ----------
 * Open once the four records after it are all in state 1 (else picture $E); it cycles through
 * pictures 8-$D (+$16 = 2, 4, .. 12). With the player in its cell at x $56-$5A, y $54-$57 it sets
 * four map words to 1 ($80, $C0, $100, $140) and counts +$18 = 16 frames of the exit picture
 * (G_56DE, full view G_56E0 = $81), healing (life G_56EA + 3, G_56E6 + 6, to $8E); at the end
 * the level changes (G_57DC = 4, G_57D8 / G_57D4 the start, G_57DA the heading, G_57DE, G_57F2). */
static uint16_t t2_next(Regs *r) {
    static const uint16_t pic[7] = {0, 8, 9, 0xA, 0xB, 0xC, 0xD};
    uint16_t s = (uint16_t)(r16(r->a[4] + 0x16) + 2);
    if (s >= 0xD) s = 2;
    w16(r->a[4] + 0x16, s);
    return pic[s / 2];
}
static uint16_t heal(uint16_t v, uint16_t by) { v = (uint16_t)(v + by); return v < 0x8E ? v : 0x8E; }
void fo_S7_06A8(Regs *r) {
    if (!r16(r->a[4] + 0x18)) {
        for (uint32_t k = 1; k <= 4; k++)
            if (r16(r->a[4] + 0x40 * k + 0x16) != 1) { set_pic(r, 0xE); return; }
        set_pic(r, t2_next(r));
        if (r16(r->a[4] + 0x30) != og16(0x56D0)) return;
        uint16_t x = og16(0x56D8), y = og16(0x56D4);
        if (x < 0x56 || x >= 0x5B || y < 0x54 || y >= 0x58) return;
        for (uint16_t at = 0x80; at <= 0x140; at += 0x40) call_d0d1(r, fo_S7_1278, 1, at);
        w16(r->a[4] + 0x18, 0x10);
    }
    sog16(0x56DE, t2_next(r));
    sog16(0x56E0, 0x81);
    uint16_t t = (uint16_t)(r16(r->a[4] + 0x18) - 1);
    w16(r->a[4] + 0x18, t);
    if (!t) {
        sog16(0x57DC, 4); sog32(0x57D8, 0x780000); sog32(0x57D4, 0x5E0000);
        sog16(0x57DA, og16(0x56DA)); sog8(0x57DE, 1); sog8(0x57F2, 1);
    }
    sog16(0x56EA, heal(og16(0x56EA), 3));
    sog16(0x56E6, heal(og16(0x56E6), 6));
}

/* fo_S10_2624: the spin-and-move (fn_S7_0A9C), only every other frame (G_56C4 odd) while the
 * object is above y $8F; else D0.l = -1 */
void fo_S10_2624(Regs *r) {
    set_w(&r->d[1], r16(r->a[4] + 8));
    if ((uint16_t)r->d[1] < 0x8F) {
        set_w(&r->d[1], og16(0x56C4) & 1);
        if (!(uint16_t)r->d[1]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    }
    mz_seg_of(7, 0x0A9C), fn_S7_0A9C(r);
}
