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
static void o_event(Regs *r, const uint16_t *list, think_fn think) {
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

/* ---------- type 0 (fn_S7_0528) ----------
 * States: 0 (picture 0), 1 (picture $1A), 2-3 a 4-tick spell (picture $1B), then 0. Hits by types
 * $17-$1A switch between them. */
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

/* ---------- type 2, the level's exit (fn_S7_06A8) ----------
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

static void random_d0(Regs *r) { mz_seg_of(2, 0x3532), fn_S2_3532(r); }
static void unlink_obj(Regs *r) {                           /* fn_S2_29E2: out of its cell's list */
    set_w(&r->d[0], og16(0x3834)); set_w(&r->d[1], r16(r->a[4] + 0x30));
    mz_seg_of(2, 0x3410), fn_S2_3410(r);
}

/* ---------- three of a kind (fn_S10_1974, fn_S10_1B2E, fn_S10_1D96) ----------
 * States: 0 waiting (+$18 counts; negative and from -$46 up: once bit 3 of the level's flag
 * word 0 is set, +$18 = $7FFF and on to 1; below that, 3), 1 cycling +$C 0..$2D, then 2, 2 resting
 * (back to 1 one time in 8), 3 removed (out of its cell, type | $8000), 4 gone, 5-6 hit by types
 * $17 / $13 (+5 / +10 to +$18 up to a limit; a flag word set), 7 hit by type $18 (-5, flag clear).
 * In states 1 and 2 they act differently: 1974 pushes the objects in its cell (fn_S10_1F4E) and,
 * without its flag, the player (fn_S10_2096) when the record before it is not negative; 1B2E
 * passes on event 4 from the record after it; 1D96 sends event 4 itself. */
typedef struct { uint16_t flag, limit, act0; uint8_t mark, kind; } Kind3;
static const Kind3 kind3[3] = {{0x10, 0x64, 0, 1, 0}, {0x50, 0x64, 0, 1, 1}, {0x10, 0x1E, 2, 0, 2}};
static const Kind3 *k3;
static void k3_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint32_t o = r->a[4];
        int16_t t = (int16_t)r16(o + 0x18);
        switch ((uint16_t)r->d[7]) {
        case 0:
            if (t >= 0) { set_w(&r->d[0], 1); continue; }
            if ((uint16_t)t < 0xFFBA) { set_w(&r->d[0], 3); continue; }
            call_d0d1(r, fo_S7_1282, 0, 8);
            if (!(uint16_t)r->d[0]) return;
            w16(o + 0x18, 0x7FFF); set_w(&r->d[0], 1); continue;
        case 1: {
            if (t < 0) { set_w(&r->d[0], 0); continue; }
            uint16_t c = (uint16_t)(r16(o + 0xC) + 1);
            if (c >= 0x2E) { w16(o + 0xC, 0); set_w(&r->d[0], 2); continue; }
            w16(o + 0xC, c); return;
        }
        case 2:
            if (t < 0) { set_w(&r->d[0], 0); continue; }
            random_d0(r);
            if ((uint16_t)r->d[0] & 7) return;
            set_w(&r->d[0], 1); continue;
        case 3:
            unlink_obj(r);
            w16(o + 2, r16(o + 2) | 0x8000);
            set_w(&r->d[0], 4); continue;
        case 5: case 6: {
            uint16_t by = (uint16_t)r->d[7] == 5 ? 5 : 10;
            if (by == 5) w16(o + k3->flag, 1);
            if (t < 0) {
                if (by == 5 && k3->mark) call_d0d1(r, fo_S7_1292, 0, 0x80);
                w16(o + 0x18, by);
            } else if ((uint16_t)t < k3->limit) w16(o + 0x18, (uint16_t)(t + by));
            set_w(&r->d[0], 1); continue;
        }
        case 7:
            if ((uint16_t)t != 0x7FFF) w16(o + 0x18, (uint16_t)(t - 5));
            w16(o + k3->flag, 0);
            set_w(&r->d[0], 1); continue;
        default: return;
        }
    }
}
/* fn_S10_1FCE: object number D0.b in the cell, if of type 9 or $12, is pushed one step along the
 * axis of +$1A towards its cell's edge (low four bits of x / y towards 0 below 8, else to $F); a
 * type 9 also moves its marked target cell that way when that cell is free. Then event 4. */
