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
/* fn_S7_11D4: a speech that has ended (G_56B8 set once speech G_56C0 has been shown its time,
 * fn_S10_3B08) against the state's { speech, new state }: a match switches state */
static void o_said(Regs *r, const uint16_t *e, think_fn think) {
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
 * A hit by a type $18 object: in states 0-12 sets bit $20 of the game's flag word 0 (the first
 * time also sending event $101 to records 2, 3 and 4) and becomes state -1 (picture $1C);
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
 * pictures 8-$D (+$16 = 2, 4, .. 12). With the player in its cell at x $56-$5A, y $54-$57 it sends
 * event 1 to records 2-5 and counts +$18 = 16 frames of the exit picture
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

/* ---------- the Eashire Cat (fn_S10_400A, type $1D, record 5 on level 1) ----------
 * It goes round a ring of 36 points (fn_S10_43B8), and when the player is near
 * (within 8, G_382E) and facing it (fn_S7_1472 of its heading against the player's, < $10)
 * counts +$1A = $1E down to attacking (5-6: it jumps onto the player, life - $28) or, near
 * record 1 (George), it is drawn in (8: four steps of the ring, at distance < 3 of record 1 event
 * $1D02 to George and 9-11: a straight run of +$1A steps, a wait, then turning towards the player);
 * 3-4 dying (pictures 7, 4, $C, then removed and event $1D01 to George); 7 walking the path
 * of three points (fn_S10_4324) until near the player. Event 1 kills it in states 0 and 7-11. */
