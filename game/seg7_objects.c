/* CODE segment 7, continued: the object behaviours (per-type handlers called by fn_S7_2E9A).
 *
 * Each handler runs one tick of a state machine for the object record at A4 (64 bytes in G_1DC4;
 * +2 type, +$16 state, +$1A cooldown, +$1E timer, ...). The original keeps three tables per type
 * next to its code: "think" code per state (entered with JMP (A3), which also serves as "switch to
 * state D0 and think again"), "act" code per state, and per-state collision lists for
 * fn_S7_3098. Here each table is a switch over the state.
 *
 * The handlers' register results are not compared: their caller fn_S7_2E9A restores D3-D7/A2-A4
 * and its own caller treats the rest as scratch (subs.txt: ignore=all). Inside, registers are
 * still kept as the original sets them, since the assembly helpers they call take their inputs in
 * registers. */
#include "mz.h"
#include "mzasm.h"
#include "fns.h"

/* fn_S7_3098: collision reaction. If the object at A4 has been hit (+$14 = the other object's
 * record offset, also put in G_00F0; G_00F0 = 0 otherwise), looks up the other object's type in
 * the state's list { type, new state }..., $FFFF and on a match switches to that state (think);
 * the hit is consumed either way. */
static void obj_collide(Regs *r, const uint16_t *list, void (*think)(Regs *)) {
    sg16(0xF0, 0);
    set_w(&r->d[0], r16(r->a[4] + 0x14));
    if (!(uint16_t)r->d[0]) return;
    r->a[0] = 0;                               /* the original's A0 walks the list in its code */
    sg16(0xF0, (uint16_t)r->d[0]);
    r->a[1] = g32(0x1DC4);
    set_w(&r->d[0], r16(r->a[1] + sxw(r->d[0]) + 2) & 0x7FFF);
    for (int k = 0;; k += 2) {
        set_w(&r->d[1], list[k]);
        r->a[0] += 2;
        if ((uint16_t)r->d[1] == (uint16_t)r->d[0]) {
            set_w(&r->d[0], list[k + 1]);
            think(r);
            break;
        }
        if ((uint16_t)r->d[1] == 0xFFFF) break;
        r->a[0] += 2;
    }
    w16(r->a[4] + 0x14, 0);
}

/* sound level for a sound made by an object: $FF - 2 * (G_00EC, the distance-map value between it
 * and the player, or $55 when not known), set with fn_S4_141A */
static void obj_sound_level(Regs *r) {
    set_w(&r->d[1], g16(0xEC));
    if ((uint16_t)r->d[1] & 0x8000) set_w(&r->d[1], 0x55);
    lsl_w(r, &r->d[1], 1);
    set_w(&r->d[0], 0xFF);
    sub_w(r, &r->d[0], (uint16_t)r->d[1]);
    mz_seg_of(4, 0x141A), fn_S4_141A(r);
}

/* ---------- types $20 and $21: the hunters ----------
 * States: 0 idle (watching; wakes when it sees the player close by or something enters its cell),
 * 2 hunting, 3/4 chasing towards the cell where it last saw the player (G_20CA in +$1C, giving
 * up after $78 ticks), 5-6 hit (sound, 10-tick animation), 7-8 dead: removed from the map for
 * $1F4 ticks, 9 waiting to respawn in the corner cell $3DE0 (when it is out of the player's sight
 * and free), 10-11 respawning (28-tick animation). In states 0, 2 and 4 a hit by an object of type
 * $18, $20, $21 or $29 means state 5. Hunting: when the player is in reach and in line (within 2
 * steps of its heading, or the player is right next to it) it fires a type $22 ($23 for type $21)
 * shot and waits $2D ticks; type $21 also turns towards the player twice as fast. Hits count in
 * states 0, 2, 4 and 11. */
static const uint16_t t20_think_entry[12] = {0x18, 0x6C, 0x70, 0x14A, 0x158, 0x1BA, 0x1E6, 0x244, 0x282, 0x2A0, 0x306, 0x34A};
static const uint16_t t20_act_entry[12]   = {0xB0, 0xC2, 0xD2, 0x1AA, 0x1D0, 0x238, 0x252, 0x2D4, 0x2F2, 0x358, 0x39C, 0x3BC};

/* think: D7.w = state D0.w; D0.w = the offset of its code in the original's table (as left by
 * the dispatch) */
static void t20_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7];
        if (s < 12) set_w(&r->d[0], t20_think_entry[s]);
        uint32_t o = r->a[4];
        switch (s) {
        case 0:
            fn_S7_32DC(r);
            set_w(&r->d[0], g16(0xEC));
            if (!((uint16_t)r->d[0] & 0x8000)) {
                if ((uint16_t)r->d[0] < 5) goto wake;
                set_w(&r->d[0], g16(0xEE)); sub_w(r, &r->d[0], r16(o + 0xC));
                fn_S7_367E(r);
                if ((uint16_t)r->d[0] < 0xF) goto wake;
            }
            fn_S7_3FF0(r);
            if (!(uint16_t)r->d[0]) return;
            w16(o + 0x1C, g16(0x20CA));
            set_w(&r->d[0], 3); continue;
        wake:
            w16(o + 0x1A, 0xC);
            set_w(&r->d[0], 2); continue;
        case 2:
            fn_S7_32DC(r);
            if (r->ccr & CCR_Z) return;
            set_w(&r->d[0], 3); continue;
        case 3:
            w16(o + 0x1E, 0x78);
            set_w(&r->d[0], 4); continue;
        case 4:
            fn_S7_32DC(r);
            if (!(uint16_t)r->d[0]) { set_w(&r->d[0], 2); continue; }
            set_w(&r->d[0], r16(o + 0x1E));
            if (!(uint16_t)r->d[0]) w16(o + 0x1C, 0);
            set_w(&r->d[0], r16(o + 0x1C));
            if ((uint16_t)r->d[0]) return;
            set_w(&r->d[0], 0); continue;
        case 5:
            fn_S7_3128(r);
            /* fall into the sound level */
            obj_sound_level(r);
            mz_seg_of(4, 0x1432), fn_S4_1432();
            w16(o + 0x1E, 0xFFFF);
            set_w(&r->d[0], 6); continue;
        case 6:
            set_w(&r->d[0], (uint16_t)(r16(o + 0x1E) + 1));
            w16(o + 0x1E, (uint16_t)r->d[0]);
            if ((uint16_t)r->d[0] < 0xA) return;
            set_w(&r->d[0], 7); continue;
        case 7:
            set_w(&r->d[0], r16(o + 0xE));
            r->a[0] = g32(0x1DCC);
            w16(r->a[0] + sxw(r->d[0]) + 0xE, 0);
            set_w(&r->d[0], g16(0xF2));
            set_w(&r->d[1], r16(o + 0x30));
            mz_seg_of(2, 0x3410), fn_S2_3410(r);
            w16(o + 0x10, r16(o + 0x30));
            r->d[0] = 0;
            w32(o + 4, 0); w32(o + 8, 0); w16(o + 0x30, 0); w16(o + 0xE, 0);
            w16(o + 0x1E, 0x1F4);
            set_w(&r->d[0], 8); continue;
        case 8:
            set_w(&r->d[0], (uint16_t)(r16(o + 0x1E) - 1));
            w16(o + 0x1E, (uint16_t)r->d[0]);
            if (!((uint16_t)r->d[0] & 0x8000)) return;
            set_w(&r->d[0], g16(0xF2));
            mz_seg_of(2, 0x3480), fn_S2_3480(r);
            set_w(&r->d[0], 9); continue;
        case 9: {
            set_w(&r->d[1], 0x3DE0);
            r->a[0] = g32(0x1DCC);
            uint32_t cell = r->a[0] + 0x3DE0;
            sg16(0xEA, r16(cell + 0x1C)); sg16(0xE6, r16(cell + 0x1E));
            sg16(0xE2, 0x3DE0);
            set_w(&r->d[0], r16(cell + 0xE));
            if ((uint16_t)r->d[0]) return;
            fn_S7_3972(r);
            if ((uint16_t)r->d[0]) return;
            set_w(&r->d[0], 0x3DE0);
            r->a[0] = g32(0x1DCC);
            if (r8(r->a[0] + 0x3DE0 + 0x12) & 1) return;
            sg16(0xE0, 5);
            fn_S7_3ACA(r);
            if ((uint16_t)r->d[0]) {                       /* too close to the player: cost a life point */
                mz_seg_of(4, 0x1734), fn_S4_1734();
                set_w(&r->d[0], (uint16_t)(g16(0x20FE) - 1));
                if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
                sg16(0x20FE, (uint16_t)r->d[0]);
                return;
            }
            fn_S7_3784(r);
            set_w(&r->d[0], 0xA); continue; }
        case 10:
            fn_S7_32DC(r);
            obj_sound_level(r);
            mz_seg_of(4, 0x15CE), fn_S4_15CE();
            w16(o + 0xE, r16(o + 0x30));
            set_w(&r->d[1], r16(o + 0x30));
            r->a[0] = g32(0x1DCC);
            w16(r->a[0] + sxw(r->d[1]) + 0xE, 1);
            w16(o + 0x10, 0);
            w16(o + 0x1E, 0xFFFF);
            set_w(&r->d[0], 0xB); continue;
        case 11:
            set_w(&r->d[0], (uint16_t)(r16(o + 0x1E) + 1));
            w16(o + 0x1E, (uint16_t)r->d[0]);
            if ((uint16_t)r->d[0] < 0x1C) return;
            set_w(&r->d[0], g16(0xF2));
            mz_seg_of(2, 0x3480), fn_S2_3480(r);
            set_w(&r->d[0], 0); continue;
        default:                                    /* 1 */
            return;
        }
    }
}

