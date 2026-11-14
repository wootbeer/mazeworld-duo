/* Mazeworld Original's creatures (the object behaviours its dispatcher fn_S7_0F78 calls by type, in
 * segments 4, 7, 9, 10 and 11), named by Original's offsets. As in Catacombs (game/seg7_objects.c)
 * each is a state machine over the record at A4 (64 bytes in G_53C8: +2 type, +$12 event, +$14 hit
 * by, +$16 state, +$18 timer, +$3A picture, ...): "think" code per state (the original's JMP (A3),
 * which also means "switch to state D0 and think again"), "act" code per state, and per-state
 * lists for the reactions below. Here the tables are switches. Their register results are not
 * compared (the dispatcher restores what its caller keeps). */
#include "mzasm.h"
#include "orig_names.h"
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
    uint16_t type = r16(og32(OG_OBJECTS) + by + 2) & 0x7FFF;
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
    if ((e[0] & 0x7FFF) != (og16(OG_MESSAGE) & 0x7FFF)) return;
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
    set_w(&r->d[7], r16(r->a[4] + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t0_think(r);
    o_hit(r, (uint16_t)r->d[7] < 4 ? lists[(uint16_t)r->d[7]] : NULL, t0_think);
    if ((uint16_t)r->d[7] == 3) set_pic(r, 0x1B);
    w16(r->a[4] + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type 1 (fn_S7_0606) ----------
 * A hit by a type $18 object: in states 0-12 sets bit $20 of the game's flag word 0 (the first
 * time also sending event $101 to records 2, 3 and 4) and becomes state -1 (picture $1C);
 * from state 13 on it goes back 13 states. */
void fo_S7_0606(Regs *r) {
    set_w(&r->d[2], r16(r->a[4] + 0x14));
    if ((uint16_t)r->d[2]) {
        r->a[0] = og32(OG_OBJECTS);
        set_w(&r->d[0], r16(r->a[0] + sxw(r->d[2]) + 2));
        uint16_t s = r16(r->a[4] + OB_STATE);
        if ((uint16_t)r->d[0] == 0x18 && !(s & 0x8000)) {
            if ((int16_t)s >= 0xD) w16(r->a[4] + OB_STATE, (uint16_t)(s - 0xD));
            else {
                call_d0d1(r, fo_S7_1282, 0, 0x20);
                if ((uint16_t)r->d[0]) {
                    call_d0d1(r, fo_S7_1278, 0x101, 0x80);
                    call_d0d1(r, fo_S7_1278, 0x101, 0xC0);
                    call_d0d1(r, fo_S7_1278, 0x101, 0x100);
                }
                call_d0d1(r, fo_S7_1292, 0, 0x20);
                r->d[0] = 0xFFFFFFFFu;
                w16(r->a[4] + OB_STATE, 0xFFFF);
            }
        }
        w16(r->a[4] + 0x14, 0);
    }
    set_pic(r, (r16(r->a[4] + OB_STATE) & 0x8000) ? 0x1C : 0);
}

/* ---------- type 2, the level's exit (fn_S7_06A8) ----------
 * Open once the four records after it are all in state 1 (else picture $E); it cycles through
 * pictures 8-$D (+$16 = 2, 4, .. 12). With the player in its cell at x $56-$5A, y $54-$57 it sends
 * event 1 to records 2-5 and counts +$18 = 16 frames of the exit picture
 * (G_56DE, full view G_56E0 = $81), healing (life G_56EA + 3, G_56E6 + 6, to $8E); at the end
 * the level changes (G_57DC = 4, G_57D8 / G_57D4 the start, G_57DA the heading, G_57DE, G_57F2). */
static uint16_t t2_next(Regs *r) {
    static const uint16_t pic[7] = {0, 8, 9, 0xA, 0xB, 0xC, 0xD};
    uint16_t s = (uint16_t)(r16(r->a[4] + OB_STATE) + 2);
    if (s >= 0xD) s = 2;
    w16(r->a[4] + OB_STATE, s);
    return pic[s / 2];
}
static uint16_t heal(uint16_t v, uint16_t by) { v = (uint16_t)(v + by); return v < 0x8E ? v : 0x8E; }
void fo_S7_06A8(Regs *r) {
    if (!r16(r->a[4] + 0x18)) {
        for (uint32_t k = 1; k <= 4; k++)
            if (r16(r->a[4] + 0x40 * k + 0x16) != 1) { set_pic(r, 0xE); return; }
        set_pic(r, t2_next(r));
        if (r16(r->a[4] + OB_CELL) != og16(OG_PLAYER_CELL)) return;
        uint16_t x = og16(OG_PLAYER_X), y = og16(OG_PLAYER_Y);
        if (x < 0x56 || x >= 0x5B || y < 0x54 || y >= 0x58) return;
        for (uint16_t at = 0x80; at <= 0x140; at += 0x40) call_d0d1(r, fo_S7_1278, 1, at);
        w16(r->a[4] + 0x18, 0x10);
    }
    sog16(0x56DE, t2_next(r));
    sog16(OG_FULL_VIEW, 0x81);
    uint16_t t = (uint16_t)(r16(r->a[4] + 0x18) - 1);
    w16(r->a[4] + 0x18, t);
    if (!t) {
        sog16(OG_GO_LEVEL, 4); sog32(OG_GO_X, 0x780000); sog32(OG_GO_Y, 0x5E0000);
        sog16(OG_GO_HEADING, og16(OG_PLAYER_HEADING)); sog8(0x57DE, 1); sog8(0x57F2, 1);
    }
    sog16(OG_LIFE, heal(og16(OG_LIFE), 3));
    sog16(0x56E6, heal(og16(0x56E6), 6));
}

/* fo_S10_2624: the spin-and-move (fn_S7_0A9C), only every other frame (G_56C4 odd) while the
 * object is above y $8F; else D0.l = -1 */
void fo_S10_2624(Regs *r) {
    set_w(&r->d[1], r16(r->a[4] + OB_Y));
    if ((uint16_t)r->d[1] < 0x8F) {
        set_w(&r->d[1], og16(0x56C4) & 1);
        if (!(uint16_t)r->d[1]) { r->d[0] = 0xFFFFFFFFu; cc_nz_l(r, r->d[0]); return; }
    }
    mz_seg_of(7, 0x0A9C), fn_S7_0A9C(r);
}

static void random_d0(Regs *r) { mz_seg_of(2, 0x3532), fn_S2_3532(r); }
static void unlink_obj(Regs *r) {                           /* fn_S2_29E2: out of its cell's list */
    set_w(&r->d[0], og16(OG_SELF)); set_w(&r->d[1], r16(r->a[4] + OB_CELL));
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
            uint16_t c = (uint16_t)(r16(o + OB_HEADING) + 1);
            if (c >= 0x2E) { w16(o + OB_HEADING, 0); set_w(&r->d[0], 2); continue; }
            w16(o + OB_HEADING, c); return;
        }
        case 2:
            if (t < 0) { set_w(&r->d[0], 0); continue; }
            random_d0(r);
            if ((uint16_t)r->d[0] & 7) return;
            set_w(&r->d[0], 1); continue;
        case 3:
            unlink_obj(r);
            w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000);
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
    uint32_t a1 = og32(OG_OBJECTS) + ((uint32_t)n << 6), map = og32(OG_MAP);
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
    if (s == 0) { w16(o + 0x12, k3->act0); w16(o + OB_HEADING, 0); return; }
    if (s != 1 && s != 2) return;
    if (k3->kind == 2) { if (s == 1) w16(o + 0x12, 4); return; }
    w16(o + 0x12, 2);
    uint32_t t = og32(OG_OBJECTS);
    if (k3->kind == 1) {
        if (r16(t + sxw((uint16_t)(og16(OG_SELF) + 0x40)) + 0x12) == 4) w16(o + 0x12, 4);
        return;
    }
    if (r16(t + sxw((uint16_t)(og16(OG_SELF) - 0x40)) + 0x18) & 0x8000) return;
    uint16_t cell = r16(o + OB_CELL);                          /* fn_S10_1F4E: the objects listed in the cell */
    for (uint32_t k = 1; k <= 10; k++) {
        uint8_t n = r8(og32(OG_MAP) + sxw(cell) + k);
        if (!n) break;
        k3_push(r, n, cell);
    }
    if (r16(o + 0x10)) return;
    if (og16(OG_PLAYER_CELL) != cell) return;                       /* fn_S10_2096 */
    for (int k = 0; k < 3; k++) k3_push_player(r, r16(o + 0x1A) == 2 ? 0x56D4 : 0x56D8);
}
static void kind3_tick(Regs *r, int v) {
    static const uint16_t hits[] = {0x13, 6, 0x17, 5, 0x18, 7, 0xFFFF};
    k3 = &kind3[v];
    set_w(&r->d[7], r16(r->a[4] + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    k3_think(r);
    o_hit(r, (uint16_t)r->d[7] <= 2 ? hits : NULL, k3_think);
    k3_act(r);
    w16(r->a[4] + OB_STATE, (uint16_t)r->d[7]);
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
            uint16_t c = (uint16_t)(r16(r->a[4] + OB_HEADING) + 1);
            if (c >= 0x2E) { w16(r->a[4] + OB_HEADING, 0); set_w(&r->d[0], 1); continue; }
            w16(r->a[4] + OB_HEADING, c); return;
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
    set_w(&r->d[7], r16(r->a[4] + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t1ce2_think(r);
    o_hit(r, (uint16_t)r->d[7] <= 1 ? hits : NULL, t1ce2_think);
    if ((uint16_t)r->d[7] == 2) {
        call_d0d1(r, fo_S7_1292, 0, 0x80);
        w16(r->a[4] + OB_TYPE, 0x1B);
        set_w(&r->d[0], og16(OG_SELF));
        mz_seg_of(2, 0x3480), fn_S2_3480(r);
        w16(r->a[4] + OB_STATE, 0);
    }
    w16(r->a[4] + OB_STATE, (uint16_t)r->d[7]);
}

/* fn_S10_2B92: the level's twelve counters (words of G_53C8 + $1E: low byte counts to $10, then a
 * new random high nibble and low byte 0 or $FF), the map's refresh (fn_S7_3128), and while the
 * player is near (G_382E < 4) life G_56EA and G_56E6 + 10 each, to $8E */
void fo_S10_2B92(Regs *r) {
    uint32_t a2 = og32(OG_OBJECTS) + 0x1E;
    for (uint16_t d3 = 0; d3 < 0x18; d3 += 2) {
        uint16_t w = r16(a2 + d3), n = (uint16_t)((w & 0xFF) + 1);
        if (n != 0x11) { w16(a2 + d3, (uint16_t)((w & 0xFF00) | n)); continue; }
        random_d0(r);
        uint16_t hi = (uint16_t)(((uint16_t)r->d[0] & 0xF) << 8);
        random_d0(r);
        w16(a2 + d3, (uint16_t)(hi | ((uint16_t)-((uint16_t)r->d[0] & 1) & 0xFF)));
    }
    mz_seg_of(7, 0x3128), fn_S7_3128(r);
    if (og16(OG_NEAR_DIST) < 4) { sog16(OG_LIFE, heal(og16(OG_LIFE), 0xA)); sog16(0x56E6, heal(og16(0x56E6), 0xA)); }
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
    uint32_t o = r->a[4], map = og32(OG_MAP);
#define CELL(c, f) (map + sxw(c) + (f))
    uint16_t at = r16(o + 0xE);
    int16_t dx = (int16_t)(r16(CELL(at, 0x1C)) - r16(o + OB_X)), dy = (int16_t)(r16(CELL(at, 0x1E)) - r16(o + OB_Y));
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
    uint32_t o = r->a[4], map = og32(OG_MAP);
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t dist = r16(map + sxw(r16(o + 0xE)) + 0x10);
        switch ((uint16_t)r->d[7]) {
        case 0: if (dist >= 0xFFFD) { set_w(&r->d[0], 0xC); continue; } return;
        case 1:
            if (r16(o + 0xE) >= 0x2000) { set_w(&r->d[0], 6); continue; }
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (og16(OG_NEAR_DIST) < 0x19) { set_w(&r->d[0], 5); continue; }
            return;
        case 2:
            w16(o + 0x18, 0xC0);
            sog16(0x56A8, 0x14); sog16(0x56A6, og16(OG_PLAYER_CELL));
            mz_seg_of(2, 0x366A), fn_S2_366A(r); fn_S2_3568(r);    /* fn_S2_2B3E */
            set_w(&r->d[0], 3); continue;
        case 3: {
            uint16_t t = r16(o + 0x18);
            if (r16(o + OB_Y) >= 0x88) t = (uint16_t)(t - 2);
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
            if (og16(OG_NEAR_DIST) >= 0x19) { set_w(&r->d[0], 1); continue; }
            return;
        case 6:
            if (r16(o + 0xE) < 0x2000) { set_w(&r->d[0], 1); continue; }
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (og16(OG_NEAR_DIST) < 0x19) { set_w(&r->d[0], 5); continue; }
            return;
        case 8: {
            if (dist) { set_w(&r->d[0], 9); continue; }
            uint32_t t = og32(OG_OBJECTS);
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
    w16(og32(OG_MAP) + sxw(r16(r->a[4] + 0xE)) + 0xE, 0);
    unlink_obj(r);
    w16(r->a[4] + OB_TYPE, r16(r->a[4] + OB_TYPE) | 0x8000);
}
/* fn_S7_1F60: an object of type `type` made in the first free record (type $FFFF, from the last
 * down) at G_382C / G_3828 in cell G_3824 (fn_S2_2976 refusing it: none), heading c, +$10 f10; its
 * record offset, or -1 */
static int make_object(Regs *r, uint16_t type, uint16_t c, uint16_t f10) {
    uint32_t t = og32(OG_OBJECTS);
    uint16_t rec = 0x1DC0;
    while (r16(t + rec + 2) != 0xFFFF) { rec -= 0x40; if (!rec) return -1; }
    set_w(&r->d[0], rec); set_w(&r->d[1], og16(OG_MOVE_CELL));
    mz_seg_of(2, 0x33A4), fn_S2_33A4(r);
    if ((uint16_t)r->d[0]) return -1;
    uint32_t o = t + rec;
    w16(o + OB_TYPE, type); w16(o + OB_X, og16(OG_MOVE_X)); w16(o + OB_Y, og16(OG_MOVE_Y));
    w16(o + 6, 0x8000); w16(o + 0xA, 0x8000); w16(o + 0xE, 0);
    w16(o + OB_HEADING, c); w16(o + 0x10, f10); w16(o + 0x14, 0); w16(o + 0x12, 0xFFFF);
    set_w(&r->d[0], rec);
    mz_seg_of(2, 0x3480), fn_S2_3480(r);
    return rec;
}
/* fn_S7_1E66: at the player: a step towards him (fn_S7_1EA2, when it can see him), two more beyond
 * 6; within 6 an object of type $2A (made at 0, 0) */
static void rush_step(Regs *r) {
    mz_seg_of(7, 0x3128), fn_S7_3128(r);                    /* fn_S7_12BE */
    if (og16(OG_NEAR_DIR) & 0x8000) return;
    set_w(&r->d[0], og16(OG_NEAR_DIR));
    mz_seg_of(7, 0x3722), fn_S7_3722(r);
    r->a[0] = OADDR(0x4F30);
    mz_seg_of(7, 0x374C), fn_S7_374C(r);
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
}
static void rush(Regs *r) {
    rush_step(r);
    if (og16(OG_NEAR_DIST) >= 6) { rush_step(r); rush_step(r); }
    if (og16(OG_NEAR_DIST) < 6) {
        sog16(OG_MOVE_X, 0); sog16(OG_MOVE_Y, 0); sog16(OG_MOVE_CELL, 0);
        make_object(r, 0x2A, 0, 0);
    }
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
    set_w(&r->d[7], r16(o + OB_STATE));
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
        sog16(OG_TARGET_X, og16(OG_PLAYER_X)); sog16(OG_TARGET_Y, og16(OG_PLAYER_Y));
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
        uint32_t map = og32(OG_MAP), t = og32(OG_OBJECTS);
        for (uint32_t k = 1; k <= 10; k++) {
            uint8_t n = r8(map + sxw(r16(o + OB_CELL)) + k);
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
    w16(o + OB_STATE, (uint16_t)r->d[7]);
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
static void g_target(uint16_t x, uint16_t y) { sog16(OG_TARGET_X, x); sog16(OG_TARGET_Y, y); }
static void g_record1(void) { uint32_t t = og32(OG_OBJECTS); g_target(r16(t + 0x44), r16(t + 0x48)); }
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
        sog16(OG_TARGET_X, path[i / 2]);
        if (path[i / 2] & 0x8000) { r->d[0] = r->d[1] = 0xFFFFFFFFu; return -1; }
        sog16(OG_TARGET_Y, path[i / 2 + 1]);
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
            if (og16(OG_NEAR_DIST) < 8) { set_w(&r->d[0], 1); continue; }
            g_record1(); fo_S7_14C0(r);
            if ((uint16_t)r->d[0] < 6) { set_w(&r->d[0], 8); continue; }
            if ((uint16_t)r->d[0] >= 9) return;
            if (r16(og32(OG_OBJECTS) + 0x56) == 2) { set_w(&r->d[0], 8); continue; }
            return;
        case 1: w16(o + 0x1A, 0x1E); set_w(&r->d[0], 2); continue;
        case 2: {
            near_d0(r);
            if (og16(OG_NEAR_DIST) >= 0xC) { set_w(&r->d[0], 5); continue; }
            set_w(&r->d[0], (uint16_t)((og16(OG_NEAR_DIR) ^ 0x20) - og16(OG_PLAYER_HEADING)));
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
            if (og16(OG_NEAR_DIST) < 6) { set_w(&r->d[0], 1); continue; }
            return;
        case 8:
            for (int k = 0; k < 4; k++) g_ring(r);
            g_record1(); fo_S7_14C0(r);
            if ((uint16_t)r->d[0] >= 3) return;
            call_d0d1(r, fo_S7_1278, 0x1D02, 0x40);
            sog16(OG_MESSAGE, 0);
            w16(o + 0x1A, 6);
            set_w(&r->d[0], 9); continue;
        case 9: {
            int16_t n = (int16_t)(r16(o + 0x1A) - 1);
            set_w(&r->d[3], (uint16_t)n);
            if (n < 0) { w16(o + 0x1A, 7); set_w(&r->d[0], 0xA); continue; }
            w16(o + 0x1A, (uint16_t)n);
            do {
                r->a[0] = OADDR(0x4F30);
                set_w(&r->d[0], r16(o + OB_HEADING));
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
            set_w(&r->d[0], og16(OG_NEAR_DIR));
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
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    guard_think(r);
    o_event(r, (uint16_t)r->d[7] < 12 && has_ev[(uint16_t)r->d[7]] ? ev : NULL, guard_think);
    switch ((uint16_t)r->d[7]) {
    case 0:
        g_pic_random(r); g_ring(r); g_ring(r);
        g_record1(); fo_S7_14C0(r);
        if (!(og16(0x56C4) & ((uint16_t)r->d[0] >= 0xF ? 0x1F : 0x7F))) g_ring(r);
        break;
    case 2: g_pic_random(r); set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x36F4), fn_S7_36F4(r); break;
    case 4: {
        uint16_t t = (uint16_t)(r16(o + 0x1A) + 1);
        w16(o + 0x1A, t);
        if (t < 4) w16(o + 0x18, 7);
        else if (t < 8) w16(o + 0x18, 4);
        else if (t < 0xC) w16(o + 0x18, 0xC);
        else { unlink_obj(r); w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000); call_d0d1(r, fo_S7_1278, 0x1D01, 0x40); }
        break;
    }
    case 6: {
        w16(o + 0x26, 1);
        int16_t t = (int16_t)(r16(o + 0x1A) - 1);
        w16(o + 0x1A, (uint16_t)(t < 0 ? 0 : t));
        w16(o + 0x18, 0x10);
        sog32(OG_MOVE_X, og32(OG_PLAYER_X)); sog32(OG_MOVE_Y, og32(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
        mz_seg_of(7, 0x3784), fn_S7_3784(r);
        int16_t l = (int16_t)(og16(OG_LIFE) - 0x28);
        sog16(OG_LIFE, (uint16_t)(l < 0 ? 0 : l));
        break;
    }
    case 7: case 8: g_pic_random(r); break;
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
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
 * States 16-51 are his remarks (george_remark, event $304 from the snails: 16), each a text or
 * two ($1405-$141A), most setting a bit of flag words 0 / 1; 34-41 walk him to a point on level 4
 * (fo_S7_155C) and leave him there until the player goes (36-37: gone). Not all of them are
 * reached by the tests yet. */
void fo_S7_155C(Regs *r);
static void say(uint16_t text) { sog16(OG_MESSAGE, (uint16_t)(0x8000 | text)); }
static int flag(Regs *r, uint16_t word, uint16_t bits) { call_d0d1(r, fo_S7_1282, word, bits); return (uint16_t)r->d[0] != 0; }
static void flag_set(Regs *r, uint16_t word, uint16_t bits) { call_d0d1(r, fo_S7_1292, word, bits); }
static int cell_in(uint16_t cell, const uint16_t *list) { for (; *list; list++) if (cell == *list) return 1; return 0; }
static int george_cell_ends(void) {                          /* fn_S10_3D6C: the player in one of these cells ends a remark */
    static const uint16_t cells[] = {0x2580, 0x2940, 0x4BC0, 0x5340, 0x7B20, 0x4660, 0x4E80, 0x7160, 0x6E20, 0x6520,
                                     0x6540, 0x6560, 0};
    return cell_in(og16(OG_PLAYER_CELL), cells);
}
/* fn_S10_3B7C: off the map (out of his cell; x, y, cell and the showing phase +$1A cleared) */
static void george_hide(Regs *r) {
    if (!r16(r->a[4] + OB_CELL)) return;
    unlink_obj(r);
    w16(r->a[4] + OB_X, 0); w16(r->a[4] + OB_Y, 0); w16(r->a[4] + OB_CELL, 0); w16(r->a[4] + 0x1A, 0);
}
static void george_hide_shown(Regs *r) { if (r16(r->a[4] + 0x1A)) george_hide(r); }   /* fn_S10_3B9C */
/* placed where the player stands (fn_S7_1578 from G_382C / G_3828 / G_3824) */
static void george_at_player(Regs *r) {
    sog32(OG_MOVE_X, og32(OG_PLAYER_X)); sog32(OG_MOVE_Y, og32(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
}
/* fn_S10_3BA8: how he appears for a new text: 0 he goes; 1 beside the player, facing him (+$1A 1,
 * $32 frames); 2-4 at the record +$1C, facing the player, for $1F4 / $14 / 5 frames (+$1A 3) */
static void george_appear(Regs *r, uint16_t how) {
    static const uint16_t frames[5] = {0, 0, 0x1F4, 0x14, 5};
    uint32_t o = r->a[4];
    if (how == 0 || how > 4) { george_hide_shown(r); return; }    /* (texts use 0-4) */
    if (how == 1) {
        if (!r16(o + 0x1A)) { george_at_player(r); w16(o + OB_HEADING, og16(OG_PLAYER_HEADING) ^ 0x20); }
        w16(o + 0x1A, 1); w16(o + 0x22, 0x32);
        return;
    }
    w16(o + 0x22, frames[how]);
    if (!r16(o + 0x1A)) {
        uint32_t at = og32(OG_OBJECTS) + sxw(r16(o + 0x1C));
        sog32(OG_MOVE_X, r32(at + 4)); sog32(OG_MOVE_Y, r32(at + 8)); sog16(OG_MOVE_CELL, r16(at + 0x30));
        mz_seg_of(7, 0x3784), fn_S7_3784(r);
        g_target(og16(OG_PLAYER_X), og16(OG_PLAYER_Y)); fo_S7_1488(r);
        w16(o + OB_HEADING, (uint16_t)r->d[0]);
    }
    w16(o + 0x1A, 3);
}
/* fn_S10_3B08: the text G_56C0 (a new request, $8000 set, else none: 0); a new one is shown
 * (G_56BE the last, G_56E4 set) and he appears for it; then G_56BC frames of waiting, G_56BA on
 * screen, and G_56B8 = -1: said. */
static void george_speech(Regs *r) {
    sog16(0x56B8, 0);
    uint16_t t = og16(OG_MESSAGE) & 0x8000 ? og16(OG_MESSAGE) & 0x7FFF : 0;
    sog16(OG_MESSAGE, t);
    if (t != og16(0x56BE)) {
        sog16(0x56BE, t);
        set_w(&r->d[0], t); mz_seg(6), fo_S6_0000(r);
        sog8(0x56E4, 1);
        sog16(0x56BC, r16(r->a[0] + 2)); sog16(0x56BA, r16(r->a[0] + 4));
        w16(r->a[4] + 0x1C, r16(r->a[0] + 8));
        george_appear(r, r16(r->a[0] + 6));
    } else if (og16(0x56BC)) sog16(0x56BC, (uint16_t)(og16(0x56BC) - 1));
    else if (!og16(OG_MESSAGE)) return;
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
        set_w(&r->d[0], og16(OG_PLAYER_HEADING) ^ 0x20); george_turn(r);
        if (george_count(r)) w16(o + 0x1A, 6);
        break;
    case 3:
        w16(o + 0x20, 0); g_target(og16(OG_PLAYER_X), og16(OG_PLAYER_Y)); fo_S7_1488(r); george_turn(r);
        if (george_count(r)) w16(o + 0x1A, 4);
        break;
    case 4: w16(o + 0x20, 0x8000); w16(o + 0x26, 5); w16(o + 0x1A, 5); break;
    case 5: case 7: george_hide_shown(r); break;
    case 6:
        w16(o + 0x20, 0x8000); george_at_player(r);
        set_w(&r->d[0], og16(OG_PLAYER_HEADING) ^ 0x20); george_turn(r);
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
        sog16(OG_TARGET_X, path[i]);
        if (path[i] & 0x8000) { r->d[0] = 0xFFFFFFFFu; return -1; }
        sog16(OG_TARGET_Y, path[i + 1]); fo_S7_1488(r);
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
    uint16_t level = og16(OG_LEVEL), cell = og16(OG_PLAYER_CELL);
    int joined = og16(OG_GEORGE_JOINED) != 0;
    if (joined && !og16(OG_MESSAGE)) {
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
#define AREA() (set_w(&r->d[1], og16(OG_PLAYER_X)), set_w(&r->d[2], og16(OG_PLAYER_Y)), mz_seg(7), fo_S7_2038(r), (uint16_t)r->d[0])
        if (AREA() == 0xA) {
            uint32_t c = og32(OG_OBJECTS) + 0x1A;
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
        int full = og16(OG_FULL_VIEW) >= 0x80;                    /* a full-view picture is showing */
        switch (s) {
        case 0: george_hide(r); return;
        case 1: case 2:
            if (full) { next = 0; break; }
            if (og16(OG_PLAYER_X) >= 0x120) { next = 0xF; break; }
            if (s == 2) { near_d0(r); if (!(uint16_t)r->d[0]) return; next = 1; break; }
            if (og16(OG_PLAYER_Y) >= 0xD2 && og16(OG_PLAYER_X) < 0x28) return;
            near_d0(r);
            if (og16(OG_NEAR_DIR) & 0x8000) return;
            set_w(&r->d[0], (uint16_t)(og16(OG_NEAR_DIR) - r16(o + OB_HEADING)));
            mz_seg_of(7, 0x367E), fn_S7_367E(r);
            if ((uint16_t)r->d[0] >= 7 || og16(OG_NEAR_DIST) >= 0x28) return;
            next = 2; break;
        case 3:
            unlink_obj(r);
            w16(o + OB_X, 0); w16(o + OB_Y, 0); w16(o + OB_CELL, 0); w16(o + 0x1A, 0);
            w16(o + 0x1E, 0x14);
            next = 4; break;
        case 4: case 5: case 6:
            if (full) { sog16(OG_GEORGE_JOINED, 0xFFFF); next = 8; break; }
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
        /* his remarks (george_remark chooses them): mostly a text said in the act, here waiting for
         * it to end (o_said) or for a full view (8), some of them leaving when the player leaves
         * their cells (7) */
        case 16:                                            /* level 1: once, after the snails' bit 0 */
            if (!og16(OG_GEORGE_JOINED) || og16(OG_MESSAGE) || !flag(r, 0, 1) || flag(r, 0, 0x40)) { next = 7; break; }
            flag_set(r, 0, 0x40);
            next = flag(r, 0, 0x80) ? 0x12 : 0x11;
            break;
        case 17: case 18: case 21: case 23: case 26: if (!full) return; next = 8; break;
        case 19: case 44: w16(o + 0x18, 6); next = (uint16_t)(s + 1); break;
        case 20: case 45: {                                 /* standing there turned to heading $30 / $10 */
            if (s == 20 ? og16(OG_PLAYER_CELL) != 0x4240 && og16(OG_PLAYER_CELL) != 0x4280 : og16(OG_PLAYER_CELL) != 0x6940) { next = 7; break; }
            int16_t t = (int16_t)r16(o + 0x18);
            if (t >= 0) { w16(o + 0x18, (uint16_t)(t - 1)); return; }
            set_w(&r->d[0], (uint16_t)(og16(OG_PLAYER_HEADING) - (s == 20 ? 0x30 : 0x10)));
            mz_seg_of(7, 0x367E), fn_S7_367E(r);
            if ((uint16_t)r->d[0] >= 0xC) return;
            if (s == 20) flag_set(r, 0, 0x100); else flag_set(r, 1, 8);
            next = s == 20 ? 0x15 : 0x2E;
            break;
        }
        case 22: next = og16(OG_PLAYER_CELL) == 0x4240 || og16(OG_PLAYER_CELL) == 0x4280 ? 0x17 : 7; break;
        case 24: flag_set(r, 0, 0x200); w16(o + 0x18, 0x2D); next = 0x19; break;
        case 25: {
            int16_t t = (int16_t)(r16(o + 0x18) - 1);
            w16(o + 0x18, (uint16_t)t);
            if (t >= 0) return;
            uint16_t c = og16(OG_PLAYER_CELL);
            next = c == 0x1FE0 || c == 0x1FC0 || c == 0x1BE0 ? 0x1A : 0x1B;
            break;
        }
        case 27: return;
        case 28: case 29: case 30: case 31: case 32: case 33: case 46: case 51:
            if (full) { next = 8; break; }
            if (george_cell_ends()) { next = 7; break; }
            if (s == 29) {
                if (og16(OG_PLAYER_CELL) == 0x50E0) { next = 0x1E; break; }
                if (og16(OG_PLAYER_CELL) == 0x60E0) { flag_set(r, 0, 0x1000); next = 7; break; }
            }
            if (s == 31 && (og16(OG_PLAYER_CELL) == 0x2B40 || og16(OG_PLAYER_CELL) == 0x2BC0)) { next = 0x20; break; }
            return;
        case 34: case 38: case 40: {                        /* walking to a point: his part in the story */
            static const uint16_t to[41][3] = {[34] = {0x1B8, 0x178, 2}, [38] = {0x1A8, 0x188, 2}, [40] = {0x188, 0x188, 4}};
            sog16(OG_MOVE_X, to[s][0]); sog16(OG_MOVE_Y, to[s][1]);
            mz_seg(7), fo_S7_155C(r);
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            if ((uint16_t)r->d[0]) return;
            flag_set(r, 1, to[s][2]);
            w16(o + 0x18, 0xFFFF);
            next = (uint16_t)(s + 1);
            break;
        }
        case 35: case 39: case 41: {                        /* there, until the player leaves */
            uint16_t c = og16(OG_PLAYER_CELL);
            int here = s == 35 ? c == 0x5F60 : s == 39 ? c == 0x6360 || c == 0x6340 || c == 0x6320 : c == 0x6300 || c == 0x5F00;
            if (!full && !george_cell_ends() && !here) return;
            next = 0x24;
            break;
        }
        case 36: w16(o + 0x18, 2); next = 0x25; break;
        case 37:
            w16(o + 0x18, (uint16_t)(r16(o + 0x18) - 1));
            if (r16(o + 0x18)) return;
            unlink_obj(r);
            w16(o + OB_X, 0); w16(o + 6, 0); w16(o + OB_Y, 0); w16(o + 0xA, 0); w16(o + OB_CELL, 0);
            next = 7;
            break;
        case 42: if (!george_cell_ends()) return; next = 0x2B; break;
        case 43: next = 7; break;
        case 47: case 48: case 49: case 50: {               /* level 5: while the player stays in the area */
            static const uint8_t area[4] = {0xA, 0x11, 0xF, 0xE};
            set_w(&r->d[1], og16(OG_PLAYER_X)); set_w(&r->d[2], og16(OG_PLAYER_Y)); mz_seg(7), fo_S7_2038(r);
            if ((uint16_t)r->d[0] == area[s - 47]) return;
            next = 7;
            break;
        }
        default: return;
        }
        set_w(&r->d[0], next);
    }
}
static void george_act(Regs *r) {
    uint16_t s = (uint16_t)r->d[7];
    switch (s) {
    case 1: g_ring_at(r, 0x1E); g_ring_at(r, 0x1E); break;
    case 2: {
        uint16_t t = og16(OG_MESSAGE) & 0x7FFF;
        if (t != 0x1401 && t != 0x141B) {
            uint32_t cat = og32(OG_OBJECTS) + 0x140;
            g_target(r16(cat + 4), r16(cat + 8)); fo_S7_14C0(r);
            t = (uint16_t)r->d[0] < 0xC ? 0x141B : 0x1401;
        }
        say(t);
        g_ring_at(r, 0x1E); g_ring_at(r, 0x1E);
        break;
    }
    case 5: say(0x1402); break;
    case 6: say(0x1404); sog16(OG_GEORGE_JOINED, 0xFFFF); break;
    case 8: george_hide_shown(r); break;
    case 10: case 11:                                       /* turning to the player */
        near_d0(r);
        if (!(og16(OG_NEAR_DIR) & 0x8000)) { set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r); fn_S7_3722(r); }
        if (s == 10) say(0x1403);
        break;
    case 12: say(0x1403); break;
    case 22: if (og16(OG_FULL_VIEW) >= 0x80) { set_w(&r->d[0], 8); george_think(r); } break;   /* the original's act is think 23's code */
    case 28: flag_set(r, 0, 0x400); say(0x140B); break;
    case 29: flag_set(r, 0, 0x800); say(0x140C); break;
    case 30: flag_set(r, 0, 0x1000); say(0x140D); break;
    case 31: flag_set(r, 0, 0x2000); say(0x140E); break;
    case 32: flag_set(r, 0, 0x4000); say(0x140F); break;
    case 33: flag_set(r, 1, 1); say(0x1410); break;
    case 35: case 39: case 41: {                            /* turned to the player, shown on the second frame */
        uint32_t o = r->a[4];
        w16(o + 0x20, 0x8000);
        w16(o + 0x18, (uint16_t)(r16(o + 0x18) + 1));
        if (r16(o + 0x18)) w16(o + 0x20, 0);
        say(s == 35 ? 0x1411 : s == 39 ? 0x1412 : 0x1413);
        near_d0(r);
        if (!(og16(OG_NEAR_DIR) & 0x8000)) w16(o + OB_HEADING, og16(OG_NEAR_DIR));
        break;
    }
    case 37: w16(r->a[4] + 0x20, 0x8000); break;
    case 51: flag_set(r, 1, 0x200); say(0x141A); break;
    default: {
        static const uint16_t text[51] = {[17] = 0x1405, [18] = 0x1406, [21] = 0x1407, [23] = 0x1408, [26] = 0x1409,
            [27] = 0x140A, [42] = 0x1414, [46] = 0x1415, [47] = 0x1416, [48] = 0x1417, [49] = 0x1418, [50] = 0x1419};
        if (s < 51 && text[s]) say(text[s]);
        break;
    }
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
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    george_think(r);
    uint16_t s = (uint16_t)r->d[7];
    if (s < 52) o_said(r, said[s], george_think);
    s = (uint16_t)r->d[7];
    o_event(r, (s >= 1 && s <= 2) || (s >= 10 && s <= 13) ? ev : s == 7 ? ev7 : NULL, george_think);
    george_act(r);
    george_speech(r);
    george_showing(r);
    w16(o + OB_STATE, (uint16_t)r->d[7]);
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
    sog16(OG_GO_LEVEL, level);
    sog32(OG_GO_X, x + r32(steps + 0x40));
    sog32(OG_GO_Y, y + r32(steps));
    sog16(OG_GO_HEADING, og16(OG_PLAYER_HEADING)); sog8(0x57DE, 1); sog8(0x57F2, 1);
}
void fo_S7_007C(Regs *r) {
    static const struct { uint16_t level; uint32_t x, y; } to[7] = {
        {2, 0xF80000, 0x1E80000}, {1, 0x1680000, 0x1480000}, {1, 0, 0}, {5, 0x480000, 0x480000},
        {4, 0xF80000, 0xC80000}, {2, 0x1B80000, 0x480000}, {2, 0xF80000, 0x1580000}};
    uint32_t o = r->a[4];
    int16_t s = (int16_t)r16(o + OB_STATE);
    if (!r16(o + 6)) {
        if (r16(o + OB_CELL) != og16(OG_PLAYER_CELL)) return;
        mz_seg_of(7, 0x3128), fn_S7_3128(r);
        if (og16(OG_NEAR_DIST) >= 5) return;
        w16(o + 6, s >= 0xA ? 0xA : 0x14);
    }
    if (s == 2) { sog16(OG_LIFE, heal(og16(OG_LIFE), 3)); sog16(0x56E6, heal(og16(0x56E6), 6)); }
    uint32_t pics = OADDR(r16(o + OB_TYPE) == 0x10 ? 0x3840 : 0x384C);
    int16_t c = (int16_t)(r16(o + OB_HEADING) + 1);
    if (c >= 6) c = 0;
    w16(o + OB_HEADING, (uint16_t)c);
    sog16(0x56DE, r16(pics + 2 * (uint32_t)c));
    sog16(OG_FULL_VIEW, 0x80);
    uint16_t t = (uint16_t)(r16(o + 6) - 1);
    w16(o + 6, t);
    if (t) return;
    g_target(og16(OG_PLAYER_X), og16(OG_PLAYER_Y));
    fo_S7_1488(r);
    uint16_t dir = (uint16_t)r->d[0];
    if (s >= 0xA) {
        uint16_t cell = r16(OADDR(0x5248) + sxw((uint16_t)((((uint16_t)(s - 0xA)) ^ 1) << 1)));
        uint32_t map = og32(OG_MAP) + sxw(cell);
        gate_go(5, (uint32_t)r16(map + 0x1C) << 16, (uint32_t)r16(map + 0x1E) << 16, dir);
    } else if (s == 2) {
        sog16(OG_GO_LEVEL, 1); sog32(OG_GO_X, 0x580000); sog32(OG_GO_Y, 0x500000);
        sog16(OG_GO_HEADING, og16(OG_PLAYER_HEADING)); sog8(0x57DE, 1); sog8(0x57F2, 1);
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
 * for the player. Hit by type $18 in states 0, 1, 9 and 10: 2. In 10 (the player near again) it
 * follows him, marking him as in 1 (and record 3's +$20 = 2): beyond $A, by the cells when it
 * cannot see him (fo_S7_2132), straight at him when it can (fn_S7_1FF8 the step); otherwise it
 * turns to him. */
void fo_S7_2132(Regs *r), fo_S7_1FF8(Regs *r), fo_S7_226E(Regs *r);
static void t30_think(Regs *r) {
    uint32_t o = r->a[4], map = og32(OG_MAP), tbl = og32(OG_OBJECTS);
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t next;
        switch ((uint16_t)r->d[7]) {
        case 0: near_d0(r); if ((uint16_t)r->d[0]) return; next = 1; break;
        case 1: near_d0(r); if (og16(OG_NEAR_DIST) < 0x20) return; next = 2; break;
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
            w16(o + 0x10, r16(o + OB_CELL));
            w16(o + OB_X, 0); w16(o + OB_Y, 0); w16(o + OB_CELL, 0); w16(o + 0xE, 0);
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
            sog16(OG_MOVE_CELL, cell);
            sog16(OG_MOVE_X, r16(map + sxw(cell) + 0x1C)); sog16(OG_MOVE_Y, r16(map + sxw(cell) + 0x1E));
            if (r16(map + sxw(cell) + 0xE)) return;
            mz_seg_of(7, 0x3972), fn_S7_3972(r); if ((uint16_t)r->d[0]) return;
            sog16(0x3822, 5);
            fn_S7_3ACA(r); if ((uint16_t)r->d[0]) return;
            fn_S7_3784(r); if ((uint16_t)r->d[0]) return;
            next = 7; break;
        }
        case 7:
            w16(o + 0xE, r16(o + OB_CELL));
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
    uint32_t o = r->a[4], tbl = og32(OG_OBJECTS);
    r->a[2] = tbl;
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t30_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_hit(r, s <= 1 || s == 9 || s == 10 ? hits : NULL, t30_think);
    switch ((uint16_t)r->d[7]) {
    case 0: case 9: w16(tbl + 0x98, 0); break;
    case 1:
        set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r); fn_S7_3722(r);
        w16(tbl + 0x98, 1);
        w16(tbl + 0xDA, og16(OG_PLAYER_CELL)); w16(tbl + 0xDE, og16(OG_PLAYER_CELL));
        set_w(&r->d[1], og16(OG_PLAYER_X)); set_w(&r->d[2], og16(OG_PLAYER_Y)); mz_seg(7), fo_S7_2038(r);
        w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
        break;
    case 3: w16(o + 0x3A, 0x7E); break;
    case 8: w16(o + 0x3A, r16(o + 0x18) ? 0 : 0x81); break;
    case 10:                                                /* following the player */
        w16(tbl + 0x98, 1);
        w16(tbl + 0xDA, og16(OG_PLAYER_CELL)); w16(tbl + 0xDE, og16(OG_PLAYER_CELL));
        w16(tbl + 0xE0, 2);
        set_w(&r->d[1], og16(OG_PLAYER_X)); set_w(&r->d[2], og16(OG_PLAYER_Y)); mz_seg(7), fo_S7_2038(r);
        w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
        if (og16(OG_NEAR_DIST) >= 0xA) {
            mz_seg(7), fo_S7_226E(r);
            if (!(uint16_t)r->d[0]) {                       /* out of sight: the way by the cells */
                sog16(0x3814, og16(OG_PLAYER_CELL));
                mz_seg(7), fo_S7_2132(r);
            } else {                                        /* in sight: straight at him */
                sog16(OG_TARGET_X, og16(OG_PLAYER_X)); sog16(OG_TARGET_Y, og16(OG_PLAYER_Y));
                mz_seg_of(7, 0x3B36), fn_S7_3B36(r);
                uint16_t d = (uint16_t)r->d[0];
                mz_seg_of(7, 0x3D0E), fn_S7_3D0E(r);
                set_w(&r->d[0], d);
            }
            if (!((uint16_t)r->d[0] & 0x8000)) { mz_seg(7), fo_S7_1FF8(r); break; }
        }
        set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r);
        break;
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $31 (fn_S4_2AF0, level 5) ----------
 * Waits (0) until record 3 has its +$22 set and an area (+$1C), then (1) roams: in the area
 * record 3 names it heads for record 3's cell (+$1E; reached: both cleared), elsewhere for the
 * passage between its area and that one (the cell G_5248 lists by G_51E0) and through it; with
 * record 3's +$20 set and fn_S7_226E seeing the player (2) it chases him, and at distance < 2
 * takes 1 from his life (G_56EA) and gets a random +$26. Hit by types $18 / $2E in states 0-2:
 * 3: it marks the player's place for record 3 as type $30 does, shows picture $80 for a frame
 * (4), is gone from the map for $4B frames (5-6), waits for record 3's +$24 to clear (7) and comes
 * back in a free cell of the eight G_4F40 lists (8; +$24 = $14 again), then 0. */
void fo_S7_226E(Regs *r);
/* the distance-map index of map cell c from the object (G_53BC / G_53C0 are 128 x 128 maps) */
static uint16_t t31_index(uint32_t o, uint32_t map, uint16_t c) {
    uint16_t y = (uint16_t)(((r16(map + sxw(c) + 0x1E) - r16(o + OB_Y)) & 0x7F) << 7);
    return (uint16_t)(y | ((r16(map + sxw(c) + 0x1C) - r16(o + OB_X)) & 0x7F));
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
    uint32_t o = r->a[4], map = og32(OG_MAP);
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
    uint32_t o = r->a[4], map = og32(OG_MAP), dist = og32(0x53BC);
    uint16_t at = r16(o + 0x12), i = t31_index(o, map, at);
    if (r8(dist + sxw(i)) >= 2) { r->d[0] = r8(og32(0x53C0) + sxw(i)); return; }
    uint16_t to = og16(0x3814);
    if (to == at) { r->d[0] = 0xFFFFFFFEu; return; }
    sog16(OG_TARGET_X, r16(map + sxw(to) + 0x1C)); sog16(OG_TARGET_Y, r16(map + sxw(to) + 0x1E));
    uint16_t best = 0xFFFF, bcell = 0xFFFF, c = at;
    for (int k = 0; k < 4; k++) {
        c = (uint16_t)(c + t31_steps[k]);
        if (r8(map + sxw(c)) & 1) continue;
        uint16_t y = (uint16_t)((((uint16_t)(r16(map + sxw(c) + 0x1E) - og16(OG_TARGET_Y)) >> 1) & 0x7F) << 7);
        uint16_t x = (uint16_t)(((uint16_t)(r16(map + sxw(c) + 0x1C) - og16(OG_TARGET_X)) >> 1) & 0x7F);
        uint16_t w = (uint16_t)(r8(dist + sxw((uint16_t)(y | x))) | (c == r16(o + 0x10) ? 0x100 : 0));
        if (w < best) { best = w; bcell = c; }
    }
    w16(o + 0x10, at); w16(o + 0x12, bcell);
    r->d[0] = r8(og32(0x53C0) + sxw(t31_index(o, map, bcell)));
}
static int t31_sees(Regs *r) { mz_seg(7), fo_S7_226E(r); return (uint16_t)r->d[0] != 0; }
static void t31_think(Regs *r) {
    uint32_t tbl = og32(OG_OBJECTS);
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
        case 3: {                                           /* hit: marks the player for record 3 */
            uint16_t c = og16(OG_PLAYER_CELL);
            w16(r->a[4] + 0x18, 0xFFFF);
            w16(tbl + 0xDA, c); w16(tbl + 0xDE, c);
            set_w(&r->d[1], og16(OG_PLAYER_X)); set_w(&r->d[2], og16(OG_PLAYER_Y)); mz_seg(7), fo_S7_2038(r);
            w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
            next = 4; break;
        }
        case 4: {                                           /* a frame of picture $80 */
            uint16_t t = (uint16_t)(r16(r->a[4] + 0x18) + 1);
            w16(r->a[4] + 0x18, t);
            if (t < 1) return;
            next = 5; break;
        }
        case 5: {                                           /* gone from the map for $4B frames */
            uint32_t o = r->a[4];
            unlink_obj(r);
            w16(o + OB_X, 0); w16(o + OB_Y, 0); w16(o + OB_CELL, 0);
            w16(o + 0x18, 0x4B);
            next = 6; break;
        }
        case 6: {
            int16_t t = (int16_t)(r16(r->a[4] + 0x18) - 1);
            w16(r->a[4] + 0x18, (uint16_t)t);
            if (t >= 0) return;
            next = 7; break;
        }
        case 7: if (r16(tbl + 0xE4)) return; next = 8; break;   /* until record 3's +$24 runs out */
        case 8: {                                           /* back in one of the eight cells (G_4F40), at random */
            uint32_t map = og32(OG_MAP);
            random_d0(r);
            uint16_t c = r16(OADDR(0x4F40) + 2 * ((uint16_t)r->d[0] & 7));
            w16(r->a[4] + 0x12, c);
            sog16(OG_MOVE_CELL, c);
            sog16(OG_MOVE_X, r16(map + sxw(c) + 0x1C)); sog16(OG_MOVE_Y, r16(map + sxw(c) + 0x1E));
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            if ((uint16_t)r->d[0]) return;
            w16(tbl + 0xE4, 0x14);
            next = 0; break;
        }
        default: return;
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_2AF0(Regs *r) {
    static const uint16_t hits[] = {0x18, 3, 0x2E, 3, 0xFFFF};
    uint32_t o = r->a[4], tbl = og32(OG_OBJECTS);
    r->a[2] = tbl;
    set_w(&r->d[7], r16(o + OB_STATE));
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
        set_w(&r->d[1], r16(o + OB_X)); set_w(&r->d[2], r16(o + OB_Y)); mz_seg(7), fo_S7_2038(r);
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
                sog16(OG_MOVE_CELL, c);
                uint32_t map = og32(OG_MAP);
                sog16(OG_MOVE_X, r16(map + sxw(c) + 0x1C)); sog16(OG_MOVE_Y, r16(map + sxw(c) + 0x1E));
                mz_seg_of(7, 0x3784), fn_S7_3784(r);
                if (!(uint16_t)r->d[0]) w16(o + 0x12, og16(OG_MOVE_CELL));
                break;
            }
            t31_step(r);
        }
        break;
    }
    case 2:
        w16(o + 0x12, og16(OG_PLAYER_CELL));
        for (int k = 0; k < 3; k++) { near_d0(r); set_w(&r->d[0], og16(OG_NEAR_DIR)); t31_step(r); }
        if (og16(OG_NEAR_DIST) < 2) {
            int16_t l = (int16_t)(og16(OG_LIFE) - 1);
            sog16(OG_LIFE, (uint16_t)(l < 0 ? 0 : l));
            random_d0(r); w16(o + 0x26, (uint16_t)r->d[0] & 1);
        }
        w16(o + 0x3A, 0x7D);
        break;
    case 4: w16(o + 0x3A, 0x80); break;
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $1F (fn_S4_0C44, level 5) ----------
 * Roams the map (fn_S4_1096: on to a free neighbouring cell at random, the last one less likely;
 * standing on a gate's cell (type $B / $10, state $A or more), one time in two through it to the
 * paired gate's cell, G_5248) until it sees the player close (fn_S7_1334: distance < 6, or
 * facing him within $11): 2, chasing him (marking him for record 3: +$18 / +$1C his area, +$1A /
 * +$1E his cell, +$20 = 2) until he is out of sight: 3, which notes where he was, and 1: following
 * record 3's marks, through the gates between areas (G_51E0 / G_5248) to his last cell; there,
 * with no trace (record 3's +$18 cleared, or +$22), back to 0. Hit by types $18 / $2E in states
 * 0-2: 4, marking the player as in 2; 5 two frames of pictures $7E, $7F; 6-7 gone from the map for
 * $96 frames; 8 back, once record 3's +$24 is clear, in a free cell of the eight G_4F40 lists
 * (+$24 = 5); 9-10 picture $81, then 0. */
void fo_S7_201C(Regs *r), fo_S7_2132(Regs *r), fo_S7_1FF8(Regs *r);
/* the gate record in cell c's object list (its first object), if of type $B / $10 in state $A+ */
static int t1f_gate(uint16_t c, uint16_t *state) {
    uint8_t n = r8(og32(OG_MAP) + sxw(c) + 1);
    if (!n) return 0;
    uint32_t g = og32(OG_OBJECTS) + ((uint32_t)n << 6);
    if (r16(g + 2) != 0xB && r16(g + 2) != 0x10) return 0;
    *state = r16(g + 0x16);
    return *state >= 0xA;
}
void hx_log(const char *f, ...);
static void t1f_at(uint16_t c) {                            /* G_3824 / G_382C / G_3828: cell c and its centre */
    uint32_t map = og32(OG_MAP);
    sog16(OG_MOVE_CELL, c); sog16(OG_MOVE_X, r16(map + sxw(c) + 0x1C)); sog16(OG_MOVE_Y, r16(map + sxw(c) + 0x1E));
}
/* moved into cell c if it is free (fn_S7_1766 / 1578), taking it over in the map; then fn_S7_1CCC */
static void t1f_through(Regs *r, uint16_t c) {
    uint32_t o = r->a[4], map = og32(OG_MAP);
    t1f_at(c);
    if (r16(map + sxw(c) + 0xE)) return;
    mz_seg_of(7, 0x3972), fn_S7_3972(r); if ((uint16_t)r->d[0]) return;
    fn_S7_3784(r); if ((uint16_t)r->d[0]) return;
    w16(map + sxw(r16(o + 0xE)) + 0xE, 0);
    w16(o + 0xE, og16(OG_MOVE_CELL));
    w16(map + sxw(og16(OG_MOVE_CELL)) + 0xE, 1);
    mz_seg_of(7, 0x3EE2), fn_S7_3EE2(r);
}
/* fn_S4_1096: D0.w = the direction to the target cell +$E; there, the next one (see above) */
static void t1f_route(Regs *r) {
    static const int16_t step[4] = {-0x400, 0x3E0, 0x40, 0x3E0};
    static const uint16_t dir[4] = {0x10, 0, 0x20, 0x30};
    uint32_t o = r->a[4], map = og32(OG_MAP);
    uint16_t at = r16(o + 0xE), st;
    int16_t dx = (int16_t)(r16(map + sxw(at) + 0x1C) - r16(o + OB_X)), dy = (int16_t)(r16(map + sxw(at) + 0x1E) - r16(o + OB_Y));
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
                if (!(uint16_t)r->d[0]) { fn_S7_3784(r); w16(o + 0xE, og16(OG_MOVE_CELL)); }
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
    if (og16(OG_NEAR_DIST) & 0x8000) return 0;
    if (og16(OG_NEAR_DIST) < 6) return 1;
    set_w(&r->d[0], (uint16_t)(og16(OG_NEAR_DIR) - r16(r->a[4] + OB_HEADING)));
    mz_seg_of(7, 0x367E), fn_S7_367E(r);
    return (uint16_t)r->d[0] < 0x11;
}
static void mark_player(Regs *r) {                          /* record 3: the player's cell and area */
    uint32_t tbl = og32(OG_OBJECTS);
    w16(tbl + 0xDA, og16(OG_PLAYER_CELL)); w16(tbl + 0xDE, og16(OG_PLAYER_CELL));
    set_w(&r->d[1], og16(OG_PLAYER_X)); set_w(&r->d[2], og16(OG_PLAYER_Y)); mz_seg(7), fo_S7_2038(r);
    w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
}
static void t1f_think(Regs *r) {
    uint32_t o = r->a[4], tbl = og32(OG_OBJECTS), map = og32(OG_MAP);
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
            if (!(og16(OG_NEAR_DIST) & 0x8000)) return;
            next = 3; break;
        case 3: mark_player(r); w16(r->a[4] + 0x10, 0); next = 1; break;
        case 4: {                                           /* hit: marks the player, as type $31 does */
            uint16_t c = og16(OG_PLAYER_CELL);
            w16(o + 0x18, 0xFFFF);
            w16(tbl + 0xDA, c); w16(tbl + 0xDE, c);
            set_w(&r->d[1], og16(OG_PLAYER_X)); set_w(&r->d[2], og16(OG_PLAYER_Y)); mz_seg(7), fo_S7_2038(r);
            w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
            next = 5; break;
        }
        case 5: {                                           /* two frames of pictures $7E, $7F */
            uint16_t t = (uint16_t)(r16(o + 0x18) + 1);
            w16(o + 0x18, t);
            if (t < 2) return;
            next = 6; break;
        }
        case 6:                                             /* gone from the map for $96 frames */
            w16(map + sxw(r16(o + 0xE)) + 0xE, 0);
            unlink_obj(r);
            w16(o + 0x10, r16(o + OB_CELL));
            w16(o + OB_X, 0); w16(o + OB_Y, 0); w16(o + OB_CELL, 0); w16(o + 0xE, 0);
            w16(o + 0x18, 0x96);
            next = 7; break;
        case 7: {
            int16_t t = (int16_t)(r16(o + 0x18) - 1);
            w16(o + 0x18, (uint16_t)t);
            if (t >= 0) return;
            next = 8; break;
        }
        case 8: {                                           /* back in a free cell of the eight (G_4F40) */
            if (r16(tbl + 0xE4)) return;
            random_d0(r);
            uint16_t c = r16(OADDR(0x4F40) + 2 * ((uint16_t)r->d[0] & 7));
            set_w(&r->d[2], c);
            sog16(OG_MOVE_CELL, c);
            sog16(OG_MOVE_X, r16(map + sxw(c) + 0x1C)); sog16(OG_MOVE_Y, r16(map + sxw(c) + 0x1E));
            if (r16(map + sxw(c) + 0xE)) return;
            mz_seg_of(7, 0x3972), fn_S7_3972(r); if ((uint16_t)r->d[0]) return;
            sog16(0x3822, 5);
            fn_S7_3ACA(r); if ((uint16_t)r->d[0]) return;
            fn_S7_3784(r); if ((uint16_t)r->d[0]) return;
            next = 9; break;
        }
        case 9:
            w16(o + 0xE, r16(o + OB_CELL));
            w16(map + sxw(r16(o + 0xE)) + 0xE, 1);
            w16(o + 0x10, 0); w16(o + 0x18, 0xFFFF);
            w16(tbl + 0xE4, 5);
            next = 10; break;
        case 10: {
            uint16_t t = (uint16_t)(r16(o + 0x18) - 1);
            w16(o + 0x18, t);
            if (t < 2) return;
            next = 0; break;
        }
        default: return;
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_0C44(Regs *r) {
    static const uint16_t hits[] = {0x18, 4, 0x2E, 4, 0xFFFF};
    uint32_t o = r->a[4], tbl = og32(OG_OBJECTS);
    r->a[2] = tbl;
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t1f_think(r);
    o_hit(r, (uint16_t)r->d[7] <= 2 ? hits : NULL, t1f_think);
    uint16_t s = (uint16_t)r->d[7];
    switch (s) {
    case 0: t1f_route(r); mz_seg(7), fo_S7_201C(r); break;
    case 1: {
        set_w(&r->d[1], r16(o + OB_X)); set_w(&r->d[2], r16(o + OB_Y)); mz_seg(7), fo_S7_2038(r);
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
        w16(tbl + 0xDA, og16(OG_PLAYER_CELL)); w16(tbl + 0xDE, og16(OG_PLAYER_CELL));
        w16(tbl + 0xE0, 2);
        g_target(og16(OG_PLAYER_X), og16(OG_PLAYER_Y));
        set_w(&r->d[1], og16(OG_PLAYER_X)); set_w(&r->d[2], og16(OG_PLAYER_Y)); mz_seg(7), fo_S7_2038(r);
        w16(tbl + 0xD8, (uint16_t)r->d[0]); w16(tbl + 0xDC, (uint16_t)r->d[0]);
        if (og16(OG_NEAR_DIST) >= 0xA) {
            mz_seg_of(7, 0x3B36), fn_S7_3B36(r); fn_S7_3D0E(r);
            set_w(&r->d[0], og16(0x3812));
            if (!(og16(0x3812) & 0x8000)) { mz_seg(7), fo_S7_1FF8(r); break; }
        }
        set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r);
        break;
    case 5: { static const uint16_t pic[2] = {0x7E, 0x7F}; w16(o + 0x3A, pic[r16(o + 0x18)]); break; }
    case 10: { static const uint16_t pic[2] = {0x81, 0}; w16(o + 0x3A, pic[r16(o + 0x18)]); break; }
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* types $1E and $20 when hit: off the map for a while (its cell kept in +$10, count in field f),
 * then back in that cell once it is free (G_3822 = 6), then on the map again */
static void off_map(Regs *r, uint32_t f, uint16_t frames) {
    uint32_t o = r->a[4], map = og32(OG_MAP);
    w16(map + sxw(r16(o + 0xE)) + 0xE, 0);
    unlink_obj(r);
    w16(o + 0x10, r16(o + OB_CELL));
    w16(o + OB_X, 0); w16(o + 6, 0); w16(o + OB_Y, 0); w16(o + 0xA, 0); w16(o + OB_CELL, 0); w16(o + 0xE, 0);
    w16(o + f, frames);
}
static int back_in_cell(Regs *r) {                          /* 1 once it is there */
    uint32_t map = og32(OG_MAP);
    uint16_t c = r16(r->a[4] + 0x10);
    sog16(OG_MOVE_X, r16(map + sxw(c) + 0x1C)); sog16(OG_MOVE_Y, r16(map + sxw(c) + 0x1E));
    sog16(OG_MOVE_CELL, c);
    if (r16(map + sxw(c) + 0xE)) return 0;
    mz_seg_of(7, 0x3972), fn_S7_3972(r); if ((uint16_t)r->d[0]) return 0;
    sog16(0x3822, 6);
    fn_S7_3ACA(r); if ((uint16_t)r->d[0]) return 0;
    fn_S7_3784(r);
    return 1;
}
static void on_map(Regs *r, uint32_t f) {
    uint32_t o = r->a[4];
    w16(o + 0xE, r16(o + OB_CELL));
    w16(og32(OG_MAP) + sxw(r16(o + 0xE)) + 0xE, 1);
    w16(o + 0x10, 0); w16(o + f, 0xFFFF);
}
static int count_down(Regs *r, uint32_t f, uint16_t by) {  /* the field less `by`: 1 once negative */
    int16_t t = (int16_t)(r16(r->a[4] + f) - by);
    w16(r->a[4] + f, (uint16_t)t);
    return t < 0;
}
static int count_up(Regs *r, uint32_t f, uint16_t to) {    /* the field plus 1: 1 once `to` (unsigned) */
    uint16_t t = (uint16_t)(r16(r->a[4] + f) + 1);
    w16(r->a[4] + f, t);
    return t >= to;
}
static void turn_to_player(Regs *r, int times) {
    near_d0(r);
    if (og16(OG_NEAR_DIR) & 0x8000) return;
    set_w(&r->d[0], og16(OG_NEAR_DIR));
    mz_seg_of(7, 0x3722);
    while (times--) fn_S7_3722(r);
}

/* ---------- type $1E (fn_S4_0782, level 4) ----------
 * A hunter: roams (0, fn_S7_1CCC's way, two steps a frame while facing it) until it sees the player
 * close (fn_S7_1334: distance < 5, or in front within $14): 3, where it closes in along his scent
 * (fn_S7_1B46) and, near him (fn_S7_1B1E) or without a way, turns to him and within 6 of facing
 * him fires (fn_S7_1EC0, $3D) with a pause of $22-$31 frames (+$1A, 4: walking off meanwhile,
 * the pause shortened by 4 when it has no way); out of sight: 5, then 2, following the scent in its
 * cell (map +$10) for $14 steps (+$1C) or until it ends. The player moving to a new cell while it
 * chases is noted for the scent (G_56A6 / G_56A8 = $23, fn_S2_2B3E). Hit by type $18 in states 0,
 * 2, 3, 4 and 12: 6: pictures $77, $78 (7), gone from the map for 750 frames (8-9, faster under a
 * full view), back in its old cell once free (10-11) and 24 frames of pictures $79, $7A, 0 (12),
 * then 4. State 1 (no test reaches it from play) leads by 13 (turning to the player) or 14 to 15,
 * picture $78 rushing at him (fn_S7_1E66). */
void fo_S7_1E54(Regs *r);
static void t1e_step(Regs *r) { r->a[0] = OADDR(0x4F30); mz_seg(7), fo_S7_1E54(r); }
static void t1e_dir_roam(Regs *r) { mz_seg_of(7, 0x3EE2), fn_S7_3EE2(r); }
static void t1e_dir_scent(Regs *r) {                        /* fn_S7_1B46, then fn_S7_1B02 (D0 kept) */
    mz_seg_of(7, 0x3D5C), fn_S7_3D5C(r);
    uint32_t d = r->d[0];
    fn_S7_3D0E(r);
    set_w(&r->d[0], (uint16_t)d);
}
/* a direction from dir(): if it is not its heading, it turns (fn_S7_1516) and steps; if it is,
 * it steps and, still facing the next direction, steps again. 0, or -1 if there is none. */
static int t1e_walk(Regs *r, void (*dir)(Regs *)) {
    dir(r);
    if ((uint16_t)r->d[0] & 0x8000) return -1;
    if ((uint16_t)r->d[0] != r16(r->a[4] + OB_HEADING)) { mz_seg_of(7, 0x3722), fn_S7_3722(r); t1e_step(r); return 0; }
    t1e_step(r);
    dir(r);
    if ((uint16_t)r->d[0] == r16(r->a[4] + OB_HEADING)) t1e_step(r);
    return 0;
}
static void t1e_scent_reset(Regs *r) {
    sog16(0x56A8, 0); sog16(0x56A6, 0);
    mz_seg_of(2, 0x366A), fn_S2_366A(r); fn_S2_3568(r);    /* fn_S2_2B3E */
}
static void t1e_think(Regs *r) {
    uint32_t o = r->a[4], map = og32(OG_MAP);
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t next;
        switch ((uint16_t)r->d[7]) {
        case 0:
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (!(og16(OG_NEAR_DIST) & 0x8000)) {
                if (og16(OG_NEAR_DIST) < 5) { next = 3; break; }
                set_w(&r->d[0], (uint16_t)(og16(OG_NEAR_DIR) - r16(o + OB_HEADING)));
                mz_seg_of(7, 0x367E), fn_S7_367E(r);
                if ((uint16_t)r->d[0] < 0x14) { next = 3; break; }
            }
            if (!r16(map + sxw(r16(o + OB_CELL)) + 0x10)) return;
            next = 5; break;
        case 2: {
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (!(uint16_t)r->d[0]) { next = 3; break; }
            uint16_t sc = r16(map + sxw(r16(o + OB_CELL)) + 0x10);
            if (!sc) { next = 0; break; }
            if (sc != 0xFFFF && !(r16(o + 0x1C) & 0x8000)) return;
            t1e_scent_reset(r);
            next = 0; break;
        }
        case 3:
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if ((uint16_t)r->d[0]) { next = 5; break; }
            if (!r16(o + 0x1A)) return;
            next = 4; break;
        case 4: if (r16(o + 0x1A)) return; next = 0; break;
        case 5: w16(o + 0x1C, 0x14); next = 2; break;
        case 1:                                             /* (no picture: 13, else 14) */
            if (!r16(o + 0x3A)) { w16(o + 0x1C, 7); next = 0xD; } else { w16(o + 0x1C, 5); next = 0xE; }
            break;
        case 6: w16(o + 0x1C, 0xFFFF); next = 7; break;
        case 7: if (!count_up(r, 0x1C, 2)) return; next = 8; break;   /* hit: two frames of pictures $77, $78 */
        case 8: off_map(r, 0x1C, 0x2EE); next = 9; break;  /* gone for 750 frames */
        case 9: if (!count_down(r, 0x1C, og16(OG_FULL_VIEW) >= 0x80 ? 100 : 1)) return; next = 0xA; break;   /* faster under a full view */
        case 10: if (!back_in_cell(r)) return; next = 0xB; break;
        case 11: on_map(r, 0x1C); next = 0xC; break;
        case 12: if (!count_up(r, 0x1C, 0x18)) return; next = 4; break;   /* 24 frames of pictures $79, $7A, 0 */
        case 13: if (!count_down(r, 0x1C, 1)) return; w16(o + 0x1C, 5); next = 0xE; break;
        case 14: if (!count_down(r, 0x1C, 1)) return; next = 0xF; break;
        default: return;                                    /* 15 */
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_0782(Regs *r) {
    static const uint16_t hits[] = {0x18, 6, 0xFFFF};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t1e_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_hit(r, s == 0 || (s >= 2 && s <= 4) || s == 12 ? hits : NULL, t1e_think);
    s = (uint16_t)r->d[7];
    switch (s) {
    case 0: t1e_walk(r, t1e_dir_roam); break;
    case 2: if (t1e_walk(r, t1e_dir_scent) < 0) w16(o + 0x1C, (uint16_t)(r16(o + 0x1C) - 1)); break;
    case 3:
        if (og16(OG_PLAYER_CELL) != og16(0x582C)) {
            sog16(0x56A6, og16(OG_PLAYER_CELL)); sog16(0x56A8, 0x23);
            mz_seg_of(2, 0x366A), fn_S2_366A(r); fn_S2_3568(r);
        }
        mz_seg_of(7, 0x3D2A), fn_S7_3D2A(r);
        if (!((uint16_t)r->d[0] & 0x8000) && t1e_walk(r, t1e_dir_scent) == 0) break;
        set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r); fn_S7_3722(r);
        set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] - r16(o + OB_HEADING)));
        fn_S7_367E(r);
        if ((uint16_t)r->d[0] >= 6) break;
        set_w(&r->d[0], 0x3D); set_w(&r->d[1], og16(OG_NEAR_DIR)); set_w(&r->d[2], og16(OG_SELF));
        r->a[0] = OADDR(0x48F0); mz_seg_of(7, 0x40D6), fn_S7_40D6(r);
        random_d0(r);
        w16(o + 0x1A, (uint16_t)(((uint16_t)r->d[0] & 0xF) + 0x22));
        break;
    case 4:
        if (t1e_walk(r, t1e_dir_roam) < 0) { int16_t t = (int16_t)(r16(o + 0x1A) - 4); w16(o + 0x1A, (uint16_t)(t < 0 ? 0 : t)); }
        break;
    case 7: { static const uint16_t pic[2] = {0x77, 0x78}; w16(o + 0x3A, pic[r16(o + 0x1C)]); break; }
    case 9: case 14: w16(o + 0x3A, 0x78); break;
    case 12: { static const uint16_t pic[3] = {0x79, 0x7A, 0}; w16(o + 0x3A, pic[r16(o + 0x1C) >> 3]); break; }
    case 13: turn_to_player(r, 2); break;
    case 15: w16(o + 0x3A, 0x78); rush(r); break;           /* at the player */
    }
    { int16_t t = (int16_t)(r16(o + 0x1A) - 1); w16(o + 0x1A, (uint16_t)(t < 0 ? 0 : t)); }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $20 (fn_S4_1256, level 4) ----------
 * Stands (0) turning to whatever fn_S7_1CCC gives, a step when it faces it, until it sees the
 * player close (distance < 5, or in front within $F): 2, firing (fn_S7_1EC0, $22) when facing him
 * at most every $1E frames (+$1A), closing in while he is $A or more away; out of sight: 3-4, $1E
 * frames (+$1E) going for his last cell (+$1C), then 0. Noticing him another way (fn_S7_1DDA) also
 * starts 3. Hit by types $17 / $19 / $1A in states 0, 2 and 4: 3; by $18: 5 (also in 11): as type
 * $1E, pictures $33-$37, gone for 750 frames (faster while G_56E0 is set), back in its old cell,
 * pictures $38-$3D, $32, then 0. State 1 leads by 12 / 13 (turning to the player) to 14, picture $37
 * rushing at him (fn_S7_1E66). */
/* fn_S4_1620: turns towards D0 (fn_S7_1516); facing it, a step (fn_S7_1540) unless blocked
 * (fn_S7_1766, D0 = -1) */
static void t20_step(Regs *r) {
    mz_seg_of(7, 0x3722), fn_S7_3722(r);
    if ((uint16_t)r->d[0] != r16(r->a[4] + OB_HEADING)) { r->d[0] = 0; return; }
    r->a[0] = OADDR(0x4F30); fn_S7_374C(r);
    fn_S7_3972(r);
    if ((uint16_t)r->d[0]) { r->d[0] = 0xFFFFFFFFu; return; }
    fn_S7_3784(r);
}
/* to the point G_381C / G_3818 by the distance map (fn_S7_192A, fn_S7_1B02): a step, or none */
static int t20_go(Regs *r) {
    mz_seg_of(7, 0x3B36), fn_S7_3B36(r); fn_S7_3D0E(r);
    set_w(&r->d[0], og16(0x3812));
    if (og16(0x3812) & 0x8000) return 0;
    t20_step(r);
    return 1;
}
static void t20_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t next;
        switch ((uint16_t)r->d[7]) {
        case 0:
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (!(og16(OG_NEAR_DIST) & 0x8000)) {
                int close = og16(OG_NEAR_DIST) < 5;
                if (!close) {
                    set_w(&r->d[0], (uint16_t)(og16(OG_NEAR_DIR) - r16(o + OB_HEADING)));
                    mz_seg_of(7, 0x367E), fn_S7_367E(r);
                    close = (uint16_t)r->d[0] < 0xF;
                }
                if (close) { w16(o + 0x1A, 5); next = 2; break; }
            }
            mz_seg_of(7, 0x3FF0), fn_S7_3FF0(r);
            if (!(uint16_t)r->d[0]) return;
            w16(o + 0x1C, og16(OG_PLAYER_CELL)); next = 3; break;
        case 2: mz_seg_of(7, 0x319E), fn_S7_319E(r); if (!(uint16_t)r->d[0]) return; next = 3; break;
        case 3: w16(o + 0x1E, 0x1E); next = 4; break;
        case 4:
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (!(uint16_t)r->d[0]) { next = 2; break; }
            if (!r16(o + 0x1E)) w16(o + 0x1C, 0);
            if (r16(o + 0x1C)) return;
            next = 0; break;
        case 1:                                             /* (picture $32: 12, else 13) */
            if (r16(o + 0x3A) == 0x32) { w16(o + 0x1E, 7); next = 0xC; } else { w16(o + 0x1E, 5); next = 0xD; }
            break;
        case 5: w16(o + 0x1E, 0xFFFF); next = 6; break;
        case 6: if (!count_up(r, 0x1E, 5)) return; next = 7; break;   /* hit by $18: pictures $33-$37 */
        case 7: off_map(r, 0x1E, 0x2EE); next = 8; break;
        case 8: if (!count_down(r, 0x1E, og16(OG_FULL_VIEW) ? 100 : 1)) return; next = 9; break;
        case 9: if (!back_in_cell(r)) return; next = 0xA; break;
        case 10: on_map(r, 0x1E); next = 0xB; break;
        case 11: if (!count_up(r, 0x1E, 0x1C)) return; next = 0; break;   /* 28 frames of pictures $38-$3D, $32 */
        case 12: if (!count_down(r, 0x1E, 1)) return; w16(o + 0x1E, 5); next = 0xD; break;
        case 13: if (!count_down(r, 0x1E, 1)) return; next = 0xE; break;
        default: return;                                    /* 14 */
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_1256(Regs *r) {
    static const uint16_t hits[] = {0x17, 3, 0x18, 5, 0x19, 3, 0x1A, 3, 0xFFFF}, hits11[] = {0x18, 5, 0xFFFF};
    uint32_t o = r->a[4], map = og32(OG_MAP);
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t20_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_hit(r, s == 0 || s == 2 || s == 4 ? hits : s == 11 ? hits11 : NULL, t20_think);
    s = (uint16_t)r->d[7];
    switch (s) {
    case 0: mz_seg_of(7, 0x3EE2), fn_S7_3EE2(r); if ((uint16_t)r->d[0] < 0x40) t20_step(r); break;
    case 2:
        w16(o + 0x1C, og16(OG_PLAYER_CELL));
        if (!r16(o + 0x1A)) {
            set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r);
            if (r16(o + OB_HEADING) != og16(OG_NEAR_DIR)) break;
            set_w(&r->d[0], 0x22); set_w(&r->d[1], r16(o + OB_HEADING)); set_w(&r->d[2], og16(OG_SELF));
            r->a[0] = OADDR(0x4CB0); mz_seg_of(7, 0x40D6), fn_S7_40D6(r);
            w16(o + 0x1A, 0x1E);
            break;
        }
        if (og16(OG_NEAR_DIST) >= 0xA) { g_target(og16(OG_PLAYER_X), og16(OG_PLAYER_Y)); if (t20_go(r)) break; }
        set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r);
        break;
    case 3: break;
    case 4: {
        uint16_t c = r16(o + 0x1C);
        g_target(r16(map + sxw(c) + 0x1C), r16(map + sxw(c) + 0x1E));
        if (t20_go(r)) break;
        if (r16(o + OB_CELL) == r16(o + 0x1C)) w16(o + 0x1C, 0);
        else w16(o + 0x1E, (uint16_t)(r16(o + 0x1E) - 1));
        break;
    }
    case 6: { static const uint16_t pic[5] = {0x33, 0x34, 0x35, 0x36, 0x37}; w16(o + 0x3A, pic[r16(o + 0x1E)]); break; }
    case 8: w16(o + 0x3A, 0x37); break;
    case 11: { static const uint16_t pic[7] = {0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x32}; w16(o + 0x3A, pic[r16(o + 0x1E) >> 2]); break; }
    case 12: turn_to_player(r, 1); break;
    case 13: w16(o + 0x3A, 0x37); turn_to_player(r, 1); break;
    case 14: w16(o + 0x3A, 0x37); rush(r); break;           /* at the player */
    }
    { int16_t t = (int16_t)(r16(o + 0x1A) - 1); w16(o + 0x1A, (uint16_t)(t < 0 ? 0 : t)); }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $2D (fn_S4_1DFA, level 4) ----------
 * Like type $20, firing in a spread: stands (12) turning to fn_S7_1CCC's direction, a step when
 * facing it, until it sees the player (G_3830 set): 2, firing every 4 frames (fn_S7_1EC0, $36)
 * within 3 of his direction at random, moving to him (step table G_4B70) while he is $A or more
 * away; out of sight 3-4 as type $20, then 12. Hit by types $17 / $19 / $1A in states 0, 2 and 4:
 * 3; by $18: 5 (also in 11 and 12): as type $1E, pictures $71-$73, gone for 750 frames, back in
 * its old cell, pictures $74-$76, 0, then 12. State 1 leads by 13 / 14 to 15, rushing at the
 * player (fn_S7_1E66). */
static void t2d_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t next;
        switch ((uint16_t)r->d[7]) {
        case 0: next = 0xC; break;
        case 2: mz_seg_of(7, 0x319E), fn_S7_319E(r); if (!(uint16_t)r->d[0]) return; next = 3; break;
        case 3: w16(o + 0x1E, 0x1E); next = 4; break;
        case 4:
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (!(uint16_t)r->d[0]) { next = 2; break; }
            if (!r16(o + 0x1E)) w16(o + 0x1C, 0);
            if (r16(o + 0x1C)) return;
            next = 0xC; break;
        case 12:
            mz_seg_of(7, 0x319E), fn_S7_319E(r);
            if (!(og16(OG_NEAR_DIR) & 0x8000)) { w16(o + 0x1A, 6); next = 2; break; }
            mz_seg_of(7, 0x3FF0), fn_S7_3FF0(r);
            if (!(uint16_t)r->d[0]) return;
            w16(o + 0x1C, og16(OG_PLAYER_CELL)); next = 3; break;
        case 1:                                             /* (no picture: 13, else 14) */
            if (!r16(o + 0x3A)) { w16(o + 0x1E, 7); next = 0xD; } else { w16(o + 0x1E, 5); next = 0xE; }
            break;
        case 5: w16(o + 0x1E, 0xFFFF); next = 6; break;
        case 6: if (!count_up(r, 0x1E, 3)) return; next = 7; break;   /* hit by $18: pictures $71-$73 */
        case 7: off_map(r, 0x1E, 0x2EE); next = 8; break;
        case 8: if (!count_down(r, 0x1E, og16(OG_FULL_VIEW) >= 0x80 ? 100 : 1)) return; next = 9; break;
        case 9: if (!back_in_cell(r)) return; next = 0xA; break;
        case 10: on_map(r, 0x1E); next = 0xB; break;
        case 11: if (!count_up(r, 0x1E, 0x10)) return; next = 0xC; break;   /* 16 frames of pictures $74-$76, 0 */
        case 13: if (!count_down(r, 0x1E, 1)) return; w16(o + 0x1E, 5); next = 0xE; break;
        case 14: if (!count_down(r, 0x1E, 1)) return; next = 0xF; break;
        default: return;                                    /* 15 */
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_1DFA(Regs *r) {
    static const uint16_t hits[] = {0x17, 3, 0x18, 5, 0x19, 3, 0x1A, 3, 0xFFFF}, hits18[] = {0x18, 5, 0xFFFF};
    uint32_t o = r->a[4], map = og32(OG_MAP);
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t2d_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_hit(r, s == 0 || s == 2 || s == 4 ? hits : s == 11 || s == 12 ? hits18 : NULL, t2d_think);
    s = (uint16_t)r->d[7];
    switch (s) {
    case 0: case 3: break;
    case 2:
        w16(o + 0x1C, og16(OG_PLAYER_CELL));
        if (!r16(o + 0x1A)) {
            random_d0(r);
            int16_t spread = (int16_t)(((uint16_t)r->d[0] & 7) - 3);
            if (spread == 4) spread = 0;
            set_w(&r->d[1], (uint16_t)((spread + og16(OG_NEAR_DIR)) & 0x3F));
            set_w(&r->d[0], 0x36); set_w(&r->d[2], og16(OG_SELF));
            r->a[0] = OADDR(0x48F0); mz_seg_of(7, 0x40D6), fn_S7_40D6(r);
            w16(o + 0x1A, 4);
            break;
        }
        if (og16(OG_NEAR_DIST) < 0xA) break;
        g_target(og16(OG_PLAYER_X), og16(OG_PLAYER_Y));
        mz_seg_of(7, 0x3B36), fn_S7_3B36(r); fn_S7_3D0E(r);
        set_w(&r->d[0], og16(0x3812));
        if (!(og16(0x3812) & 0x8000)) { r->a[0] = OADDR(0x4B70); mz_seg(7), fo_S7_1E54(r); }
        break;
    case 4: {
        uint16_t c = r16(o + 0x1C);
        g_target(r16(map + sxw(c) + 0x1C), r16(map + sxw(c) + 0x1E));
        mz_seg_of(7, 0x3B36), fn_S7_3B36(r); fn_S7_3D0E(r);
        set_w(&r->d[0], og16(0x3812));
        if (!(og16(0x3812) & 0x8000)) t1e_step(r);
        if (r16(o + OB_CELL) == r16(o + 0x1C)) w16(o + 0x1C, 0);
        else w16(o + 0x1E, (uint16_t)(r16(o + 0x1E) - 1));
        break;
    }
    case 12: mz_seg_of(7, 0x3EE2), fn_S7_3EE2(r); if ((uint16_t)r->d[0] < 0x40) t1e_step(r); break;
    case 6: { static const uint16_t pic[3] = {0x71, 0x72, 0x73}; w16(o + 0x3A, pic[r16(o + 0x1E)]); break; }
    case 8: case 14: w16(o + 0x3A, 0x73); break;
    case 11: { static const uint16_t pic[4] = {0x74, 0x75, 0x76, 0}; w16(o + 0x3A, pic[r16(o + 0x1E) >> 2]); break; }
    case 13: turn_to_player(r, 1); break;
    case 15: w16(o + 0x3A, 0x73); rush(r); break;           /* at the player */
    }
    { int16_t t = (int16_t)(r16(o + 0x1A) - 1); w16(o + 0x1A, (uint16_t)(t < 0 ? 0 : t)); }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- Lips, the brain (type $2E, fn_S4_21D6, level 5) ----------
 * Speaks the texts $2Exx ("Lips says, \"I can see you, but you'll never find me...\"", later "The
 * brain says, \"Wait! Wait! Let me live...\""), watching record 2's +$18 (set by type $30 while
 * the player is near it) and George's company (G_56C2); each frame it counts down record 3's
 * +$20 and +$24 (fn_S4_2770). States: 0 waiting for record 2; 1 "I can see you..." (said: 31),
 * left when record 2 clears or George is gone: 2, which sets record 3's +$22 and +$24 (the hunt
 * of types $1F / $31 begins), 3 waiting for record 2 again; 31 waiting for it to clear. 4 (hit)
 * calls types $1F / $31 to it (fn_S4_2790: those below y $1C0 get a hit by record 3; flag word 1
 * bit $20) and 14-15 waits 30 frames for the player; 5-13 and 20-23 are its texts ($2E02-$2E09,
 * most with fn_S4_27C4 calling the hunters to area $E, cell $3380) while record 2 stays set; 16-18
 * three pictures ($82, $83, 0), then the call again and, the player below y $180, 25-26 the end;
 * 24 makes an object of type $37 where the player is; 27-29 three pictures ($82, $84, $84) and it
 * is gone with record 2. (State 9 clears the word at address $E4, not record 3's +$E4: the
 * original's slip, kept.) The lists below are the original's for all states. */
static void t2e_count(void) {                               /* fn_S4_2770 */
    uint32_t tbl = og32(OG_OBJECTS);
    if (r16(tbl + 0xE0)) w16(tbl + 0xE0, (uint16_t)(r16(tbl + 0xE0) - 1));
    uint16_t t = r16(tbl + 0xE4);
    if (t && !(t & 0x8000)) w16(tbl + 0xE4, (uint16_t)(t - 1));
}
static void t2e_call(void) {                                /* fn_S4_2790: types $1F / $31 low on the map hit by record 3 */
    uint32_t t = og32(OG_OBJECTS);
    for (uint16_t rec = 0x1DC0; rec; rec -= 0x40) {
        uint16_t ty = r16(t + rec + 2);
        if ((ty == 0x1F || ty == 0x31) && r16(t + rec + 8) >= 0x1C0) w16(t + rec + 0x14, 0xC0);
    }
}
static void t2e_mark(void) {                                /* fn_S4_27C4: record 3's marks: area $E, cell $3380 */
    uint32_t t = og32(OG_OBJECTS);
    w16(t + 0xD8, 0xE); w16(t + 0xDC, 0xE); w16(t + 0xDA, 0x3380); w16(t + 0xDE, 0x3380);
}
static void t2e_think(Regs *r) {
    uint32_t tbl = og32(OG_OBJECTS);
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7], next;
        switch (s) {
        case 0: if (!r16(tbl + 0x98)) return; next = 1; break;
        case 1: if (r16(tbl + 0x98) && og16(OG_GEORGE_JOINED)) return; next = 2; break;
        case 2: w16(tbl + 0xE2, 1); w16(tbl + 0xE4, 1); next = 3; break;
        case 3: if (!r16(tbl + 0x98)) return; next = 4; break;
        case 31: if (r16(tbl + 0x98)) return; next = 2; break;
        case 4: w16(tbl + 0xE4, 0xFFFF); t2e_call(); flag_set(r, 1, 0x20); next = 14; break;
        case 5: case 20: if (r16(tbl + 0x98) && og16(OG_GEORGE_JOINED)) return; next = s == 5 ? 12 : 25; break;
        case 6: if (r16(tbl + 0x98)) return; next = 12; break;
        case 7: w16(tbl + 0x96, 50); next = 8; break;
        case 8: {
            if (!r16(tbl + 0x98)) { next = 12; break; }
            int16_t t = (int16_t)(r16(tbl + 0x96) - 1);
            w16(tbl + 0x96, (uint16_t)t);
            if (t >= 0) return;
            next = 9; break;
        }
        case 9: w16(0xE4, 0); next = 10; break;
        case 10: if (r16(tbl + 0x98)) return; next = 13; break;
        case 11: w16(tbl + 0xE4, 0); if (r16(tbl + 0x98)) return; next = 13; break;
        case 12: w16(tbl + 0xE4, 0); next = 13; break;
        case 14: w16(tbl + 0x96, 30); next = 15; break;
        case 15: {
            if (!r16(tbl + 0x98)) { w16(tbl + 0xE4, 0); next = 3; break; }
            int16_t t = (int16_t)(r16(tbl + 0x96) - 1);
            w16(tbl + 0x96, (uint16_t)t);
            if (t >= 0) return;
            next = 5; break;
        }
        case 16: case 27: w16(tbl + 0x96, 0xFFFF); next = (uint16_t)(s + 1); break;
        case 17: case 28: {
            uint16_t t = (uint16_t)(r16(tbl + 0x96) + 1);
            w16(tbl + 0x96, t);
            if (t < 3) return;
            next = (uint16_t)(s + 1); break;
        }
        case 18:
            w16(tbl + 0xE4, 5);
            t2e_call();
            if (og16(OG_PLAYER_Y) < 0x180) { next = 25; break; }
            if (!r16(tbl + 0x98)) return;
            w16(tbl + 0xE4, 0xFFFF);
            next = 20; break;
        case 21: case 22: if (!r16(tbl + 0x98)) { next = 25; break; } return;
        case 23: if (r16(tbl + 0x98)) return; next = 24; break;
        case 24:
            sog16(OG_MOVE_X, og16(OG_PLAYER_X)); sog16(OG_MOVE_Y, og16(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
            make_object(r, 0x37, 0, 0);
            next = 30; break;
        case 25: w16(tbl + 0xE4, 0); next = 26; break;
        case 29: {                                          /* gone, and record 2 with it */
            uint32_t o = r->a[4];
            w16(og32(OG_MAP) + sxw(r16(o + 0xE)) + 0xE, 0);
            unlink_obj(r);
            w16(o + OB_X, 0); w16(o + OB_Y, 0); w16(o + OB_CELL, 0); w16(o + 0xE, 0);
            set_w(&r->d[0], 0x80); set_w(&r->d[1], r16(tbl + 0xB0));
            mz_seg_of(2, 0x3410), fn_S2_3410(r);
            w16(tbl + 0x84, 0); w16(tbl + 0x88, 0); w16(tbl + 0xB0, 0);
            w16(tbl + 0xE4, 0xFFFF); w16(tbl + 0xE2, 0);
            next = 30; break;
        }
        default: return;                                    /* 13, 19, 26, 30 */
        }

        set_w(&r->d[0], next);
    }
}
void fo_S4_21D6(Regs *r) {
    static uint16_t said[32][2], *hit[32];
    static uint16_t hl[14][7];
    if (!said[1][0]) {
        static const uint16_t sv[][3] = {{1, 0xAE01, 31}, {5, 0xAE02, 6}, {6, 0xAE03, 7}, {8, 0xAE04, 11}, {10, 0xAE04, 11},
                                         {11, 0xAE05, 13}, {20, 0xAE06, 21}, {21, 0xAE07, 22}, {22, 0xAE08, 23}, {23, 0xAE09, 24}};
        for (unsigned k = 0; k < sizeof sv / sizeof sv[0]; k++) { said[sv[k][0]][0] = sv[k][1]; said[sv[k][0]][1] = sv[k][2]; }
        /* hits: { state, by $17 ->, by $18 ->, by $19 -> } (0: none) */
        static const uint8_t hv[14][4] = {{5, 6, 16, 6}, {6, 7, 16, 7}, {8, 11, 16, 11}, {10, 11, 16, 11}, {11, 13, 16, 13},
            {13, 0, 16, 0}, {15, 0, 16, 0}, {18, 0, 27, 0}, {19, 0, 27, 0}, {20, 21, 27, 21}, {21, 22, 27, 22},
            {22, 23, 27, 23}, {23, 24, 27, 24}, {26, 0, 27, 0}};
        for (int k = 0; k < 14; k++) {
            int n = 0;
            for (int j = 0; j < 3; j++) if (hv[k][j + 1]) { hl[k][n++] = (uint16_t)(0x17 + j); hl[k][n++] = hv[k][j + 1]; }
            hl[k][n] = 0xFFFF;
            hit[hv[k][0]] = hl[k];
        }
    }
    uint32_t o = r->a[4];
    r->a[2] = og32(OG_OBJECTS);
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t2e_think(r);
    uint16_t s = (uint16_t)r->d[7];
    if (s < 32) o_said(r, said[s], t2e_think);
    s = (uint16_t)r->d[7];
    o_hit(r, s < 32 ? hit[s] : NULL, t2e_think);
    s = (uint16_t)r->d[7];
    switch (s) {
    case 0: case 2: break;
    case 1: t2e_count(); say(0x2E01); break;
    case 3: case 31: t2e_count(); break;
    case 5: case 6: case 8: case 20: case 21: case 22: case 23: {
        static const uint16_t text[24] = {[5] = 0x2E02, [6] = 0x2E03, [8] = 0x2E04, [20] = 0x2E06, [21] = 0x2E07,
                                          [22] = 0x2E08, [23] = 0x2E09};
        t2e_count(); t2e_mark(); say(text[s]);
        break;
    }
    case 10: t2e_count(); say(0x2E04); break;
    case 11: t2e_count(); say(0x2E05); break;
    case 13: case 26: case 30: t2e_count(); break;
    case 15: t2e_count(); t2e_call(); t2e_mark(); break;
    case 17: { static const uint16_t pic[3] = {0x82, 0x83, 0}; w16(r->a[4] + 0x3A, pic[r16(og32(OG_OBJECTS) + 0x96)]); t2e_count(); break; }
    case 18: t2e_count(); t2e_mark(); break;
    case 28: { static const uint16_t pic[3] = {0x82, 0x84, 0x84}; w16(r->a[4] + 0x3A, pic[r16(og32(OG_OBJECTS) + 0x96)]); t2e_count(); break; }
    }
    sog32(0x5752, (uint16_t)r->d[7]);
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $29 (fn_S9_1AE0, level 3) ----------
 * Grows (+$C, kept in record 0's +$1C between frames: fn_S9_1908, unless bit 4 of flag word 0 is
 * set, steps 0-4 after 3, 7, 7, 100 frames); with the player standing at x $33-$3C, y below $12
 * (no full-view picture showing): 4, then 5: 7 frames of full view $83 and the way back to level 2
 * (at $1EB, $78, turned $10 to the left). Hit by type $19: 2 (+$C one more, up to 4), by $1A: 1
 * (one less; at 0 with flag word 0 bit 2 set, -1); +$C negative: 3, where each frame takes 2 from
 * the player's life (G_56EE = $27). Their acts do nothing. */
static void t29_grow(Regs *r) {                             /* fn_S9_1908 */
    static const uint16_t step[5][2] = {{3, 1}, {7, 2}, {7, 3}, {100, 3}, {100, 4}};
    uint32_t o = r->a[4];
    if (flag(r, 0, 4)) return;
    uint16_t c = r16(o + OB_HEADING), n = (uint16_t)(r16(o + 0x10) + 1);
    w16(o + 0x10, n);
    if (n < step[c][0]) return;
    w16(o + OB_HEADING, step[c][1]); w16(o + 0x10, 0);
}
static uint16_t t29_smaller(Regs *r) {                     /* hit by $1A: smaller (at 0, with flag 0 bit 2: state 3) */
    uint32_t o = r->a[4];
    int16_t t = (int16_t)(r16(o + OB_HEADING) - 1);
    if (t >= 0) w16(o + OB_HEADING, (uint16_t)t);
    else if (flag(r, 0, 4)) { w16(o + OB_HEADING, 0xFFFF); return 3; }
    w16(o + 0x10, 0);
    return 0;
}
static void t29_bigger(Regs *r) {                           /* hit by $19: bigger, up to 4 */
    uint16_t t = (uint16_t)(r16(r->a[4] + OB_HEADING) + 1);
    w16(r->a[4] + OB_HEADING, t >= 5 ? 4 : t);
}
static void t29_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t next;
        switch ((uint16_t)r->d[7]) {
        case 0:
            if (r16(o + OB_HEADING) & 0x8000) { next = 3; break; }
            if (og16(OG_FULL_VIEW) >= 0x80 || og16(OG_PLAYER_Y) >= 0x12 || og16(OG_PLAYER_X) < 0x33 || og16(OG_PLAYER_X) >= 0x3D) return;
            next = 4; break;
        case 4: w16(o + 0x12, 7); next = 5; break;
        case 5: {
            sog16(OG_FULL_VIEW, 0x83);
            uint16_t t = (uint16_t)(r16(o + 0x12) - 1);
            w16(o + 0x12, t);
            if (t) return;
            sog16(OG_GO_LEVEL, 2); sog32(OG_GO_X, 0x1EB0000); sog32(OG_GO_Y, 0x780000);
            sog16(OG_GO_HEADING, (uint16_t)((og16(OG_PLAYER_HEADING) - 0x10) & 0x3F)); sog8(0x57DE, 1); sog8(0x57F2, 1);
            next = 0; break;
        }
        case 1: next = t29_smaller(r); break;
        case 2: t29_bigger(r); next = 0; break;
        case 3:                                             /* gone: each frame 2 from the player's life */
            sog32(OG_MOVE_X, 0); sog32(OG_MOVE_Y, 0); sog16(OG_MOVE_CELL, 0);
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            { int16_t l = (int16_t)(og16(OG_LIFE) - 2); sog16(OG_LIFE, (uint16_t)(l < 0 ? 0 : l)); }
            sog16(0x56EE, 0x27);
            return;
        default: return;
        }
        set_w(&r->d[0], next);
    }
}
void fo_S9_1AE0(Regs *r) {
    static const uint16_t hits[] = {0x19, 2, 0x1A, 1, 0xFFFF};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    w16(o + OB_HEADING, r16(og32(OG_OBJECTS) + 0x1C));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t29_think(r);
    o_hit(r, (uint16_t)r->d[7] == 0 ? hits : NULL, t29_think);
    if ((uint16_t)r->d[7] == 0 || (uint16_t)r->d[7] == 5) t29_grow(r);
    w16(og32(OG_OBJECTS) + 0x1C, r16(o + OB_HEADING));
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- the snails (types 3, 4 and 5: fn_S10_0028, level 1) ----------
 * Three snails. The talker (type 3, record 2) speaks to the player once George is with him
 * (G_56C2), its texts $83xx: it wakes (state 1) when the player comes within 15 (G_382E) and
 * rests beyond 20. Awake it wanders after the player, and with George there waits +$1C frames
 * (2-3) for the player to face it with nothing being said, then speaks: each text ended (or a
 * hit by types $17 / $19) moves it on through 4-12, 16-20, 22-23, 25-48 (the long story,
 * $8309-$8320), 54-57, 59-60 and 62; out of range it waits in a resting state (13-15, 21, 24,
 * 53, 61) until the player is back. A hit by $1A jumps ahead; by $18 (most states) it dies (78,
 * 96, 79: event $301 to the others, and shrinking away). In 64 flag word 0 bit 0 is set (it is
 * cleared when one dies); 63-67 start the journey: event $301 calls the other two (types 4 and 5,
 * records 3 and 4, otherwise wandering near their places: 0-1), and all three walk the ways below
 * (68-77; 2-11 for the others), the talker waiting 50 frames on the way, telling records 6-11 (event
 * $302), setting bit 15 of flag word 0, pushing the objects of type 9 in their cells (event $303)
 * and, the last of them still walking, taking the view along (G_56A6). At the end they wait (80-88:
 * the talker speaks again when the player, within 32, looks at it) until event 1 ends it all (89; 16
 * for the others): the type 9 objects removed if one is above y $88, and the three snails with them.
 * Event $101 sends the talker to 90 (resting) or 93 (awake): 90-95 talk once more ($832D, $832E);
 * $304 sends it to 63. */
static const uint16_t snail_ways[136] = {                 /* points { x, y } ..., $FFFF: the original's at S10:$1044 */
    0x18, 0x98, 0x18, 0x78, 0x28, 0x78, 0x28, 0x4B, 0x48, 0x4B, 0x68, 0x4B, 0x98, 0x37, 0x98, 0x28, 0xB8, 0x28, 0xFFFF, 0xFFFF,
    0xE8, 0x28, 0x108, 0x28, 0x108, 0x38, 0x118, 0x38, 0x118, 0x58, 0x118, 0x78, 0x148, 0x78, 0x148, 0x68, 0x168, 0x68,
    0x188, 0x68, 0x188, 0x78, 0x1A8, 0x78, 0x1C8, 0x78, 0x1C8, 0x58, 0x1A8, 0x58, 0x1A8, 0x28, 0x1C8, 0x28, 0xFFFF, 0xFFFF,
    0x1D6, 0x28, 0xFFFF, 0xFFFF,
    0x1D6, 0x18, 0x1C8, 0x18, 0x1C8, 0x28, 0x1A8, 0x28, 0x1A8, 0x58, 0x1C8, 0x58, 0x1C8, 0x78, 0x1A8, 0x78, 0x188, 0x78,
    0x188, 0x68, 0x168, 0x68, 0x168, 0x98, 0x168, 0xB8, 0x148, 0xB8, 0x128, 0xB8, 0x128, 0xD8, 0x128, 0xF8, 0xFFFF, 0xFFFF,
    0x128, 0x128, 0x128, 0x148, 0x158, 0x148, 0x158, 0x128, 0x168, 0x128, 0xFFFF, 0xFFFF,
    0x148, 0xF8, 0x148, 0x118, 0x168, 0x118, 0xFFFF, 0xFFFF,
    0x148, 0xF8, 0x148, 0x118, 0x158, 0x118, 0xFFFF, 0xFFFF,
    0x128, 0x128, 0x128, 0x148, 0x158, 0x148, 0x158, 0x118, 0x148, 0x118, 0xFFFF, 0xFFFF};
enum { WAY_1044 = 0, WAY_106C = 0x28, WAY_10B4 = 0x70, WAY_10BC = 0x78,       /* byte offsets */
       WAY_1104 = 0xC0, WAY_111C = 0xD8, WAY_112C = 0xE8, WAY_113C = 0xF8 };
static int snail_within(Regs *r, uint16_t d) { near_d0(r); return og16(OG_NEAR_DIST) < d; }   /* fn_S10_115A (15), 1168 (20) */
static int snail_looked_at(Regs *r, uint16_t dir, uint16_t within) {   /* the player looks this way */
    set_w(&r->d[0], (uint16_t)(dir - og16(OG_PLAYER_HEADING)));
    mz_seg_of(7, 0x367E), fn_S7_367E(r);
    return (uint16_t)r->d[0] < within;
}
static int snail_faced(Regs *r) { return snail_looked_at(r, (og16(OG_NEAR_DIR) + 0x20) & 0x3F, 8); }   /* fn_S10_1176 */
static int snail_watched(Regs *r) {                        /* within 32 and looked at (within 12) */
    near_d0(r);
    return og16(OG_NEAR_DIST) < 0x20 && snail_looked_at(r, og16(OG_NEAR_DIR) ^ 0x20, 12);
}
static void snail_wander(Regs *r, uint16_t dir) {          /* fn_S10_16FC: about heading dir */
    uint32_t o = r->a[4];
    int16_t t = (int16_t)(r16(o + 0x1A) - 1);
    w16(o + 0x1A, (uint16_t)t);
    if (t < 0) {                                            /* a new heading every 45-76 frames */
        random_d0(r); w16(o + 0x1A, (uint16_t)(((uint16_t)r->d[0] & 0x1F) + 0x2D));
        random_d0(r); w16(o + 0x18, (uint16_t)((((uint16_t)r->d[0] & 0xF) - 8 + dir) & 0x3F));
    }
    set_w(&r->d[0], r16(o + 0x18));
    mz_seg_of(7, 0x372C), fn_S7_372C(r);
}
static void tell(Regs *r, uint16_t ev, uint16_t rec) { call_d0d1(r, fo_S7_1278, ev, rec); }   /* event ev to a record */
static void flag0_clear(uint16_t bits) { uint32_t f = og32(OG_OBJECTS) + 0x16; w16(f, r16(f) & ~bits); }   /* fn_S7_12A6 */
static int gone(uint16_t rec) { return (r16(og32(OG_OBJECTS) + rec + 2) & 0x8000) != 0; }
/* fn_S10_167A: the way from byte +$1C on: the heading to its point (fn_S7_1488), the next point
 * once there; -1 at its end. Kept at -2(A6) for the act. */
static int snail_way(Regs *r, uint16_t way) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t i = (uint16_t)((way + r16(o + 0x1C)) / 2);
        sog16(OG_TARGET_X, snail_ways[i]);
        if (snail_ways[i] & 0x8000) { r->d[0] = 0xFFFFFFFFu; break; }
        sog16(OG_TARGET_Y, snail_ways[i + 1]);
        fo_S7_1488(r);
        if (!((uint16_t)r->d[0] & 0x8000)) break;
        w16(o + 0x1C, (uint16_t)(r16(o + 0x1C) + 4));
    }
    w16(r->a[6] - 2, (uint16_t)r->d[0]);
    return ((uint16_t)r->d[0] & 0x8000) != 0;
}
static void refresh(Regs *r) {                              /* fn_S2_2B3E */
    mz_seg_of(2, 0x366A), fn_S2_366A(r);
    mz_seg_of(2, 0x3568), fn_S2_3568(r);
}
static void unlink_rec(Regs *r, uint16_t rec) {             /* fn_S2_29E2 for another record */
    set_w(&r->d[0], rec); set_w(&r->d[1], r16(og32(OG_OBJECTS) + rec + 0x30));
    mz_seg_of(2, 0x3410), fn_S2_3410(r);
}
/* the walk along a way (the act): fn_S10_1742 (table G_4A30) / 1746, pushing (fn_S10_176A) and
 * the view (fn_S10_17FA) as asked */
enum { WALK_FAST = 1, WALK_PUSH = 2, WALK_VIEW = 4, WALK_PIC = 8 };
static void snail_walk(Regs *r, int how) {
    uint32_t o = r->a[4];
    if (how & WALK_PIC) set_pic(r, 0x21);
    set_w(&r->d[0], r16(r->a[6] - 2));
    r->a[0] = OADDR(how & WALK_FAST ? 0x4B70 : 0x4A30);
    mz_seg_of(7, 0x36F4), fn_S7_36F4(r);                     /* fn_S7_14E8: turned that way? */
    if ((uint16_t)r->d[0] == r16(o + OB_HEADING)) {
        mz_seg_of(7, 0x374C), fn_S7_374C(r);
        mz_seg_of(7, 0x3972), fn_S7_3972(r);
        if (!(uint16_t)r->d[0]) mz_seg_of(7, 0x3784), fn_S7_3784(r);
    }
    if (how & WALK_PUSH) {                                  /* the type 9 objects in its cell get event $303 */
        uint32_t map = og32(OG_MAP), t = og32(OG_OBJECTS);
        for (uint32_t k = 1; k <= 10; k++) {
            uint8_t n = r8(map + sxw(r16(o + OB_CELL)) + k);
            if (!n) break;
            if (r16(t + ((uint32_t)n << 6) + 2) == 9) w16(t + ((uint32_t)n << 6) + 0x12, 0x303);
        }
    }
    if ((how & WALK_VIEW) && r16(o + OB_CELL) != og16(0x582C)) {   /* the view goes with it */
        sog16(0x56A6, r16(o + OB_CELL)); sog16(0x56A8, 100);
        refresh(r);
    }
}
static void snail_call_others(Regs *r) {                    /* fn_S10_16AE: event $302 to records 6-11 */
    w16(r->a[4] + 0x1C, 0);
    for (uint16_t rec = 0x180; rec <= 0x2C0; rec += 0x40) tell(r, 0x302, rec);
}
static void snail_remove(Regs *r) {
    unlink_obj(r);
    w16(r->a[4] + OB_TYPE, r16(r->a[4] + OB_TYPE) | 0x8000);
    sog16(0x56A8, 0); sog16(0x56A6, 0);
    refresh(r);
}
static void snail_shrink(Regs *r, uint16_t pic, int tell_george) {   /* dying: 7 pictures from pic + 1, then gone */
    uint16_t n = (uint16_t)(r16(r->a[4] + 0x1C) + 1);
    if (n < 7) { w16(r->a[4] + 0x1C, n); set_pic(r, (uint16_t)(pic + n)); return; }
    if (tell_george) { near_d0(r); if (!(og16(OG_NEAR_DIST) & 0x8000)) tell(r, 0x304, 0x40); }
    snail_remove(r);
}
static void snail_sink(Regs *r) {                           /* at the end of the way: pictures $20, $22, $22, then gone */
    static const uint16_t pic[3] = {0x20, 0x22, 0x22};
    uint16_t n = (uint16_t)(r16(r->a[4] + 0x1C) + 2);
    w16(r->a[4] + 0x1C, n);
    if (n < 6) set_pic(r, pic[n / 2]); else snail_remove(r);
}
static void snail_finale(Regs *r) {                         /* fn_S10_1816 */
    uint32_t t = og32(OG_OBJECTS);
    int above = 0;
    for (uint16_t rec = 0x1DC0; rec && !above; rec -= 0x40)
        above = r16(t + rec + 2) == 9 && r16(t + rec + 8) < 0x88;
    if (above)                                              /* all the type 9 objects removed */
        for (uint16_t rec = 0x1DC0; rec; rec -= 0x40) {
            if (r16(t + rec + 2) != 9) continue;
            w16(og32(OG_MAP) + sxw(r16(t + rec + 0xE)) + 0xE, 0);
            unlink_rec(r, rec);
            w16(t + rec + 2, r16(t + rec + 2) | 0x8000);
        }
    for (uint16_t rec = 0x80; rec <= 0x100; rec += 0x40) {  /* and the snails */
        w16(t + rec + 2, r16(t + rec + 2) | 0x8000);
        unlink_rec(r, rec);
    }
    sog16(0x56A8, 0); sog16(0x56A6, 0);
}
/* the talker (type 3) */
static void talker_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        switch (s) {
        /* resting: the player back within 15 */
        case 0: case 13: case 14: case 15: case 21: case 24: case 53: case 61: case 66: case 90: case 93: {
            static const uint8_t back[94] = {[0] = 1, [13] = 7, [14] = 10, [15] = 16, [21] = 22, [24] = 22,
                                             [53] = 59, [61] = 59, [66] = 65, [90] = 91, [93] = 94};
            if (!snail_within(r, 15)) return;
            next = back[s];
            break;
        }
        case 2: w16(o + 0x1C, 20); w16(o + 0x18, og16(OG_NEAR_DIST)); w16(o + 0x1A, 25); next = 3; break;
        case 5: case 8: case 11: w16(o + 0x1C, 200); next = (uint16_t)(s + 1); break;
        case 63: next = 67; break;
        case 64: call_d0d1(r, fo_S7_1292, 0, 1); next = 67; break;
        case 67: w16(o + 0x1C, 4); tell(r, 0x301, 0xC0); tell(r, 0x301, 0x100); next = 68; break;
        case 68: if (!snail_way(r, WAY_1044)) return; next = 69; break;
        case 69: snail_call_others(r); next = 70; break;
        case 70: if (!snail_way(r, WAY_106C)) return; next = 71; break;
        case 71: w16(o + 0x1C, 50); next = 72; break;
        case 72: {
            int16_t t = (int16_t)(r16(o + 0x1C) - 1);
            w16(o + 0x1C, (uint16_t)t);
            if (t >= 0) return;
            w16(o + 0x1C, 0); next = 73;
            break;
        }
        case 73: if (!snail_way(r, WAY_10B4)) return; next = 74; break;
        case 74: call_d0d1(r, fo_S7_1292, 0, 0x8000); w16(o + 0x1C, 0); next = 75; break;
        case 75: if (!snail_way(r, WAY_10BC)) return; next = 76; break;
        case 76: w16(o + 0x1C, gone(0xC0) && gone(0x100) ? WAY_113C : WAY_1104); next = 77; break;
        case 77: if (!snail_way(r, WAY_1044)) return; next = 82; break;
        case 78: tell(r, 0x301, 0xC0); tell(r, 0x301, 0x100); flag0_clear(1); next = 96; break;   /* dying */
        case 79: case 81: case 89: return;
        case 80: w16(o + 0x1C, 0xFFFE); next = 81; break;
        case 82: next = flag(r, 0, 1) ? 83 : 80; break;
        case 83: if (!snail_watched(r)) return; next = 86; break;
        case 84: w16(o + 0x1C, 1000); next = 85; break;
        case 85: {
            if (snail_watched(r)) { next = 86; break; }
            int16_t t = (int16_t)(r16(o + 0x1C) - 1);
            w16(o + 0x1C, (uint16_t)t);
            if (t >= 0) return;
            next = 80;
            break;
        }
        case 86: case 87: case 88: near_d0(r); if (!(og16(OG_NEAR_DIR) & 0x8000)) return; next = 80; break;
        case 96: w16(o + 0x1C, 0xFFFF); next = 79; break;
        default:
            /* awake: beyond 20 it rests */
            if (!snail_within(r, 20)) {
                next = s <= 3 ? 0 : s <= 6 ? 13 : s <= 9 ? 14 : s <= 16 ? 15 : s <= 20 ? 21 : s <= 23 ? 24
                     : s <= 58 ? 53 : s <= 60 ? 61 : s == 62 ? 63 : s == 65 ? 66 : s == 91 ? 90 : 93;
                break;
            }
            if ((s == 1 || s == 91) && og16(OG_GEORGE_JOINED) && (s == 1 || !og16(OG_MESSAGE))) { next = s == 1 ? 2 : 92; break; }
            if (s == 3) {
                int16_t t = (int16_t)r16(o + 0x1C);
                if (t >= 0) { w16(o + 0x1C, (uint16_t)(t - 1)); return; }
                if (!snail_faced(r) || og16(OG_MESSAGE)) return;
                next = 4;
                break;
            }
            if (s == 6 || s == 9) {
                int16_t t = (int16_t)(r16(o + 0x1C) - 1);
                w16(o + 0x1C, (uint16_t)t);
                if (t >= 0) return;
                next = (uint16_t)(s + 1);
                break;
            }
            return;
        }
        set_w(&r->d[0], next);
    }
}
static const uint16_t *talker_hits(uint16_t s) {           /* { type, new state } ..., $FFFF */
    static const uint8_t row[][4] = {                      /* { state, by $17, by $19, by $1A } */
        {3, 0, 2, 0}, {4, 19, 5, 5}, {6, 19, 0, 0}, {7, 19, 8, 8}, {9, 19, 0, 0}, {10, 20, 11, 11}, {12, 20, 0, 0},
        {16, 17, 0, 0}, {17, 18, 18, 0}, {18, 22, 22, 0}, {19, 22, 22, 0}, {20, 22, 22, 0}, {22, 25, 25, 59},
        {23, 25, 25, 59}, {48, 55, 55, 62}, {54, 55, 55, 62}, {55, 0, 56, 58}, {56, 0, 64, 58}, {57, 0, 64, 58},
        {59, 0, 64, 60}, {60, 0, 64, 0}, {62, 63, 63, 63}, {92, 95, 94, 94}, {95, 94, 94, 94}};
    static uint16_t l[11];
    uint8_t to17 = 0, to19 = 0, to1a = 0;
    if (s >= 25 && s <= 47) to17 = to19 = (uint8_t)(s + 1), to1a = 58;   /* the story: on, or ahead */
    for (unsigned k = 0; k < sizeof row / sizeof row[0]; k++)
        if (row[k][0] == s) to17 = row[k][1], to19 = row[k][2], to1a = row[k][3];
    int n = 0, dies = (s <= 48 && s != 2 && s != 5 && s != 8 && s != 11) || (s >= 53 && s <= 62) || s == 65
                      || s == 66 || s == 68 || s == 70 || s == 72 || (s >= 90 && s <= 95);
    if (to17) l[n++] = 0x17, l[n++] = to17;
    if (dies) l[n++] = 0x18, l[n++] = 78;
    if (to19) l[n++] = 0x19, l[n++] = to19;
    if (to1a) l[n++] = 0x1A, l[n++] = to1a;
    if (s == 70) l[n++] = 0x1B, l[n++] = 78;
    l[n] = 0xFFFF;
    return l;
}
static const uint16_t *talker_events(uint16_t s) {         /* { event, new state } ..., $FFFF */
    static uint16_t l[9];
    int n = 0, any = (s <= 62 && s != 2 && s != 5 && s != 8 && s != 11) || (s >= 90 && s <= 95);
    if (s == 68 || s == 70 || s == 72 || s == 73 || s == 75 || s == 77 || s == 83 || (s >= 85 && s <= 88))
        l[n++] = 1, l[n++] = 89;
    if (any && s <= 61) {
        int resting = s == 0 || s == 1 || s == 3 || s == 13 || s == 14 || s == 15 || s == 21 || s == 24 || s == 53 || s == 61;
        l[n++] = 0x101, l[n++] = resting ? 90 : 93;
    }
    if (any) l[n++] = 0x304, l[n++] = 63;
    if (s == 70) l[n++] = 0x901, l[n++] = 96;
    l[n] = 0xFFFF;
    return l;
}
static void talker_act(Regs *r) {
    uint16_t s = (uint16_t)r->d[7];
    static const uint16_t says[96] = {[4] = 0x8301, [7] = 0x8302, [10] = 0x8303, [17] = 0x8304, [18] = 0x8305,
        [19] = 0x8306, [20] = 0x8307, [22] = 0x8308, [55] = 0x8325, [56] = 0x8326, [59] = 0x8327, [62] = 0x8328,
        [86] = 0x832A, [87] = 0x832B, [88] = 0x832C, [92] = 0x832D, [95] = 0x832E};
    switch (s) {
    case 0: case 13: case 14: case 15: case 21: case 24: case 53: case 61: case 66: case 90: case 93:
        snail_wander(r, 0x2A); break;
    case 68: case 73: snail_walk(r, 0); break;
    case 70: snail_walk(r, WALK_VIEW); break;
    case 75: snail_walk(r, WALK_PIC | WALK_PUSH | WALK_VIEW); break;
    case 77: snail_walk(r, WALK_PIC | WALK_FAST | WALK_PUSH | WALK_VIEW); break;
    case 79: snail_shrink(r, 0x8E, 0); break;
    case 81: snail_sink(r); break;
    case 83: sog16(OG_MESSAGE, 0x8329); set_pic(r, 0x21); break;
    case 85: snail_wander(r, 0x19); set_pic(r, 0x21); break;
    case 89: snail_finale(r); break;
    case 1: case 3: case 4: case 6: case 7: case 9: case 10: case 12: case 16: case 17: case 18: case 19: case 20:
    case 22: case 23: case 54: case 55: case 56: case 57: case 58: case 59: case 60: case 62: case 65:
    case 86: case 87: case 88: case 91: case 92: case 94: case 95:
        if (says[s]) sog16(OG_MESSAGE, says[s]);
        if (s >= 86 && s <= 88) set_pic(r, 0x21);
        snail_wander(r, og16(OG_NEAR_DIR));                      /* after the player */
        break;
    default:
        if (s >= 25 && s <= 48) { sog16(OG_MESSAGE, (uint16_t)(0x8309 + s - 25)); snail_wander(r, og16(OG_NEAR_DIR)); }
        break;                                              /* the rest do nothing */
    }
}
/* the other two (types 4 and 5): states 0-1 wandering near their places (headings $18 / 8), 2-11
 * the journey (following the talker), 14-15 at its end (sinking once the ones before them are gone:
 * record 2, for type 5 also record 3), 16 the end; hit by $18 (or $1B in 5): 12, 17, 13 dying (pictures
 * from $96 / $9D; type 4 tells George, event $304, when the player can see it) */
static int follower_last(Regs *r) {                         /* the ones before it gone */
    return gone(0x80) && (r16(r->a[4] + OB_TYPE) == 4 || gone(0xC0));
}
static void follower_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        switch (s) {
        case 0: if (!snail_within(r, 15)) return; next = 1; break;
        case 1: if (snail_within(r, 20)) return; next = 0; break;
        case 2: case 6: w16(o + 0x1C, 0); next = (uint16_t)(s + 1); break;
        case 3: if (!snail_way(r, WAY_1044)) return; next = 4; break;
        case 4: snail_call_others(r); next = 5; break;
        case 5: if (!snail_way(r, WAY_106C)) return; next = 6; break;
        case 7: if (!snail_way(r, WAY_10B4)) return; next = 8; break;
        case 8: call_d0d1(r, fo_S7_1292, 0, 0x8000); w16(o + 0x1C, 0); next = 9; break;
        case 9: if (!snail_way(r, WAY_10BC)) return; next = 10; break;
        case 10:
            if (r16(o + OB_TYPE) == 4) w16(o + 0x1C, !gone(0x80) ? WAY_111C : !gone(0x100) ? WAY_1104 : WAY_113C);
            else w16(o + 0x1C, gone(0x80) && gone(0xC0) ? WAY_113C : WAY_112C);
            next = 11;
            break;
        case 11: if (!snail_way(r, WAY_1044)) return; next = 14; break;
        case 12: for (uint16_t rec = 0x80; rec <= 0x100; rec += 0x40) tell(r, 0x304, rec);   /* dying */
                 flag0_clear(1); next = 17; break;
        case 14: if (!follower_last(r)) return; w16(o + 0x1C, 0xFFFE); next = 15; break;
        case 17: w16(o + 0x1C, 0xFFFF); next = 13; break;
        default: return;                                    /* 13, 15, 16 */
        }
        set_w(&r->d[0], next);
    }
}
static void follower_act(Regs *r) {
    int t4 = r16(r->a[4] + OB_TYPE) == 4, view = follower_last(r) ? WALK_VIEW : 0;
    switch ((uint16_t)r->d[7]) {
    case 0: snail_wander(r, t4 ? 0x18 : 8); break;
    case 1: snail_wander(r, og16(OG_NEAR_DIR)); break;
    case 3: case 7: snail_walk(r, 0); break;
    case 5: snail_walk(r, view); break;
    case 9: snail_walk(r, WALK_PIC | WALK_PUSH | view); break;
    case 11: snail_walk(r, WALK_PIC | WALK_FAST | WALK_PUSH | view); break;
    case 13: snail_shrink(r, t4 ? 0x95 : 0x9C, t4); break;
    case 14: set_pic(r, 0x21); snail_wander(r, 0); break;
    case 15: snail_sink(r); break;
    case 16: snail_finale(r); break;
    }
}
void fo_S10_0028(Regs *r) {
    static uint16_t said[97][2];                            /* the talker's { text that ended, next state } */
    if (!said[4][0]) {
        static const uint16_t sv[][3] = {{4, 0x8301, 5}, {7, 0x8302, 8}, {10, 0x8303, 11}, {17, 0x8304, 18},
            {18, 0x8305, 22}, {19, 0x8306, 22}, {20, 0x8307, 22}, {22, 0x8308, 23}, {48, 0x8320, 54}, {55, 0x8325, 56},
            {56, 0x8326, 57}, {59, 0x8327, 60}, {62, 0x0328, 63}, {83, 0x8329, 84}, {86, 0x832A, 87}, {87, 0x832B, 88},
            {88, 0x832C, 80}, {92, 0x832D, 94}, {95, 0x832E, 94}};
        for (unsigned k = 0; k < sizeof sv / sizeof sv[0]; k++) { said[sv[k][0]][0] = sv[k][1]; said[sv[k][0]][1] = sv[k][2]; }
        for (int k = 25; k <= 47; k++) { said[k][0] = (uint16_t)(0x8309 + k - 25); said[k][1] = (uint16_t)(k + 1); }
    }
    static const uint16_t h[] = {0x18, 12, 0xFFFF}, h5[] = {0x18, 12, 0x1B, 12, 0xFFFF};
    static const uint16_t e01[] = {0x301, 2, 0xFFFF}, e1[] = {1, 16, 0xFFFF}, e5[] = {1, 16, 0x901, 17, 0xFFFF};
    static const uint16_t *const f_hits[18] = {h, h, NULL, h, NULL, h5, NULL, h};
    static const uint16_t *const f_events[18] = {e01, e01, NULL, e1, NULL, e5, NULL, e1, NULL, e1, NULL, e1};
    uint32_t o = r->a[4], a6 = r->a[6];
    uint16_t type = r16(o + OB_TYPE);
    if (type < 3 || type > 5) return;
    r->a[6] = r->a[7] - 4;                                  /* LINK A6, #-2: the way's heading, for the act */
    r->a[7] -= 6;
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    if (type == 3) {
        talker_think(r);
        uint16_t s = (uint16_t)r->d[7];
        if (s < 97) o_said(r, said[s], talker_think);
        o_hit(r, talker_hits((uint16_t)r->d[7]), talker_think);
        o_event(r, talker_events((uint16_t)r->d[7]), talker_think);
        talker_act(r);
    } else {
        follower_think(r);
        uint16_t s = (uint16_t)r->d[7];
        o_hit(r, s < 18 ? f_hits[s] : NULL, follower_think);
        s = (uint16_t)r->d[7];
        o_event(r, s < 18 ? f_events[s] : NULL, follower_think);
        follower_act(r);
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
    r->a[7] += 6; r->a[6] = a6;
}

/* ---------- type $D (fn_S4_0028, level 4) ----------
 * While the player is in its room (x $90-$E7, y $100-$1CF) it goes round the room's cells (the
 * list below, +$10 its place in it) two steps a frame, holding the 3 x 3 cells around its target
 * in the map (+$E) and four doorways always; within 4 of the player it takes his place: he is moved
 * to it, turned 6 to the left and his life is gone (+$26 = 2, fn_S2_2AD2). */
static void hold_around(uint16_t v, uint16_t c) {          /* fn_S4_0164 */
    uint32_t map = og32(OG_MAP);
    static const int16_t at[9] = {-0x420, -0x400, -0x3E0, -0x20, 0, 0x20, 0x3E0, 0x400, 0x420};
    for (int k = 0; k < 9; k++) {
        uint16_t n = (uint16_t)(c + at[k]);
        if (!(r8(map + sxw(n)) & 1)) w16(map + sxw(n) + 0xE, v);
    }
    w16(map + 0x694E, 1); w16(map + 0x6D4E, 1); w16(map + 0x496E, 1); w16(map + 0x456E, 1);
}
static void td_step(Regs *r) {                             /* fn_S4_0090 */
    static const uint16_t way[45] = {0x6540, 0x6560, 0x6580, 0x61A0, 0x5D80, 0x5D60, 0x5D40, 0x5920, 0x5540,
        0x5560, 0x5580, 0x51A0, 0x4D80, 0x4D60, 0x4D40, 0x5120, 0x5520, 0x5940, 0x5960, 0x5980, 0x5DA0, 0x61A0,
        0x6580, 0x6560, 0x0460, 0x6120, 0x5D40, 0x5D60, 0x5D80, 0x59A0, 0x5580, 0x5560, 0x5540, 0x5120, 0x4D40,
        0x4D60, 0x4D80, 0x51A0, 0x55A0, 0x5980, 0x5960, 0x5940, 0x5D20, 0x6120, 0};   /* (sic: $0460) */
    uint32_t o = r->a[4], map = og32(OG_MAP);
    for (;;) {
        uint16_t c = way[r16(o + 0x10) / 2];
        hold_around(1, c);
        g_target(r16(map + sxw(c) + 0x1C), r16(map + sxw(c) + 0x1E));
        fo_S7_1488(r);
        if (!((uint16_t)r->d[0] & 0x8000)) {
            mz_seg_of(7, 0x3722), fn_S7_3722(r);
            set_w(&r->d[0], r16(o + OB_HEADING));
            r->a[0] = OADDR(0x4F30);
            mz_seg_of(7, 0x374C), fn_S7_374C(r);
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            return;
        }
        hold_around(0, c);                                  /* there: the next cell */
        uint16_t i = (uint16_t)(r16(o + 0x10) + 2);
        w16(o + 0x10, way[i / 2] ? i : 0);
    }
}
void fo_S4_0028(Regs *r) {
    uint16_t x = og16(OG_PLAYER_X), y = og16(OG_PLAYER_Y);
    if (x < 0x90 || x >= 0xE8 || y >= 0x1D0 || y < 0x100) return;
    td_step(r); td_step(r);
    near_d0(r);
    if (og16(OG_NEAR_DIST) >= 4) return;
    uint32_t o = r->a[4];
    sog16(OG_PLAYER_X, r16(o + OB_X)); sog16(OG_PLAYER_Y, r16(o + OB_Y)); sog16(OG_PLAYER_CELL, r16(o + OB_CELL));
    sog16(OG_PLAYER_HEADING, (uint16_t)((og16(OG_PLAYER_HEADING) - 6) & 0x3F));
    w16(o + 0x26, 2);
    sog16(OG_LIFE, 0);
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
}

/* ---------- type $E (fn_S4_020C, level 4) ----------
 * While the player is in its room (x $90-$127, y $100-$1CF) it goes for him four steps a frame
 * (straight at him in his cell, else to the free cell around it nearest him, fn_S4_030E) or, while
 * he is in parts of the room, for fixed points; within 4 it takes his place as type $D does
 * (turned 6 to the right). */
static uint16_t te_next_cell(Regs *r, uint16_t tx, uint16_t ty) {   /* fn_S4_030E */
    uint32_t map = og32(OG_MAP), dist = og32(0x53BC);
    uint16_t best = 0xFFFF, cell = 0x5960, c = r16(r->a[4] + OB_CELL);
    #define TRY(n) do { uint16_t n_ = (n); if (!r16(map + sxw(n_) + 0xE)) { \
        uint16_t y_ = (uint16_t)((((uint16_t)(ty - r16(map + sxw(n_) + 0x1E)) >> 1) & 0x7F) << 7); \
        uint16_t x_ = (uint16_t)(((uint16_t)(tx - r16(map + sxw(n_) + 0x1C)) >> 1) & 0x7F); \
        uint16_t w_ = r8(dist + sxw((uint16_t)(y_ | x_))); if (w_ < best) { best = w_; cell = n_; } } } while (0)
    static const int16_t line[4] = {0x20, -0x20, 0x400, -0x400};
    TRY(c);
    for (int k = 0; k < 4; k++) {                           /* each way two cells, unless the first is a wall */
        uint16_t n = (uint16_t)(c + line[k]);
        if (r8(map + sxw(n)) & 1) continue;
        TRY(n);
        TRY((uint16_t)(n + line[k]));
    }
    static const int16_t diag[4] = {-0x420, -0x3E0, 0x3E0, 0x420};
    for (int k = 0; k < 4; k++) TRY((uint16_t)(c + diag[k]));
    #undef TRY
    return cell;
}
void fo_S4_020C(Regs *r) {
    uint32_t map = og32(OG_MAP);
    uint16_t x = og16(OG_PLAYER_X), y = og16(OG_PLAYER_Y), tx, ty;
    if (x < 0x90 || x >= 0x128 || y >= 0x1D0 || y < 0x100) return;
    if (x >= 0xE0) tx = 0xB8, ty = 0x168;
    else if (y < 0x130) tx = 0xC8, ty = 0x148;
    else if (y >= 0x1A0) tx = 0xA8, ty = 0x188;
    else tx = x, ty = y;
    for (int k = 0; k < 4; k++) {                           /* fn_S4_02D4 */
        uint16_t c = te_next_cell(r, tx, ty);
        if (c == og16(OG_PLAYER_CELL)) g_target(og16(OG_PLAYER_X), og16(OG_PLAYER_Y));
        else g_target(r16(map + sxw(c) + 0x1C), r16(map + sxw(c) + 0x1E));
        fo_S7_1488(r);
        if ((uint16_t)r->d[0] & 0x8000) continue;
        r->a[0] = OADDR(0x4F30);
        mz_seg_of(7, 0x374C), fn_S7_374C(r);
        mz_seg_of(7, 0x3784), fn_S7_3784(r);
    }
    near_d0(r);
    if (og16(OG_NEAR_DIST) >= 4) return;
    uint32_t o = r->a[4];
    sog16(OG_PLAYER_X, r16(o + OB_X)); sog16(OG_PLAYER_Y, r16(o + OB_Y)); sog16(OG_PLAYER_CELL, r16(o + OB_CELL));
    sog16(OG_PLAYER_HEADING, (uint16_t)((og16(OG_PLAYER_HEADING) + 6) & 0x3F));
    w16(o + 0x26, 2);
    sog16(OG_LIFE, 0);
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
}

/* ---------- type $F (fn_S4_04AE, record 2 on level 4) ----------
 * It turns the object +$18 bytes on from it (in 0 and 1: 2 a frame, either way by that one's +$18
 * sign) while the player is within $20 (1; beyond: 3, 0). Hit by $19 (states 0-1): 2, the next
 * object ($40 on, wrapping below $500); by $18 / $1A: 4, the object pulled to its place and set
 * going (state 1, +$18 | $4000): a type $23 one starts 6-9: 15 frames, picture $45, 20 frames of
 * $46 / $47 at random, event $F01 to record 3, two frames of $48, the event again; then 8 removes
 * the types $1E, $20 and $2D and itself. */
static void tf_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        switch (s) {
        case 1: near_d0(r); if (og16(OG_NEAR_DIST) < 0x20) return; next = 3; break;
        case 2: {
            uint16_t n = (uint16_t)(r16(o + 0x18) + 0x40);
            w16(o + 0x18, n >= 0x500 ? 0x40 : n);
            w16(o + 0x1A, 1);
            next = 1; break;
        }
        case 3: w16(o + 0x1A, 0xFFFF); next = 0; break;
        case 4: {
            sog16(OG_MOVE_X, r16(o + OB_X)); sog16(OG_MOVE_Y, r16(o + OB_Y)); sog16(OG_MOVE_CELL, r16(o + OB_CELL));
            uint16_t self = og16(OG_SELF), off = r16(o + 0x18);
            sog16(OG_SELF, (uint16_t)(self + off));
            uint32_t p = o + off;
            r->a[4] = p;
            w16(p + 0x18, r16(p + 0x18) | 0x4000);
            w16(og32(OG_MAP) + sxw(r16(p + 0xE)) + 0xE, 0);
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            w16(p + 0x16, 1);
            int t23 = r16(p + 2) == 0x23;
            r->a[4] = o; sog16(OG_SELF, self);
            w16(o + 0x1A, 0xFFFF);
            if (t23) { w16(o + 0x1C, 15); next = 6; } else next = 5;
            break;
        }
        case 6: if (!count_down(r, 0x1C, 1)) return; w16(o + 0x1C, 20); next = 7; break;
        case 7: if (!count_down(r, 0x1C, 1)) return; w16(o + 0x1C, 2); tell(r, 0xF01, 0xC0); next = 9; break;
        case 8: {                                           /* fn_S4_06F4: the types $1E, $20, $2D gone; then itself */
            uint32_t t = og32(OG_OBJECTS);
            for (uint16_t rec = 0x1DC0; rec; rec -= 0x40) {
                uint16_t ty = r16(t + rec + 2);
                if (ty != 0x1E && ty != 0x20 && ty != 0x2D) continue;
                unlink_rec(r, rec);
                w16(t + rec + 4, 0); w16(t + rec + 6, 0); w16(t + rec + 8, 0); w16(t + rec + 0xA, 0);
                w16(t + rec + 0x30, 0); w16(t + rec + 0xE, 0);
                w16(t + rec + 2, ty | 0x8000);
                w16(t + rec + 0x18, r16(t + rec + 0x18) | 0x4000);
            }
            unlink_obj(r);
            w16(o + OB_X, 0); w16(o + OB_Y, 0); w16(o + OB_CELL, 0);
            w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000);
            next = 5; break;
        }
        case 9: if (!count_down(r, 0x1C, 1)) return; tell(r, 0xF01, 0xC0); next = 8; break;
        default: return;                                    /* 0, 5 */
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_04AE(Regs *r) {
    static const uint16_t hits[] = {0x18, 4, 0x19, 2, 0x1A, 4, 0xFFFF};
    uint32_t o = r->a[4], a6 = r->a[6];
    r->a[6] = r->a[7] - 4;                                  /* LINK A6, #-6 */
    r->a[7] -= 10;
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    tf_think(r);
    o_hit(r, (uint16_t)r->d[7] <= 1 ? hits : NULL, tf_think);
    switch ((uint16_t)r->d[7]) {
    case 1: {                                               /* turning the object it points at */
        uint32_t p = o + r16(o + 0x18);
        uint16_t v = r16(p + 0x18);
        uint16_t h = (uint16_t)(((v & 0x8000) ? v - 2 : v + 2) & 0x3F);
        w16(o + OB_HEADING, h);
        w16(p + 0x18, (uint16_t)((v & 0xFF00) | h));
        break;
    }
    case 6: set_pic(r, 0x45); break;
    case 7: { random_d0(r); static const uint16_t pic[2] = {0x46, 0x47}; set_pic(r, pic[(uint16_t)r->d[0] & 1]); break; }
    case 9: set_pic(r, 0x48); break;
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
    r->a[7] += 10; r->a[6] = a6;
}

/* ---------- type $11 (fn_S9_0046, level 2) ----------
 * While record 8's +$1E is set it turns (+$C through 0-19) and bends the shots near it (types
 * $13 and $17-$1A in the 3 x 3 cells around: one coming within $14 of straight at it, from 3 to
 * $F away, is turned two steps towards it, the abilities' shots six); either way it pulls in the
 * player within 3 (moving onto him, +$26 = 2) unless he is against a wall. Record 8 clear: state -1,
 * and once set again it only pulls until its state (counting up) is no longer negative. */
static int t11_player_at_wall(void) {                       /* fn_S9_00C2: within 4 / 5 of a wall cell around */
    uint32_t c = og32(OG_MAP) + og16(OG_PLAYER_CELL);
    uint16_t px = og16(OG_PLAYER_X), py = og16(OG_PLAYER_Y), cx = r16(c + 0x1C), cy = r16(c + 0x1E);
    #define WALL(o) (r8(c + (uint32_t)(int32_t)(o)) & 1)
    if (WALL(0x20) && (uint16_t)(cx + 4) < px) return 1;
    if (WALL(0x400) && (uint16_t)(cy + 4) < py) return 1;
    if (WALL(-0x20) && (uint16_t)(cx - 5) >= px) return 1;
    if (WALL(-0x400) && (uint16_t)(cy - 5) >= py) return 1;
    if (WALL(0x420) && (uint16_t)(cx + 4) < px && (uint16_t)(cy + 4) < py) return 1;
    if (WALL(0x3E0) && (uint16_t)(cx - 5) >= px && (uint16_t)(cy + 4) < py) return 1;
    if (WALL(-0x420) && (uint16_t)(cx - 5) >= px && (uint16_t)(cy - 5) >= py) return 1;
    if (WALL(-0x3E0) && (uint16_t)(cx + 4) < px && (uint16_t)(cy - 5) >= py) return 1;
    #undef WALL
    return 0;
}
static void t11_pull(Regs *r) {                             /* fn_S9_0086 */
    near_d0(r);
    if (og16(OG_NEAR_DIST) >= 3 || t11_player_at_wall()) return;
    sog16(OG_MOVE_X, og16(OG_PLAYER_X)); sog16(OG_MOVE_Y, og16(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
    mz_seg_of(7, 0x3972), fn_S7_3972(r);
    if ((uint16_t)r->d[0]) return;
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
    w16(r->a[4] + 0x26, 2);
}
static void t11_bend(Regs *r, uint32_t shot) {             /* fn_S9_028A */
    uint32_t o = r->a[4];
    uint16_t ty = r16(shot + 2);
    if (!((ty >= 0x17 && ty < 0x1B) || ty == 0x13)) return;
    uint16_t i = (uint16_t)((((r16(o + OB_Y) - r16(shot + 8)) & 0x7F) << 7) | ((r16(o + OB_X) - r16(shot + 4)) & 0x7F));
    uint16_t dir = r8(og32(0x53C0) + sxw(i));
    if (r16(shot + 0xC) & 0x8000) { w16(shot + 0xC, dir); return; }
    set_w(&r->d[0], (uint16_t)(dir - r16(shot + 0xC)));
    mz_seg_of(7, 0x367E), fn_S7_367E(r);
    if ((uint16_t)r->d[0] >= 0x14) return;
    uint16_t d = r8(og32(0x53BC) + sxw(i));
    if (d < 3 || d >= 0x10) return;
    r->a[4] = shot;                                         /* the shot turned towards it */
    r->a[0] = og32(0x53C0);
    set_w(&r->d[0], dir);
    for (int k = ty >= 0x17 ? 6 : 2; k; k--) mz_seg_of(7, 0x36F4), fn_S7_36F4(r);
    r->a[4] = o;
}
void fo_S9_0046(Regs *r) {
    uint32_t o = r->a[4];
    if (!r16(og32(OG_OBJECTS) + 0x21E)) { w16(o + OB_STATE, 0xFFFF); t11_pull(r); return; }
    int16_t st = (int16_t)r16(o + OB_STATE);
    if (st < 0) { w16(o + OB_STATE, (uint16_t)(st + 1)); if (st + 1 < 0) { t11_pull(r); return; } }
    uint16_t t = (uint16_t)(r16(o + OB_HEADING) + 1);
    w16(o + OB_HEADING, t >= 0x14 ? 0 : t);
    t11_pull(r);
    uint32_t map = og32(OG_MAP), tbl = og32(OG_OBJECTS);       /* fn_S9_01C4: the 3 x 3 cells around */
    static const int16_t at[9] = {-0x420, -0x400, -0x3E0, -0x20, 0, 0x20, 0x3E0, 0x400, 0x420};
    for (int k = 0; k < 9; k++) {
        uint16_t c = (uint16_t)(r16(o + OB_CELL) + at[k]);
        for (uint32_t j = 1; j <= 10; j++) {
            uint8_t n = r8(map + sxw(c) + j);
            if (!n) break;
            t11_bend(r, tbl + ((uint32_t)n << 6));
        }
    }
}

/* ---------- types $2C and $2B (fn_S9_1CAC, fn_S9_1D0C, level 3) ----------
 * Passages within level 3: the player crossing y $98 (from his last y, G_56CA) with x $56-$5A
 * (type $2C) is moved $B0 right and $C0 down; crossing y $158 with x $106-$10A (type $2B), as far
 * back (G_57DC = 3, the level, and the move made at the frame's end, G_57DE / G_57F2). Type $2C
 * also sets G_56EE by the player's y (fn_S9_1D68: $18 above $90, $1E above $A0, else $2A). */
static void passage(uint16_t x0, uint16_t y, int16_t dx, int16_t dy) {
    uint16_t px = og16(OG_PLAYER_X), py = og16(OG_PLAYER_Y), last = og16(0x56CA);
    if (px < x0 || px >= x0 + 5) return;
    if (!(y >= py ? y < last : y >= last)) return;           /* not crossed */
    sog16(OG_GO_LEVEL, 3);
    sog16(OG_GO_X, (uint16_t)(px + dx)); sog16(OG_GO_Y, (uint16_t)(py + dy));
    sog16(OG_GO_HEADING, og16(OG_PLAYER_HEADING));
    sog8(0x57DE, 1); sog8(0x57F2, 1);
}
void fo_S9_1CAC(Regs *r) {
    (void)r;
    uint16_t py = og16(OG_PLAYER_Y);
    sog16(0x56EE, py >= 0xA0 ? 0x2A : py >= 0x90 ? 0x1E : 0x18);
    passage(0x56, 0x98, 0xB0, 0xC0);
}
void fo_S9_1D0C(Regs *r) { (void)r; passage(0x106, 0x158, -0xB0, -0xC0); }

/* ---------- type $32 (fn_S9_1E46, level 3) ----------
 * A talker (texts $32xx): its mouth moves (fn_S7_22EE); with George there (G_56C2) and the player
 * within 7, looking at it within $A, it turns to him and says $3201 (flag word 0 bit 2), then
 * $3202 (heading $10) while he stays within $14 and looks within $1A; going away: 5, event $3202
 * to record 4, then 4 (waiting, heading $10). Event $3201 to record 4 (3), a wait of 5 frames (6-7)
 * and $3203, $3204 (8-9) while the player stays in the box fn_S7_22BC names; then 5. */
static void talk_mouth(Regs *r) {                           /* fn_S7_22EE: a new mouth every 16 frames */
    uint32_t o = r->a[4];
    uint16_t t = (uint16_t)(r16(o + 0x18) + 1);
    if (t >= 0x10) { random_d0(r); w16(o + 0x1A, (uint16_t)(((uint16_t)r->d[0] & 3) << 4)); t = 0; }
    w16(o + 0x18, t);
}
static uint16_t t32_where(void) {                           /* fn_S7_22BC: 0 above y $A0, 1 in x $D0-$13F / y $120-$19F, else 2 */
    uint16_t x = og16(OG_PLAYER_X), y = og16(OG_PLAYER_Y);
    if (y < 0xA0) return 0;
    return y >= 0x120 && y < 0x1A0 && x >= 0xD0 && x < 0x140 ? 1 : 2;
}
static int t32_watched(Regs *r, uint16_t within, uint16_t look) {
    near_d0(r);
    if (og16(OG_NEAR_DIST) >= within) return 0;
    set_w(&r->d[0], (uint16_t)(og16(OG_NEAR_DIR) - r16(r->a[4] + OB_HEADING)));
    mz_seg_of(7, 0x367E), fn_S7_367E(r);
    return (uint16_t)r->d[0] < look;
}
static void t32_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        switch (s) {
        case 0: if (!t32_watched(r, 7, 0xA) || !og16(OG_GEORGE_JOINED)) return; next = 1; break;
        case 1: case 2: if (t32_watched(r, 0x14, 0x1A)) return; next = 5; break;
        case 3: tell(r, 0x3201, 0x100); next = 6; break;
        case 5: tell(r, 0x3202, 0x100); next = 4; break;
        case 6: w16(o + 0x1C, 4); next = 7; break;
        case 7: if (!count_down(r, 0x1C, 1)) return; next = 8; break;
        case 8: case 9: if (t32_where()) return; next = 5; break;
        default: return;                                    /* 4 */
        }
        set_w(&r->d[0], next);
    }
}
static void t32_turn(Regs *r, uint16_t to) { set_w(&r->d[0], to); mz_seg_of(7, 0x36F4), fn_S7_36F4(r); }
void fo_S9_1E46(Regs *r) {
    static const uint16_t said[10][2] = {[1] = {0xB201, 2}, [2] = {0xB202, 3}, [8] = {0xB203, 9}, [9] = {0xB204, 5}};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t32_think(r);
    uint16_t s = (uint16_t)r->d[7];
    if (s < 10) o_said(r, said[s], t32_think);
    switch ((uint16_t)r->d[7]) {
    case 0: talk_mouth(r); break;
    case 1: talk_mouth(r); sog16(OG_MESSAGE, 0xB201); flag_set(r, 0, 4); t32_turn(r, og16(OG_NEAR_DIR)); break;
    case 2: talk_mouth(r); sog16(OG_MESSAGE, 0xB202); t32_turn(r, 0x10); break;
    case 4: case 7: talk_mouth(r); t32_turn(r, 0x10); break;
    case 8: talk_mouth(r); sog16(OG_MESSAGE, 0xB203); t32_turn(r, 0x10); break;
    case 9: talk_mouth(r); sog16(OG_MESSAGE, 0xB204); t32_turn(r, 0x10); break;
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $34 (fn_S9_2072, level 3) ----------
 * With +$C clear it walks a fixed way round (four pixels a frame, one axis at a time; at the end
 * back to $58, $98 and round again); with +$C set it goes to $108, $15B a pixel a frame and then
 * stands at $108, $164. Once flag word 0 bit 2 is set it is gone. */
static void t34_move(Regs *r) { mz_seg(7), fo_S7_155C(r); mz_seg_of(7, 0x3784), fn_S7_3784(r); }
static void t34_toward(Regs *r, uint16_t tx, uint16_t ty) { /* a pixel along x, else along y; 0 if there */
    uint32_t o = r->a[4];
    sog16(OG_MOVE_X, r16(o + OB_X)); sog16(OG_MOVE_Y, r16(o + OB_Y));
    int16_t d = (int16_t)(tx - r16(o + OB_X));
    if (d) sog16(OG_MOVE_X, (uint16_t)(og16(OG_MOVE_X) + (d < 0 ? -1 : 1)));
    else if ((d = (int16_t)(ty - r16(o + OB_Y))) != 0) sog16(OG_MOVE_Y, (uint16_t)(og16(OG_MOVE_Y) + (d < 0 ? -1 : 1)));
}
void fo_S9_2072(Regs *r) {
    static const uint16_t way[16] = {0x58, 0x78, 0x38, 0x78, 0x38, 0x58, 0x58, 0x58, 0x58, 0x28, 0x38, 0x28, 0x38, 0x10, 0, 0};
    uint32_t o = r->a[4];
    if (flag(r, 0, 4)) { unlink_obj(r); w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000); return; }
    if (r16(o + OB_HEADING)) {                                     /* fn_S9_2140 */
        if (r16(o + OB_X) == 0x108 && r16(o + OB_Y) == 0x15B) { sog16(OG_MOVE_X, 0x108); sog16(OG_MOVE_Y, 0x164); }
        else t34_toward(r, 0x108, 0x15B);
        t34_move(r);
        return;
    }
    for (int k = 0; k < 4; k++) {                           /* fn_S9_20B0 */
        for (;;) {
            uint16_t i = r16(o + 0x10) / 2;
            if (!way[i]) { sog16(OG_MOVE_X, 0x58); sog16(OG_MOVE_Y, 0x98); t34_move(r); w16(o + 0x10, 0); break; }
            if (r16(o + OB_X) != way[i] || r16(o + OB_Y) != way[i + 1]) { t34_toward(r, way[i], way[i + 1]); t34_move(r); break; }
            w16(o + 0x10, (uint16_t)(r16(o + 0x10) + 4));
        }
    }
}

/* ---------- type $35 (fn_S9_219A, level 3) ----------
 * A ghost in the box fn_S7_22BC names: away from the player (0) until he is out of it (2), then a
 * wait of 16-79 frames, and it appears near him (3: within $20 either way, +$1A counting its
 * appearances), facing him for a frame and 50 more (4-5), then gone (6-8, off to 0, 0) and back to
 * 1. From its thirteenth appearance on (9): 10, taking $28 from the player's life and pushing him on
 * along his heading until he is clear (11: fn_S7_15B8), facing him (12), then 20. Event $3201 from
 * type $32 sends it to 11, $3202 to 19-15 (200 frames off the map) and 16: back at $58, $98 while
 * the player is above y $A0 (no full view), following his scent (17; under a full view 18) until
 * near him: 9. */
static void face_player(Regs *r) { near_d0(r); if (!(og16(OG_NEAR_DIR) & 0x8000)) w16(r->a[4] + OB_HEADING, og16(OG_NEAR_DIR)); }
static void move_to(Regs *r, uint16_t x, uint16_t y, uint16_t c) {   /* G_382C / G_3828 / G_3824 and fn_S7_1578 */
    sog16(OG_MOVE_X, x); sog16(OG_MOVE_Y, y); sog16(OG_MOVE_CELL, c);
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
}
static void t35_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        switch (s) {
        case 0: if (t32_where() != 2) return; next = 1; break;
        case 1: random_d0(r); w16(o + 0x1C, (uint16_t)(((uint16_t)r->d[0] & 0x3F) + 0x10)); next = 2; break;
        case 2: if (t32_where() != 2) { next = 0; break; } if (!count_down(r, 0x1C, 1)) return; next = 3; break;
        case 3:
            if (r16(o + 0x1A) >= 0xD) { next = 9; break; }
            random_d0(r); sog16(OG_MOVE_X, (uint16_t)(((uint16_t)r->d[0] & 0x3F) - 0x20 + og16(OG_PLAYER_X)));
            random_d0(r); sog16(OG_MOVE_Y, (uint16_t)(((uint16_t)r->d[0] & 0x3F) - 0x20 + og16(OG_PLAYER_Y)));
            mz_seg(7), fo_S7_155C(r);
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            w16(o + 0x1C, 1);
            next = 4; break;
        case 4: case 5:
            if (t32_where() != 2) { next = 6; break; }
            if (count_down(r, 0x1C, 1)) {
                if (s == 5) { next = 6; break; }
                w16(o + 0x1C, 0x32); w16(o + 0x18, 5); next = 5; break;
            }
            if (s == 4) w16(o + 0x18, 0xF);
            near_d0(r);
            if (og16(OG_NEAR_DIR) & 0x8000) { next = 6; break; }
            w16(o + OB_HEADING, og16(OG_NEAR_DIR));
            return;
        case 6: w16(o + 0x1C, 1); next = 7; break;
        case 7:
            if (!count_down(r, 0x1C, 1)) { w16(o + 0x18, 0xF); return; }
            w16(o + 0x1A, (uint16_t)(r16(o + 0x1A) + 1));
            next = 8; break;
        case 8: move_to(r, 0, 0, 0); next = 1; break;
        case 9: w16(o + 0x1A, 6); next = 0xA; break;
        case 11:                                            /* onto the player, pushing him on until clear */
            move_to(r, og16(OG_PLAYER_X), og16(OG_PLAYER_Y), og16(OG_PLAYER_CELL));
            r->a[0] = OADDR(0x4DF0); set_w(&r->d[0], og16(OG_PLAYER_HEADING));
            mz_seg_of(7, 0x374C), fn_S7_374C(r); mz_seg_of(7, 0x3784), fn_S7_3784(r);
            do {
                r->a[0] = OADDR(0x4F30); set_w(&r->d[0], og16(OG_PLAYER_HEADING));
                mz_seg_of(7, 0x374C), fn_S7_374C(r); mz_seg_of(7, 0x3784), fn_S7_3784(r);
                sog16(0x3820, og16(OG_SELF)); sog16(0x3822, 4);
                mz_seg_of(7, 0x37C4), fn_S7_37C4(r);
            } while ((uint16_t)r->d[0]);
            w16(o + 0x1C, 1);
            next = 0xC; break;
        case 12:
            if (count_down(r, 0x1C, 1)) { w16(o + 0x18, 5); next = 0x14; break; }
            w16(o + 0x18, 0xF);
            face_player(r);
            return;
        case 13: case 15: if (!count_down(r, 0x1C, 1)) return; next = (uint16_t)(s + 1); break;
        case 14: move_to(r, 0, 0, 0); w16(o + 0x1C, 0xA); next = 0xF; break;
        case 16:
            if (og16(OG_FULL_VIEW) >= 0x80) return;
            if (t32_where()) { next = 9; break; }
            sog16(OG_MOVE_X, 0x58); sog16(OG_MOVE_Y, 0x98);
            mz_seg(7), fo_S7_155C(r);
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            w16(o + 0xE, r16(o + OB_CELL));
            next = 0x11; break;
        case 17:
            w16(o + 0x18, 5);
            if (og16(OG_FULL_VIEW) >= 0x80) { next = 0x12; break; }
            mz_seg_of(7, 0x3D2A), fn_S7_3D2A(r);
            if (!((uint16_t)r->d[0] & 0x8000)) return;
            next = 9; break;
        case 18: if (og16(OG_FULL_VIEW) >= 0x80) return; next = 0x11; break;
        case 19: w16(o + 0x1C, 0xC8); next = 0xD; break;
        default: return;                                    /* 10, 20 */
        }
        set_w(&r->d[0], next);
    }
}
void fo_S9_219A(Regs *r) {
    static const uint16_t e0[] = {0x3201, 11, 0x3202, 19, 0xFFFF}, e2[] = {0x3202, 19, 0xFFFF};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t35_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_event(r, s == 0 ? e0 : s == 12 || s == 20 ? e2 : NULL, t35_think);
    switch ((uint16_t)r->d[7]) {
    case 7: near_d0(r); if (!(og16(OG_NEAR_DIST) & 0x8000)) w16(o + OB_HEADING, og16(OG_NEAR_DIST)); break;   /* (sic: the distance) */
    case 10: {                                              /* on the player: $28 from his life */
        w16(o + 0x26, 1);
        int16_t t = (int16_t)(r16(o + 0x1A) - 1);
        w16(o + 0x1A, (uint16_t)(t < 0 ? 0 : t));
        w16(o + 0x18, 0x10);
        sog32(OG_MOVE_X, og32(OG_PLAYER_X)); sog32(OG_MOVE_Y, og32(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
        mz_seg_of(7, 0x3784), fn_S7_3784(r);
        int16_t l = (int16_t)(og16(OG_LIFE) - 0x28);
        sog16(OG_LIFE, (uint16_t)(l < 0 ? 0 : l));
        break;
    }
    case 13: case 20: face_player(r); break;
    case 17:                                                /* along his scent */
        if (og16(OG_PLAYER_CELL) != og16(0x582C)) { sog16(0x56A6, og16(OG_PLAYER_CELL)); sog16(0x56A8, 0x1A); refresh(r); }
        mz_seg_of(7, 0x3D5C), fn_S7_3D5C(r);
        { uint16_t d = (uint16_t)r->d[0];
          fn_S7_3D0E(r);
          set_w(&r->d[0], d);
          if (d & 0x8000) break; }
        mz_seg_of(7, 0x3722), fn_S7_3722(r);
        if ((uint16_t)r->d[0] != r16(o + OB_HEADING)) break;
        r->a[0] = OADDR(0x4F30);
        mz_seg_of(7, 0x374C), fn_S7_374C(r); mz_seg_of(7, 0x3784), fn_S7_3784(r);
        break;
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $28 (fn_S9_1738, level 2) ----------
 * Level 2's type $29: grows (+$C in record 0's +$1C, fn_S9_1908) and is hit smaller / bigger the
 * same way; with the player at x $1EE on, y $74-$7A (no full view): 4-5, 7 frames of full view
 * $82 and the way on to level 3 (at $38, $14, turned $10 to the right). +$C negative: 3, gone from
 * the map: records 15-18 get +$18 = $7FFF, flag word 0 bit 3, and two frames later (6) its type
 * is negative. Under the level's start (cells below $2800, no full view) it sets G_56EE = 8. */
static void t28_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        switch (s) {
        case 0:
            if (r16(o + OB_HEADING) & 0x8000) { next = 3; break; }
            if (og16(OG_FULL_VIEW) >= 0x80 || og16(OG_PLAYER_X) < 0x1EE || og16(OG_PLAYER_Y) < 0x74 || og16(OG_PLAYER_Y) >= 0x7B) return;
            next = 4; break;
        case 1: next = t29_smaller(r); break;
        case 2: t29_bigger(r); next = 0; break;
        case 3: {
            sog16(0x56EE, 0x2B);
            unlink_obj(r);
            w16(o + OB_X, 0); w16(o + OB_Y, 0); w16(o + OB_CELL, 0);
            uint32_t t = og32(OG_OBJECTS);
            for (uint16_t rec = 0x3C0; rec <= 0x480; rec += 0x40) w16(t + rec + 0x18, 0x7FFF);
            flag_set(r, 0, 8);
            w16(o + 0x12, 2);
            next = 6; break;
        }
        case 4: w16(o + 0x12, 7); next = 5; break;
        case 5:
            sog16(OG_FULL_VIEW, 0x82);
            w16(o + 0x12, (uint16_t)(r16(o + 0x12) - 1));
            if (r16(o + 0x12)) return;
            sog16(OG_GO_LEVEL, 3); sog32(OG_GO_X, 0x380000); sog32(OG_GO_Y, 0x140000);
            sog16(OG_GO_HEADING, (uint16_t)((og16(OG_PLAYER_HEADING) + 0x10) & 0x3F)); sog8(0x57DE, 1); sog8(0x57F2, 1);
            next = 0; break;
        case 6: if (count_down(r, 0x12, 1)) w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000); return;
        default: return;
        }
        set_w(&r->d[0], next);
    }
}
void fo_S9_1738(Regs *r) {
    static const uint16_t hits[] = {0x19, 2, 0x1A, 1, 0xFFFF};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    w16(o + OB_HEADING, r16(og32(OG_OBJECTS) + 0x1C));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t28_think(r);
    o_hit(r, (uint16_t)r->d[7] == 0 ? hits : NULL, t28_think);
    switch ((uint16_t)r->d[7]) {
    case 0:
        t29_grow(r);
        if (og16(OG_FULL_VIEW) < 0x80 && og16(OG_PLAYER_CELL) < 0x2800) sog16(0x56EE, 8);   /* fn_S9_1954 */
        break;
    case 5: t29_grow(r); break;
    case 6: sog16(0x56EE, 0x2B); break;
    }
    w16(og32(OG_OBJECTS) + 0x1C, r16(o + OB_HEADING));
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $16 (fn_S11_00C0, level 6) ----------
 * Hit by type $17 in state 0: 1, which sets its +$12 to 1 and becomes 2. Nothing else. */
static void t16_think(Regs *r) {
    if ((uint16_t)r->d[0] == 1) { w16(r->a[4] + 0x12, 1); set_w(&r->d[0], 2); }
    set_w(&r->d[7], (uint16_t)r->d[0]);
}
void fo_S11_00C0(Regs *r) {
    static const uint16_t hits[] = {0x17, 1, 0xFFFF};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t16_think(r);
    o_hit(r, (uint16_t)r->d[7] == 0 ? hits : NULL, t16_think);
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- types $38 and $39 (fn_S11_019A, fn_S11_0382, level 6) ----------
 * Two actors in level 6's ending, moved by record 9's events. $38: event $3A01 puts it at $A8, $82
 * and it walks to $A8, $9B (step table G_4B70), then sends record 9 event $3801 and turns to the
 * player; event $3A03: 5, facing record 8; $3A04 / $3A06: gone. $39: event $3A02 puts it at $A8,
 * $A6 (heading $10), talking (fn_S7_22EE) until event $3A04 (3: it turns to the player and is gone,
 * event $3A05 to record 9) or a hit by type $17 (4, the same without the turn). */
static void gone_from_map(Regs *r) {                        /* out of its cell, type negative */
    uint32_t o = r->a[4];
    unlink_obj(r);
    w16(o + OB_X, 0); w16(o + 6, 0); w16(o + OB_Y, 0); w16(o + 0xA, 0); w16(o + OB_CELL, 0);
    w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000);
}
static void t38_think(Regs *r) {
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        switch (s) {
        case 1: sog16(OG_MOVE_X, 0xA8); sog16(OG_MOVE_Y, 0x82); t34_move(r); next = 2; break;
        case 2:
            g_target(0xA8, 0x9B);
            fo_S7_1488(r);
            if (!((uint16_t)r->d[0] & 0x8000)) {
                mz_seg_of(7, 0x3722), fn_S7_3722(r);
                r->a[0] = OADDR(0x4B70);
                mz_seg_of(7, 0x374C), fn_S7_374C(r);
                mz_seg_of(7, 0x3784), fn_S7_3784(r);
                return;
            }
            next = 3; break;
        case 3: tell(r, 0x3801, 0x240); next = 4; break;
        case 6: gone_from_map(r); return;
        default: return;                                    /* 0, 4, 5 */
        }
        set_w(&r->d[0], next);
    }
}
void fo_S11_019A(Regs *r) {
    static const uint16_t e0[] = {0x3A01, 1, 0xFFFF}, e4[] = {0x3A03, 5, 0xFFFF}, e5[] = {0x3A04, 6, 0x3A06, 6, 0xFFFF};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t38_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_event(r, s == 0 ? e0 : s == 4 ? e4 : s == 5 ? e5 : NULL, t38_think);
    switch ((uint16_t)r->d[7]) {
    case 4: near_d0(r); set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r); break;
    case 5: {                                               /* facing record 8 */
        uint32_t t = og32(OG_OBJECTS);
        if (!r16(t + 0x204)) break;
        g_target(r16(t + 0x204), r16(t + 0x208));
        fo_S7_1488(r);
        mz_seg_of(7, 0x3722), fn_S7_3722(r);
        break;
    }
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}
static void t39_gone(Regs *r) { gone_from_map(r); tell(r, 0x3A05, 0x240); }
static void t39_think(Regs *r) {
    for (;;) {
        uint16_t s = (uint16_t)r->d[0];
        set_w(&r->d[7], s);
        if (s == 1) { sog16(OG_MOVE_X, 0xA8); sog16(OG_MOVE_Y, 0xA6); t34_move(r); w16(r->a[4] + OB_HEADING, 0x10); set_w(&r->d[0], 2); continue; }
        if (s == 4) t39_gone(r);
        return;
    }
}
void fo_S11_0382(Regs *r) {
    static const uint16_t e0[] = {0x3A02, 1, 0xFFFF}, e2[] = {0x3A04, 3, 0xFFFF}, h2[] = {0x17, 4, 0xFFFF};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t39_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_event(r, s == 0 ? e0 : s == 2 ? e2 : NULL, t39_think);
    o_hit(r, (uint16_t)r->d[7] == 2 ? h2 : NULL, t39_think);
    switch ((uint16_t)r->d[7]) {
    case 2: talk_mouth(r); break;
    case 3:                                                 /* turned to the player, then gone (the act runs on into 4's think) */
        near_d0(r); set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x36F4), fn_S7_36F4(r);
        t39_gone(r);
        break;
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $23 (fn_S4_1670, record 3 on level 4) ----------
 * A talker at the door of a little room (x $90-$CF, y $A0-$AF; fn_S4_1C86: 0 with the player
 * outside it or a full view, -1 in its west part, 1 east of x $B9). The player stepping into the
 * west part is pushed out east (13-15, 17: fn_S4_1CC2, a type $2A object left where he was, two
 * pixels a frame to x $BE). In the east part with George there and the player looking at it within
 * 7 it says $2301 (3), $2302 (4) and, once he comes back (flag word 0 bit 1), $2303 / $2304 (8-9:
 * flag word 1 bit 4). Event $F01 (from type $F) in 1 starts 19 (picture $49), again: 20, the end:
 * without George it goes in 15 frames, with him it turns to the player and says $2305-$230D (21-29,
 * hits by $17 / $19 moving on, $18 ending it) until he is $20 away; then (30-32) flag word 0 bit 4,
 * picture $49 for 3 frames, and gone. State 18 (a hit by $18) pushes the player on east (fn_S4_1D2E). */
static int t23_where(void) {
    uint16_t x = og16(OG_PLAYER_X), y = og16(OG_PLAYER_Y);
    if (og16(OG_FULL_VIEW) >= 0x80 || y < 0xA0 || y >= 0xB0 || x < 0x90 || x >= 0xD0) return 0;
    return x >= 0xB9 ? 1 : -1;
}
static void t23_player_to(Regs *r, uint16_t x) {          /* the player at x (his cell kept in the map's +$C), fn_S2_2AD2 */
    uint32_t map = og32(OG_MAP);
    uint16_t c = (uint16_t)(((x & 0x1F0) << 1) | ((og16(OG_PLAYER_Y) & 0x1F0) << 6));
    sog16(OG_PLAYER_X, x);
    if (c != og16(OG_PLAYER_CELL)) { w16(map + sxw(og16(OG_PLAYER_CELL)) + 0xC, 0); w16(map + sxw(c) + 0xC, 1); sog16(OG_PLAYER_CELL, c); }
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
}
static int t23_leave_mark(Regs *r) {                        /* a type $2A object where the player is: its record or 0 */
    sog16(OG_MOVE_X, og16(OG_PLAYER_X)); sog16(OG_MOVE_Y, og16(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
    int rec = make_object(r, 0x2A, 0, 0);
    return rec < 0 ? 0 : rec;
}
static int t23_push(Regs *r) {                              /* fn_S4_1CC2: 1 while not yet out */
    t23_leave_mark(r);
    t23_player_to(r, (uint16_t)(og16(OG_PLAYER_X) + 2));
    return og16(OG_PLAYER_X) < 0xBE;
}
static void t23_push_on(Regs *r) {                          /* fn_S4_1D2E */
    int rec = t23_leave_mark(r);
    if (rec) w16(og32(OG_OBJECTS) + rec + 0x12, 2);
    int w = t23_where();
    if (!w) return;
    uint16_t x = og16(OG_PLAYER_X);
    if (w > 0) x = (uint16_t)(x + 3);
    sog16(OG_MOVE_X, x);
    uint16_t c = (uint16_t)(((x & 0x1F0) << 1) | ((og16(OG_PLAYER_Y) & 0x1F0) << 6));
    if (r8(og32(OG_MAP) + sxw(c)) & 1) { mz_seg_of(2, 0x3500), fn_S2_3500(r); return; }
    t23_player_to(r, x);
}
static void t23_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        int w;
        set_w(&r->d[7], s);
        switch (s) {
        case 0: if (t23_where() <= 0) return; next = 2; break;
        case 2:
            w = t23_where();
            if (w < 0) { next = 0xD; break; }
            if (!w) { next = 0; break; }
            if (!og16(OG_GEORGE_JOINED)) return;
            near_d0(r);
            set_w(&r->d[0], (uint16_t)((og16(OG_NEAR_DIR) ^ 0x20) - og16(OG_PLAYER_HEADING)));
            mz_seg_of(7, 0x367E), fn_S7_367E(r);
            if ((uint16_t)r->d[0] >= 7) return;
            next = 3; break;
        case 3: case 4: case 6: case 9: case 10: {          /* out of the room / into its west part */
            static const uint8_t out[11] = {[3] = 0x10, [4] = 7, [6] = 7, [9] = 0xB, [10] = 0xB};
            static const uint8_t west[11] = {[3] = 0xE, [4] = 0xF, [6] = 0xF, [9] = 0x11, [10] = 0x11};
            w = t23_where();
            if (w > 0) return;
            next = w ? west[s] : out[s];
            break;
        }
        case 5: next = flag(r, 0, 2) ? 8 : 6; break;
        case 7: if (t23_where() <= 0) return; next = 6; break;
        case 8:
            flag_set(r, 0, 2); flag_set(r, 1, 0x10);
            w = t23_where();
            if (w > 0) return;
            next = w ? 0x11 : 0xB; break;
        case 11: if (t23_where() <= 0) return; next = 0xA; break;
        case 12: flag_set(r, 0, 2); next = 4; break;
        case 13: case 14: case 15: case 17: {               /* pushed out east, then on */
            static const uint8_t done[18] = {[14] = 4, [15] = 6, [17] = 0xA};
            if (t23_push(r)) { t23_push(r); return; }
            next = s == 13 ? (og16(OG_GEORGE_JOINED) ? 3 : 2) : done[s];
            break;
        }
        case 16: if (t23_where() <= 0) return; next = 4; break;
        case 20:
            sog16(0x56EE, 0x2B);
            w16(o + 0x3A, 0x3E);
            if (og16(OG_GEORGE_JOINED)) { next = 0x15; break; }
            w16(o + 0x1A, 0xF); next = 0x21; break;
        case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29:
            near_d0(r);
            if (og16(OG_NEAR_DIST) < 0x20) return;
            next = 0x1E; break;
        case 30: w16(o + 0x1A, 2); flag_set(r, 0, 0x10); next = 0x1F; break;
        case 31: case 33: if (!count_down(r, 0x1A, 1)) return; next = s == 31 ? 0x20 : 0x1E; break;
        case 32:
            unlink_obj(r);
            w16(o + OB_X, 0); w16(o + OB_Y, 0); w16(o + OB_CELL, 0);
            w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000);
            return;
        default: return;                                    /* 1, 18, 19 */
        }
        set_w(&r->d[0], next);
    }
}
void fo_S4_1670(Regs *r) {
    static const uint16_t said[34][2] = {[3] = {0xA301, 4}, [4] = {0xA302, 5}, [8] = {0xA303, 9}, [9] = {0xA304, 0xA},
        [21] = {0xA305, 22}, [22] = {0xA306, 23}, [23] = {0xA307, 24}, [24] = {0xA308, 25}, [25] = {0xA309, 26},
        [26] = {0xA30A, 27}, [27] = {0xA30B, 28}, [28] = {0xA30C, 29}, [29] = {0xA30D, 30}};
    static const uint16_t h18[] = {0x18, 0x12, 0xFFFF}, h3[] = {0x17, 0xC, 0x18, 0x12, 0x19, 4, 0x1A, 4, 0xFFFF},
        h4[] = {0x17, 8, 0x18, 0x12, 0x19, 5, 0x1A, 5, 0xFFFF}, h6[] = {0x17, 8, 0x18, 0x12, 0xFFFF},
        h8[] = {0x17, 9, 0x18, 0x12, 0x19, 9, 0x1A, 0xA, 0xFFFF}, h9[] = {0x17, 0xA, 0x18, 0x12, 0x19, 0xA, 0x1A, 0xA, 0xFFFF};
    static uint16_t hs[7];
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t23_think(r);
    uint16_t s = (uint16_t)r->d[7];
    if (s < 34) o_said(r, said[s], t23_think);
    s = (uint16_t)r->d[7];
    const uint16_t *h = NULL;
    switch (s) {
    case 0: case 2: case 7: case 10: case 11: case 16: h = h18; break;
    case 3: h = h3; break;
    case 4: h = h4; break;
    case 6: h = h6; break;
    case 8: h = h8; break;
    case 9: h = h9; break;
    default:
        if (s >= 21 && s <= 29) {                           /* the closing texts: $17 / $19 on, $18 to the end */
            hs[0] = 0x17; hs[1] = (uint16_t)(s + 1); hs[2] = 0x18; hs[3] = 0x1E; hs[4] = 0x19; hs[5] = (uint16_t)(s + 1); hs[6] = 0xFFFF;
            h = hs;
        }
    }
    o_hit(r, h, t23_think);
    s = (uint16_t)r->d[7];
    uint16_t e[3] = {0xF01, (uint16_t)(s == 1 ? 19 : 20), 0xFFFF};
    o_event(r, s == 1 || s == 19 ? e : NULL, t23_think);
    s = (uint16_t)r->d[7];
    switch (s) {
    case 1: near_d0(r); if (!(og16(OG_NEAR_DIR) & 0x8000)) { set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r); } break;
    case 3: sog16(OG_MESSAGE, 0xA301); break;
    case 4: sog16(OG_MESSAGE, 0xA302); break;
    case 8: sog16(OG_MESSAGE, 0xA303); break;
    case 9: sog16(OG_MESSAGE, 0xA304); break;
    case 18: t23_push_on(r); break;
    case 19: case 31: w16(o + 0x3A, 0x49); break;
    default:
        if (s >= 21 && s <= 29) {
            set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r);
            sog16(OG_MESSAGE, (uint16_t)(0xA305 + s - 21));
        }
    }
    w16(o + OB_STATE, s);
}

/* ---------- type $12 (fn_S9_0372, record 8 on level 2) ----------
 * Level 2's keeper. It hurts the player near it (life and the screen's G_56EE glow: fn_S9_09B4 /
 * 0A2C / 0ABC by the distance), keeps the objects around it going (fn_S9_0B64: types $26 / $27 and
 * $11 counted down, type 8 in its cell stopped, a type $13 shot marked as hitting it), and:
 * 0 waits; at the level's start (fn_S7_22A0) 7-8: the view to cell $55E0 and along the player's
 * scent until there and cell $1360 is free, where it goes (9), heads for its post (+$18: the table
 * below) and stands (10). 1 roams (fn_S7_1CCC) until the player is within $28 (2: after him, by
 * the cells beyond $14, straight within, near enough to stop beyond $32); at the start: 6 (scent,
 * the view, back to 0 at cell $55E0). Hit by a type $13 shot: 3, events $1201 (+$18 below 2) or
 * $1203 to records 11-14 and 4: along the scent and, record 9's +$18 negative, $1202 to them and 5
 * (+$18 below 2: 6, else 1). Event $1201 / $1203 sends it to 4, $1202 to 5. A full view $82
 * (fn_S9_098A) takes it to its post (11: records 2-7, 9 and 10 get +$18 = -1, the type $13 shots
 * gone) and 10; flag word 0 bit 2 then to the other post (13-14); bit 3 (fn_S9_099C): gone (12). */
static const uint16_t t12_post[2][4][3] = {               /* { x, y, heading } by +$18 */
    {{0x1B8, 0x58, 0x30}, {0x1C8, 0x48, 0x20}, {0x1B8, 0x38, 0x10}, {0x1A8, 0x48, 0}},
    {{0x1E8, 0x48, 0x30}, {0x1E8, 0x52, 0x30}, {0x1B8, 0x78, 0x20}, {0x1C2, 0x78, 0x20}}};
static void life_less(uint16_t n) { int16_t l = (int16_t)(og16(OG_LIFE) - n); sog16(OG_LIFE, (uint16_t)(l < 0 ? 0 : l)); }
static void glow(uint16_t v) { sog16(0x56EE, v >= 0x28 ? 0x27 : v); }
static void t12_hurt_still(Regs *r) {                       /* fn_S9_09B4 */
    near_d0(r);
    uint16_t d = og16(OG_NEAR_DIST);
    if (d >= 0x14) return;
    glow((uint16_t)((0x17 - d) * 2 + og16(0x56EE)));
    if (d < 6) sog16(OG_LIFE, 0);
    else life_less(d < 0xA ? 0x14 : d < 0xC ? 2 : 1);
}
static void t12_hurt(Regs *r) {                             /* fn_S9_0A2C */
    near_d0(r);
    uint16_t d = og16(OG_NEAR_DIST);
    if (d >= 0x20) return;
    if (d >= 0x14) { glow((uint16_t)((og16(0x56EE) + 3) * 2)); return; }
    glow((uint16_t)((0x17 - d) * 2 + og16(0x56EE)));
    if (d < 6) life_less(0x14); else if (d < 0xA) life_less(0xA); else if (d < 0xE) life_less(2);
}
static void t12_hurt_post(Regs *r) {                        /* fn_S9_0ABC */
    near_d0(r);
    uint16_t d = og16(OG_NEAR_DIST);
    if (d < 0x27) { uint16_t v = (uint16_t)(0x28 - d); if (v >= 0x28) v = 0x27; if (v >= og16(0x56EE)) sog16(0x56EE, v); }
    mz_seg_of(7, 0x3D2A), fn_S7_3D2A(r);
    d = og16(OG_NEAR_DIST);
    if ((uint16_t)r->d[0] & 0x8000) life_less(d < 6 ? 0x23 : 0x1C);
    else if (d < 0x16) life_less(d < 0xE ? 2 : 1);
}
static void t12_guard(void) {                               /* fn_S9_0CB4: records 15-18 stopped; at its posts the player loses $28 */
    uint32_t t = og32(OG_OBJECTS);
    for (uint16_t rec = 0x3C0; rec <= 0x480; rec += 0x40) w16(t + rec + 0x18, 0xFFFF);
    if (og16(OG_PLAYER_CELL) == 0x1360 || og16(OG_PLAYER_CELL) == 0x55E0) life_less(0x28);
}
static void t12_step(Regs *r, uint16_t dir) {              /* fn_S9_0B4C: turned, and facing it a step (G_4A30) */
    set_w(&r->d[0], dir);
    mz_seg_of(7, 0x3722), fn_S7_3722(r);
    if ((uint16_t)r->d[0] != r16(r->a[4] + OB_HEADING)) return;
    r->a[0] = OADDR(0x4A30);
    mz_seg_of(7, 0x374C), fn_S7_374C(r); mz_seg_of(7, 0x3784), fn_S7_3784(r);
}
static int t12_scent_step(Regs *r) {                        /* along the player's scent; 0 if there is none */
    mz_seg_of(7, 0x3D5C), fn_S7_3D5C(r);
    uint16_t d = (uint16_t)r->d[0];
    fn_S7_3D0E(r);
    if (d & 0x8000) return 0;
    t12_step(r, d);
    return 1;
}
static int t12_toward(Regs *r, uint16_t x, uint16_t y) {   /* straight at x, y; 0 when there */
    sog16(OG_TARGET_X, x); sog16(OG_TARGET_Y, y);
    mz_seg_of(7, 0x3B36), fn_S7_3B36(r);
    uint16_t d = (uint16_t)r->d[0];
    mz_seg_of(7, 0x3D0E), fn_S7_3D0E(r);
    if (d & 0x8000) return 0;
    t12_step(r, d);
    return 1;
}
static void t12_view(Regs *r, uint16_t cell) { sog16(0x56A8, 0xAA); sog16(0x56A6, cell); refresh(r); }
static void t12_tell(Regs *r, uint16_t ev) { for (uint16_t rec = 0x2C0; rec <= 0x380; rec += 0x40) tell(r, ev, rec); }
static int t12_to_post(Regs *r, int which) {               /* placed at the post; 0 if it could not be */
    uint32_t o = r->a[4], map = og32(OG_MAP);
    const uint16_t *p = t12_post[which][r16(o + 0x18)];
    sog16(OG_MOVE_X, p[0]); sog16(OG_MOVE_Y, p[1]); w16(o + OB_HEADING, p[2]);
    mz_seg(7), fo_S7_155C(r);
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
    if ((uint16_t)r->d[0]) return 0;
    w16(map + sxw(r16(o + 0xE)) + 0xE, 0);
    w16(map + sxw(r16(o + OB_CELL)) + 0xE, 1);
    w16(o + 0xE, r16(o + OB_CELL));
    return 1;
}
static void t12_think(Regs *r) {
    uint32_t o = r->a[4], tbl = og32(OG_OBJECTS), map = og32(OG_MAP);
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        int start = og16(OG_FULL_VIEW) < 0x80 && og16(OG_PLAYER_CELL) < 0x2800;   /* fn_S7_22A0 */
        if ((s <= 1 || s == 4 || s == 6 || s == 8 || s == 9) && og16(OG_FULL_VIEW) == 0x82) { next = 0xB; goto on; }
        if ((s <= 2 || s == 4 || s == 6 || (s >= 8 && s <= 10) || s == 14) && flag(r, 0, 8)) { next = 0xC; goto on; }
        switch (s) {
        case 0: if (!start) return; next = 7; break;
        case 1:
            if (start) { t12_view(r, 0x55E0); next = 6; break; }
            near_d0(r);
            if (og16(OG_NEAR_DIST) >= 0x28) return;
            w16(o + 0x10, 0); next = 2; break;
        case 2: near_d0(r); if (og16(OG_NEAR_DIST) < 0x32) return; next = 1; break;
        case 3:
            t12_view(r, 0x31E0);
            t12_tell(r, r16(o + 0x18) < 2 ? 0x1201 : 0x1203);
            next = 4; break;
        case 4:
            if (t12_scent_step(r)) return;
            if (!(r16(tbl + 0x258) & 0x8000)) return;
            t12_tell(r, 0x1202);
            t12_view(r, 0x55E0);
            next = 5; break;
        case 5: next = r16(o + 0x18) < 2 ? 6 : 1; break;
        case 6:
            if (t12_scent_step(r)) return;
            if (r16(o + OB_CELL) == 0x55E0) { t12_view(r, 0x51E0); next = 0; break; }
            near_d0(r);
            if (og16(OG_NEAR_DIST) < 0x28) { set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x3722), fn_S7_3722(r); }
            return;
        case 7: t12_view(r, 0x55E0); next = 8; break;
        case 8:
            if (t12_scent_step(r)) return;
            if (r16(o + OB_CELL) != 0x55E0 || r16(map + 0x136E)) return;
            w16(map + sxw(r16(o + 0xE)) + 0xE, 0);
            sog16(OG_MOVE_X, 0x1B8); sog16(OG_MOVE_Y, 0x48); sog16(OG_MOVE_CELL, 0x1360);
            w16(o + 0xE, 0x1360); w16(map + 0x136E, 1);
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            next = 9; break;
        case 9: {
            const uint16_t *p = t12_post[0][r16(o + 0x18)];
            if (t12_toward(r, p[0], p[1])) return;
            next = 0xA; break;
        }
        case 10: if (!flag(r, 0, 4)) return; next = 0xD; break;
        case 11: {
            if (!t12_to_post(r, 0)) return;
            static const uint16_t stop[8] = {0x98, 0xD8, 0x118, 0x158, 0x198, 0x1D8, 0x258, 0x298};
            for (int k = 0; k < 8; k++) w16(tbl + stop[k], 0xFFFF);
            for (uint16_t rec = 0x1DC0; rec; rec -= 0x40)     /* fn_S9_0CEE: the type $13 shots gone */
                if (r16(tbl + rec + 2) == 0x13) { unlink_rec(r, rec); w16(tbl + rec + 2, 0xFFFF); }
            next = 0xA; break;
        }
        case 12:
            w16(map + sxw(r16(o + 0xE)) + 0xE, 0);
            unlink_obj(r);
            w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000);
            return;
        case 13: if (!t12_to_post(r, 1)) return; next = 0xE; break;
        default: return;                                    /* 14 */
        }
    on:
        set_w(&r->d[0], next);
    }
}
static void t12_around(Regs *r) {                           /* fn_S9_0B64 */
    uint32_t o = r->a[4], map = og32(OG_MAP), tbl = og32(OG_OBJECTS);
    static const int16_t at[9] = {-0x420, -0x400, -0x3E0, -0x20, 0, 0x20, 0x3E0, 0x400, 0x420};
    for (int k = 0; k < 9; k++) {
        uint16_t c = (uint16_t)(r16(o + OB_CELL) + at[k]);
        for (uint32_t j = 1; j <= 10; j++) {
            uint8_t n = r8(map + sxw(c) + j);
            if (!n) break;
            uint16_t rec = (uint16_t)(n << 6), ty = r16(tbl + rec + 2);
            uint32_t q = tbl + rec;
            if (ty == 0x26 || ty == 0x27 || (ty == 0x11 && r16(q + 0x30) == r16(o + OB_CELL))) {
                uint32_t f = q + (ty == 0x11 ? 0x16 : 0x18);
                uint16_t v = r16(f);
                if (!(v & 0x8000) || v >= 0xFFF6) w16(f, (uint16_t)(v - (ty == 0x11 ? 2 : 1)));
            } else if (ty == 8) {
                if (r16(q + 0x30) == r16(o + OB_CELL) && !(r16(q + 0x18) & 0x8000)) w16(q + 0x18, 0xFFFF);
            } else if (ty == 0x13) w16(o + 0x14, rec);
        }
    }
}
void fo_S9_0372(Regs *r) {
    static const uint16_t h13[] = {0x13, 3, 0xFFFF}, e1201[] = {0x1201, 4, 0xFFFF}, e1202[] = {0x1202, 5, 0xFFFF},
        e6[] = {0x1201, 4, 0x1203, 4, 0xFFFF};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t12_think(r);
    uint16_t s = (uint16_t)r->d[7];
    o_hit(r, s <= 2 || s == 6 ? h13 : NULL, t12_think);
    s = (uint16_t)r->d[7];
    o_event(r, s <= 2 ? e1201 : s == 4 ? e1202 : s == 6 ? e6 : NULL, t12_think);
    switch ((uint16_t)r->d[7]) {
    case 0: case 9: case 10: t12_hurt_still(r); t12_guard(); break;
    case 1:
        t12_hurt(r);
        mz_seg_of(7, 0x3EE2), fn_S7_3EE2(r);
        if (!((uint16_t)r->d[0] & 0x8000)) t12_step(r, (uint16_t)r->d[0]);
        break;
    case 2: {
        t12_hurt(r);
        uint16_t d = og16(OG_NEAR_DIST);
        if (d < 0xA) break;
        if (d < 0x14) { t12_toward(r, og16(OG_PLAYER_X), og16(OG_PLAYER_Y)); break; }
        sog16(0x3814, og16(OG_PLAYER_CELL));
        fo_S7_2132(r);
        if (!((uint16_t)r->d[0] & 0x8000)) t12_step(r, (uint16_t)r->d[0]);
        break;
    }
    case 4: case 6: t12_hurt(r); break;
    case 8: t12_hurt(r); if (r16(o + OB_CELL) == 0x55E0) t12_guard(); break;
    case 13: if (flag(r, 0, 8)) { set_w(&r->d[0], 0xC); t12_think(r); } break;   /* the original's act is think 14's code */
    case 14:                                                /* at the other post: after the player within $12, else turned to him */
        w16(o + 0x32, 4);
        near_d0(r);
        if ((uint16_t)r->d[0] & 0x8000) break;
        t12_hurt_post(r);
        if (og16(OG_NEAR_DIST) < 0x12) { set_w(&r->d[0], og16(OG_NEAR_DIR)); mz_seg_of(7, 0x36F4), fn_S7_36F4(r); break; }
        sog16(OG_TARGET_X, og16(OG_PLAYER_X)); sog16(OG_TARGET_Y, og16(OG_PLAYER_Y));
        mz_seg_of(7, 0x3B36), fn_S7_3B36(r);
        { uint16_t d = (uint16_t)r->d[0];
          mz_seg_of(7, 0x3D0E), fn_S7_3D0E(r);
          if (d & 0x8000) break;
          set_w(&r->d[0], d); }
        mz_seg_of(7, 0x3722), fn_S7_3722(r);
        if ((uint16_t)r->d[0] != r16(o + OB_HEADING)) break;
        r->a[0] = OADDR(0x4B70);
        mz_seg_of(7, 0x374C), fn_S7_374C(r);
        mz_seg_of(7, 0x3972), fn_S7_3972(r);
        if (!(uint16_t)r->d[0]) mz_seg_of(7, 0x3784), fn_S7_3784(r);
        break;
    }
    t12_around(r);
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $24 (fn_S9_0D76, record 8 on level 2) ----------
 * A face at the level's start, alive while records 9 and 10 (+$18) are not negative. With the
 * player within 8 it talks (+$18 / +$1A its mouth and eyes, from the tables below while it talks,
 * else blinking at random: fn_S9_14CE): 2-4 ($2401), 5-7 ($2402), 8-10 ($2403) as long as he stays
 * within $19 (hits by $17 / $19 move it on), the first time only after flag word 0 bit 0 (else 21-23:
 * $2405 while George is there). He leaves (11): it fires a type $13 shot (+$1E = -1) and waits 100
 * frames, then as long as a shot is in its cell (12-13). Records 9 / 10 negative: 14 (closed), back
 * to 13 when they are not; flag word 0 bit 3: 15-19, records 9 and 10 set to $7FFF and, the player
 * within 8, its last text ($2404); 20 the end. Every frame it moves records 2-7's negative +$18 on
 * by 2 (fn_S9_14F0). */
static void t24_blink(Regs *r) {                            /* fn_S9_14CE */
    random_d0(r);
    w16(r->a[4] + 0x18, 0); w16(r->a[4] + 0x1A, ((uint16_t)r->d[0] & 7) ? 1 : 0);
}
static void t24_others(void) {                              /* fn_S9_14F0 */
    uint32_t t = og32(OG_OBJECTS);
    for (uint16_t rec = 0x80; rec < 0x200; rec += 0x40) {
        uint16_t v = r16(t + rec + 0x18);
        if (v & 0x8000) w16(t + rec + 0x18, (uint16_t)(v + 2));
    }
}
static int t24_shot_here(Regs *r) {                         /* fn_S9_1514: a type $13 in its cell */
    uint32_t c = og32(OG_MAP) + r16(r->a[4] + OB_CELL), t = og32(OG_OBJECTS);
    for (uint32_t k = 1; k <= 10; k++) {
        uint8_t n = r8(c + k);
        if (!n) return 0;
        if (r16(t + ((uint32_t)n << 6) + 2) == 0x13) return 1;
    }
    return 0;
}
static void t24_think(Regs *r) {
    uint32_t o = r->a[4];
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        int open = !(r16(o + 0x58) & 0x8000) && !(r16(o + 0x98) & 0x8000);   /* records 9 and 10 */
        switch (s) {
        case 0: if (!open) return; next = 1; break;
        case 1:
            if (!open) { next = 0; break; }
            near_d0(r);
            if (og16(OG_NEAR_DIST) >= 8) return;
            next = flag(r, 0, 1) ? 2 : 0x15; break;
        case 2: case 5: case 8: case 17: w16(o + 0x1C, 0xFFFE); next = (uint16_t)(s + 1); break;
        case 3:
            if (!open) { next = 0; break; }
            near_d0(r);
            if (og16(OG_NEAR_DIST) >= 0x14) { next = 1; break; }
            {
                uint16_t t = (uint16_t)(r16(o + 0x1C) + 2);
                w16(o + 0x1C, t);
                if (t < 0x3A) return;
            }
            next = 4; break;
        case 4: case 6: case 7: case 9: case 10: {          /* talking while he stays within $19 */
            static const uint8_t end[11] = {[6] = 0x1C, [9] = 0x36};
            if (!open) { next = 0xE; break; }
            near_d0(r);
            if (og16(OG_NEAR_DIST) >= 0x19) { next = 0xB; break; }
            if (!end[s]) return;
            uint16_t t = (uint16_t)(r16(o + 0x1C) + 2);
            w16(o + 0x1C, t);
            if (t < end[s]) return;
            next = (uint16_t)(s + 1); break;
        }
        case 11:                                            /* he left: a shot */
            w16(o + 0x1E, 0xFFFF);
            sog16(OG_MOVE_X, r16(o + OB_X)); sog16(OG_MOVE_Y, r16(o + OB_Y)); sog16(OG_MOVE_CELL, r16(o + OB_CELL));
            make_object(r, 0x13, 0xFFFF, og16(OG_SELF));
            w16(o + 0x1C, 100);
            next = 0xC; break;
        case 12: if (!open) { next = 0xE; break; } if (!count_down(r, 0x1C, 1)) return; next = 0xD; break;
        case 13: if (!open) { next = 0xE; break; } if (t24_shot_here(r)) return; next = 0xB; break;
        case 14:
            if (flag(r, 0, 8)) { next = 0xF; break; }
            if (!open) return;
            w16(o + 0x1E, 0xFFFF); next = 0xD; break;
        case 15: w16(o + 0x58, 0x7FFF); w16(o + 0x98, 0x7FFF); w16(o + 0x1E, 0xFFFF); next = 0x10; break;
        case 16: near_d0(r); if (og16(OG_NEAR_DIST) >= 8) return; next = 0x11; break;
        case 18: {
            near_d0(r);
            if (og16(OG_NEAR_DIST) >= 0x19) { next = 0x14; break; }
            uint16_t t = (uint16_t)(r16(o + 0x1C) + 2);
            w16(o + 0x1C, t);
            if (t < 0x3A) return;
            next = 0x13; break;
        }
        case 19: near_d0(r); if (og16(OG_NEAR_DIST) < 0x19) return; next = 0x14; break;
        case 21:
            if (!og16(OG_GEORGE_JOINED)) { next = 0x16; break; }
            if (!open) { next = 0x17; break; }
            near_d0(r);
            if (og16(OG_NEAR_DIST) >= 0x19) { next = 0x16; break; }
            return;
        case 22: if (open) return; next = 0x17; break;
        case 23: if (!open) return; next = 0x16; break;
        default: return;                                    /* 20 */
        }
        set_w(&r->d[0], next);
    }
}
static void t24_face(Regs *r, const uint8_t *mouth, const uint8_t *eyes) {   /* the talking tables, by +$1C */
    uint16_t i = r16(r->a[4] + 0x1C) / 2;
    w16(r->a[4] + 0x18, mouth[i]); w16(r->a[4] + 0x1A, eyes[i]);
    t24_others();
}
void fo_S9_0D76(Regs *r) {
    static const uint16_t said[24][2] = {[4] = {0xA401, 5}, [7] = {0xA402, 8}, [10] = {0xA403, 11}, [19] = {0xA404, 20},
        [21] = {0xA405, 22}};
    static uint16_t h[5] = {0x17, 0, 0x19, 0, 0xFFFF};
    static const uint8_t m3[30] = {0, 2, 4, 6, 4, 4, 2, 4, 6, 4, 6, 4, 2, 2, 2, 4, 6, 4, 2, 4, 6, 4, 2, 4, 2, 4, 6, 4, 2, 0},
        e3[30] = {2, 3, 4, 3, 2, 3, 2, 3, 4, 3, 4, 3, 2, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 2, 3, 4, 2, 2, 2},
        m6[14] = {0, 2, 4, 6, 4, 2, 4, 6, 4, 6, 4, 2, 4, 0}, e6[14] = {2, 3, 4, 4, 3, 2, 3, 4, 3, 4, 3, 2, 3, 2},
        m9[27] = {0, 2, 4, 6, 4, 2, 2, 4, 2, 4, 6, 4, 2, 4, 6, 6, 4, 6, 4, 2, 4, 6, 4, 2, 4, 2, 0},
        e9[27] = {2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 4, 3, 2, 3, 4, 4, 3, 4, 3, 2, 3, 4, 3, 2, 3, 2, 1},
        m18[30] = {0, 2, 4, 6, 6, 4, 6, 4, 2, 4, 6, 4, 2, 2, 4, 2, 4, 6, 4, 6, 4, 2, 4, 2, 4, 6, 6, 4, 2, 0},
        e18[30] = {2, 3, 4, 4, 4, 3, 4, 3, 2, 3, 4, 3, 2, 1, 2, 1, 2, 3, 2, 3, 2, 1, 2, 1, 2, 3, 4, 3, 2, 2};
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t24_think(r);
    uint16_t s = (uint16_t)r->d[7];
    if (s < 24) o_said(r, said[s], t24_think);
    s = (uint16_t)r->d[7];
    int hit = s == 4 || s == 7 || s == 10 || s == 19;
    if (hit) h[1] = h[3] = (uint16_t)(s + 1);
    o_hit(r, hit ? h : NULL, t24_think);
    switch (s = (uint16_t)r->d[7]) {
    case 1: case 16: case 20: case 22: t24_blink(r); t24_others(); break;
    case 3: t24_face(r, m3, e3); break;
    case 4: case 7: case 10: case 19: case 21: {
        static const uint16_t text[22] = {[4] = 0xA401, [7] = 0xA402, [10] = 0xA403, [19] = 0xA404, [21] = 0xA405};
        t24_blink(r); t24_others(); sog16(OG_MESSAGE, text[s]);
        break;
    }
    case 5: set_w(&r->d[0], 6); t24_think(r); break;      /* the original's act is think 6's code */
    case 6: t24_face(r, m6, e6); break;
    case 9: t24_face(r, m9, e9); break;
    case 12:
        if (t24_shot_here(r)) { w16(o + 0x18, 6); w16(o + 0x1A, 0); } else t24_blink(r);
        t24_others();
        break;
    case 13: t24_others(); w16(o + 0x18, 6); w16(o + 0x1A, 0); break;
    case 14: case 23: w16(o + 0x18, 0); w16(o + 0x1A, 0xFFFF); w16(o + 0x1E, 0); break;
    case 18: t24_face(r, m18, e18); break;
    }
    w16(o + OB_STATE, (uint16_t)r->d[7]);
}

/* ---------- type $3A (fn_S11_046E, record 9 on level 6: the ending) ----------
 * It waits (0) until the player leaves the start corner (x $30 / y $27), then (44) until records
 * 3-6's +$12 are all set. Then the ending runs with the view its own (fn_S11_0CB4: the player put
 * where it is, facing as it faces): it takes his place (1-3), walks to $A5, $A0 (4), brings in type
 * $38 at record 7 (5, event $3A01) and faces it until it is done (6, event $3801), calls record 2
 * (+$12 = 1) and talks ($3A01-$3A04, 8-11; or, with flag word 0 bits 4 and 3, $3A05-$3A09, 15-19),
 * or walks to record 2 (12-13) and the player's life is gone (14). 20 brings in type $39 at record 8
 * (events $3A02 / $3A03) and it talks to it ($3A0A, $3A0B); 23-25 a count of $88 frames, the glow
 * G_56EE rising and record 7's +$18 counting at the end, and the player within reach of records 7
 * (29), 8 or 2 (28: life gone); the count out: 26-27 (events $3A04; the player losing $14 a frame).
 * 29 takes record 7's place (it is removed; life and G_56E6 $8D), 30 draining it by 4 until event
 * $3A05 (31) or it is gone (43); 31-34 flashes, 35-39 record 10 brought in as type $3B on the player,
 * opening (+$26 0-7, then pictures $20-$25) and removed; record 2's +$12 = -1, 40-41 ($3A0D), and 42
 * walking to record 2: the game won (G_5787 / G_5786, G_57F2 / G_57F1). Beyond state 0 it also opens
 * the map behind the player as he goes (fn_S11_0D00 / 0E30: +$1A the furthest of 15 places). */
static void t3a_view(Regs *r) {                             /* fn_S11_0CB4 */
    uint32_t o = r->a[4];
    sog16(OG_PLAYER_X, r16(o + OB_X)); sog16(OG_PLAYER_Y, r16(o + OB_Y)); sog16(OG_PLAYER_CELL, r16(o + OB_CELL)); sog16(OG_PLAYER_HEADING, r16(o + OB_HEADING));
    mz_seg_of(2, 0x3500), fn_S2_3500(r);
}
static int t3a_walk(Regs *r, uint16_t x, uint16_t y, int slow) {   /* towards x, y (fn_S11_0C9E / 0CD2); 0 when there */
    g_target(x, y);
    fo_S7_1488(r);
    if ((uint16_t)r->d[0] & 0x8000) return 0;
    if (slow) {
        mz_seg_of(7, 0x36F4), fn_S7_36F4(r);
        if ((uint16_t)r->d[0] != r16(r->a[4] + OB_HEADING)) return 1;
        r->a[0] = OADDR(0x4B70);
    } else {
        mz_seg_of(7, 0x3722), fn_S7_3722(r);
        r->a[0] = OADDR(0x4F30);
    }
    mz_seg_of(7, 0x374C), fn_S7_374C(r); mz_seg_of(7, 0x3784), fn_S7_3784(r);
    return 1;
}
static void t3a_bring(Regs *r, uint16_t rec, uint16_t type) {   /* record rec made type `type`, on the map */
    uint32_t t = og32(OG_OBJECTS);
    w16(t + rec + 2, type); w16(t + rec + 0xE, 0);
    set_w(&r->d[0], rec); mz_seg_of(2, 0x3480), fn_S2_3480(r);
}
static void t3a_remove(Regs *r, uint16_t rec) {
    uint32_t t = og32(OG_OBJECTS);
    unlink_rec(r, rec);
    for (int k = 4; k < 0xC; k += 2) w16(t + rec + k, 0);
    w16(t + rec + 0x30, 0); w16(t + rec + 2, r16(t + rec + 2) | 0x8000);
}
static void t3a_face(Regs *r, uint16_t rec, int twice) {  /* turned towards record rec, if it is on the map */
    uint32_t t = og32(OG_OBJECTS);
    if (!r16(t + rec + 4)) return;
    g_target(r16(t + rec + 4), r16(t + rec + 8));
    fo_S7_1488(r);
    if (twice) mz_seg_of(7, 0x3722), fn_S7_3722(r); else mz_seg_of(7, 0x36F4), fn_S7_36F4(r);
}
static uint16_t t3a_dist(Regs *r, uint16_t rec) {          /* the player's distance from record rec */
    uint32_t o = r->a[4]; uint16_t self = og16(OG_SELF);
    w32(r->a[6] - 4, o); w16(r->a[6] - 6, self);           /* kept in the handler's locals */
    r->a[4] = og32(OG_OBJECTS) + rec; sog16(OG_SELF, rec);
    near_d0(r);
    r->a[4] = o; sog16(OG_SELF, self);
    return og16(OG_NEAR_DIST);
}
static void t3a_flash(void) { sog16(0x56EE, 0x2B); sog16(0x56E2, 1); }   /* fn_S11_0CEE */
static void t3a_think(Regs *r) {
    uint32_t o = r->a[4], t = og32(OG_OBJECTS);
    for (;;) {
        uint16_t s = (uint16_t)r->d[0], next;
        set_w(&r->d[7], s);
        switch (s) {
        case 0: if (og16(OG_PLAYER_X) < 0x30 && og16(OG_PLAYER_Y) < 0x27) return; next = 0x2C; break;
        case 1: w16(o + 0x18, 0xA); next = 2; break;
        case 2: if (!count_down(r, 0x18, 1)) return; next = 3; break;
        case 3:
            sog16(OG_MOVE_X, og16(OG_PLAYER_X)); sog16(OG_MOVE_Y, og16(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            w16(o + OB_HEADING, og16(OG_PLAYER_HEADING));
            next = 4; break;
        case 4:
            if (t3a_walk(r, 0xA5, 0xA0, 0)) { t3a_view(r); if (t3a_walk(r, 0xA5, 0xA0, 0)) { t3a_view(r); return; } }
            next = 5; break;
        case 5: t3a_bring(r, 0x1C0, 0x38); tell(r, 0x3A01, 0x1C0); next = 6; break;
        case 7:
            w16(t + 0x92, 1);
            next = flag(r, 0, 0x10) && flag(r, 0, 8) ? 0xF : 8; break;
        case 12: if (t3a_walk(r, r16(t + 0x84), 0xA0, 1)) { t3a_view(r); return; } next = 0xD; break;
        case 13: if (t3a_walk(r, r16(t + 0x84), r16(t + 0x88), 1)) { t3a_view(r); return; } next = 0xE; break;
        case 20:
            tell(r, 0x3A02, 0x200); tell(r, 0x3A03, 0x1C0);
            t3a_bring(r, 0x200, 0x39);
            next = 0x15; break;
        case 23: w16(o + 0x18, 0); next = 0x18; break;
        case 24: case 25: {
            uint16_t c = (uint16_t)(r16(o + 0x18) + 1);
            w16(o + 0x18, c);
            if (c >= 0x80) { if (c - 0x80 >= 8) { next = 0x1A; break; } w16(t + 0x1D8, (uint16_t)(c - 0x80 + 4)); }
            sog16(0x56EE, (uint16_t)(r16(o + 0x18) >> 2));
            if (t3a_dist(r, 0x1C0) < 3) { next = 0x1D; break; }
            if (t3a_dist(r, 0x200) < 3 || t3a_dist(r, 0x80) < 4) { next = 0x1C; break; }
            return;
        }
        case 26: tell(r, 0x3A04, 0x1C0); tell(r, 0x3A04, 0x200); next = 0x1B; break;
        case 28: sog16(OG_LIFE, 0); return;
        case 29:                                            /* in record 7's place */
            sog16(OG_MOVE_X, r16(t + 0x1C4)); sog16(OG_MOVE_Y, (uint16_t)(r16(t + 0x1C8) + 2));
            mz_seg(7), fo_S7_155C(r);
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            t3a_remove(r, 0x1C0);
            w16(o + OB_HEADING, 0x30);
            sog16(OG_LIFE, 0x8D); sog16(0x56E6, 0x8D);
            next = 0x1E; break;
        case 30: life_less(4); if (og16(OG_LIFE)) return; next = 0x2B; break;
        case 31: w16(o + 0x18, 2); w16(t + 0x92, 0); next = 0x20; break;
        case 32: case 34: case 37: case 40: {
            static const uint8_t to[41] = {[32] = 0x21, [34] = 0x23, [37] = 0x26, [40] = 0x29};
            if (!count_down(r, 0x18, 1)) return;
            if (s == 37) w16(o + 0x18, 0xFFFF);
            next = to[s]; break;
        }
        case 33: w16(o + 0x18, 0xC); next = 0x22; break;
        case 35: {                                          /* record 10 brought in on the player */
            uint32_t a4 = r->a[4]; uint16_t self = og16(OG_SELF);
            w32(r->a[6] - 4, a4); w16(r->a[6] - 6, self);
            r->a[4] = t + 0x280; sog16(OG_SELF, 0x280);
            w16(t + 0x282, 0x3B);
            set_w(&r->d[0], 0x280); mz_seg_of(2, 0x3480), fn_S2_3480(r);
            sog16(OG_MOVE_X, og16(OG_PLAYER_X)); sog16(OG_MOVE_Y, og16(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
            mz_seg_of(7, 0x3784), fn_S7_3784(r);
            w16(t + 0x28E, 0);
            r->a[4] = a4; sog16(OG_SELF, self);
            w16(o + 0x18, 0); next = 0x24; break;
        }
        case 36: case 38: {
            uint16_t c = (uint16_t)(r16(o + 0x18) + 1);
            w16(o + 0x18, c);
            c >>= 1;
            if (s == 36 && c < 8) { w16(t + 0x2A6, c); return; }
            if (s == 38 && c < 6) { w16(t + 0x28C, (uint16_t)(c + 0x20)); w16(t + 0x2A6, 7); return; }
            w16(o + 0x18, s == 36 ? 5 : 1);
            next = (uint16_t)(s + 1); break;
        }
        case 39:
            if (!count_down(r, 0x18, 1)) return;
            t3a_remove(r, 0x280);
            w16(o + 0x18, 0xA); w16(t + 0x92, 0xFFFF);
            next = 0x28; break;
        case 42:
            if (t3a_walk(r, r16(t + 0x84), r16(t + 0x88), 1)) { t3a_view(r); return; }
            sog8(0x5787, 1); sog16(0x5786, 1); sog8(0x57F2, 1); sog8(0x57F1, 1);   /* won */
            return;
        case 44:
            if (!r16(t + 0xD2) || !r16(t + 0x112) || !r16(t + 0x152) || !r16(t + 0x192)) return;
            next = 1; break;
        default: return;
        }
        set_w(&r->d[0], next);
    }
}
static void t3a_open(Regs *r) {                             /* fn_S11_0D00 / 0E30 */
    static const uint16_t cells[][2] = {{0x420, 1}, {0x440, 1}, {0x820, 1}, {0x840, 1}, {0x460, 2}, {0x860, 2},
        {0x480, 2}, {0x880, 2}, {0x4A0, 3}, {0x8A0, 3}, {0x4C0, 4}, {0x8C0, 4}, {0x4E0, 5}, {0x8E0, 5}, {0x500, 6},
        {0x900, 6}, {0x520, 7}, {0x920, 7}, {0x540, 8}, {0x940, 8}, {0xD40, 9}, {0x1140, 0xA}, {0x1540, 0xB},
        {0x1940, 0xC}, {0x1D40, 0xD}, {0x2140, 0xE}, {0x2540, 0xF}, {0x2520, 0xF}, {0x2560, 0xF}};
    static const uint16_t walls[16][7][2] = {   /* { map offset, byte } ... */
        {{0}}, {{0x820, 0xA0}, {0x840, 0xA0}, {0xC20, 0x81}, {0xC40, 0x81}}, {{0x440, 0x41}, {0x840, 0x41}},
        {{0x460, 0x41}, {0x860, 0x41}}, {{0x480, 0x41}, {0x880, 0x41}}, {{0x4A0, 0x41}, {0x8A0, 0x41}},
        {{0x4C0, 0x41}, {0x8C0, 0x41}}, {{0x4E0, 0x41}, {0x8E0, 0x41}},
        {{0x520, 0x41}, {0x920, 0x41}, {0x540, 0x50}, {0x940, 0x50}, {0x560, 0x41}, {0x960, 0x41}},
        {{0x920, 0x41}, {0x540, 0x81}, {0x940, 0x50}, {0x960, 0x41}}, {{0x940, 0x81}}, {{0xD40, 0x81}},
        {{0x1140, 0x81}}, {{0x1540, 0x81}}, {{0x1940, 0x81}},
        {{0x2140, 0x81}, {0x2D40, 0xA0}, {0x3140, 0x81}, {0x2900, 0x41}, {0x2960, 0x50}, {0x2980, 0x41}}};
    uint32_t o = r->a[4], map = og32(OG_MAP);
    uint16_t at = 0;
    for (unsigned k = 0; k < sizeof cells / sizeof cells[0]; k++) if (og16(OG_PLAYER_CELL) == cells[k][0]) { at = cells[k][1]; break; }
    if (at >= r16(o + 0x1A)) w16(o + 0x1A, at);
    uint16_t n = r16(o + 0x1A);
    for (int k = 0; k < 7 && walls[n][k][0]; k++) w8(map + walls[n][k][0], (uint8_t)walls[n][k][1]);
}
void fo_S11_046E(Regs *r) {
    static const uint16_t said[45][2] = {[8] = {0xBA01, 9}, [9] = {0xBA02, 10}, [10] = {0xBA03, 11}, [11] = {0xBA04, 12},
        [15] = {0xBA05, 16}, [16] = {0xBA06, 17}, [17] = {0xBA07, 18}, [18] = {0xBA08, 19}, [19] = {0xBA09, 20},
        [21] = {0xBA0A, 22}, [22] = {0xBA0B, 23}, [24] = {0xBA0C, 25}, [41] = {0xBA0D, 42}};
    static const uint16_t e6[] = {0x3801, 7, 0xFFFF}, e30[] = {0x3A05, 31, 0xFFFF};
    uint32_t o = r->a[4], a6 = r->a[6];
    r->a[6] = r->a[7] - 4;                                  /* LINK A6, #-6 */
    r->a[7] -= 10;
    set_w(&r->d[7], r16(o + OB_STATE));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t3a_think(r);
    uint16_t s = (uint16_t)r->d[7];
    if (s < 45) o_said(r, said[s], t3a_think);
    s = (uint16_t)r->d[7];
    o_event(r, s == 6 ? e6 : s == 30 ? e30 : NULL, t3a_think);
    switch (s = (uint16_t)r->d[7]) {
    case 6: t3a_face(r, 0x1C0, 1); t3a_view(r); break;
    case 8: case 9: case 10: case 11: t3a_view(r); sog16(OG_MESSAGE, (uint16_t)(0xBA01 + s - 8)); break;
    case 14: sog16(OG_LIFE, 0); t3a_view(r); break;
    case 15: case 16: case 17: case 18: case 19: t3a_view(r); sog16(OG_MESSAGE, (uint16_t)(0xBA05 + s - 15)); break;
    case 21: case 22: t3a_face(r, 0x200, 0); t3a_view(r); sog16(OG_MESSAGE, s == 21 ? 0xBA0A : 0xBA0B); break;
    case 24: sog16(OG_MESSAGE, 0xBA0C); break;
    case 27: sog16(0x56EE, 0x2A); life_less(0x14); break;
    case 30: t3a_view(r); t3a_flash(); break;
    case 32: t3a_view(r); t3a_flash(); sog16(0x56EE, 0x2C); break;
    case 34: t3a_view(r); t3a_flash(); sog16(0x56EE, r16(o + 0x18) >= 4 ? 0x2D : 0x2B); break;
    case 37: w16(og32(OG_OBJECTS) + 0x2A6, 7); t3a_view(r); break;
    case 36: case 38: case 39: case 40: case 43: t3a_view(r); break;
    case 41: sog16(OG_MESSAGE, 0xBA0D); t3a_view(r); break;
    }
    if ((uint16_t)r->d[7]) t3a_open(r);
    w16(o + OB_STATE, (uint16_t)r->d[7]);
    r->a[7] += 10; r->a[6] = a6;
}

/* ---------- type $1B (fn_S10_1F28) ----------
 * A blast: after 5 frames (+$16 counting) it hits everything in its cell (+$14 = it; the player
 * there: life 0, fn_S10_2126) and is gone. */
void fo_S10_1F28(Regs *r) {
    uint32_t o = r->a[4];
    uint16_t t = (uint16_t)(r16(o + OB_STATE) + 1);
    w16(o + OB_STATE, t);
    if (t < 5) return;
    uint32_t c = og32(OG_MAP) + sxw(r16(o + OB_CELL)), tbl = og32(OG_OBJECTS);
    if (r16(c + 0xC)) sog16(OG_LIFE, 0);
    for (uint32_t k = 1; k <= 10; k++) {
        uint8_t n = r8(c + k);
        if (!n) break;
        w16(tbl + ((uint32_t)n << 6) + 0x14, og16(OG_SELF));
    }
    unlink_obj(r);
    w16(o + OB_TYPE, r16(o + OB_TYPE) | 0x8000);
}

/* ---------- shots and spells (fn_S7_087A, 099C, 0A5A, 0AB0, 0B7C, 0C9C) ----------
 * Objects made by firing (types $13, $22, $36, $3D) or by other creatures ($2A, $37). +$12 is
 * their state: negative while flying (+$C the heading, a step of table G_4F30 a frame, or more);
 * a wall ends it (0); so does running into an object: one of a kind fn_S7_18BE finds by G_3822
 * (the spells: +$12 = their time on the player, they stay there taking life each frame), or any
 * object fn_S7_15B8 finds within a cell (the hit one's +$14 = this record, for a type $13). At 0
 * they are gone (record free, type $FFFF). */
static void shot_free(Regs *r) { unlink_obj(r); w16(r->a[4] + OB_TYPE, 0xFFFF); }
static int shot_step(Regs *r, uint16_t kind, uint16_t hold) {   /* fn_S7_09FC, 0B1E, 0BE2: -1 when it ends */
    uint32_t o = r->a[4];
    r->a[0] = OADDR(0x4F30); set_w(&r->d[0], r16(o + OB_HEADING));
    mz_seg_of(7, 0x374C), fn_S7_374C(r); mz_seg_of(7, 0x3784), fn_S7_3784(r);
    if (r8(og32(OG_MAP) + sxw(r16(o + OB_CELL))) & 1) { w16(o + 0x12, 0); return -1; }
    sog16(0x3822, kind);
    mz_seg_of(7, 0x3ACA), fn_S7_3ACA(r);
    if ((uint16_t)r->d[0]) { w16(o + 0x12, hold); return -1; }
    sog16(0x3822, 1); sog16(0x3820, r16(o + 0x10));
    mz_seg_of(7, 0x37C4), fn_S7_37C4(r);
    if ((uint16_t)r->d[0]) { w16(o + 0x12, 0); return -1; }
    return 0;
}
static void spell_hold(Regs *r, uint16_t drain) {          /* on the player: moved with him, life - drain */
    uint32_t o = r->a[4];
    w16(o + 0x12, (uint16_t)(r16(o + 0x12) - 1));
    sog32(OG_MOVE_X, og32(OG_PLAYER_X)); sog32(OG_MOVE_Y, og32(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
    w16(o + 0x26, 0);
    int16_t l = (int16_t)(og16(OG_LIFE) - drain);
    sog16(OG_LIFE, (uint16_t)(l < 0 ? 0 : l));
}
static void spell(Regs *r, int steps, uint16_t kind, uint16_t hold, uint16_t f14, uint16_t drain) {
    uint32_t o = r->a[4];
    int16_t st = (int16_t)r16(o + 0x12);
    if (st < 0) {
        w16(o + 0x14, f14);
        while (steps-- && !shot_step(r, kind, hold)) {}
    } else if (!st) shot_free(r);
    else spell_hold(r, drain);
}
void fo_S7_099C(Regs *r) { spell(r, 2, 4, 9, 4, 4); }       /* type $22 */
void fo_S7_0AB0(Regs *r) { spell(r, 4, 3, 3, 0, 3); }       /* type $36 */
void fo_S7_0B7C(Regs *r) { spell(r, 3, 4, 4, 0, 3); }       /* type $3D */
void fo_S7_0A5A(Regs *r) {                                  /* type $2A (made by a rush): 2 frames on the player, 5 a frame */
    uint32_t o = r->a[4];
    if (r16(o + 0x12) & 0x8000) w16(o + 0x12, 2);
    if (!r16(o + 0x12)) shot_free(r); else spell_hold(r, 5);
}
void fo_S7_087A(Regs *r) {                                  /* type $13: the hit object's +$14 = it */
    uint32_t o = r->a[4];
    if (!(r16(o + 0x12) & 0x8000)) { shot_free(r); return; }
    if (r16(o + OB_HEADING) & 0x8000) return;
    r->a[0] = OADDR(0x4F30);
    mz_seg_of(7, 0x374C), fn_S7_374C(r); mz_seg_of(7, 0x3784), fn_S7_3784(r);
    if (r8(og32(OG_MAP) + sxw(r16(o + OB_CELL))) & 1) { w16(o + 0x12, 0); return; }
    sog16(0x3822, 1); sog16(0x3820, r16(o + 0x10));
    mz_seg_of(7, 0x37C4), fn_S7_37C4(r);
    if (!(uint16_t)r->d[0]) return;
    w16(og32(OG_OBJECTS) + sxw((uint16_t)r->d[0]) + 0x14, og16(OG_SELF));
    w16(o + 0x12, 0);
}
void fo_S7_0C9C(Regs *r) {                                  /* type $37 (Lips' end): 5 frames on the player, then level 6 */
    uint32_t o = r->a[4];
    if (r16(o + 0x12) & 0x8000) w16(o + 0x12, 5);
    uint16_t t = (uint16_t)(r16(o + 0x12) - 1);
    w16(o + 0x12, t); w16(o + 0x26, t);
    sog16(OG_MOVE_X, og16(OG_PLAYER_X)); sog16(OG_MOVE_Y, og16(OG_PLAYER_Y)); sog16(OG_MOVE_CELL, og16(OG_PLAYER_CELL));
    mz_seg_of(7, 0x3784), fn_S7_3784(r);
    if (t) return;
    sog16(OG_GO_LEVEL, 6); sog32(OG_GO_X, 0x140000); sog32(OG_GO_Y, 0x140000);
    sog16(OG_GO_HEADING, og16(OG_PLAYER_HEADING)); sog8(0x57DE, 1); sog8(0x57F2, 1);
}

/* ---------- fn_S7_0F78: one object's tick (Original's dispatcher, for fn_S7_2E9A) ----------
 * The record at D0.w of G_53C8 (G_3834 = D0) runs the behaviour of its type (+2); types without
 * one do nothing. Keeps D3-D7 / A2-A4. */
void fo_S10_0028(Regs *r),
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
    sog16(OG_SELF, (uint16_t)r->d[0]);
    r->d[0] &= 0xFFFF;
    r->a[4] = og32(OG_OBJECTS) + r->d[0];
    uint16_t type = r16(r->a[4] + OB_TYPE);
    if (type < 63 && by_type[type].f) {
        if (by_type[type].f != type_23) mz_seg(by_type[type].seg);
        by_type[type].f(r);
    }
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->d[6] = d6; r->d[7] = d7;
    r->a[2] = a2; r->a[3] = a3; r->a[4] = a4;
    cc_nz_l(r, d7);
}