static void g_target(uint16_t x, uint16_t y) { sog16(0x381C, x); sog16(0x3818, y); }
static void g_record1(void) { uint32_t t = og32(0x53C8); g_target(r16(t + 0x44), r16(t + 0x48)); }
static void g_step(Regs *r) {                               /* fn_S7_14E8, fn_S7_1540, fn_S7_1578 */
    mz_seg_of(7, 0x36F4), fn_S7_36F4(r);
    fn_S7_374C(r);
    fn_S7_3784(r);
}
static void g_pic_random(Regs *r) {                          /* fn_S10_43A0 */
    w16(r->a[4] + 0x18, 5);
    random_d0(r);
    if (!((uint16_t)r->d[0] & 0x1F)) w16(r->a[4] + 0x18, 7);
}
static void g_ring_at(Regs *r, uint32_t f) {                 /* fn_S10_43B8 (f = $1A), fn_S10_3E22 ($1E) */
    static const uint16_t ring[72] = {
        0x58, 0x26, 0x4F, 0x27, 0x47, 0x29, 0x3F, 0x2D, 0x38, 0x32, 0x32, 0x38, 0x2D, 0x3F, 0x29, 0x47, 0x27, 0x4F,
        0x26, 0x58, 0x27, 0x61, 0x29, 0x69, 0x2D, 0x71, 0x32, 0x78, 0x38, 0x7E, 0x3F, 0x83, 0x47, 0x87, 0x4F, 0x89,
        0x58, 0x8A, 0x61, 0x89, 0x69, 0x87, 0x71, 0x83, 0x78, 0x7E, 0x7E, 0x78, 0x83, 0x71, 0x87, 0x69, 0x89, 0x61,
        0x8A, 0x58, 0x89, 0x4F, 0x87, 0x47, 0x83, 0x3F, 0x7E, 0x38, 0x78, 0x32, 0x71, 0x2D, 0x69, 0x29, 0x61, 0x27};
    for (;;) {
        set_w(&r->d[0], r16(r->a[4] + f));
        r->a[0] = 0;                                        /* the original's A0 points into its table */
        g_target(ring[(uint16_t)r->d[0] / 2], ring[(uint16_t)r->d[0] / 2 + 1]);
        fo_S7_1488(r);
        if (!((uint16_t)r->d[0] & 0x8000)) { r->a[0] = OADDR(0x4F30); g_step(r); return; }
        uint16_t n = (uint16_t)(r16(r->a[4] + f) + 4);
        w16(r->a[4] + f, n >= 0x90 ? 0 : n);
    }
}
static void g_ring(Regs *r) { g_ring_at(r, 0x1A); }
static int g_path(Regs *r) {                                 /* fn_S10_4324: 0, or -1 at the path's end */
    static const uint16_t path[8] = {0x88, 0x62, 0x68, 0x62, 0x48, 0x62, 0xFFFF, 0xFFFF};
    uint16_t i = r16(r->a[4] + 0x1A);
    g_target(path[i / 2], path[i / 2 + 1]);
    fo_S7_1488(r);
    if ((uint16_t)r->d[0] & 0x8000) {
        i = (uint16_t)(i + 4);
        w16(r->a[4] + 0x1A, i);
        sog16(0x381C, path[i / 2]);
        if (path[i / 2] & 0x8000) { r->d[0] = r->d[1] = 0xFFFFFFFFu; return -1; }
        sog16(0x3818, path[i / 2 + 1]);
        fo_S7_1488(r);
    }
    uint16_t d = (uint16_t)r->d[0];
    mz_seg_of(7, 0x36F4), fn_S7_36F4(r);
    set_w(&r->d[0], d);
    r->a[0] = OADDR(0x4F30);
    fn_S7_374C(r); fn_S7_3784(r);
    r->d[0] = r->d[1] = 0;
    return 0;
}
static void near_d0(Regs *r) { mz_seg_of(7, 0x3128), fn_S7_3128(r); }    /* fn_S7_12BE: G_382E / G_3830 */
static void guard_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        switch ((uint16_t)r->d[7]) {
        case 0:
            near_d0(r);
            if (og16(0x382E) < 8) { set_w(&r->d[0], 1); continue; }
            g_record1(); fo_S7_14C0(r);
            if ((uint16_t)r->d[0] < 6) { set_w(&r->d[0], 8); continue; }
            if ((uint16_t)r->d[0] >= 9) return;
            if (r16(og32(0x53C8) + 0x56) == 2) { set_w(&r->d[0], 8); continue; }
            return;
        case 1: w16(o + 0x1A, 0x1E); set_w(&r->d[0], 2); continue;
        case 2: {
            near_d0(r);
            if (og16(0x382E) >= 0xC) { set_w(&r->d[0], 5); continue; }
            set_w(&r->d[0], (uint16_t)((og16(0x3830) ^ 0x20) - og16(0x56DA)));
            mz_seg_of(7, 0x367E), fn_S7_367E(r);
            if ((uint16_t)r->d[0] >= 0x10) { set_w(&r->d[0], 5); continue; }
            uint16_t t = (uint16_t)(r16(o + 0x1A) - 1);
            w16(o + 0x1A, t);
            if (t & 0x8000) { set_w(&r->d[0], 3); continue; }
            return;
        }
        case 3: w16(o + 0x1A, 0); set_w(&r->d[0], 4); continue;
        case 5: w16(o + 0x1A, 6); set_w(&r->d[0], 6); continue;
        case 7:
            if (g_path(r) < 0 || g_path(r) < 0) { w16(o + 0x1A, 0x28); set_w(&r->d[0], 0); continue; }
            near_d0(r);
            if (og16(0x382E) < 6) { set_w(&r->d[0], 1); continue; }
            return;
        case 8:
            for (int k = 0; k < 4; k++) g_ring(r);
            g_record1(); fo_S7_14C0(r);
            if ((uint16_t)r->d[0] >= 3) return;
            call_d0d1(r, fo_S7_1278, 0x1D02, 0x40);
            sog16(0x56C0, 0);
            w16(o + 0x1A, 6);
            set_w(&r->d[0], 9); continue;
        case 9: {
            int16_t n = (int16_t)(r16(o + 0x1A) - 1);
            set_w(&r->d[3], (uint16_t)n);
            if (n < 0) { w16(o + 0x1A, 7); set_w(&r->d[0], 0xA); continue; }
            w16(o + 0x1A, (uint16_t)n);
            do {
                r->a[0] = OADDR(0x4F30);
                set_w(&r->d[0], r16(o + 0xC));
                g_step(r);
                set_w(&r->d[3], (uint16_t)((uint16_t)r->d[3] - 1));
            } while (!((uint16_t)r->d[3] & 0x8000));
            return;
        }
        case 10: {
            uint16_t t = (uint16_t)(r16(o + 0x1A) - 1);
            w16(o + 0x1A, t);
            if (!(t & 0x8000)) return;
            w16(o + 0x1A, 0x28); set_w(&r->d[0], 0xB); continue;
        }
        case 11: {
            uint16_t t = (uint16_t)(r16(o + 0x1A) - 1);
            w16(o + 0x1A, t);
            if (t & 0x8000) { set_w(&r->d[0], 3); continue; }
            near_d0(r);
            set_w(&r->d[0], og16(0x3830));
            if ((uint16_t)r->d[0] & 0x8000) { set_w(&r->d[0], 3); continue; }
            mz_seg_of(7, 0x36F4), fn_S7_36F4(r);
            return;
        }
        default: return;
        }
    }
}
void fo_S10_400A(Regs *r) {
    static const uint16_t ev[] = {1, 3, 0xFFFF};
    static const uint8_t has_ev[12] = {1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    guard_think(r);
    o_event(r, (uint16_t)r->d[7] < 12 && has_ev[(uint16_t)r->d[7]] ? ev : NULL, guard_think);
    switch ((uint16_t)r->d[7]) {
    case 0:
        g_pic_random(r); g_ring(r); g_ring(r);
        g_record1(); fo_S7_14C0(r);
        if (!(og16(0x56C4) & ((uint16_t)r->d[0] >= 0xF ? 0x1F : 0x7F))) g_ring(r);
        break;
    case 2: g_pic_random(r); set_w(&r->d[0], og16(0x3830)); mz_seg_of(7, 0x36F4), fn_S7_36F4(r); break;
    case 4: {
        uint16_t t = (uint16_t)(r16(o + 0x1A) + 1);
        w16(o + 0x1A, t);
        if (t < 4) w16(o + 0x18, 7);
        else if (t < 8) w16(o + 0x18, 4);
        else if (t < 0xC) w16(o + 0x18, 0xC);
        else { unlink_obj(r); w16(o + 2, r16(o + 2) | 0x8000); call_d0d1(r, fo_S7_1278, 0x1D01, 0x40); }
        break;
    }
    case 6: {
        w16(o + 0x26, 1);
        int16_t t = (int16_t)(r16(o + 0x1A) - 1);
        w16(o + 0x1A, (uint16_t)(t < 0 ? 0 : t));
        w16(o + 0x18, 0x10);
        sog32(0x382C, og32(0x56D8)); sog32(0x3828, og32(0x56D4)); sog16(0x3824, og16(0x56D0));
        mz_seg_of(7, 0x3784), fn_S7_3784(r);
        int16_t l = (int16_t)(og16(0x56EA) - 0x28);
        sog16(0x56EA, (uint16_t)(l < 0 ? 0 : l));
        break;
    }
    case 7: case 8: g_pic_random(r); break;
    }
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- George (fn_S10_2C7C, type $14, record 1) ----------
 * The invisible translator ("My name is George..."): he speaks the texts $14xx of segment 6 and
 * appears beside the player when he does. A text is asked for by setting G_56C0 = $8000 | text
 * every frame (say()); fn_S10_3B08 shows a new one (fo_S6_0000: its record gives a wait G_56BC, a
 * time on screen G_56BA, a record +$1C and how he appears, fn_S10_3BA8) and sets G_56B8 when its
 * time is up, which o_said() turns into his next state. Record 5 is the Eashire Cat (fo_S10_400A), which chases him.
 * Level 1, states 0-15: 0 gone; 1 circling the forest's ring (+$1E) until the player is near,
 * facing him, within $28: 2; 2 "Run for your life! It's the Eashire Cat!!" ("Heeeeeeelp!" while
 * within $C of the cat), then 1; past x $120: 15, which sends the cat event 1; the cat's end
 * (event $1D01) makes him 3-6: gone for $14 frames, "Whew! Thank you! ..." and "My name is George
 * ..." (he joins: G_56C2), then 7; event $1D02 sends him away (0). 7 is his remarks (george_remark),
 * 8 waits out a full-view picture (G_56E0 >= $80, which in most states sends him to 0 or 8); 9-14
 * (event $1D02 at the start): $1C frames, "Welcome to the Mazeworld...", a walk along two points
 * (george_path), then 1.
 * States 16-51 (his remarks on later levels and the snails' greeting, event $304) are not reached
 * by the tests yet: their think and act run the translation in orig_gen.c (gen_S10_*). */
void gen_S10_2E6C(Regs *r), gen_S10_35E6(Regs *r), gen_S10_35FA(Regs *r), gen_S10_3614(Regs *r),
    gen_S10_3628(Regs *r), gen_S10_3670(Regs *r), gen_S10_3684(Regs *r), gen_S10_36A8(Regs *r),
    gen_S10_36BA(Regs *r), gen_S10_36D8(Regs *r), gen_S10_3704(Regs *r), gen_S10_3718(Regs *r),
    gen_S10_3722(Regs *r), gen_S10_3748(Regs *r), gen_S10_379E(Regs *r), gen_S10_37B0(Regs *r),
    gen_S10_37F6(Regs *r), gen_S10_3808(Regs *r), gen_S10_381A(Regs *r), gen_S10_3860(Regs *r),
    gen_S10_3884(Regs *r), gen_S10_38B6(Regs *r), gen_S10_38E0(Regs *r), gen_S10_391A(Regs *r),
    gen_S10_394A(Regs *r), gen_S10_39A2(Regs *r), gen_S10_39CC(Regs *r), gen_S10_3A00(Regs *r),
    gen_S10_3A0E(Regs *r), gen_S10_3A1C(Regs *r), gen_S10_3A60(Regs *r), gen_S10_3A62(Regs *r),
    gen_S10_3A84(Regs *r), gen_S10_3AA6(Regs *r), gen_S10_3AC8(Regs *r), gen_S10_3AEA(Regs *r),
    gen_S10_3AF2(Regs *r);
static void (*const george_gen_act[36])(Regs *) = {
    gen_S10_35E6, gen_S10_35FA, gen_S10_3614, gen_S10_3628, gen_S10_3670, gen_S10_3684, gen_S10_36A8, gen_S10_36BA,
    gen_S10_36D8, gen_S10_3704, gen_S10_3718, gen_S10_3722, gen_S10_3748, gen_S10_379E, gen_S10_37B0, gen_S10_37F6,
    gen_S10_3808, gen_S10_381A, gen_S10_3860, gen_S10_3884, gen_S10_38B6, gen_S10_38E0, gen_S10_391A, gen_S10_394A,
    gen_S10_39A2, gen_S10_39CC, gen_S10_3A00, gen_S10_3A0E, gen_S10_3A1C, gen_S10_3A60, gen_S10_3A62, gen_S10_3A84,
    gen_S10_3AA6, gen_S10_3AC8, gen_S10_3AEA, gen_S10_3AF2};
static void say(uint16_t text) { sog16(0x56C0, (uint16_t)(0x8000 | text)); }
static int flag(Regs *r, uint16_t word, uint16_t bits) { call_d0d1(r, fo_S7_1282, word, bits); return (uint16_t)r->d[0] != 0; }
static void flag_set(Regs *r, uint16_t word, uint16_t bits) { call_d0d1(r, fo_S7_1292, word, bits); }
static int cell_in(uint16_t cell, const uint16_t *list) { for (; *list; list++) if (cell == *list) return 1; return 0; }
/* fn_S10_3B7C: off the map (out of his cell; x, y, cell and the showing phase +$1A cleared) */
static void george_hide(Regs *r) {
    if (!r16(r->a[4] + 0x30)) return;
    unlink_obj(r);
    w16(r->a[4] + 4, 0); w16(r->a[4] + 8, 0); w16(r->a[4] + 0x30, 0); w16(r->a[4] + 0x1A, 0);
}
static void george_hide_shown(Regs *r) { if (r16(r->a[4] + 0x1A)) george_hide(r); }   /* fn_S10_3B9C */
/* placed where the player stands (fn_S7_1578 from G_382C / G_3828 / G_3824) */
static void george_at_player(Regs *r) {
    sog32(0x382C, og32(0x56D8)); sog32(0x3828, og32(0x56D4)); sog16(0x3824, og16(0x56D0));
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
}
/* fn_S10_3BA8: how he appears for a new text: 0 he goes; 1 beside the player, facing him (+$1A 1,
 * $32 frames); 2-4 at the record +$1C, facing the player, for $1F4 / $14 / 5 frames (+$1A 3) */
static void george_appear(Regs *r, uint16_t how) {
    static const uint16_t frames[5] = {0, 0, 0x1F4, 0x14, 5};
    uint32_t o = r->a[4];
    if (how == 0 || how > 4) { george_hide_shown(r); return; }    /* (texts use 0-4) */
    if (how == 1) {
        if (!r16(o + 0x1A)) { george_at_player(r); w16(o + 0xC, og16(0x56DA) ^ 0x20); }
        w16(o + 0x1A, 1); w16(o + 0x22, 0x32);
        return;
    }
    w16(o + 0x22, frames[how]);
    if (!r16(o + 0x1A)) {
        uint32_t at = og32(0x53C8) + sxw(r16(o + 0x1C));
        sog32(0x382C, r32(at + 4)); sog32(0x3828, r32(at + 8)); sog16(0x3824, r16(at + 0x30));
        mz_seg_of(7, 0x3784), fn_S7_3784(r);
        g_target(og16(0x56D8), og16(0x56D4)); fo_S7_1488(r);
        w16(o + 0xC, (uint16_t)r->d[0]);
    }
    w16(o + 0x1A, 3);
}
/* fn_S10_3B08: the text G_56C0 (a new request, $8000 set, else none: 0); a new one is shown
 * (G_56BE the last, G_56E4 set) and he appears for it; then G_56BC frames of waiting, G_56BA on
 * screen, and G_56B8 = -1: said. */
static void george_speech(Regs *r) {
    sog16(0x56B8, 0);
    uint16_t t = og16(0x56C0) & 0x8000 ? og16(0x56C0) & 0x7FFF : 0;
    sog16(0x56C0, t);
    if (t != og16(0x56BE)) {
        sog16(0x56BE, t);
        set_w(&r->d[0], t); mz_seg(6), fo_S6_0000(r);
        sog8(0x56E4, 1);
        sog16(0x56BC, r16(r->a[0] + 2)); sog16(0x56BA, r16(r->a[0] + 4));
        w16(r->a[4] + 0x1C, r16(r->a[0] + 8));
        george_appear(r, r16(r->a[0] + 6));
    } else if (og16(0x56BC)) sog16(0x56BC, (uint16_t)(og16(0x56BC) - 1));
    else if (!og16(0x56C0)) return;
    else if (og16(0x56BA)) sog16(0x56BA, (uint16_t)(og16(0x56BA) - 1));
    else sog16(0x56B8, 0xFFFF);
}
/* fn_S10_3C60: his showing, by +$1A: 1 fading in (+$20 = $8000) to 2, beside the player facing
 * him for +$22 frames, then 6 (fading, one more frame) and 7 gone; 3 at a record turning to the
 * player for +$22 frames, then 4 (fading) and 5 gone. +$26 = 5 while shown. */
static void george_turn(Regs *r) { mz_seg_of(7, 0x36F4), fn_S7_36F4(r); w16(r->a[4] + 0x26, 5); }
static int george_count(Regs *r) { int16_t t = (int16_t)(r16(r->a[4] + 0x22) - 1); w16(r->a[4] + 0x22, (uint16_t)t); return t < 0; }
static void george_showing(Regs *r) {
    uint32_t o = r->a[4];
    switch (r16(o + 0x1A)) {
    case 1: w16(o + 0x20, 0x8000); w16(o + 0x1A, 2); w16(o + 0x26, 5); break;
    case 2:
        w16(o + 0x20, 0); george_at_player(r);
        set_w(&r->d[0], og16(0x56DA) ^ 0x20); george_turn(r);
        if (george_count(r)) w16(o + 0x1A, 6);
        break;
    case 3:
        w16(o + 0x20, 0); g_target(og16(0x56D8), og16(0x56D4)); fo_S7_1488(r); george_turn(r);
        if (george_count(r)) w16(o + 0x1A, 4);
        break;
    case 4: w16(o + 0x20, 0x8000); w16(o + 0x26, 5); w16(o + 0x1A, 5); break;
    case 5: case 7: george_hide_shown(r); break;
    case 6:
        w16(o + 0x20, 0x8000); george_at_player(r);
        set_w(&r->d[0], og16(0x56DA) ^ 0x20); george_turn(r);
        george_count(r); w16(o + 0x1A, 7);
        break;
    }
}
/* fn_S10_3DC0: a step along two points from +$1E; -1 at the end */
static int george_path(Regs *r) {
    static const uint16_t path[6] = {0x40, 0x4E, 0x26, 0x58, 0xFFFF, 0xFFFF};
    uint32_t o = r->a[4];
    uint16_t i = r16(o + 0x1E) / 2;
    g_target(path[i], path[i + 1]); fo_S7_1488(r);
    if ((uint16_t)r->d[0] & 0x8000) {
        i = (uint16_t)(r16(o + 0x1E) + 4); w16(o + 0x1E, i); i /= 2;
        sog16(0x381C, path[i]);
        if (path[i] & 0x8000) { r->d[0] = 0xFFFFFFFFu; return -1; }
        sog16(0x3818, path[i + 1]); fo_S7_1488(r);
    }
    mz_seg_of(7, 0x36F4), fn_S7_36F4(r);
    r->a[0] = OADDR(0x4F30); fn_S7_374C(r); fn_S7_3784(r);
    r->d[0] = 0;
    return 0;
}
/* fn_S10_3058's choice (state 7): once he has joined (G_56C2) and is not speaking, a remark for
 * where the player stands (level G_56DC, cell G_56D0) and what has happened (the game's flag words
 * 0 and 1, fo_S7_1282 / 1292); level 5 also by the area fn_S7_2038 gives for his position. The
 * state for it, or -1. */
void fo_S7_2038(Regs *r);
static int george_remark(Regs *r) {
    static const uint16_t gates[] = {0x20A0, 0x580, 0x2240, 0}, c1f[] = {0x3720, 0x3740, 0x3760, 0x3780, 0},
        c20[] = {0x2B40, 0x2BC0, 0}, c21[] = {0x49E0, 0x4A00, 0}, c22[] = {0x5760, 0x5B60, 0}, c26[] = {0x6F40, 0x6780, 0},
        c28[] = {0x6360, 0x6340, 0x6320, 0}, c30[] = {0x5A40, 0x5A60, 0}, c32[] = {0x2BA0, 0x2FA0, 0};
    uint16_t level = og16(0x56DC), cell = og16(0x56D0);
    int joined = og16(0x56C2) != 0;
    if (joined && !og16(0x56C0)) {
        if (level == 1 && (cell == 0x4240 || cell == 0x4280) && !flag(r, 0, 1) && !flag(r, 0, 0x8000) && !flag(r, 0, 0x100)) return 0x13;
        if (level == 2 && flag(r, 0, 8) && !flag(r, 0, 0x200)) return 0x18;
    }
    if (joined && level == 4 && !flag(r, 0, 0x10)) {
        if (!flag(r, 1, 0x10) && cell_in(cell, gates) && !flag(r, 0, 0x400)) {
            if (flag(r, 0, 2)) flag_set(r, 1, 0x200);
            return 0x1C;
        }
        /* (the original also tests the flags once with D0 = $400, a slip without effect) */
        if (flag(r, 0, 0x400) && !flag(r, 1, 0x10) && flag(r, 0, 2) && cell_in(cell, gates) && !flag(r, 1, 0x200)) return 0x33;
        if (flag(r, 1, 0x10)) {
            if (cell == 0x58E0 && !flag(r, 0, 0x800) && !flag(r, 1, 2)) return 0x1D;
            if (flag(r, 0, 0x800) && !flag(r, 0, 0x1000)) {
                if (cell == 0x50E0) return 0x1E;
                if (cell == 0x60E0) flag_set(r, 0, 0x1000);
            }
            if (!flag(r, 0, 0x2000) && !flag(r, 1, 2) && cell_in(cell, c1f)) return 0x1F;
            if (flag(r, 0, 0x2000) && !flag(r, 1, 2) && !flag(r, 0, 0x4000) && cell_in(cell, c20)) return 0x20;
            if (!flag(r, 1, 1) && !flag(r, 1, 2) && cell_in(cell, c21)) { flag_set(r, 0, 0x4000); return 0x21; }
            if (!flag(r, 1, 2) && cell_in(cell, c22)) return 0x22;
            if (!flag(r, 1, 2) && cell_in(cell, c26)) return 0x26;
            if (!flag(r, 1, 4) && cell_in(cell, c28)) return 0x28;
            if (cell == 0x6940 && !flag(r, 1, 8)) return 0x2C;
        }
    }
    if (joined && level == 5 && !flag(r, 1, 0x20)) {
#define AREA() (set_w(&r->d[1], og16(0x56D8)), set_w(&r->d[2], og16(0x56D4)), mz_seg(7), fo_S7_2038(r), (uint16_t)r->d[0])
        if (AREA() == 0xA) {
            uint32_t c = og32(0x53C8) + 0x1A;
            if (!(r16(c) & 0x8000)) {
                uint16_t n = (uint16_t)(r16(c) + 1);
                w16(c, n);
                if (n >= 0xC8) { w16(c, 0xFFFF); return 0x2F; }
            }
        }
        if (AREA() == 0x11 && !flag(r, 1, 0x40) && cell_in(cell, c30)) { flag_set(r, 1, 0x40); return 0x30; }
        if (AREA() == 0xF && !flag(r, 1, 0x80) && cell == 0x5CA0) { flag_set(r, 1, 0x80); return 0x31; }
        if (AREA() == 0xE && !flag(r, 1, 0x100) && cell_in(cell, c32)) { flag_set(r, 1, 0x100); return 0x32; }
#undef AREA
    }
    return -1;
}
static void george_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7], next;
        int full = og16(0x56E0) >= 0x80;                    /* a full-view picture is showing */
        switch (s) {
        case 0: george_hide(r); return;
        case 1: case 2:
            if (full) { next = 0; break; }
            if (og16(0x56D8) >= 0x120) { next = 0xF; break; }
            if (s == 2) { near_d0(r); if (!(uint16_t)r->d[0]) return; next = 1; break; }
            if (og16(0x56D4) >= 0xD2 && og16(0x56D8) < 0x28) return;
            near_d0(r);
            if (og16(0x3830) & 0x8000) return;
            set_w(&r->d[0], (uint16_t)(og16(0x3830) - r16(o + 0xC)));
            mz_seg_of(7, 0x367E), fn_S7_367E(r);
            if ((uint16_t)r->d[0] >= 7 || og16(0x382E) >= 0x28) return;
            next = 2; break;
        case 3:
            unlink_obj(r);
            w16(o + 4, 0); w16(o + 8, 0); w16(o + 0x30, 0); w16(o + 0x1A, 0);
            w16(o + 0x1E, 0x14);
            next = 4; break;
        case 4: case 5: case 6:
            if (full) { sog16(0x56C2, 0xFFFF); next = 8; break; }
            if (s != 4) return;
            { int16_t t = (int16_t)(r16(o + 0x1E) - 1); w16(o + 0x1E, (uint16_t)t); if (t >= 0) return; }
            next = 5; break;
        case 7: {
            if (full) { next = 8; break; }
            int n = george_remark(r);
            if (n < 0) return;
            next = (uint16_t)n; break;
        }
        case 8: if (full) return; next = 7; break;
        case 9: w16(o + 0x1E, 0x1C); next = 0xA; break;
        case 10: case 11: {
            if (full) { next = 0; break; }
            int16_t t = (int16_t)(r16(o + 0x1E) - 1);
            w16(o + 0x1E, (uint16_t)t);
            if (t >= 0) return;
            w16(o + 0x1E, 0); next = s == 10 ? 0xC : 0xD; break;
        }
        case 12: case 13:
            if (full) { next = 0; break; }
            if (george_path(r) >= 0 && george_path(r) >= 0) return;
            next = 0xE; break;
        case 14: w16(o + 0x1E, 0x28); next = 1; break;
        case 15: call_d0d1(r, fo_S7_1278, 1, 0x140); next = 0; break;   /* event 1 to the cat */
        default:
            r->a[3] = mz_seg_base(10) + 0x2E6C;                  /* the original's think, for its JMP (A3) */
            gen_S10_2E6C(r);
            return;
        }
        set_w(&r->d[0], next);
    }
}
static void george_act(Regs *r) {
    uint16_t s = (uint16_t)r->d[7];
    switch (s) {
    case 1: g_ring_at(r, 0x1E); g_ring_at(r, 0x1E); break;
    case 2: {
        uint16_t t = og16(0x56C0) & 0x7FFF;
        if (t != 0x1401 && t != 0x141B) {
            uint32_t cat = og32(0x53C8) + 0x140;
            g_target(r16(cat + 4), r16(cat + 8)); fo_S7_14C0(r);
            t = (uint16_t)r->d[0] < 0xC ? 0x141B : 0x1401;
        }
        say(t);
        g_ring_at(r, 0x1E); g_ring_at(r, 0x1E);
        break;
    }
    case 5: say(0x1402); break;
    case 6: say(0x1404); sog16(0x56C2, 0xFFFF); break;
    case 8: george_hide_shown(r); break;
    case 10: case 11:                                       /* turning to the player */
        near_d0(r);
        if (!(og16(0x3830) & 0x8000)) { set_w(&r->d[0], og16(0x3830)); mz_seg_of(7, 0x3722), fn_S7_3722(r); fn_S7_3722(r); }
        if (s == 10) say(0x1403);
        break;
    case 12: say(0x1403); break;
    default: if (s >= 16 && s < 52) george_gen_act[s - 16](r); break;
    }
}
void fo_S10_2C7C(Regs *r) {
    static uint16_t said[52][2];
    static const uint16_t ev[] = {0x1D01, 3, 0x1D02, 0, 0xFFFF}, ev7[] = {0x304, 0x10, 0xFFFF};
    if (!said[2][0]) {                                      /* { text that ended, next state } by state */
        static const uint8_t st[] = {2, 5, 6, 10, 17, 18, 21, 23, 35, 39, 41, 42};
        static const uint16_t sv[][2] = {{0x9401, 1}, {0x9402, 6}, {0x9404, 7}, {0x9403, 11}, {0x9405, 7}, {0x9406, 7},
                                         {0x9407, 0x16}, {0x9408, 7}, {0x9411, 0x24}, {0x9412, 0x24}, {0x9413, 0x24}, {0x9414, 0x2B}};
        for (unsigned k = 0; k < sizeof st; k++) { said[st[k]][0] = sv[k][0]; said[st[k]][1] = sv[k][1]; }
        for (int k = 26; k <= 33; k++) { said[k][0] = (uint16_t)(0x9409 + k - 26); said[k][1] = 7; }
        for (int k = 46; k <= 51; k++) { said[k][0] = (uint16_t)(0x9415 + k - 46); said[k][1] = 7; }
    }
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    george_think(r);
    uint16_t s = (uint16_t)r->d[7];
    if (s < 52) o_said(r, said[s], george_think);
    s = (uint16_t)r->d[7];
    o_event(r, (s >= 1 && s <= 2) || (s >= 10 && s <= 13) ? ev : s == 7 ? ev7 : NULL, george_think);
    george_act(r);
    george_speech(r);
    george_showing(r);
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- fn_S7_007C: the gates between levels ----------
 * When the player stands in its cell close to it (G_382E < 5) it starts a count of $14 frames
 * ($A for states $A and up) showing the full-view picture sequence G_384C (G_3840 for type $10)
 * as G_56DE (G_56E0 = $80), healing in state 2; at the end the level changes (G_57DC the level,
 * G_57D8 / G_57D4 the arrival point, a step from it in the direction fn_S7_1488 gives towards the
 * player, G_57DA the heading, G_57DE, G_57F2). States 0-6 lead to fixed points of levels 2, 1, 1
 * (exactly $58, $50), 5, 4, 2, 2 (state 0 also sends event 1 to records 2-4); states $A and up to level 5,
 * at the centre of the cell G_5248 lists for the paired gate (state ^ 1). (States 7-9 jump into the
 * original's code after its table: no level has them.) */
static void gate_go(uint16_t level, uint32_t x, uint32_t y, uint16_t dir) {
    uint32_t steps = OADDR(0x4DF0) + sxw((uint16_t)(dir << 2));
    sog16(0x57DC, level);
    sog32(0x57D8, x + r32(steps + 0x40));
    sog32(0x57D4, y + r32(steps));
    sog16(0x57DA, og16(0x56DA)); sog8(0x57DE, 1); sog8(0x57F2, 1);
}
void fo_S7_007C(Regs *r) {
    static const struct { uint16_t level; uint32_t x, y; } to[7] = {
        {2, 0xF80000, 0x1E80000}, {1, 0x1680000, 0x1480000}, {1, 0, 0}, {5, 0x480000, 0x480000},
        {4, 0xF80000, 0xC80000}, {2, 0x1B80000, 0x480000}, {2, 0xF80000, 0x1580000}};
    uint32_t o = r->a[4];
    int16_t s = (int16_t)r16(o + 0x16);
    if (!r16(o + 6)) {
        if (r16(o + 0x30) != og16(0x56D0)) return;
        mz_seg_of(7, 0x3128), fn_S7_3128(r);
        if (og16(0x382E) >= 5) return;
        w16(o + 6, s >= 0xA ? 0xA : 0x14);
    }
    if (s == 2) { sog16(0x56EA, heal(og16(0x56EA), 3)); sog16(0x56E6, heal(og16(0x56E6), 6)); }
    uint32_t pics = OADDR(r16(o + 2) == 0x10 ? 0x3840 : 0x384C);
    int16_t c = (int16_t)(r16(o + 0xC) + 1);
    if (c >= 6) c = 0;
    w16(o + 0xC, (uint16_t)c);
    sog16(0x56DE, r16(pics + 2 * (uint32_t)c));
    sog16(0x56E0, 0x80);
    uint16_t t = (uint16_t)(r16(o + 6) - 1);
    w16(o + 6, t);
    if (t) return;
    g_target(og16(0x56D8), og16(0x56D4));
    fo_S7_1488(r);
    uint16_t dir = (uint16_t)r->d[0];
    if (s >= 0xA) {
        uint16_t cell = r16(OADDR(0x5248) + sxw((uint16_t)((((uint16_t)(s - 0xA)) ^ 1) << 1)));
        uint32_t map = og32(0x53D4) + sxw(cell);
        gate_go(5, (uint32_t)r16(map + 0x1C) << 16, (uint32_t)r16(map + 0x1E) << 16, dir);
    } else if (s == 2) {
        sog16(0x57DC, 1); sog32(0x57D8, 0x580000); sog32(0x57D4, 0x500000);
        sog16(0x57DA, og16(0x56DA)); sog8(0x57DE, 1); sog8(0x57F2, 1);
    } else if (s >= 0 && s < 7) {
        gate_go(to[s].level, to[s].x, to[s].y, dir);
        if (s == 0) for (uint16_t at = 0x80; at <= 0x100; at += 0x40) call_d0d1(r, fo_S7_1278, 1, at);
    }
}

/* ---------- type $30 (fn_S4_280A, level 5) ----------
 * Waits (0) until the player is near, then (1) turns to him and marks him for records 2 and 3
 * (record 2's +$18 = 1; record 3's +$18 / +$1C the area fn_S7_2038 gives for his position, +$1A /
 * +$1E his cell) until he is $20 away; then (2-5) picture $7E for 3 frames and gone from the map for
 * $10 frames; (6) back, once record 3's +$22 is set, in a free cell of the eight around it (G_4F40,
 * at random) that the player is not close to; (7-8) two frames of picture $81, then (9) waiting
 * for the player. Hit by type $18 in states 0, 1, 9 and 10: 2. State 10 (the player near again:
 * it follows him) is not reached by the tests yet: its act is the translation gen_S4_2A42. */
void gen_S4_2A42(Regs *r);
static void t30_think(Regs *r) {
    uint32_t o = r->a[4], map = og32(0x53D4), tbl = og32(0x53C8);
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t next;
        switch ((uint16_t)r->d[7]) {
        case 0: near_d0(r); if ((uint16_t)r->d[0]) return; next = 1; break;
        case 1: near_d0(r); if (og16(0x382E) < 0x20) return; next = 2; break;
        case 2: w16(o + 0x18, 0xFFFF); w16(tbl + 0x98, 0); next = 3; break;
        case 3: case 8: {
            uint16_t t = (uint16_t)(r16(o + 0x18) + 1);
            w16(o + 0x18, t);
            if (t < ((uint16_t)r->d[7] == 3 ? 3 : 2)) return;
            next = (uint16_t)r->d[7] == 3 ? 4 : 9; break;
        }
        case 4:
            w16(map + sxw(r16(o + 0xE)) + 0xE, 0);
            unlink_obj(r);
            w16(o + 0x10, r16(o + 0x30));
            w16(o + 4, 0); w16(o + 8, 0); w16(o + 0x30, 0); w16(o + 0xE, 0);
            w16(o + 0x18, 0x10);
            next = 5; break;
        case 5: {
            int16_t t = (int16_t)(r16(o + 0x18) - 1);
            w16(o + 0x18, (uint16_t)t);
            if (t >= 0) return;
            next = 6; break;
        }
        case 6: {
            if (!r16(tbl + 0xE2)) return;
            random_d0(r);
            uint16_t cell = r16(OADDR(0x4F40) + 2 * ((uint16_t)r->d[0] & 7));
            set_w(&r->d[2], cell);
            sog16(0x3824, cell);
            sog16(0x382C, r16(map + sxw(cell) + 0x1C)); sog16(0x3828, r16(map + sxw(cell) + 0x1E));
            if (r16(map + sxw(cell) + 0xE)) return;
            mz_seg_of(7, 0x3972), fn_S7_3972(r); if ((uint16_t)r->d[0]) return;
            sog16(0x3822, 5);
            fn_S7_3ACA(r); if ((uint16_t)r->d[0]) return;
            fn_S7_3784(r); if ((uint16_t)r->d[0]) return;
            next = 7; break;
        }
        case 7:
            w16(o + 0xE, r16(o + 0x30));
            w16(map + sxw(r16(o + 0xE)) + 0xE, 1);
            w16(o + 0x10, 0); w16(o + 0x18, 0xFFFF);
            next = 8; break;
        case 9: near_d0(r); if ((uint16_t)r->d[0]) return; next = 10; break;
        case 10: near_d0(r); if (!(uint16_t)r->d[0]) return; next = 9; break;
        default: return;
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_280A(Regs *r) {
    static const uint16_t hits[] = {0x18, 2, 0xFFFF};
    uint32_t o = r->a[4], tbl = og32(0x53C8);
    r->a[2] = tbl;
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t30_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_hit(r, s <= 1 || s == 9 || s == 10 ? hits : NULL, t30_think);
    switch ((uint16_t)r->d[7]) {
    case 0: case 9: w16(tbl + 0x98, 0); break;
    case 1:
        set_w(&r->d[0], og16(0x3830)); mz_seg_of(7, 0x3722), fn_S7_3722(r); fn_S7_3722(r);
        w16(tbl + 0x98, 1);
        w16(tbl + 0xDA, og16(0x56D0)); w16(tbl + 0xDE, og16(0x56D0));
        set_w(&r->d[1], og16(0x56D8)); set_w(&r->d[2], og16(0x56D4)); mz_seg(7), fo_S7_2038(r);
        w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
        break;
    case 3: w16(o + 0x3A, 0x7E); break;
    case 8: w16(o + 0x3A, r16(o + 0x18) ? 0 : 0x81); break;
    case 10: gen_S4_2A42(r); break;
    }
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- type $31 (fn_S4_2AF0, level 5) ----------
 * Waits (0) until record 3 has its +$22 set and an area (+$1C), then (1) roams: in the area
 * record 3 names it heads for record 3's cell (+$1E; reached: both cleared), elsewhere for the
 * passage between its area and that one (the cell G_5248 lists by G_51E0) and through it; with
 * record 3's +$20 set and fn_S7_226E seeing the player (2) it chases him, and at distance < 2
 * takes 1 from his life (G_56EA) and gets a random +$26. Hit by types $18 / $2E in states 0-2:
 * 3. States 3-8 (vanishing and coming back) are not reached by the tests yet: the translation
 * gen_S4_2B50 / gen_S4_2D52 runs them. */
void gen_S4_2B50(Regs *r), gen_S4_2D52(Regs *r), fo_S7_226E(Regs *r);
/* the distance-map index of map cell c from the object (G_53BC / G_53C0 are 128 x 128 maps) */
static uint16_t t31_index(uint32_t o, uint32_t map, uint16_t c) {
    uint16_t y = (uint16_t)(((r16(map + sxw(c) + 0x1E) - r16(o + 8)) & 0x7F) << 7);
    return (uint16_t)(y | ((r16(map + sxw(c) + 0x1C) - r16(o + 4)) & 0x7F));
}
static const int16_t t31_steps[4] = {-0x400, 0x3E0, 0x40, 0x3E0};   /* north, west, east, south in turn */
/* fn_S4_2DE2: a step in direction D0 (none if negative) */
static void t31_step(Regs *r) {
    if ((uint16_t)r->d[0] & 0x8000) return;
    r->a[0] = OADDR(0x4F30); mz_seg_of(7, 0x374C), fn_S7_374C(r); fn_S7_3784(r);
}
/* fn_S4_2DF6: D0 = the direction to the target cell +$12; within 2 of it the next one is the
 * best of its free neighbours by a random weight, more for one other than the last (+$10) */
static void t31_wander(Regs *r) {
    uint32_t o = r->a[4], map = og32(0x53D4);
    uint16_t at = r16(o + 0x12), i = t31_index(o, map, at);
    if (r8(og32(0x53BC) + sxw(i)) >= 2) { r->d[0] = r8(og32(0x53C0) + sxw(i)); return; }
    uint16_t best = 0, bcell = 0xFFFF, c = at;
    for (int k = 0; k < 4; k++) {
        c = (uint16_t)(c + t31_steps[k]);
        if (r8(map + sxw(c)) & 1) continue;
        random_d0(r);
        uint16_t w = (uint16_t)(((uint16_t)r->d[0] & 0x3F) | (c == r16(o + 0x10) ? 0x40 : 0x80));
        if (w >= best) { best = w; bcell = c; }
    }
    w16(o + 0x10, at); w16(o + 0x12, bcell);
    r->d[0] = r8(og32(0x53C0) + sxw(t31_index(o, map, bcell)));
}
/* fn_S4_2EF0: D0 = the direction to the target cell +$12 on the way to cell G_3814 (-2 there):
 * within 2 of it the next one is the free neighbour nearest G_3814 (half-scale distance map),
 * the last one (+$10) only if nothing else */
static void t31_toward(Regs *r) {
    uint32_t o = r->a[4], map = og32(0x53D4), dist = og32(0x53BC);
    uint16_t at = r16(o + 0x12), i = t31_index(o, map, at);
    if (r8(dist + sxw(i)) >= 2) { r->d[0] = r8(og32(0x53C0) + sxw(i)); return; }
    uint16_t to = og16(0x3814);
    if (to == at) { r->d[0] = 0xFFFFFFFEu; return; }
    sog16(0x381C, r16(map + sxw(to) + 0x1C)); sog16(0x3818, r16(map + sxw(to) + 0x1E));
    uint16_t best = 0xFFFF, bcell = 0xFFFF, c = at;
    for (int k = 0; k < 4; k++) {
        c = (uint16_t)(c + t31_steps[k]);
        if (r8(map + sxw(c)) & 1) continue;
        uint16_t y = (uint16_t)((((uint16_t)(r16(map + sxw(c) + 0x1E) - og16(0x3818)) >> 1) & 0x7F) << 7);
        uint16_t x = (uint16_t)(((uint16_t)(r16(map + sxw(c) + 0x1C) - og16(0x381C)) >> 1) & 0x7F);
        uint16_t w = (uint16_t)(r8(dist + sxw((uint16_t)(y | x))) | (c == r16(o + 0x10) ? 0x100 : 0));
        if (w < best) { best = w; bcell = c; }
    }
    w16(o + 0x10, at); w16(o + 0x12, bcell);
    r->d[0] = r8(og32(0x53C0) + sxw(t31_index(o, map, bcell)));
}
static int t31_sees(Regs *r) { mz_seg(7), fo_S7_226E(r); return (uint16_t)r->d[0] != 0; }
static void t31_think(Regs *r) {
    uint32_t tbl = og32(0x53C8);
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t next;
        switch ((uint16_t)r->d[7]) {
        case 0: if (!r16(tbl + 0xE2) || !r16(tbl + 0xDC)) return; next = 1; break;
        case 1:
            if (!r16(tbl + 0xE2) || !r16(tbl + 0xDC)) { next = 0; break; }
            if (!r16(tbl + 0xE0) || !t31_sees(r)) return;
            next = 2; break;
        case 2:
            if (!r16(tbl + 0xE2)) { next = 0; break; }
            if (r16(tbl + 0xE0) && t31_sees(r)) return;
            next = 1; break;
        default:
            r->a[3] = mz_seg_base(4) + 0x2B50;                   /* the original's think, for its JMP (A3) */
            gen_S4_2B50(r);
            return;
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_2AF0(Regs *r) {
    static const uint16_t hits[] = {0x18, 3, 0x2E, 3, 0xFFFF};
    uint32_t o = r->a[4], tbl = og32(0x53C8);
    r->a[2] = tbl;
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t31_think(r);
    o_hit(r, (uint16_t)r->d[7] <= 2 ? hits : NULL, t31_think);
    switch ((uint16_t)r->d[7]) {
    case 0:
        for (int k = 0; k < 3; k++) { t31_wander(r); t31_step(r); }
        w16(o + 0x3A, 0x7C);
        break;
    case 1: {
        w16(o + 0x3A, r16(tbl + 0xE0) ? 0x7D : 0x7C);
        set_w(&r->d[1], r16(o + 4)); set_w(&r->d[2], r16(o + 8)); mz_seg(7), fo_S7_2038(r);
        uint16_t area = (uint16_t)r->d[0], gate = 0;
        if (area == r16(tbl + 0xDC)) sog16(0x3814, r16(tbl + 0xDE));
        else {
            gate = (uint16_t)(r8(OADDR(0x51E0) + sxw((uint16_t)((area << 5) | r16(tbl + 0xDC)))) << 1);
            sog16(0x3814, r16(OADDR(0x5248) + sxw(gate)));
        }
        for (int k = 0; k < 3; k++) {
            t31_toward(r);
            if ((uint16_t)r->d[0] & 0x8000) {
                if (area == r16(tbl + 0xDC)) { w16(tbl + 0xDC, 0); w16(tbl + 0xDE, 0); break; }
                uint16_t c = r16(OADDR(0x5248) + sxw((uint16_t)(gate ^ 2)));   /* through: the passage's other side */
                sog16(0x3824, c);
                uint32_t map = og32(0x53D4);
                sog16(0x382C, r16(map + sxw(c) + 0x1C)); sog16(0x3828, r16(map + sxw(c) + 0x1E));
                mz_seg_of(7, 0x3784), fn_S7_3784(r);
                if (!(uint16_t)r->d[0]) w16(o + 0x12, og16(0x3824));
                break;
            }
            t31_step(r);
        }
        break;
    }
    case 2:
        w16(o + 0x12, og16(0x56D0));
        for (int k = 0; k < 3; k++) { near_d0(r); set_w(&r->d[0], og16(0x3830)); t31_step(r); }
        if (og16(0x382E) < 2) {
            int16_t l = (int16_t)(og16(0x56EA) - 1);
            sog16(0x56EA, (uint16_t)(l < 0 ? 0 : l));
            random_d0(r); w16(o + 0x26, (uint16_t)r->d[0] & 1);
        }
        w16(o + 0x3A, 0x7D);
        break;
    case 4: gen_S4_2D52(r); break;
    }
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- type $1F (fn_S4_0C44, level 5) ----------
 * Roams the map (fn_S4_1096: on to a free neighbouring cell at random, the last one less likely;
 * standing on a gate's cell (type $B / $10, state $A or more), one time in two through it to the
 * paired gate's cell, G_5248) until it sees the player close (fn_S7_1334: distance < 6, or
 * facing him within $11): 2, chasing him (marking him for record 3: +$18 / +$1C his area, +$1A /
 * +$1E his cell, +$20 = 2) until he is out of sight: 3, which notes where he was, and 1: following
 * record 3's marks, through the gates between areas (G_51E0 / G_5248) to his last cell; there,
 * with no trace (record 3's +$18 cleared, or +$22), back to 0. Hit by types $18 / $2E in states
 * 0-2: 4. States 4-10 are not reached by the tests yet (the translation gen_S4_0CAC and its acts). */
void gen_S4_0CAC(Regs *r), gen_S4_0F64(Regs *r), gen_S4_0F7E(Regs *r), gen_S4_0FD0(Regs *r), gen_S4_0FE0(Regs *r),
    gen_S4_103A(Regs *r), gen_S4_1066(Regs *r), gen_S4_107E(Regs *r), fo_S7_201C(Regs *r), fo_S7_2132(Regs *r),
    fo_S7_1FF8(Regs *r);
/* the gate record in cell c's object list (its first object), if of type $B / $10 in state $A+ */
static int t1f_gate(uint16_t c, uint16_t *state) {
    uint8_t n = r8(og32(0x53D4) + sxw(c) + 1);
    if (!n) return 0;
    uint32_t g = og32(0x53C8) + ((uint32_t)n << 6);
    if (r16(g + 2) != 0xB && r16(g + 2) != 0x10) return 0;
    *state = r16(g + 0x16);
    return *state >= 0xA;
}
void hx_log(const char *f, ...);
static void t1f_at(uint16_t c) {                            /* G_3824 / G_382C / G_3828: cell c and its centre */
    uint32_t map = og32(0x53D4);
    sog16(0x3824, c); sog16(0x382C, r16(map + sxw(c) + 0x1C)); sog16(0x3828, r16(map + sxw(c) + 0x1E));
}
/* moved into cell c if it is free (fn_S7_1766 / 1578), taking it over in the map; then fn_S7_1CCC */
static void t1f_through(Regs *r, uint16_t c) {
    uint32_t o = r->a[4], map = og32(0x53D4);
    t1f_at(c);
    if (r16(map + sxw(c) + 0xE)) return;
    mz_seg_of(7, 0x3972), fn_S7_3972(r); if ((uint16_t)r->d[0]) return;
    fn_S7_3784(r); if ((uint16_t)r->d[0]) return;
    w16(map + sxw(r16(o + 0xE)) + 0xE, 0);
    w16(o + 0xE, og16(0x3824));
    w16(map + sxw(og16(0x3824)) + 0xE, 1);
    mz_seg_of(7, 0x3EE2), fn_S7_3EE2(r);
}
/* fn_S4_1096: D0.w = the direction to the target cell +$E; there, the next one (see above) */
static void t1f_route(Regs *r) {
    static const int16_t step[4] = {-0x400, 0x3E0, 0x40, 0x3E0};
    static const uint16_t dir[4] = {0x10, 0, 0x20, 0x30};
    uint32_t o = r->a[4], map = og32(0x53D4);
    uint16_t at = r16(o + 0xE), st;
    int16_t dx = (int16_t)(r16(map + sxw(at) + 0x1C) - r16(o + 4)), dy = (int16_t)(r16(map + sxw(at) + 0x1E) - r16(o + 8));
    if (dx) { set_w(&r->d[0], dx < 0 ? 0 : 0x20); return; }
    if (dy) { set_w(&r->d[0], dy < 0 ? 0x10 : 0x30); return; }
    w16(map + sxw(at) + 0xE, 0);
    if (t1f_gate(at, &st) && st != 0x3C) {
        random_d0(r);
        if (!((uint16_t)r->d[0] & 1)) {
            uint16_t c = r16(OADDR(0x5248) + sxw((uint16_t)(((st - 0xA) ^ 1) << 1)));
            t1f_at(c);
            if (!r16(map + sxw(c) + 0xE)) {
                mz_seg_of(7, 0x3972), fn_S7_3972(r);
                if (!(uint16_t)r->d[0]) { fn_S7_3784(r); w16(o + 0xE, og16(0x3824)); }
            }
        }
        at = r16(o + 0xE);
    }
    uint16_t best = 0, bcell = 0xFFFF, bdir = 0xFFFF, c = at;
    for (int k = 0; k < 4; k++) {
        c = (uint16_t)(c + step[k]);
        if (r16(map + sxw(c) + 0xE)) continue;
        random_d0(r);
        uint16_t w = (uint16_t)(((uint16_t)r->d[0] & 3) | (c == r16(o + 0x10) ? 4 : 8));
        if (w >= best) { best = w; bcell = c; bdir = dir[k]; }
    }
    if (!(bcell & 0x8000)) { w16(o + 0x10, r16(o + 0xE)); w16(o + 0xE, bcell); w16(map + sxw(bcell) + 0xE, 1); }
    else w16(map + sxw(r16(o + 0xE)) + 0xE, 1);
    set_w(&r->d[0], bdir);
}
/* fn_S7_1334 (fn_S7_319E): the player close (< 6) or in front (within $11 of its heading)? */
static int t1f_sees(Regs *r) {
    mz_seg_of(7, 0x319E), fn_S7_319E(r);
    if (og16(0x382E) & 0x8000) return 0;
    if (og16(0x382E) < 6) return 1;
    set_w(&r->d[0], (uint16_t)(og16(0x3830) - r16(r->a[4] + 0xC)));
    mz_seg_of(7, 0x367E), fn_S7_367E(r);
    return (uint16_t)r->d[0] < 0x11;
}
static void mark_player(Regs *r) {                          /* record 3: the player's cell and area */
    uint32_t tbl = og32(0x53C8);
    w16(tbl + 0xDA, og16(0x56D0)); w16(tbl + 0xDE, og16(0x56D0));
    set_w(&r->d[1], og16(0x56D8)); set_w(&r->d[2], og16(0x56D4)); mz_seg(7), fo_S7_2038(r);
    w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
}
static void t1f_think(Regs *r) {
    uint32_t tbl = og32(0x53C8);
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t next;
        switch ((uint16_t)r->d[7]) {
        case 0:
            if (r16(tbl + 0xE2) && r16(tbl + 0xD8)) { next = 1; break; }
            if (!t1f_sees(r)) return;
            next = 2; break;
        case 1:
            if (!r16(tbl + 0xD8) || !r16(tbl + 0xE2)) { next = 0; break; }
            if (!t1f_sees(r)) return;
            next = 2; break;
        case 2:
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (!(og16(0x382E) & 0x8000)) return;
            next = 3; break;
        case 3: mark_player(r); w16(r->a[4] + 0x10, 0); next = 1; break;
        default:
            r->a[3] = mz_seg_base(4) + 0xCAC;                    /* the original's think, for its JMP (A3) */
            gen_S4_0CAC(r);
            return;
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_0C44(Regs *r) {
    static const uint16_t hits[] = {0x18, 4, 0x2E, 4, 0xFFFF};
    static void (*const gen_act[7])(Regs *) = {gen_S4_0F64, gen_S4_0F7E, gen_S4_0FD0, gen_S4_0FE0, gen_S4_103A, gen_S4_1066, gen_S4_107E};
    uint32_t o = r->a[4], tbl = og32(0x53C8);
    r->a[2] = tbl;
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t1f_think(r);
    o_hit(r, (uint16_t)r->d[7] <= 2 ? hits : NULL, t1f_think);
    uint16_t s = (uint16_t)r->d[7];
    switch (s) {
    case 0: t1f_route(r); mz_seg(7), fo_S7_201C(r); break;
    case 1: {
        set_w(&r->d[1], r16(o + 4)); set_w(&r->d[2], r16(o + 8)); mz_seg(7), fo_S7_2038(r);
        uint16_t area = (uint16_t)r->d[0], gate = 0;
        int same = area == r16(tbl + 0xD8);
        if (same) sog16(0x3814, r16(tbl + 0xDA));
        else {
            gate = (uint16_t)(r8(OADDR(0x51E0) + sxw((uint16_t)((area << 5) | r16(tbl + 0xD8)))) << 1);
            sog16(0x3814, r16(OADDR(0x5248) + sxw(gate)));
        }
        mz_seg(7), fo_S7_2132(r);
        int16_t d = (int16_t)r->d[0];
        if (d >= 0) { if (same) fo_S7_1FF8(r); else fo_S7_201C(r); break; }
        if (d != -2) break;
        if (!same) { t1f_through(r, r16(OADDR(0x5248) + sxw((uint16_t)(gate ^ 2)))); break; }
        w16(tbl + 0xD8, 0); w16(tbl + 0xDA, 0);             /* at his last cell: a gate there? through it */
        uint16_t st;
        if (t1f_gate(r16(o + 0xE), &st)) t1f_through(r, r16(OADDR(0x5248) + sxw((uint16_t)(((st - 0xA) ^ 1) << 1))));
        break;
    }
    case 2:
        w16(tbl + 0xDA, og16(0x56D0)); w16(tbl + 0xDE, og16(0x56D0));
        w16(tbl + 0xE0, 2);
        g_target(og16(0x56D8), og16(0x56D4));
        set_w(&r->d[1], og16(0x56D8)); set_w(&r->d[2], og16(0x56D4)); mz_seg(7), fo_S7_2038(r);
        w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
        if (og16(0x382E) >= 0xA) {
            mz_seg_of(7, 0x3B36), fn_S7_3B36(r); fn_S7_3D0E(r);
            set_w(&r->d[0], og16(0x3812));
            if (!(og16(0x3812) & 0x8000)) { mz_seg(7), fo_S7_1FF8(r); break; }
        }
        set_w(&r->d[0], og16(0x3830)); mz_seg_of(7, 0x3722), fn_S7_3722(r);
        break;
    case 3: break;
    default: if (s >= 4 && s <= 10) gen_act[s - 4](r); break;
    }
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- fn_S7_0F78: one object's tick (Original's dispatcher, for fn_S7_2E9A) ----------
 * The record at D0.w of G_53C8 (G_3834 = D0) runs the behaviour of its type (+2); types without
 * one do nothing. Keeps D3-D7 / A2-A4. The behaviours not yet written by hand are the translations
 * in orig_gen.c (levels 2-6, the snails fo_S10_0028 and type $1B). */
void fo_S10_0028(Regs *r), fo_S4_0028(Regs *r), fo_S4_020C(Regs *r), fo_S4_04AE(Regs *r), fo_S9_0046(Regs *r),
    fo_S9_0372(Regs *r), fo_S7_087A(Regs *r), fo_S11_00C0(Regs *r), fo_S10_1F28(Regs *r), fo_S4_0782(Regs *r),
    fo_S4_0C44(Regs *r), fo_S4_1256(Regs *r), fo_S7_099C(Regs *r), fo_S4_1670(Regs *r), fo_S9_0D76(Regs *r),
    fo_S9_1738(Regs *r), fo_S9_1AE0(Regs *r), fo_S7_0A5A(Regs *r), fo_S9_1D0C(Regs *r), fo_S9_1CAC(Regs *r),
    fo_S4_1DFA(Regs *r), fo_S4_21D6(Regs *r), fo_S4_280A(Regs *r), fo_S4_2AF0(Regs *r), fo_S9_1E46(Regs *r),
    fo_S9_2072(Regs *r), fo_S9_219A(Regs *r), fo_S7_0AB0(Regs *r), fo_S7_0C9C(Regs *r), fo_S11_019A(Regs *r),
    fo_S11_0382(Regs *r), fo_S11_046E(Regs *r), fo_S7_0B7C(Regs *r);
static void type_23(Regs *r) { mz_seg_of(7, 0x0E46), fn_S7_0E46(r); }   /* fn_S7_03FE, shared with Catacombs */
void fo_S7_0F78(Regs *r) {
    static const struct { uint8_t seg; void (*f)(Regs *); } by_type[63] = {
        {7, fo_S7_0528}, {7, fo_S7_0606}, {7, fo_S7_06A8}, {10, fo_S10_0028}, {10, fo_S10_0028}, {10, fo_S10_0028},
        {10, fo_S10_1974}, {10, fo_S10_1CE2}, {10, fo_S10_1D96}, {10, fo_S10_21DC}, {10, fo_S10_2B92}, {7, fo_S7_007C},
        {0, NULL}, {4, fo_S4_0028}, {4, fo_S4_020C}, {4, fo_S4_04AE}, {7, fo_S7_007C}, {9, fo_S9_0046},
        {9, fo_S9_0372}, {7, fo_S7_087A}, {10, fo_S10_2C7C}, {0, NULL}, {11, fo_S11_00C0}, {7, type_23},
        {7, type_23}, {7, type_23}, {7, type_23}, {10, fo_S10_1F28}, {10, fo_S10_1B2E}, {10, fo_S10_400A},
        {4, fo_S4_0782}, {4, fo_S4_0C44}, {4, fo_S4_1256}, {0, NULL}, {7, fo_S7_099C}, {4, fo_S4_1670},
        {9, fo_S9_0D76}, {0, NULL}, {10, fo_S10_1974}, {10, fo_S10_1B2E}, {9, fo_S9_1738}, {9, fo_S9_1AE0},
        {7, fo_S7_0A5A}, {9, fo_S9_1D0C}, {9, fo_S9_1CAC}, {4, fo_S4_1DFA}, {4, fo_S4_21D6}, {0, NULL},
        {4, fo_S4_280A}, {4, fo_S4_2AF0}, {9, fo_S9_1E46}, {0, NULL}, {9, fo_S9_2072}, {9, fo_S9_219A},
        {7, fo_S7_0AB0}, {7, fo_S7_0C9C}, {11, fo_S11_019A}, {11, fo_S11_0382}, {11, fo_S11_046E}, {0, NULL},
        {10, fo_S10_1D96}, {7, fo_S7_0B7C}, {0, NULL}};
    uint32_t d3 = r->d[3], d4 = r->d[4], d5 = r->d[5], d6 = r->d[6], d7 = r->d[7];
    uint32_t a2 = r->a[2], a3 = r->a[3], a4 = r->a[4];
    sog16(0x3834, (uint16_t)r->d[0]);
    r->d[0] &= 0xFFFF;
    r->a[4] = og32(0x53C8) + r->d[0];
    uint16_t type = r16(r->a[4] + 2);
    if (type < 63 && by_type[type].f) {
        if (by_type[type].f != type_23) mz_seg(by_type[type].seg);
        by_type[type].f(r);
    }
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->d[6] = d6; r->d[7] = d7;
    r->a[2] = a2; r->a[3] = a3; r->a[4] = a4;
    cc_nz_l(r, d7);
}