static void t20_act(Regs *r) {
    uint32_t o = r->a[4];
    uint16_t s = (uint16_t)r->d[7];
    set_w(&r->d[0], (uint16_t)(s < 12 ? t20_act_entry[s] : 0));
    switch (s) {
    case 0:
        fn_S7_3EE2(r);
        if ((uint16_t)r->d[0] >= 0x40) return;
        fn_S7_1C9A(r);
        return;
    case 2:
        w16(o + 0x1C, g16(0x20CA));
        if (r16(o + 0x1A)) {
            set_w(&r->d[0], g16(0xEC));
            if ((uint16_t)r->d[0] >= 0xA) {
                sg16(0xDA, g16(0x20D2)); sg16(0xD6, g16(0x20CE));
                fn_S7_3B36(r);
                fn_S7_3D0E(r);
                set_w(&r->d[0], g16(0xD0));
                if (!((uint16_t)r->d[0] & 0x8000)) { fn_S7_1C9A(r); return; }
            }
            set_w(&r->d[0], g16(0xEE));
            fn_S7_36F4(r);
            return;
        }
        fn_S7_1CC2(r);
        fn_S7_36F4(r);
        set_w(&r->d[0], r16(o + 2));
        if ((uint16_t)r->d[0] == 0x21) { fn_S7_1CC2(r); fn_S7_372C(r); }
        set_w(&r->d[0], g16(0xEC));
        if ((uint16_t)r->d[0] < 8) {
            set_w(&r->d[0], g16(0xEE)); sub_w(r, &r->d[0], r16(o + 0xC));
            fn_S7_367E(r);
            if ((uint16_t)r->d[0] >= 2) return;
        } else {
            fn_S7_1CC2(r);
            if ((uint16_t)r->d[0] != r16(o + 0xC)) return;
        }
        set_w(&r->d[0], (uint16_t)(r16(o + 2) == 0x21 ? 0x23 : 0x22));
        set_w(&r->d[1], r16(o + 0xC));
        set_w(&r->d[2], g16(0xF2));
        r->a[0] = GADDR(0x0B6A);
        fn_S7_40D6(r);
        obj_sound_level(r);
        mz_seg_of(4, 0x14CC), fn_S4_14CC();
        w16(o + 0x1A, 0x2D);
        return;
    case 4: {
        set_w(&r->d[0], r16(o + 0x1C));
        r->a[0] = g32(0x1DCC);
        uint32_t cell = r->a[0] + sxw(r->d[0]);
        sg16(0xDA, r16(cell + 0x1C)); sg16(0xD6, r16(cell + 0x1E));
        fn_S7_3B36(r);
        fn_S7_3D0E(r);
        set_w(&r->d[0], g16(0xD0));
        if (!((uint16_t)r->d[0] & 0x8000)) { fn_S7_1C9A(r); return; }
        set_w(&r->d[0], r16(o + 0x30));
        if ((uint16_t)r->d[0] == r16(o + 0x1C)) w16(o + 0x1C, 0);
        else w16(o + 0x1E, (uint16_t)(r16(o + 0x1E) - 1));
        return; }
    case 6: {
        static const uint16_t pics20[5] = {4, 5, 6, 7, 8}, pics21[5] = {0x22, 0x23, 0x24, 0x25, 0x26};
        w16(o + 0x2E, 0); w16(o + 0x34, 0); w16(o + 0x32, 0);
        set_w(&r->d[0], r16(o + 2));
        r->a[0] = 0;                                /* the original points A0 at its table in the code */
        uint16_t i = (uint16_t)(r16(o + 0x1E) >> 1);
        set_w(&r->d[0], (uint16_t)(i << 1));
        set_w(&r->d[0], (r16(o + 2) == 0x21 ? pics21 : pics20)[i]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    case 11: {
        static const uint16_t pics[7] = {0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x21};
        uint16_t i = (uint16_t)(r16(o + 0x1E) >> 2);
        r->a[0] = 0;
        set_w(&r->d[0], pics[i]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    default:                                        /* 1, 3, 5, 7, 8, 9, 10 */
        return;
    }
}

static const uint16_t t20_hits[] = {0x18, 5, 0x20, 5, 0x21, 5, 0x29, 5, 0xFFFF};
static const uint16_t no_hits[] = {0xFFFF};

/* fn_S7_186C: one tick of a type $20 / $21 object: a hit from its cell's flag (+$12 bit 0: by
 * object G_00F2), think, collisions, act, then the cooldown +$1A counts down and the state is
 * stored in +$16. */
void fn_S7_186C(Regs *r) {
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], r16(o + 0x30));
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + sxw(r->d[0]) + 0x12) & 1) w16(o + 0x14, g16(0xF2));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t20_think(r);
    uint16_t s = (uint16_t)r->d[7];
    obj_collide(r, (s == 0 || s == 2 || s == 4 || s == 11) ? t20_hits : no_hits, t20_think);
    t20_act(r);
    set_w(&r->d[0], (uint16_t)(r16(o + 0x1A) - 1));
    if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
    w16(o + 0x1A, (uint16_t)r->d[0]);
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- pick-ups ----------
 * The jewels (types 2, 4, 5, 6) spin (+$16 counts 0..15); one is picked up when the player is in its
 * cell and close (G_00EC < 3, fn_S7_3128): taken off its cell's list (fn_S2_3410) and marker,
 * flagged gone (+2 bit 15), a pick-up sound (fn_S4_1802), its points to the bonus G_20EA and one
 * jewel less to find (G_225E); the last one plays fn_S4_16C6. Type 1 refills life and energy
 * (sound fn_S4_16D8); type 7 (spinning on +$C, 0..11) sets the bonus multiplier G_225B (and
 * G_2258, G_225C) and marks its cell (+$12 bit 0). */
static int pickup(Regs *r) {
    uint32_t o = r->a[4];
    set_w(&r->d[0], r16(o + 0x30));
    if ((uint16_t)r->d[0] != g16(0x20CA)) return 0;
    fn_S7_3128(r);
    set_w(&r->d[0], g16(0xEC));
    if ((uint16_t)r->d[0] >= 3) return 0;
    set_w(&r->d[0], g16(0xF2));
    set_w(&r->d[1], r16(o + 0x30));
    mz_seg_of(2, 0x3410), fn_S2_3410(r);
    w16(o + 2, (uint16_t)(r16(o + 2) | 0x8000));
    set_w(&r->d[0], r16(o + 0x30));
    r->a[0] = g32(0x1DCC);
    w16(r->a[0] + sxw(r->d[0]) + 0xE, 0);
    return 1;
}
static void jewel(Regs *r, uint32_t points) {
    uint32_t o = r->a[4];
    set_w(&r->d[0], (uint16_t)((r16(o + 0x16) + 1) & 0xF));
    w16(o + 0x16, (uint16_t)r->d[0]);
    if (!pickup(r)) return;
    mz_seg_of(4, 0x1802), fn_S4_1802();
    sg32(0x20EA, g32(0x20EA) + points);
    set_w(&r->d[0], (uint16_t)(g16(0x225E) - 1));
    sg16(0x225E, (uint16_t)r->d[0]);
    if (!(uint16_t)r->d[0]) mz_seg_of(4, 0x16C6), fn_S4_16C6();
}
void fn_S7_01AC(Regs *r) { jewel(r, 500); }     /* type 2 */
void fn_S7_03C2(Regs *r) { jewel(r, 750); }     /* type 4 */
void fn_S7_0452(Regs *r) { jewel(r, 1000); }    /* type 5 */
void fn_S7_04E2(Regs *r) { jewel(r, 1500); }    /* type 6 */

/* fn_S7_0136: type 1, the refill */
void fn_S7_0136(Regs *r) {
    if (!pickup(r)) return;
    mz_seg_of(4, 0x16D8), fn_S4_16D8();
    sg16(0x20FE, g16(0x20FC));
    sg16(0x20F8, g16(0x20F6));
}

/* fn_S7_05AA: type 7, the bonus multiplier */
void fn_S7_05AA(Regs *r) {
    uint32_t o = r->a[4];
    set_w(&r->d[0], (uint16_t)(r16(o + 0xC) + 1));
    if ((uint16_t)r->d[0] >= 0xC) r->d[0] = 0;
    w16(o + 0xC, (uint16_t)r->d[0]);
    if (!pickup(r)) return;
    mz_seg_of(4, 0x16EA), fn_S4_16EA();
    sg8(0x225B, 1); sg8(0x2258, 1); sg8(0x225C, 1);
    set_w(&r->d[0], r16(o + 0x30));
    r->a[0] = g32(0x1DCC);
    w8(r->a[0] + sxw(r->d[0]) + 0x12, (uint8_t)(r8(r->a[0] + sxw(r->d[0]) + 0x12) | 1));
}

/* end-of-level sequence step, every fourth tick (fn_S4_435E), then whether it is over (fn_S4_44DE) */
static int level_end_step(Regs *r) {
    set_w(&r->d[0], g16(0x20BC) & 3);
    if (!(uint16_t)r->d[0]) mz_seg_of(4, 0x435E), fn_S4_435E();
    mz_seg_of(4, 0x44DE), fn_S4_44DE(r);
    return !(r->ccr & CCR_Z);
}

/* ---------- type 3: the exit ----------
 * fn_S7_023E: while the level-end sequence runs (G_20D8 = $84) it steps it, and when it is over
 * either ends the game (no lives left: G_21B3, G_2220, G_221F) or starts the next level (G_220C,
 * G_2220; life and energy refilled). The exit spins (+$C, 0..5, one step every other tick); it is
 * closed (+$18 counts down the type-8 triggers: when G_2256 reaches 2 it opens with a sound) and,
 * once open, entering it close by (G_00EC < 5) starts the level-end sequence (+$16 = -1,
 * fn_S4_4340): the view locks onto it (G_20D6 from the spin table G_0146, G_20D8 = $80) and when
 * the sequence ends the next level's start is recorded (G_220A = level + 1, position and heading). */
void fn_S7_023E(Regs *r) {
    uint32_t o = r->a[4];
    set_w(&r->d[0], g16(0x20D8));
    if ((uint16_t)r->d[0] == 0x84 && level_end_step(r)) {
        set_w(&r->d[0], g16(0x20E0));
        if (!(uint16_t)r->d[0]) {
            sg8(0x21B3, 1); sg16(0x21B2, 0); sg8(0x2220, 1); sg8(0x221F, 1);
        } else {
            sg8(0x220C, 1); sg8(0x21FE, 0); sg8(0x2220, 1);
            sg16(0x20FE, g16(0x20FC)); sg16(0x20F8, g16(0x20F6));
        }
    }
    set_w(&r->d[0], r16(o + 0x16));
    if (!(uint16_t)r->d[0]) {
        set_w(&r->d[0], r16(o + 0x18));
        if ((uint16_t)r->d[0]) {                    /* closed */
            set_w(&r->d[0], g16(0x2256));
            if ((uint16_t)r->d[0] != 2) return;
            set_w(&r->d[0], (uint16_t)(r16(o + 0x18) - 1));
            w16(o + 0x18, (uint16_t)r->d[0]);
            if ((uint16_t)r->d[0]) return;
            fn_S7_32DC(r);
            obj_sound_level(r);
            mz_seg_of(4, 0x175C), fn_S4_175C();
            return;
        }
        set_w(&r->d[0], r16(o + 0xC));
        set_w(&r->d[1], g16(0x20BC) & 1);
        if ((uint16_t)r->d[1]) set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] + 1));
        if ((int16_t)r->d[0] >= 6) set_w(&r->d[0], 0);
        w16(o + 0xC, (uint16_t)r->d[0]);
        set_w(&r->d[0], r16(o + 0x30));
        if ((uint16_t)r->d[0] != g16(0x20CA)) return;
        fn_S7_3128(r);
        set_w(&r->d[0], g16(0xEC));
        if ((uint16_t)r->d[0] >= 5) return;
        w16(o + 0x16, 0xFFFF);
        mz_seg_of(4, 0x4340), fn_S4_4340();
    }
    r->a[0] = GADDR(0x0146);
    set_w(&r->d[0], r16(o + 0xC));
    set_w(&r->d[1], g16(0x20BC) & 1);
    if ((uint16_t)r->d[1]) set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] + 1));
    if ((int16_t)r->d[0] >= 6) set_w(&r->d[0], 0);
    w16(o + 0xC, (uint16_t)r->d[0]);
    lsl_w(r, &r->d[0], 1);
    set_w(&r->d[0], r16(r->a[0] + sxw(r->d[0])));
    sg16(0x20D6, (uint16_t)r->d[0]);
    sg16(0x20D8, 0x80);
    if (!level_end_step(r)) return;
    set_w(&r->d[0], (uint16_t)(g16(0x20EC) + 1));
    sg16(0x220A, (uint16_t)r->d[0]);
    sg32(0x2206, r32(o + 4)); sg32(0x2202, r32(o + 8));
    sg16(0x2208, g16(0x20D4));
    sg8(0x21FE, 1); sg8(0x220C, 1); sg8(0x2220, 1);
}