static void k3_push(Regs *r, uint8_t n, uint16_t cell) {
    uint32_t a1 = og32(0x53C8) + ((uint32_t)n << 6), map = og32(0x53D4);
    uint16_t type = r16(a1 + 2);
    if (type != 9 && type != 0x12) return;
    uint32_t f = r16(r->a[4] + 0x1A) ? 8 : 4;
    int16_t step = f == 4 ? 0x20 : 0x400;
    uint16_t v = r16(a1 + f) & 0xF, to;
    if (v < 8) { if (!v) return; w16(a1 + f, (uint16_t)(r16(a1 + f) - 1)); to = (uint16_t)(cell - step); }
    else { if (v == 0xF) return; w16(a1 + f, (uint16_t)(r16(a1 + f) + 1)); to = (uint16_t)(cell + step); }
    if (type == 9 && !r16(map + sxw(to) + 0xE)) {
        w16(map + sxw(r16(a1 + 0xE)) + 0xE, 0);
        w16(a1 + 0xE, to);
        w16(map + sxw(to) + 0xE, 1);
        w16(a1 + 0x10, 0);
    }
    w16(r->a[4] + 0x12, 4);
}
/* fn_S10_20C8 / 20F6: the player (G_56D8 / G_56D4) the same way, with event 4 and fn_S2_2AD2 */
static void k3_push_player(Regs *r, uint16_t g) {
    uint16_t v = og16(g) & 0xF;
    if (v < 8) { if (!v) return; sog16(g, (uint16_t)(og16(g) - 1)); }
    else { if (v == 0xF) return; sog16(g, (uint16_t)(og16(g) + 1)); }
    w16(r->a[4] + 0x12, 4);
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
}
static void k3_act(Regs *r) {
    uint32_t o = r->a[4];
    uint16_t s = (uint16_t)r->d[7];
    if (s == 0) { w16(o + 0x12, k3->act0); w16(o + 0xC, 0); return; }
    if (s != 1 && s != 2) return;
    if (k3->kind == 2) { if (s == 1) w16(o + 0x12, 4); return; }
    w16(o + 0x12, 2);
    uint32_t t = og32(0x53C8);
    if (k3->kind == 1) {
        if (r16(t + sxw((uint16_t)(og16(0x3834) + 0x40)) + 0x12) == 4) w16(o + 0x12, 4);
        return;
    }
    if (r16(t + sxw((uint16_t)(og16(0x3834) - 0x40)) + 0x18) & 0x8000) return;
    uint16_t cell = r16(o + 0x30);                          /* fn_S10_1F4E: the objects listed in the cell */
    for (uint32_t k = 1; k <= 10; k++) {
        uint8_t n = r8(og32(0x53D4) + sxw(cell) + k);
        if (!n) break;
        k3_push(r, n, cell);
    }
    if (r16(o + 0x10)) return;
    if (og16(0x56D0) != cell) return;                       /* fn_S10_2096 */
    for (int k = 0; k < 3; k++) k3_push_player(r, r16(o + 0x1A) == 2 ? 0x56D4 : 0x56D8);
}
static void kind3_tick(Regs *r, int v) {
    static const uint16_t hits[] = {0x13, 6, 0x17, 5, 0x18, 7, 0xFFFF};
    k3 = &kind3[v];
    set_w(&r->d[7], r16(r->a[4] + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    k3_think(r);
    o_hit(r, (uint16_t)r->d[7] <= 2 ? hits : NULL, k3_think);
    k3_act(r);
    w16(r->a[4] + 0x16, (uint16_t)r->d[7]);
}
void fo_S10_1974(Regs *r) { kind3_tick(r, 0); }
void fo_S10_1B2E(Regs *r) { kind3_tick(r, 1); }
void fo_S10_1D96(Regs *r) { kind3_tick(r, 2); }

/* ---------- fn_S10_1CE2 ----------
 * States: 0 cycling +$C 0..$2D (then 1), 1 resting (back to 0 one time in 8), 2 hit by types $18,
 * $19 or $1B: sets bit 7 of the level's flag word 0 and turns into type $1B (its cell recomputed,
 * fn_S2_2A52; the state is left at 2). */
static void t1ce2_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        switch ((uint16_t)r->d[7]) {
        case 0: {
            w16(r->a[4] + 0x12, 0);
            uint16_t c = (uint16_t)(r16(r->a[4] + 0xC) + 1);
            if (c >= 0x2E) { w16(r->a[4] + 0xC, 0); set_w(&r->d[0], 1); continue; }
            w16(r->a[4] + 0xC, c); return;
        }
        case 1:
            random_d0(r);
            if ((uint16_t)r->d[0] & 7) return;
            set_w(&r->d[0], 0); continue;
        default: return;
        }
    }
}
void fo_S10_1CE2(Regs *r) {
    static const uint16_t hits[] = {0x18, 2, 0x19, 2, 0x1B, 2, 0x801B, 2, 0xFFFF};
    set_w(&r->d[7], r16(r->a[4] + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t1ce2_think(r);
    o_hit(r, (uint16_t)r->d[7] <= 1 ? hits : NULL, t1ce2_think);
    if ((uint16_t)r->d[7] == 2) {
        call_d0d1(r, fo_S7_1292, 0, 0x80);
        w16(r->a[4] + 2, 0x1B);
        set_w(&r->d[0], og16(0x3834));
        mz_seg_of(2, 0x3480), fn_S2_3480(r);
        w16(r->a[4] + 0x16, 0);
    }
    w16(r->a[4] + 0x16, (uint16_t)r->d[7]);
}

/* fn_S10_2B92: the level's twelve counters (words of G_53C8 + $1E: low byte counts to $10, then a
 * new random high nibble and low byte 0 or $FF), the map's refresh (fn_S7_3128), and while the
 * player is near (G_382E < 4) life G_56EA and G_56E6 + 10 each, to $8E */
void fo_S10_2B92(Regs *r) {
    uint32_t a2 = og32(0x53C8) + 0x1E;
    for (uint16_t d3 = 0; d3 < 0x18; d3 += 2) {
        uint16_t w = r16(a2 + d3), n = (uint16_t)((w & 0xFF) + 1);
        if (n != 0x11) { w16(a2 + d3, (uint16_t)((w & 0xFF00) | n)); continue; }
        random_d0(r);
        uint16_t hi = (uint16_t)(((uint16_t)r->d[0] & 0xF) << 8);
        random_d0(r);
        w16(a2 + d3, (uint16_t)(hi | ((uint16_t)-((uint16_t)r->d[0] & 1) & 0xFF)));
    }
    mz_seg_of(7, 0x3128), fn_S7_3128(r);
    if (og16(0x382E) < 4) { sog16(0x56EA, heal(og16(0x56EA), 0xA)); sog16(0x56E6, heal(og16(0x56E6), 0xA)); }
}

/* ---------- finding the way, cell by cell ----------
 * The object at A4 heads for the cell +$E of the map G_53D4 (cells of $20 bytes, rows of $400:
 * +$E a "taken" marker, +$10 the distance to the player, +$1C / +$1E the centre). Until it is at
 * the centre, D0.w is the direction to it ($00 west, $20 east, $10 north, $30 south); there the
 * neighbours are weighed and the best free one becomes the target (+$10 = the old one), or the
 * current one stays taken. D0.w = the chosen direction, -1 if none.
 *  ROUTE_RANDOM (fn_S10_2718 / 27D8): a random weight, more for cells other than the last one;
 *  ROUTE_FIXED (fn_S10_29AA / 2A7C): fixed preferences, the same bonus;
 *  ROUTE_CLOSER (fn_S10_2828 / 296E): only cells closer to the player; on the way, if the cell it
 *  is leaving has become closer than its target, it turns back (D0 = -1). */
enum { ROUTE_RANDOM, ROUTE_FIXED, ROUTE_CLOSER };
static uint16_t o_route(Regs *r, int mode) {
    uint32_t o = r->a[4], map = og32(0x53D4);
#define CELL(c, f) (map + sxw(c) + (f))
    uint16_t at = r16(o + 0xE);
    int16_t dx = (int16_t)(r16(CELL(at, 0x1C)) - r16(o + 4)), dy = (int16_t)(r16(CELL(at, 0x1E)) - r16(o + 8));
    if (dx || dy) {
        uint16_t dir = dx ? (dx < 0 ? 0 : 0x20) : (dy < 0 ? 0x10 : 0x30);
        if (mode != ROUTE_CLOSER) return dir;
        static const int16_t back[4] = {0x20, 0x400, -0x20, -0x400};   /* the cell behind, by direction 0, $10, $20, $30 */
        uint16_t prev = (uint16_t)(at + back[dir >> 4]);
        if (r16(CELL(at, 0x10)) < r16(CELL(prev, 0x10))) return dir;
        if (!r16(CELL(prev, 0xE))) {
            w16(CELL(at, 0xE), 0);
            w16(o + 0x10, at); w16(o + 0xE, prev);
            w16(CELL(prev, 0xE), 1);
        }
        return 0xFFFF;
    }
    w16(CELL(at, 0xE), 0);
    static const int16_t step[3][4] = {{0x400, -0x800, 0x3E0, 0x40}, {-0x400, 0x3E0, 0x40, 0x3E0}, {-0x400, 0x3E0, 0x40, 0x3E0}};
    static const uint16_t dirs[3][4] = {{0x30, 0x10, 0, 0x20}, {0x10, 0, 0x20, 0x30}, {0x10, 0, 0x20, 0x30}};
    static const uint16_t pref[3][4] = {{0, 0, 0, 0}, {3, 1, 2, 0}, {4, 5, 6, 7}};
    uint16_t best = 0, bcell = 0xFFFF, bdir = 0xFFFF, c = at;
    for (int k = 0; k < 4; k++) {
        c = (uint16_t)(c + step[mode][k]);
        if (r16(CELL(c, 0xE))) continue;
        uint16_t w;
        if (mode == ROUTE_CLOSER) {
            if (r16(CELL(c, 0x10)) >= r16(CELL(at, 0x10))) continue;
            w = pref[mode][k];
        } else {
            if (mode == ROUTE_RANDOM) {
                if (c >= 0x2000) continue;
                random_d0(r);
                w = (uint16_t)r->d[0] & 3;
            } else w = pref[mode][k];
            w |= c == r16(o + 0x10) ? 4 : 8;
        }
        if (w < best) continue;
        best = w; bcell = c; bdir = dirs[mode][k];
    }
    if (!(bcell & 0x8000)) { w16(o + 0x10, r16(o + 0xE)); w16(o + 0xE, bcell); w16(CELL(bcell, 0xE), 1); }
    else w16(CELL(r16(o + 0xE), 0xE), 1);
#undef CELL
    return bdir;
}

/* ---------- fn_S10_21DC ----------
 * A hunter on the map's paths: 0 resting (until its cell's distance is $FFFD or more: 12), 1
 * roaming at random (when it sees the player within $19: 5; on a cell off the map: 6), 2-3 hit by
 * type $18: $C0 ticks of running away along the distance map (fn_S2_2B3E; faster below y $88), 4
 * hit by type $1B: 7, dying ($2B, 5 ticks, then removed), 5 chasing the player (its cell
 * G_381C / G_3818, fn_S7_192A) and pulling (fn_S10_258C) when close (fn_S7_1B1E), 6 back to the
 * map's paths, 8-9 event $302: waiting on its target cell's distance (with the records 2-4 gone,
 * back to 1), 10-11 event $303: 5 ticks of the dying picture $1E, removed, 12-13 following the
 * scent (fn_S7_1B46) and pulling. */
static void hunter_think(Regs *r) {
    uint32_t o = r->a[4], map = og32(0x53D4);
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t dist = r16(map + sxw(r16(o + 0xE)) + 0x10);
        switch ((uint16_t)r->d[7]) {
        case 0: if (dist >= 0xFFFD) { set_w(&r->d[0], 0xC); continue; } return;
        case 1:
            if (r16(o + 0xE) >= 0x2000) { set_w(&r->d[0], 6); continue; }
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (og16(0x382E) < 0x19) { set_w(&r->d[0], 5); continue; }
            return;
        case 2:
            w16(o + 0x18, 0xC0);
            sog16(0x56A8, 0x14); sog16(0x56A6, og16(0x56D0));
            mz_seg_of(2, 0x366A), fn_S2_366A(r); fn_S2_3568(r);    /* fn_S2_2B3E */
            set_w(&r->d[0], 3); continue;
        case 3: {
            uint16_t t = r16(o + 0x18);
            if (r16(o + 8) >= 0x88) t = (uint16_t)(t - 2);
            t = (uint16_t)(t - 1);
            w16(o + 0x18, t);
            if (t & 0x8000) { w16(o + 0x10, 0); set_w(&r->d[0], 1); continue; }
            uint16_t d = o_route(r, ROUTE_CLOSER);
            set_w(&r->d[0], d);
            if (d & 0x8000) {
                uint16_t t2 = (uint16_t)(r16(o + 0x18) - 2);
                w16(o + 0x18, t2);
                if (t2 & 0x8000) { w16(o + 0x10, 0); set_w(&r->d[0], 1); continue; }
            }
            w16(r->a[6] - 2, d);                            /* for the act (the handler's local) */
            return;
        }
        case 4: w16(o + 0x18, 0); set_w(&r->d[0], 7); continue;
        case 5:
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (og16(0x382E) >= 0x19) { set_w(&r->d[0], 1); continue; }
            return;
        case 6:
            if (r16(o + 0xE) < 0x2000) { set_w(&r->d[0], 1); continue; }
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (og16(0x382E) < 0x19) { set_w(&r->d[0], 5); continue; }
            return;
        case 8: {
            if (dist) { set_w(&r->d[0], 9); continue; }
            uint32_t t = og32(0x53C8);
            if ((r16(t + 0x82) & 0x8000) && (r16(t + 0xC2) & 0x8000) && (r16(t + 0x102) & 0x8000)) { set_w(&r->d[0], 1); continue; }
            return;
        }
        case 9: if (!dist) { set_w(&r->d[0], 8); continue; } return;
        case 10: w16(o + 0x18, 5); set_w(&r->d[0], 0xB); continue;
        case 12: if (!dist) { set_w(&r->d[0], 0); continue; } return;
        default: return;
        }
    }
}
static void spin_move(Regs *r, uint16_t d0, int every_other) {   /* fn_S10_2624 / 263A, then fn_S7_1578 if it moved */
    set_w(&r->d[0], d0);
    if (every_other) fo_S10_2624(r); else { mz_seg_of(7, 0x0A9C); fn_S7_0A9C(r); }
    if (!((uint16_t)r->d[0] & 0x8000) && (every_other < 2 || !(uint16_t)r->d[0])) { mz_seg_of(7, 0x3784); fn_S7_3784(r); }
}
static int near_player(Regs *r) { mz_seg_of(7, 0x3D2A), fn_S7_3D2A(r); return (uint16_t)r->d[0] != 0; }
static void pull(Regs *r) { mz_seg_of(7, 0x09EC), fn_S7_09EC(r); }
static void remove_obj(Regs *r) {
    w16(og32(0x53D4) + sxw(r16(r->a[4] + 0xE)) + 0xE, 0);
    unlink_obj(r);
    w16(r->a[4] + 2, r16(r->a[4] + 2) | 0x8000);
}
static void scent_move(Regs *r) {                           /* fn_S7_1B46, fn_S7_1B02, then the move */
    mz_seg_of(7, 0x3D5C), fn_S7_3D5C(r);
    uint16_t d = (uint16_t)r->d[0];
    fn_S7_3D0E(r);
    spin_move(r, d, 0);
}
void fo_S10_21DC(Regs *r) {
    static const uint16_t h[] = {0x18, 2, 0x1B, 4, 0xFFFF}, h1b[] = {0x1B, 4, 0xFFFF};
    static const uint16_t e[] = {0x302, 8, 0x303, 10, 0xFFFF}, e3[] = {0x303, 10, 0xFFFF};
    static const uint16_t *const hits[14] = {NULL, h, NULL, h, NULL, h, h, NULL, h1b, h1b};
    static const uint16_t *const evs[14] = {e3, e, NULL, e, NULL, e, e, NULL, e3, e3, NULL, NULL, e3, e3};
    uint32_t o = r->a[4], a6 = r->a[6];
    r->a[6] = r->a[7] - 4;                                  /* LINK A6, #-2: the local word at -2 */
    r->a[7] -= 6;
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    hunter_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_hit(r, s < 14 ? hits[s] : NULL, hunter_think);
    s = (uint16_t)r->d[7];
    o_event(r, s < 14 ? evs[s] : NULL, hunter_think);
    switch ((uint16_t)r->d[7]) {
    case 0: spin_move(r, 0xFFFF, 1); if (near_player(r)) pull(r); break;
    case 1: spin_move(r, o_route(r, ROUTE_RANDOM), 2); break;
    case 3: spin_move(r, r16(r->a[6] - 2), 1); break;
    case 5:
        sog16(0x381C, og16(0x56D8)); sog16(0x3818, og16(0x56D4));
        mz_seg_of(7, 0x3B36), fn_S7_3B36(r);
        fn_S7_3D0E(r);
        spin_move(r, og16(0x3812), 1);
        if (near_player(r) && ((uint16_t)r->d[0] & 0x8000)) pull(r);
        break;
    case 6: spin_move(r, o_route(r, ROUTE_FIXED), 1); break;
    case 7: {
        w16(o + 0x3A, 0x2B);
        set_w(&r->d[0], 0xFFFF); fo_S10_2624(r);
        uint16_t t = (uint16_t)(r16(o + 0x18) + 1);
        w16(o + 0x18, t);
        if (t >= 5) remove_obj(r);
        break;
    }
    case 8: spin_move(r, o_route(r, ROUTE_RANDOM), 0); break;
    case 9: scent_move(r); {          /* fn_S10_267C: the objects of types 3-5 in its cell get event $901 */
        uint32_t map = og32(0x53D4), t = og32(0x53C8);
        for (uint32_t k = 1; k <= 10; k++) {
            uint8_t n = r8(map + sxw(r16(o + 0x30)) + k);
            if (!n) break;
            uint16_t ty = r16(t + ((uint32_t)n << 6) + 2);
            if (ty == 3 || ty == 4 || ty == 5) w16(t + ((uint32_t)n << 6) + 0x12, 0x901);
        }
        break; }
    case 11: {
        w16(o + 0x3A, 0x1E);
        set_w(&r->d[0], 0xFFFF); mz_seg_of(7, 0x0A9C); fn_S7_0A9C(r);
        uint16_t t = (uint16_t)(r16(o + 0x18) - 1);
        w16(o + 0x18, t);
        if (t & 0x8000) remove_obj(r);
        break;
    }
    case 12: scent_move(r); if (near_player(r) && ((uint16_t)r->d[0] & 0x8000)) pull(r); break;
    case 13: spin_move(r, 0xFFFF, 1); if (near_player(r) && ((uint16_t)r->d[0] & 0x8000)) pull(r); break;
    }
    w16(o + 0x16, (uint16_t)r->d[7]);
    r->a[7] += 6; r->a[6] = a6;
}