/* ---------- type 8: the pair objects placed by fn_S4_22E8 ----------
 * States: 0 waiting (a hit by a type $18 object switches to 1), 1 triggered (sound, G_2256 + 1),
 * 2 animating (+$C counts up every other tick to $2E), 3 resting until a 1-in-16 chance
 * (fn_S2_3532) restarts the animation. The act code does nothing in any state. */
static const uint16_t t8_think_entry[4] = {0x08, 0x0C, 0x3A, 0x62};
static const uint16_t t8_act_entry[4] = {0x32, 0x60, 0x88, 0x9C};
static void t8_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7];
        if (s < 4) set_w(&r->d[0], t8_think_entry[s]);
        uint32_t o = r->a[4];
        switch (s) {
        case 1:
            fn_S7_32DC(r);
            obj_sound_level(r);
            mz_seg_of(4, 0x1746), fn_S4_1746();
            w16(o + 0xC, 0);
            sg16(0x2256, (uint16_t)(g16(0x2256) + 1));
            set_w(&r->d[0], 2); continue;
        case 2:
            set_w(&r->d[0], r16(o + 0xC));
            set_w(&r->d[1], g16(0x20BC) & 1);
            if ((uint16_t)r->d[1]) set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] + 1));
            if ((uint16_t)r->d[0] < 0x2E) { w16(o + 0xC, (uint16_t)r->d[0]); return; }
            w16(o + 0xC, 0);
            set_w(&r->d[0], 3); continue;
        case 3:
            mz_seg_of(2, 0x3532), fn_S2_3532(r);
            set_w(&r->d[0], (uint16_t)r->d[0] & 0xF);
            if ((uint16_t)r->d[0]) return;
            set_w(&r->d[0], 2); continue;
        default:                                    /* 0 */
            return;
        }
    }
}
static const uint16_t t8_hits[] = {0x18, 1, 0xFFFF};

/* fn_S7_063C: one tick of a type 8 object: think, collisions, (act), state to +$16. */
void fn_S7_063C(Regs *r) {
    set_w(&r->d[7], r16(r->a[4] + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t8_think(r);
    obj_collide(r, (uint16_t)r->d[7] == 0 ? t8_hits : no_hits, t8_think);
    uint16_t s = (uint16_t)r->d[7];
    set_w(&r->d[0], (uint16_t)(s < 4 ? t8_act_entry[s] : 0));
    w16(r->a[4] + 0x16, (uint16_t)r->d[7]);
}

/* ---------- type 9: the grabber ----------
 * Spins in place (fn_S7_0A9C) and drifts along its route (state 0) until it sees the player in a
 * straight line within $19 (fn_S7_319E; state 1). It then homes in on the player (fn_S7_3B36) and,
 * once within 8 pixels (fn_S7_3D2A), pulls the player towards itself and drains life (fn_S7_09EC,
 * sound fn_S4_1574). Hit by a type 9, $18 or $29 object in states 0, 1 and 9: 3-4 hit (6 ticks),
 * 5-6 dead and removed for $1F4 ticks, 7 waiting to respawn in corner cell $3DE0, 8-9 respawning
 * (6 ticks). */
static const uint16_t t9_think_entry[10] = {0x14, 0x40, 0x94, 0x98, 0xC4, 0x114, 0x152, 0x170, 0x1B4, 0x1F8};
static const uint16_t t9_act_entry[10] = {0x72, 0x9C, 0xDE, 0x10A, 0x124, 0x198, 0x1B6, 0x1FA, 0x23E, 0x25E};
static void t9_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7];
        if (s < 10) set_w(&r->d[0], t9_think_entry[s]);
        uint32_t o = r->a[4];
        switch (s) {
        case 0:
            fn_S7_319E(r);
            set_w(&r->d[0], g16(0xEC));
            if ((uint16_t)r->d[0] >= 0x19) return;
            set_w(&r->d[0], 1); continue;
        case 1:
            fn_S7_319E(r);
            set_w(&r->d[0], g16(0xEC));
            if ((uint16_t)r->d[0] < 0x19) return;
            set_w(&r->d[0], 0); continue;
        case 3:
            fn_S7_3128(r);
            obj_sound_level(r);
            mz_seg_of(4, 0x145E), fn_S4_145E();
            w16(o + 0x1E, 0xFFFF);
            set_w(&r->d[0], 4); continue;
        case 4:
            set_w(&r->d[0], (uint16_t)(r16(o + 0x1E) + 1));
            w16(o + 0x1E, (uint16_t)r->d[0]);
            if ((uint16_t)r->d[0] < 6) return;
            set_w(&r->d[0], 5); continue;
        case 5:
            set_w(&r->d[0], r16(o + 0xE));
            r->a[0] = g32(0x1DCC);
            w16(r->a[0] + sxw(r->d[0]) + 0xE, 0);
            set_w(&r->d[0], g16(0xF2));
            set_w(&r->d[1], r16(o + 0x30));
            mz_seg_of(2, 0x3410), fn_S2_3410(r);
            w16(o + 0x10, r16(o + 0x30));
            r->d[0] = 0;
            w32(o + 4, 0); w32(o + 8, 0); w16(o + 0x30, 0); w16(o + 0xE, 0);
            w16(o + 0x1E, 0x1F4);
            set_w(&r->d[0], 6); continue;
        case 6:
            set_w(&r->d[0], (uint16_t)(r16(o + 0x1E) - 1));
            w16(o + 0x1E, (uint16_t)r->d[0]);
            if (!((uint16_t)r->d[0] & 0x8000)) return;
            set_w(&r->d[0], g16(0xF2));
            mz_seg_of(2, 0x3480), fn_S2_3480(r);
            set_w(&r->d[0], 7); continue;
        case 7: {
            set_w(&r->d[1], 0x3DE0);
            r->a[0] = g32(0x1DCC);
            uint32_t cell = r->a[0] + 0x3DE0;
            sg16(0xEA, r16(cell + 0x1C)); sg16(0xE6, r16(cell + 0x1E));
            sg16(0xE2, 0x3DE0);
            set_w(&r->d[0], r16(cell + 0xE));
            if ((uint16_t)r->d[0]) return;
            fn_S7_3972(r);
            if ((uint16_t)r->d[0]) return;
            set_w(&r->d[0], 0x3DE0);
            r->a[0] = g32(0x1DCC);
            if (r8(r->a[0] + 0x3DE0 + 0x12) & 1) return;
            fn_S7_3784(r);
            set_w(&r->d[0], 8); continue; }
        case 8:
            fn_S7_32DC(r);
            obj_sound_level(r);
            mz_seg_of(4, 0x15FA), fn_S4_15FA();
            w16(o + 0xE, r16(o + 0x30));
            set_w(&r->d[1], r16(o + 0x30));
            r->a[0] = g32(0x1DCC);
            w16(r->a[0] + sxw(r->d[1]) + 0xE, 1);
            w16(o + 0x10, 0);
            w16(o + 0x1E, 0xFFFF);
            set_w(&r->d[0], 9); continue;
        case 9:
            set_w(&r->d[0], (uint16_t)(r16(o + 0x1E) + 1));
            w16(o + 0x1E, (uint16_t)r->d[0]);
            if ((uint16_t)r->d[0] < 6) return;
            set_w(&r->d[0], g16(0xF2));
            mz_seg_of(2, 0x3480), fn_S2_3480(r);
            set_w(&r->d[0], 0); continue;
        default:                                    /* 2 */
            return;
        }
    }
}
static void t9_act(Regs *r) {
    uint32_t o = r->a[4];
    uint16_t s = (uint16_t)r->d[7];
    set_w(&r->d[0], (uint16_t)(s < 10 ? t9_act_entry[s] : 0));
    switch (s) {
    case 0:
        fn_S7_3EE2(r);
        w16(o + 0x18, (uint16_t)r->d[0]);
        fn_S7_0A9C(r);
        if (!(uint16_t)r->d[0]) fn_S7_3784(r);
        return;
    case 1:
        sg16(0xDA, g16(0x20D2)); sg16(0xD6, g16(0x20CE));
        fn_S7_3B36(r);
        fn_S7_3D0E(r);
        set_w(&r->d[0], g16(0xD0));
        w16(o + 0x18, (uint16_t)r->d[0]);
        fn_S7_0A9C(r);
        if (!((uint16_t)r->d[0] & 0x8000)) fn_S7_3784(r);
        fn_S7_3D2A(r);
        if ((uint16_t)r->d[0] & 0x8000) { mz_seg_of(4, 0x1574), fn_S4_1574(); fn_S7_09EC(r); }
        w16(o + 0x34, 6);
        return;
    case 4: {
        static const uint16_t pics[3] = {0x16, 0x17, 0x18};
        w16(o + 0x2E, 0); w16(o + 0x34, 0); w16(o + 0x32, 0);
        set_w(&r->d[0], pics[r16(o + 0x1E) >> 1]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        set_w(&r->d[0], r16(o + 0x18));
        fn_S7_0A9C(r);
        if (!((uint16_t)r->d[0] & 0x8000)) fn_S7_3784(r);
        return; }
    case 9: {
        static const uint16_t pics[3] = {0x18, 0x17, 0x16};
        set_w(&r->d[0], pics[r16(o + 0x1E) >> 1]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    default:
        return;
    }
}
static const uint16_t t9_hits[] = {0x9, 3, 0x18, 3, 0x29, 3, 0xFFFF};

/* fn_S7_072C: one tick of a type 9 object (see above), as fn_S7_186C. */
void fn_S7_072C(Regs *r) {
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], r16(o + 0x30));
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + sxw(r->d[0]) + 0x12) & 1) w16(o + 0x14, g16(0xF2));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t9_think(r);
    uint16_t s = (uint16_t)r->d[7];
    obj_collide(r, (s == 0 || s == 1 || s == 9) ? t9_hits : no_hits, t9_think);
    t9_act(r);
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- type $12: the spitters ----------
 * Like the hunters (types $20/$21) but slower to react: states 0 idle, 2 hunting (turns towards
 * the player two steps at a time and spits a type $24 shot when nearly in line, then waits $37
 * ticks), 3/4 chasing towards where the player was last seen (and spitting every $4B ticks),
 * 5 hit (sound, $C8 ticks of a 4-frame struggle in state 6), 7 recovering (sound, back to 4).
 * Hits by types $18 and $29 count in states 0, 2, 4 and 6. */
static const uint16_t t12_think_entry[8] = {0x10, 0x64, 0x68, 0x11A, 0x128, 0x1C4, 0x1F6, 0x236};
static const uint16_t t12_act_entry[8] = {0x90, 0xA2, 0xB2, 0x162, 0x188, 0x230, 0x256, 0x2A2};
static void t12_spit(Regs *r, uint16_t wait) {
    set_w(&r->d[0], 0x24);
    set_w(&r->d[1], r16(r->a[4] + 0xC));
    set_w(&r->d[2], g16(0xF2));
    r->a[0] = GADDR(0x0B6A);
    fn_S7_40D6(r);
    obj_sound_level(r);
    mz_seg_of(4, 0x1524), fn_S4_1524();
    w16(r->a[4] + 0x1A, wait);
}
static void t12_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7];
        if (s < 8) set_w(&r->d[0], t12_think_entry[s]);
        uint32_t o = r->a[4];
        switch (s) {
        case 0:
            fn_S7_32DC(r);
            set_w(&r->d[0], g16(0xEC));
            if (!((uint16_t)r->d[0] & 0x8000)) {
                if ((uint16_t)r->d[0] < 5) goto wake;
                set_w(&r->d[0], g16(0xEE)); sub_w(r, &r->d[0], r16(o + 0xC));
                fn_S7_367E(r);
                if ((uint16_t)r->d[0] < 0xF) goto wake;
            }
            fn_S7_3FF0(r);
            if (!(uint16_t)r->d[0]) return;
            w16(o + 0x1C, g16(0x20CA));
            set_w(&r->d[0], 3); continue;
        wake:
            w16(o + 0x1A, 0xF);
            set_w(&r->d[0], 2); continue;
        case 2:
            fn_S7_32DC(r);
            if (r->ccr & CCR_Z) return;
            set_w(&r->d[0], 3); continue;
        case 3:
            w16(o + 0x1E, 0x78);
            set_w(&r->d[0], 4); continue;
        case 4:
            fn_S7_32DC(r);
            if (!(uint16_t)r->d[0]) { set_w(&r->d[0], 2); continue; }
            set_w(&r->d[0], r16(o + 0x1E));
            if (!(uint16_t)r->d[0]) w16(o + 0x1C, 0);
            set_w(&r->d[0], r16(o + 0x1C));
            if ((uint16_t)r->d[0]) return;
            set_w(&r->d[0], 0); continue;
        case 5:
            fn_S7_3128(r);
            obj_sound_level(r);
            mz_seg_of(4, 0x14A0), fn_S4_14A0();
            w16(o + 0x1E, 0xFFFF);
            w16(o + 0x18, 0xC8);
            set_w(&r->d[0], 6); continue;
        case 6:
            w16(o + 0x18, (uint16_t)(r16(o + 0x18) - 1));
            if (!r16(o + 0x18)) { set_w(&r->d[0], 7); continue; }
            set_w(&r->d[0], (uint16_t)(r16(o + 0x1E) + 1));
            if ((uint16_t)r->d[0] >= 4) set_w(&r->d[0], 1);
            w16(o + 0x1E, (uint16_t)r->d[0]);
            return;
        case 7:
            fn_S7_32DC(r);
            obj_sound_level(r);
            mz_seg_of(4, 0x163C), fn_S4_163C();
            w16(o + 0x3A, 0x1D);
            w16(o + 0x1A, 0xA);
            set_w(&r->d[0], 4); continue;
        default:                                    /* 1 */
            return;
        }
    }
}
static void t12_act(Regs *r) {
    uint32_t o = r->a[4];
    uint16_t s = (uint16_t)r->d[7];
    set_w(&r->d[0], (uint16_t)(s < 8 ? t12_act_entry[s] : 0));
    switch (s) {
    case 0:
        fn_S7_3EE2(r);
        if ((uint16_t)r->d[0] >= 0x40) return;
        fn_S7_0DE2(r);
        return;
    case 2:
        w16(o + 0x1C, g16(0x20CA));
        if (r16(o + 0x1A)) {
            set_w(&r->d[0], g16(0xEC));
            if ((uint16_t)r->d[0] >= 0xA) {
                sg16(0xDA, g16(0x20D2)); sg16(0xD6, g16(0x20CE));
                fn_S7_3B36(r);
                fn_S7_3D0E(r);
                set_w(&r->d[0], g16(0xD0));
                if (!((uint16_t)r->d[0] & 0x8000)) { fn_S7_0DE2(r); return; }
            }
            set_w(&r->d[0], g16(0xEE));
            fn_S7_36F4(r);
            return;
        }
        set_w(&r->d[0], g16(0xEE)); sub_w(r, &r->d[0], r16(o + 0xC));
        fn_S7_367E(r);
        if ((uint16_t)r->d[0] >= 2) {
            set_w(&r->d[0], g16(0xEE));
            fn_S7_3722(r);
            set_w(&r->d[0], g16(0xEE)); sub_w(r, &r->d[0], r16(o + 0xC));
            fn_S7_367E(r);
            if ((uint16_t)r->d[0] >= 6) return;
        }
        t12_spit(r, 0x37);
        return;
    case 4: {
        set_w(&r->d[0], r16(o + 0x1C));
        r->a[0] = g32(0x1DCC);
        uint32_t cell = r->a[0] + sxw(r->d[0]);
        sg16(0xDA, r16(cell + 0x1C)); sg16(0xD6, r16(cell + 0x1E));
        fn_S7_3B36(r);
        fn_S7_3D0E(r);
        set_w(&r->d[0], g16(0xD0));
        if (!((uint16_t)r->d[0] & 0x8000)) fn_S7_0DE2(r);
        else {
            set_w(&r->d[0], r16(o + 0x30));
            if ((uint16_t)r->d[0] == r16(o + 0x1C)) w16(o + 0x1C, 0);
            else w16(o + 0x1E, (uint16_t)(r16(o + 0x1E) - 1));
        }
        set_w(&r->d[0], r16(o + 0x1A));
        if ((uint16_t)r->d[0]) return;
        t12_spit(r, 0x4B);
        return; }
    case 6: {
        static const uint16_t pics[4] = {0x27, 0x28, 0x29, 0x2A};
        set_w(&r->d[0], pics[r16(o + 0x1E) & 3]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    default:
        return;
    }
}
static const uint16_t t12_hits[] = {0x18, 5, 0x29, 5, 0xFFFF};

/* fn_S7_0B06: one tick of a type $12 object, as fn_S7_186C (without the cell-flag hit). */
void fn_S7_0B06(Regs *r) {
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t12_think(r);
    uint16_t s = (uint16_t)r->d[7];
    obj_collide(r, (s == 0 || s == 2 || s == 4 || s == 6) ? t12_hits : no_hits, t12_think);
    t12_act(r);
    set_w(&r->d[0], (uint16_t)(r16(o + 0x1A) - 1));
    if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
    w16(o + 0x1A, (uint16_t)r->d[0]);
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* turn towards direction D0.w: if already facing it, take a full step (step tables G_12EA),
 * otherwise turn one notch (fn_S7_36F4) and take a small step (G_0CAA) */
static void turn_and_step(Regs *r) {
    if ((uint16_t)r->d[0] == r16(r->a[4] + 0xC)) { r->a[0] = GADDR(0x12EA); fn_S7_406A(r); return; }
    fn_S7_36F4(r);
    r->a[0] = GADDR(0x0CAA);
    fn_S7_406A(r);
}

/* ---------- types $1E and $1F: the trackers ----------
 * They follow the player's scent (fn_S2_355E floods it from the player's cell, fn_S7_3D5C follows
 * it). States: 0 roaming (on sight within 5, or nearly facing the player: 3; on a scent: 5),
 * 2 following the scent for $32 steps (back to 0 when it runs out), 3 attacking: refresh the scent
 * when the player moves, close in, and throw a type $3D shot when nearly in line (then wait $44-$63
 * ticks, state 4), 4 waiting (backing off), 5 start following, 6-7 hit (6 ticks), 8-9 dead and
 * removed for $1F4 ticks, 10 waiting to respawn in corner cell $3DE0 (costing the player a life
 * point if it is close), 11-12 respawning. Hits by types $18, own type and $29 count in states 0,
 * 2, 3, 4 and 12. Type $1F is the fast kind: it turns faster and takes an extra step when it
 * already faces its way; the two differ only there and in their sounds. */
typedef struct {
    int fast;
    void (*shot_sound)(void), (*hit_sound)(void), (*back_sound)(void);
    const uint16_t *hits;
} TrackerKind;
static const TrackerKind *tracker;               /* the kind running now */
static const uint16_t t1e_think_entry[13] = {0x1A, 0x7E, 0x82, 0xF2, 0x1B2, 0x1F4, 0x202, 0x230, 0x27C, 0x2BA, 0x2D8, 0x33E, 0x382};
static const uint16_t t1e_act_entry[13] = {0xAC, 0xD4, 0x114, 0x162, 0x212, 0x254, 0x282, 0x29C, 0x30C, 0x32A, 0x390, 0x3D4, 0x3F4};
static void scent_reset(Regs *r) {
    sg16(0x209C, 0); sg16(0x209A, 0);
    mz_seg_of(2, 0x355E), fn_S2_355E(r);
}
/* common states of the respawning monsters: dead (removed for $1F4 ticks), waiting in the corner
 * cell $3DE0 and coming back; returns 1 if the state was handled (D0 = the next state, or -1) */
static int obj_dead(Regs *r, uint32_t o, int next) {
    set_w(&r->d[0], r16(o + 0xE));
    r->a[0] = g32(0x1DCC);
    w16(r->a[0] + sxw(r->d[0]) + 0xE, 0);
    set_w(&r->d[0], g16(0xF2));
    set_w(&r->d[1], r16(o + 0x30));
    mz_seg_of(2, 0x3410), fn_S2_3410(r);
    w16(o + 0x10, r16(o + 0x30));
    r->d[0] = 0;
    w32(o + 4, 0); w32(o + 8, 0); w16(o + 0x30, 0); w16(o + 0xE, 0);
    w16(o + 0x1E, 0x1F4);
    set_w(&r->d[0], (uint16_t)next);
    return 1;
}
static int obj_dead_wait(Regs *r, uint32_t o, int next) {
    set_w(&r->d[0], (uint16_t)(r16(o + 0x1E) - 1));
    w16(o + 0x1E, (uint16_t)r->d[0]);
    if (!((uint16_t)r->d[0] & 0x8000)) return 0;
    set_w(&r->d[0], g16(0xF2));
    mz_seg_of(2, 0x3480), fn_S2_3480(r);
    set_w(&r->d[0], (uint16_t)next);
    return 1;
}
/* waiting to respawn in the corner cell; with check_player, too close to the player costs a life
 * point instead (fn_S7_3ACA with distance G_00E0 = check_player) */
static int obj_respawn(Regs *r, int check_player, int next) {
    set_w(&r->d[1], 0x3DE0);
    r->a[0] = g32(0x1DCC);
    uint32_t cell = r->a[0] + 0x3DE0;
    sg16(0xEA, r16(cell + 0x1C)); sg16(0xE6, r16(cell + 0x1E));
    sg16(0xE2, 0x3DE0);
    set_w(&r->d[0], r16(cell + 0xE));
    if ((uint16_t)r->d[0]) return 0;
    fn_S7_3972(r);
    if ((uint16_t)r->d[0]) return 0;
    set_w(&r->d[0], 0x3DE0);
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + 0x3DE0 + 0x12) & 1) return 0;
    if (check_player) {
        sg16(0xE0, (uint16_t)check_player);
        fn_S7_3ACA(r);
        if ((uint16_t)r->d[0]) {
            mz_seg_of(4, 0x1734), fn_S4_1734();
            set_w(&r->d[0], (uint16_t)(g16(0x20FE) - 1));
            if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
            sg16(0x20FE, (uint16_t)r->d[0]);
            return 0;
        }
    }
    fn_S7_3784(r);
    set_w(&r->d[0], (uint16_t)next);
    return 1;
}
static void obj_reappear(Regs *r, uint32_t o, void (*sound)(void), int next) {
    fn_S7_32DC(r);
    obj_sound_level(r);
    mz_seg(4), sound();
    w16(o + 0xE, r16(o + 0x30));
    set_w(&r->d[1], r16(o + 0x30));
    r->a[0] = g32(0x1DCC);
    w16(r->a[0] + sxw(r->d[1]) + 0xE, 1);
    w16(o + 0x10, 0);
    w16(o + 0x1E, 0xFFFF);
    set_w(&r->d[0], (uint16_t)next);
}
static int obj_count_up(Regs *r, uint32_t field, uint16_t limit) {
    set_w(&r->d[0], (uint16_t)(r16(r->a[4] + field) + 1));
    w16(r->a[4] + field, (uint16_t)r->d[0]);
    return (uint16_t)r->d[0] >= limit;
}

/* moves towards direction D0.w. Slow kind: turn_and_step. Fast kind: when not facing that way,
 * turn (twice when roaming / backing off: `turn2`) and take a full step; when facing it, take a full
 * step and, if the direction (re-evaluated by `again`) still matches, an extra step (G_0DEA). */
static void tracker_step(Regs *r, int turn2, void (*again)(Regs *)) {
    if (!tracker->fast) { turn_and_step(r); return; }
    if ((uint16_t)r->d[0] != r16(r->a[4] + 0xC)) {
        if (turn2) fn_S7_3722(r); else fn_S7_36F4(r);
        r->a[0] = GADDR(0x12EA); fn_S7_406A(r);
        return;
    }
    r->a[0] = GADDR(0x12EA); fn_S7_406A(r);
    again(r);
    if ((uint16_t)r->d[0] != r16(r->a[4] + 0xC)) return;
    r->a[0] = GADDR(0x0DEA); fn_S7_406A(r);
}
static void again_route(Regs *r) { fn_S7_3EE2(r); }
static void again_scent(Regs *r) {
    fn_S7_3D5C(r);
    set_w(&r->d[3], (uint16_t)r->d[0]);
    fn_S7_3D0E(r);
    set_w(&r->d[0], (uint16_t)r->d[3]);
}

static void t1e_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7];
        if (s < 13) set_w(&r->d[0], t1e_think_entry[s]);
        uint32_t o = r->a[4];
        switch (s) {
        case 0:
            fn_S7_319E(r);
            set_w(&r->d[0], g16(0xEC));
            if (!((uint16_t)r->d[0] & 0x8000)) {
                if ((uint16_t)r->d[0] < 5) { set_w(&r->d[0], 3); continue; }
                set_w(&r->d[0], g16(0xEE)); sub_w(r, &r->d[0], r16(o + 0xC));
                fn_S7_367E(r);
                if ((uint16_t)r->d[0] < 0x14) { set_w(&r->d[0], 3); continue; }
            }
            set_w(&r->d[0], r16(o + 0x30));
            r->a[0] = g32(0x1DCC);
            set_w(&r->d[0], r16(r->a[0] + sxw(r->d[0]) + 0x10));
            if (!(uint16_t)r->d[0]) return;
            set_w(&r->d[0], 5); continue;
        case 2:
            fn_S7_319E(r);
            if (!(uint16_t)r->d[0]) { set_w(&r->d[0], 3); continue; }
            set_w(&r->d[0], r16(o + 0x30));
            r->a[0] = g32(0x1DCC);
            set_w(&r->d[0], r16(r->a[0] + sxw(r->d[0]) + 0x10));
            if (!(uint16_t)r->d[0]) { set_w(&r->d[0], 0); continue; }
            if ((uint16_t)r->d[0] != 0xFFFF) {
                set_w(&r->d[0], r16(o + 0x1C));
                if (!((uint16_t)r->d[0] & 0x8000)) return;
            }
            scent_reset(r);
            set_w(&r->d[0], 0); continue;
        case 3:
            fn_S7_319E(r);
            if ((uint16_t)r->d[0]) { set_w(&r->d[0], 5); continue; }
            set_w(&r->d[0], r16(o + 0x1A));
            if (!(uint16_t)r->d[0]) return;
            set_w(&r->d[0], 4); continue;
        case 4:
            set_w(&r->d[0], r16(o + 0x1A));
            if ((uint16_t)r->d[0]) return;
            set_w(&r->d[0], 0); continue;
        case 5:
            w16(o + 0x1C, 0x32);
            set_w(&r->d[0], 2); continue;
        case 6:
            fn_S7_3128(r);
            obj_sound_level(r);
            mz_seg(4), tracker->hit_sound();
            w16(o + 0x1C, 0xFFFF);
            set_w(&r->d[0], 7); continue;
        case 7:
            if (!obj_count_up(r, 0x1C, 6)) return;
            set_w(&r->d[0], 8); continue;
        case 8:  obj_dead(r, o, 9); continue;
        case 9:  if (!obj_dead_wait(r, o, 10)) return; continue;
        case 10: if (!obj_respawn(r, 4, 11)) return; continue;
        case 11: obj_reappear(r, o, tracker->back_sound, 12); continue;
        case 12:
            if (!obj_count_up(r, 0x1E, 6)) return;
            set_w(&r->d[0], g16(0xF2));
            mz_seg_of(2, 0x3480), fn_S2_3480(r);
            set_w(&r->d[0], 0); continue;
        default:                                    /* 1 */
            return;
        }
    }
}
static void t1e_act(Regs *r) {
    uint32_t o = r->a[4];
    uint16_t s = (uint16_t)r->d[7];
    set_w(&r->d[0], (uint16_t)(s < 13 ? t1e_act_entry[s] : 0));
    switch (s) {
    case 0:
        fn_S7_3EE2(r);
        if ((uint16_t)r->d[0] & 0x8000) return;
        tracker_step(r, 1, again_route);
        return;
    case 2:
        again_scent(r);
        if ((uint16_t)r->d[0] & 0x8000) { w16(o + 0x1C, (uint16_t)(r16(o + 0x1C) - 1)); return; }
        tracker_step(r, 0, again_scent);
        return;
    case 3:
        set_w(&r->d[0], g16(0x20CA));
        if ((uint16_t)r->d[0] != g16(0x22AC)) {
            sg16(0x209A, (uint16_t)r->d[0]); sg16(0x209C, 0x23);
            mz_seg_of(2, 0x355E), fn_S2_355E(r);
        }
        fn_S7_3D2A(r);
        if (!((uint16_t)r->d[0] & 0x8000)) {
            again_scent(r);
            if (!((uint16_t)r->d[0] & 0x8000)) { tracker_step(r, 0, again_scent); return; }
        }
        set_w(&r->d[0], g16(0xEE));
        fn_S7_3722(r);
        sub_w(r, &r->d[0], r16(o + 0xC));
        fn_S7_367E(r);
        if ((uint16_t)r->d[0] >= 6) return;
        obj_sound_level(r);
        mz_seg(4), tracker->shot_sound();
        set_w(&r->d[0], 0x3D);
        set_w(&r->d[1], g16(0xEE));
        set_w(&r->d[2], g16(0xF2));
        r->a[0] = GADDR(0x0B6A);
        fn_S7_40D6(r);
        mz_seg_of(2, 0x3532), fn_S2_3532(r);
        set_w(&r->d[0], (uint16_t)(((uint16_t)r->d[0] & 0x1F) + 0x44));
        w16(o + 0x1A, (uint16_t)r->d[0]);
        return;
    case 4:
        fn_S7_3EE2(r);
        if (!((uint16_t)r->d[0] & 0x8000)) { tracker_step(r, 0, again_route); return; }
        set_w(&r->d[0], (uint16_t)(r16(o + 0x1A) - 2));
        if ((uint16_t)r->d[0] & 0x8000) set_w(&r->d[0], 0);
        w16(o + 0x1A, (uint16_t)r->d[0]);
        return;
    case 7: {
        static const uint16_t pics[3] = {0x1A, 0x1B, 0x1C};
        w16(o + 0x2E, 0); w16(o + 0x34, 0); w16(o + 0x32, 0);
        set_w(&r->d[0], pics[r16(o + 0x1C) >> 1]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        set_w(&r->d[0], r16(o + 0xC));
        r->a[0] = GADDR(tracker->fast ? 0x12EA : 0x0CAA);
        fn_S7_406A(r);
        return; }
    case 12: {
        static const uint16_t pics[3] = {0x1C, 0x1B, 0x1A};
        set_w(&r->d[0], pics[r16(o + 0x1E) >> 1]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    default:
        return;
    }
}
static const uint16_t t1e_hits[] = {0x18, 6, 0x1E, 6, 0x29, 6, 0xFFFF};
static const uint16_t t1f_hits[] = {0x18, 6, 0x1F, 6, 0x29, 6, 0xFFFF};
static const TrackerKind kind1e = {0, fn_S4_14F8, fn_S4_1474, fn_S4_1610, t1e_hits};
static const TrackerKind kind1f = {1, fn_S4_150E, fn_S4_148A, fn_S4_1626, t1f_hits};

static void tracker_tick(Regs *r, const TrackerKind *k) {
    uint32_t o = r->a[4];
    tracker = k;
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], r16(o + 0x30));
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + sxw(r->d[0]) + 0x12) & 1) w16(o + 0x14, g16(0xF2));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t1e_think(r);
    uint16_t s = (uint16_t)r->d[7];
    obj_collide(r, (s == 0 || s == 2 || s == 3 || s == 4 || s == 12) ? k->hits : no_hits, t1e_think);
    t1e_act(r);
    set_w(&r->d[0], (uint16_t)(r16(o + 0x1A) - 1));
    if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
    w16(o + 0x1A, (uint16_t)r->d[0]);
    w16(o + 0x16, (uint16_t)r->d[7]);
}
/* fn_S7_0F18 / fn_S7_1376: one tick of a type $1E / $1F object, as fn_S7_186C. */
void fn_S7_0F18(Regs *r) { tracker_tick(r, &kind1e); }
void fn_S7_1376(Regs *r) { tracker_tick(r, &kind1f); }

/* ---------- type $28: the bruisers ----------
 * Like the hunters, but they store up shots (+$18, one more per tick up to $C8, $28 a shot) and
 * health (+$20, up to $15E; a hit costs $64 and only kills when it runs out, otherwise it just
 * angers them: state 2). Shots are type $29, fired fast (step table G_0DEA) towards where the
 * player will be (fn_S7_3694) or straight ahead. States: 0 idle, 2 hunting, 3/4 chasing, 5 hit,
 * 6 dying (8 ticks), 7-8 dead and removed for $7D0 ticks, 9 waiting to respawn in corner cell
 * $3DE0, 10-11 respawning (16 ticks, health back to $15E). Hits by type $18 count in states 0, 2,
 * 4 and 11. */
static const uint16_t t28_think_entry[12] = {0x18, 0x70, 0x74, 0x160, 0x16E, 0x21E, 0x280, 0x2C2, 0x300, 0x31E, 0x384, 0x3C8};
static const uint16_t t28_act_entry[12] = {0xA4, 0xBA, 0xCA, 0x1B4, 0x1DA, 0x2C6, 0x2E0, 0x346, 0x364, 0x3CA, 0x40E, 0x434};
static void t28_fire(Regs *r, uint32_t steps) {
    w16(r->a[4] + 0x18, (uint16_t)(r16(r->a[4] + 0x18) - 0x28));
    set_w(&r->d[1], (uint16_t)r->d[0]);
    set_w(&r->d[0], 0x29);
    set_w(&r->d[2], g16(0xF2));
    r->a[0] = GADDR(steps);
    fn_S7_40D6(r);
    obj_sound_level(r);
    mz_seg_of(4, 0x153A), fn_S4_153A();
    w16(r->a[4] + 0x1A, 0xA);
}
static void t28_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7];
        if (s < 12) set_w(&r->d[0], t28_think_entry[s]);
        uint32_t o = r->a[4];
        switch (s) {
        case 0:
            fn_S7_32DC(r);
            set_w(&r->d[0], g16(0xEC));
            if (!((uint16_t)r->d[0] & 0x8000)) {
                if ((uint16_t)r->d[0] < 5) goto wake;
                set_w(&r->d[0], g16(0xEE)); sub_w(r, &r->d[0], r16(o + 0xC));
                fn_S7_367E(r);
                if ((uint16_t)r->d[0] < 0xF) goto wake;
            }
            fn_S7_3FF0(r);
            if (!(uint16_t)r->d[0]) return;
            w16(o + 0x1C, g16(0x20CA));
            set_w(&r->d[0], 3); continue;
        wake:
            w16(o + 0x1A, 0xC);
            set_w(&r->d[0], 2); continue;
        case 2:
            fn_S7_32DC(r);
            if (r->ccr & CCR_Z) return;
            set_w(&r->d[0], 3); continue;
        case 3:
            w16(o + 0x1E, 0x78);
            set_w(&r->d[0], 4); continue;
        case 4:
            fn_S7_32DC(r);
            if (!(uint16_t)r->d[0]) { set_w(&r->d[0], 2); continue; }
            set_w(&r->d[0], r16(o + 0x1E));
            if (!(uint16_t)r->d[0]) w16(o + 0x1C, 0);
            set_w(&r->d[0], r16(o + 0x1C));
            if ((uint16_t)r->d[0]) return;
            set_w(&r->d[0], 0); continue;
        case 5:
            set_w(&r->d[0], r16(o + 0x20));
            sub_w(r, &r->d[0], 0x64);
            w16(o + 0x20, (uint16_t)r->d[0]);
            fn_S7_3128(r);
            obj_sound_level(r);
            if (!(r16(o + 0x20) & 0x8000)) {           /* health left: just angered */
                mz_seg_of(4, 0x1668), fn_S4_1668();
                set_w(&r->d[0], 2); continue;
            }
            mz_seg_of(4, 0x14B6), fn_S4_14B6();
            w16(o + 0x20, 0);
            w16(o + 0x1E, 0xFFFF);
            set_w(&r->d[0], 6); continue;
        case 6:
            if (!obj_count_up(r, 0x1E, 8)) return;
            set_w(&r->d[0], 7); continue;
        case 7:
            obj_dead(r, o, 8);
            w16(o + 0x1E, 0x7D0);
            continue;
        case 8:  if (!obj_dead_wait(r, o, 9)) return; continue;
        case 9:  if (!obj_respawn(r, 5, 10)) return; continue;
        case 10: obj_reappear(r, o, fn_S4_1652, 11); continue;
        case 11:
            if (!obj_count_up(r, 0x1E, 0x10)) return;
            set_w(&r->d[0], g16(0xF2));
            mz_seg_of(2, 0x3480), fn_S2_3480(r);
            w16(o + 0x20, 0x15E);
            set_w(&r->d[0], 0); continue;
        default:                                    /* 1 */
            return;
        }
    }
}
static void t28_act(Regs *r) {
    uint32_t o = r->a[4];
    uint16_t s = (uint16_t)r->d[7];
    set_w(&r->d[0], (uint16_t)(s < 12 ? t28_act_entry[s] : 0));
    switch (s) {
    case 0:
        fn_S7_3EE2(r);
        if ((uint16_t)r->d[0] >= 0x40) return;
        r->a[0] = GADDR(0x0F2A);
        fn_S7_2590(r);
        return;
    case 2:
        w16(o + 0x1C, g16(0x20CA));
        if (!r16(o + 0x1A) && r16(o + 0x18) >= 0x28) {
            set_w(&r->d[0], g16(0xEC));
            if ((uint16_t)r->d[0] >= 0x10) {
                r->a[0] = g32(0x1DCC);
                set_w(&r->d[0], g16(0x20CA));
                uint32_t cell = r->a[0] + sxw(r->d[0]);
                sg16(0xDA, r16(cell + 0x1C)); sg16(0xD6, r16(cell + 0x1E));
                fn_S7_3694(r);
                set_w(&r->d[2], (uint16_t)r->d[0]);
                sub_w(r, &r->d[0], r16(o + 0xC));
                fn_S7_367E(r);
                if ((uint16_t)r->d[0] < 0x10) { set_w(&r->d[0], (uint16_t)r->d[2]); t28_fire(r, 0x0DEA); return; }
            } else {
                set_w(&r->d[0], g16(0xEE)); sub_w(r, &r->d[0], r16(o + 0xC));
                fn_S7_367E(r);
                set_w(&r->d[0], (uint16_t)r->d[0] < 0x10 ? g16(0xEE) : r16(o + 0xC));
                t28_fire(r, 0x0DEA);
                return;
            }
        }
        set_w(&r->d[0], g16(0xEC));
        if ((uint16_t)r->d[0] >= 0xA) {
            sg16(0xDA, g16(0x20D2)); sg16(0xD6, g16(0x20CE));
            fn_S7_3B36(r);
            fn_S7_3D0E(r);
            set_w(&r->d[0], g16(0xD0));
            if (!((uint16_t)r->d[0] & 0x8000)) { r->a[0] = GADDR(0x0DEA); fn_S7_2590(r); return; }
        }
        set_w(&r->d[0], g16(0xEE));
        fn_S7_3722(r);
        return;
    case 4: {
        set_w(&r->d[0], r16(o + 0x1C));
        r->a[0] = g32(0x1DCC);
        uint32_t cell = r->a[0] + sxw(r->d[0]);
        sg16(0xDA, r16(cell + 0x1C)); sg16(0xD6, r16(cell + 0x1E));
        fn_S7_3B36(r);
        fn_S7_3D0E(r);
        set_w(&r->d[0], g16(0xD0));
        if ((uint16_t)r->d[0] & 0x8000) {
            set_w(&r->d[0], r16(o + 0x30));
            if ((uint16_t)r->d[0] == r16(o + 0x1C)) w16(o + 0x1C, 0);
            else w16(o + 0x1E, (uint16_t)(r16(o + 0x1E) - 1));
            return;
        }
        r->a[0] = GADDR(0x0F2A);
        fn_S7_2590(r);
        set_w(&r->d[0], r16(o + 0x1A));
        if ((uint16_t)r->d[0]) return;
        set_w(&r->d[0], r16(o + 0x18));
        if ((uint16_t)r->d[0] < 0x28) return;
        set_w(&r->d[0], r16(o + 0xC));
        t28_fire(r, 0x12EA);
        return; }
    case 6: {
        static const uint16_t pics[4] = {0x39, 0x3A, 0x3B, 0x3C};
        w16(o + 0x2E, 0); w16(o + 0x34, 0); w16(o + 0x32, 0);
        set_w(&r->d[0], pics[(r16(o + 0x1E) >> 1) & 3]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    case 11: {
        static const uint16_t pics[4] = {0x3C, 0x3B, 0x3A, 0x39};
        set_w(&r->d[0], pics[(r16(o + 0x1E) >> 2) & 3]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    default:
        return;
    }
}
static const uint16_t t28_hits[] = {0x18, 5, 0xFFFF};

/* fn_S7_20C8: one tick of a type $28 object, as fn_S7_186C, then shots and health recharge. */
void fn_S7_20C8(Regs *r) {
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], r16(o + 0x30));
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + sxw(r->d[0]) + 0x12) & 1) w16(o + 0x14, g16(0xF2));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t28_think(r);
    uint16_t s = (uint16_t)r->d[7];
    obj_collide(r, (s == 0 || s == 2 || s == 4 || s == 11) ? t28_hits : no_hits, t28_think);
    t28_act(r);
    set_w(&r->d[0], (uint16_t)(r16(o + 0x1A) - 1));
    if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
    w16(o + 0x1A, (uint16_t)r->d[0]);
    set_w(&r->d[0], (uint16_t)(r16(o + 0x18) + 1));
    if ((uint16_t)r->d[0] >= 0xC8) set_w(&r->d[0], 0xC8);
    w16(o + 0x18, (uint16_t)r->d[0]);
    set_w(&r->d[0], (uint16_t)(r16(o + 0x20) + 1));
    if ((uint16_t)r->d[0] >= 0x15E) set_w(&r->d[0], 0x15E);
    w16(o + 0x20, (uint16_t)r->d[0]);
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- type $2D: the gunners ----------
 * They store up shots (+$18, one a tick up to $69, $F a shot) and, while they see the player,
 * fire type $36 shots every other tick at where the player is heading (fn_S7_2A86), closing in
 * when far. States: 0 idle (seeing the player: 1, with a cooldown of 8; something in its cell: 2),
 * 1 shooting, 2/3 chasing towards where the player was last seen, 4-5 hit (6 ticks), 6-7 dead and
 * removed for $1F4 ticks, 8 waiting to respawn in corner cell $3DE0, 9-10 respawning (16 ticks).
 * Hits by types $18, $29 and $2D count in states 0, 1, 3 and 10. */
static const uint16_t t2d_think_entry[11] = {0x16, 0x56, 0xEC, 0xFA, 0x160, 0x18C, 0x1CC, 0x20A, 0x228, 0x28E, 0x2D2};
static const uint16_t t2d_act_entry[11] = {0x8E, 0xB2, 0x144, 0x16A, 0x1D6, 0x1F0, 0x254, 0x272, 0x2D8, 0x31C, 0x33C};
static void t2d_think(Regs *r) {
    for (;;) {
        set_w(&r->d[7], (uint16_t)r->d[0]);
        uint16_t s = (uint16_t)r->d[7];
        if (s < 11) set_w(&r->d[0], t2d_think_entry[s]);
        uint32_t o = r->a[4];
        switch (s) {
        case 0:
            fn_S7_32DC(r);
            set_w(&r->d[0], g16(0xEE));
            if (!((uint16_t)r->d[0] & 0x8000)) { w16(o + 0x1A, 8); set_w(&r->d[0], 1); continue; }
            fn_S7_3FF0(r);
            if (!(uint16_t)r->d[0]) return;
            w16(o + 0x1C, g16(0x20CA));
            set_w(&r->d[0], 2); continue;
        case 1:
            fn_S7_32DC(r);
            if (!(uint16_t)r->d[0]) return;
            set_w(&r->d[0], 2); continue;
        case 2:
            w16(o + 0x1E, 0x78);
            set_w(&r->d[0], 3); continue;
        case 3:
            fn_S7_32DC(r);
            if (!(uint16_t)r->d[0]) { set_w(&r->d[0], 1); continue; }
            set_w(&r->d[0], r16(o + 0x1E));
            if (!(uint16_t)r->d[0]) w16(o + 0x1C, 0);
            set_w(&r->d[0], r16(o + 0x1C));
            if ((uint16_t)r->d[0]) return;
            set_w(&r->d[0], 0); continue;
        case 4:
            fn_S7_3128(r);
            obj_sound_level(r);
            mz_seg_of(4, 0x1448), fn_S4_1448();
            w16(o + 0x1E, 0xFFFF);
            set_w(&r->d[0], 5); continue;
        case 5:
            if (!obj_count_up(r, 0x1E, 6)) return;
            set_w(&r->d[0], 6); continue;
        case 6:  obj_dead(r, o, 7); continue;
        case 7:  if (!obj_dead_wait(r, o, 8)) return; continue;
        case 8:  if (!obj_respawn(r, 5, 9)) return; continue;
        case 9:  obj_reappear(r, o, fn_S4_15E4, 10); continue;
        case 10:
            if (!obj_count_up(r, 0x1E, 0x10)) return;
            set_w(&r->d[0], g16(0xF2));
            mz_seg_of(2, 0x3480), fn_S2_3480(r);
            set_w(&r->d[0], 0); continue;
        default:
            return;
        }
    }
}
static void t2d_act(Regs *r) {
    uint32_t o = r->a[4];
    uint16_t s = (uint16_t)r->d[7];
    set_w(&r->d[0], (uint16_t)(s < 11 ? t2d_act_entry[s] : 0));
    switch (s) {
    case 0:
        fn_S7_3EE2(r);
        if ((uint16_t)r->d[0] >= 0x40) return;
        r->a[0] = GADDR(0x0F2A);
        fn_S7_406A(r);
        return;
    case 1:
        w16(o + 0x1C, g16(0x20CA));
        set_w(&r->d[0], r16(o + 0x1A));
        if ((uint16_t)r->d[0]) return;
        set_w(&r->d[0], g16(0x20BC) & 1);
        if (!(uint16_t)r->d[0]) {
            set_w(&r->d[0], r16(o + 0x18));
            if ((uint16_t)r->d[0] >= 0xF) {
                set_w(&r->d[0], (uint16_t)((uint16_t)r->d[0] - 0xF));
                w16(o + 0x18, (uint16_t)r->d[0]);
                obj_sound_level(r);
                mz_seg_of(4, 0x14E2), fn_S4_14E2();
                fn_S7_2A86(r);
                set_w(&r->d[1], (uint16_t)r->d[0]);
                set_w(&r->d[0], 0x36);
                set_w(&r->d[2], g16(0xF2));
                r->a[0] = GADDR(0x0B6A);
                fn_S7_40D6(r);
            }
        }
        set_w(&r->d[0], g16(0xEC));
        if ((uint16_t)r->d[0] < 0xA) return;
        sg16(0xDA, g16(0x20D2)); sg16(0xD6, g16(0x20CE));
        fn_S7_3B36(r);
        fn_S7_3D0E(r);
        set_w(&r->d[0], g16(0xD0));
        if ((uint16_t)r->d[0] & 0x8000) return;
        r->a[0] = GADDR(0x0DEA);
        fn_S7_406A(r);
        return;
    case 3: {
        set_w(&r->d[0], r16(o + 0x1C));
        r->a[0] = g32(0x1DCC);
        uint32_t cell = r->a[0] + sxw(r->d[0]);
        sg16(0xDA, r16(cell + 0x1C)); sg16(0xD6, r16(cell + 0x1E));
        fn_S7_3B36(r);
        fn_S7_3D0E(r);
        set_w(&r->d[0], g16(0xD0));
        if (!((uint16_t)r->d[0] & 0x8000)) { r->a[0] = GADDR(0x0F2A); fn_S7_406A(r); return; }
        set_w(&r->d[0], r16(o + 0x30));
        if ((uint16_t)r->d[0] == r16(o + 0x1C)) w16(o + 0x1C, 0);
        else w16(o + 0x1E, (uint16_t)(r16(o + 0x1E) - 1));
        return; }
    case 5: {
        static const uint16_t pics[3] = {0x13, 0x14, 0x15};
        w16(o + 0x2E, 0); w16(o + 0x34, 0); w16(o + 0x32, 0);
        set_w(&r->d[0], pics[r16(o + 0x1E) >> 1]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    case 10: {
        static const uint16_t pics[4] = {0x32, 0x33, 0x34, 0x00};
        set_w(&r->d[0], pics[(r16(o + 0x1E) >> 2) & 3]);
        w16(o + 0x3A, (uint16_t)r->d[0]);
        return; }
    default:
        return;
    }
}
static const uint16_t t2d_hits[] = {0x18, 4, 0x29, 4, 0x2D, 4, 0xFFFF};

/* fn_S7_26C4: one tick of a type $2D object, as fn_S7_186C, with the shot store refilled
 * (fn_S7_2A70) after acting. */
void fn_S7_26C4(Regs *r) {
    uint32_t o = r->a[4];
    set_w(&r->d[7], r16(o + 0x16));
    set_w(&r->d[0], r16(o + 0x30));
    r->a[0] = g32(0x1DCC);
    if (r8(r->a[0] + sxw(r->d[0]) + 0x12) & 1) w16(o + 0x14, g16(0xF2));
    set_w(&r->d[0], (uint16_t)r->d[7]);
    t2d_think(r);
    uint16_t s = (uint16_t)r->d[7];
    obj_collide(r, (s == 0 || s == 1 || s == 3 || s == 10) ? t2d_hits : no_hits, t2d_think);
    t2d_act(r);
    fn_S7_2A70(r);
    set_w(&r->d[0], (uint16_t)(r16(o + 0x1A) - 1));
    if ((uint16_t)r->d[0] & 0x8000) r->d[0] = 0;
    w16(o + 0x1A, (uint16_t)r->d[0]);
    w16(o + 0x16, (uint16_t)r->d[7]);
}

/* ---------- the per-type dispatchers ---------- */
typedef void (*ObjFn)(Regs *);
static ObjFn obj_update[63], obj_draw[63];
/* D0.w as the original's dispatch leaves it at each handler (its case's offset in the table) */
static const uint16_t obj_update_d0[63] = {
    0x82, 0x8A, 0x92, 0x9A, 0xA2, 0xAA, 0xB2, 0xBA, 0xC2, 0xCA, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E,
    0x7E, 0x7E, 0xD2, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0xDA, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0xE2, 0xEA,
    0xF2, 0xF2, 0xFA, 0x102, 0x10A, 0x7E, 0x7E, 0x7E, 0x112, 0x11A, 0x7E, 0x7E, 0x7E, 0x122, 0x7E, 0x7E,
    0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x12A, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x132, 0x7E};
static const uint16_t obj_draw_d0[63] = {
    0x82, 0x8A, 0x92, 0x9A, 0xA2, 0xAA, 0xB2, 0xBA, 0xC2, 0xCA, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E,
    0x7E, 0x7E, 0xD2, 0x7E, 0xDA, 0x7E, 0x7E, 0x7E, 0xE2, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0xEA, 0xEA,
    0xF2, 0xF2, 0xFA, 0xFA, 0xFA, 0x7E, 0x7E, 0x7E, 0x102, 0x10A, 0x7E, 0x7E, 0x7E, 0x112, 0x7E, 0x7E,
    0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x11A, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x11A, 0x7E};
static void obj_tables(void) {
    if (obj_update[0]) return;
    static const struct { uint8_t type; ObjFn update, draw; } t[] = {
        {0x00, fn_S7_0014, fn_S7_0000}, {0x01, fn_S7_0136, fn_S7_0108}, {0x02, fn_S7_01AC, fn_S7_017E},
        {0x03, fn_S7_023E, fn_S7_020E}, {0x04, fn_S7_03C2, fn_S7_0394}, {0x05, fn_S7_0452, fn_S7_0424},
        {0x06, fn_S7_04E2, fn_S7_04B4}, {0x07, fn_S7_05AA, fn_S7_0544}, {0x08, fn_S7_063C, fn_S7_0618},
        {0x09, fn_S7_072C, fn_S7_0704}, {0x12, fn_S7_0B06, fn_S7_0ADE}, {0x14, 0, fn_S7_0E0A},
        {0x18, fn_S7_0E46, fn_S7_0E2A}, {0x1E, fn_S7_0F18, fn_S7_0EF0}, {0x1F, fn_S7_1376, fn_S7_0EF0},
        {0x20, fn_S7_186C, fn_S7_1844}, {0x21, fn_S7_186C, fn_S7_1844}, {0x22, fn_S7_1DB2, fn_S7_1D0E},
        {0x23, fn_S7_1E82, fn_S7_1D0E}, {0x24, fn_S7_1FBC, fn_S7_1D0E}, {0x28, fn_S7_20C8, fn_S7_20A0},
        {0x29, fn_S7_2620, fn_S7_2600}, {0x2D, fn_S7_26C4, fn_S7_269C}, {0x36, fn_S7_2B3E, fn_S7_2AD2},
        {0x3D, fn_S7_2C60, fn_S7_2AD2},
    };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) { obj_update[t[k].type] = t[k].update; obj_draw[t[k].type] = t[k].draw; }
}

/* fn_S7_2E9A (JT 109): one tick of the object at record offset D0.w of G_1DC4 (A4; G_00F2 = its
 * offset) by its type (+2). Types without behaviour do nothing. Keeps D3-D7/A2-A4. */
void fo_S7_0F78(Regs *r);
void fn_S7_2E9A(Regs *r) {
    if (mz_original) { fo_S7_0F78(r); return; }             /* Original: its dispatcher (generated) */
    uint32_t d3 = r->d[3], d4 = r->d[4], d5 = r->d[5], d6 = r->d[6], d7 = r->d[7];
    uint32_t a2 = r->a[2], a3 = r->a[3], a4 = r->a[4];
    obj_tables();
    sg16(0xF2, (uint16_t)r->d[0]);
    r->d[0] &= 0xFFFF;
    r->a[4] = g32(0x1DC4) + r->d[0];
    set_w(&r->d[0], r16(r->a[4] + 2));
    uint16_t type = (uint16_t)r->d[0];
    if (type < 63) set_w(&r->d[0], obj_update_d0[type]);
    if (type < 63 && obj_update[type]) obj_update[type](r);
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->d[6] = d6; r->d[7] = d7;
    r->a[2] = a2; r->a[3] = a3; r->a[4] = a4;
    cc_nz_l(r, d7);
}

/* fn_S7_2D1C (JT 108): draws object A0 (G_00F2 = D1.w, its record offset) by type D0.w.
 * Keeps D3-D7/A2-A4. */
void fn_S7_2D1C(Regs *r) {
    uint32_t d3 = r->d[3], d4 = r->d[4], d5 = r->d[5], d6 = r->d[6], d7 = r->d[7];
    uint32_t a2 = r->a[2], a3 = r->a[3], a4 = r->a[4];
    obj_tables();
    r->a[4] = r->a[0];
    sg16(0xF2, (uint16_t)r->d[1]);
    uint16_t type = (uint16_t)r->d[0];
    if (type < 63) set_w(&r->d[0], obj_draw_d0[type]);
    if (type < 63 && obj_draw[type]) obj_draw[type](r);
    r->d[3] = d3; r->d[4] = d4; r->d[5] = d5; r->d[6] = d6; r->d[7] = d7;
    r->a[2] = a2; r->a[3] = a3; r->a[4] = a4;
    cc_nz_l(r, d7);
}
