/* CODE segment 8: the game loop and its event handling (see build/disasm/seg8.s, $0A24-$11EE). */
#include "mzasm.h"
#include "fns.h"

/* fn_S8_133E: activate event: the original reads the window and the activate flag but does nothing
 * with them. */
void fn_S8_133E(uint32_t ev) { (void)ev; }

/* fn_S8_11BA: suspend / resume (bit 0 of the message): resuming clears G_22C6 and resets the cursor;
 * suspending sets G_22C6 and ends the game loop (G_2220, G_221F, G_221E). */
void fn_S8_11BA(uint32_t ev) {
    if (r8(ev + 5) & 1) {
        sg8(0x22C6, 0);
        mac_ShowCursor(); mac_HideCursor();
    } else {
        sg8(0x22C6, 1);
        sg8(0x2220, 1); sg8(0x221F, 1); sg8(0x221E, 1);
    }
}

/* fn_S8_1192: operating-system event: suspend / resume messages (high byte 1) go to fn_S8_11BA. */
void fn_S8_1192(uint32_t ev) {
    if (((r32(ev + 2) >> 24) & 0xFF) == 1) fn_S8_11BA(ev);
}

/* fn_S8_10EC: dispatches event `ev` during play: key down / auto-key (fn_S8_124E), update
 * (fn_S8_11F0), activate (fn_S8_133E), operating-system (fn_S8_1192); a disk event or a high-level
 * event ends the game loop (G_2220, G_221F, G_221E). Mouse events are ignored. */
void fn_S8_10EC(uint32_t ev) {
    switch (r16(ev)) {
    case 3: case 5: fn_S8_124E(ev); break;              /* keyDown, autoKey */
    case 6: fn_S8_11F0(r32(ev + 2)); break;              /* updateEvt */
    case 8: fn_S8_133E(ev); break;                       /* activateEvt */
    case 15: fn_S8_1192(ev); break;                      /* osEvt */
    case 7: case 23:                                     /* diskEvt, kHighLevelEvent */
        sg8(0x2220, 1); sg8(0x221F, 1); sg8(0x221E, 1);
        break;
    default: break;
    }
}

/* fn_S8_0F90 / 1004 / 1078: recolour this frame's floor and ceiling runs (G_1DEC, G_2150 rows) through
 * a 16-entry table, for the shading modes $2B / $2C / $2D of G_2102. */
static void recolour_runs(const uint8_t map[16]) {
    uint32_t row = g32(0x1DEC) + 2;
    uint16_t n = g16(0x2150);
    do {
        for (uint32_t p = row; r16(p) != 0xFFFF; p += 2) {
            uint16_t w = r16(p), c = w & 0x3F;
            if (c < 0x10) w16(p, (uint16_t)((w & 0xFFC0) | map[c]));
        }
        row += 0xCC;
    } while (--n);
}
void fn_S8_0F90(void) { static const uint8_t m[16] = {14, 1, 15, 15, 15, 15, 15, 15, 8, 9, 10, 11, 12, 13, 14, 15}; recolour_runs(m); }
void fn_S8_1004(void) { static const uint8_t m[16] = {15, 1, 15, 15, 15, 15, 15, 15, 8, 9, 10, 11, 12, 13, 14, 15}; recolour_runs(m); }
void fn_S8_1078(void) { static const uint8_t m[16] = {15, 1, 14, 14, 14, 14, 14, 14, 8, 9, 10, 11, 12, 13, 15, 14}; recolour_runs(m); }

static int16_t clamp8(int16_t v) { return v > 8 ? 8 : v < -8 ? -8 : v; }

/* the player's movement over the last frame, halved from 3 ticks and quartered from 6 */
static uint32_t per_tick(uint32_t delta, uint16_t ticks) {
    int32_t d = (int32_t)delta;
    if (ticks >= 3) { d >>= 1; if (ticks >= 6) d >>= 1; }
    return (uint32_t)d;
}

/* fn_S8_0A24: the game loop, until G_2220 is set. Each pass handles the pending events (unless the
 * other window, G_21FC, is in use), then, unless suspended (G_22C6), plays one frame:
 *  - waits for at least 2 ticks of input (G_217C, counted by the VBL task with the movement in
 *    G_217E / G_2180 / G_2182), takes them (at most 8) into G_2174 / G_2176 / G_2178 / G_217A;
 *  - per tick: the player's tick (fn_S8_0E4C, 0E52, 0E68) on every other one;
 *  - the movement (fn_S8_0E80), the step taken (G_20C8 / G_20C4), a pending shot (G_2172);
 *  - per tick: the objects (fn_S2_0376) on every other one;
 *  - drawing: panels, shading, walls, floor runs, sprites, effects, scanner, the frame, sounds
 *    and the HUD; every 4th frame the bonus counter (fn_S2_4C86 with G_20DC / 4). */
void fn_S8_0A24(void) {
    MZ_FRAME;
    Regs r = {{0}, {0}, 0};
    uint32_t args = mz_local(0x40) + 0x30;                  /* stack arguments for fn_S2_3692 */
    sg16(0x217E, 0); sg16(0x2180, 0); sg16(0x2182, 0); sg16(0x217C, 0);
    sg8(0x2172, 0); sg8(0x2220, 0);
    while (!g8(0x2220)) {
        if (!g8(0x21FC)) {
            int got;
            do {
                got = mac_WaitNextEvent((int16_t)0xFFFF, GADDR(0x221C), 0, 0);
                if (got) fn_S8_10EC(GADDR(0x221C));
            } while (got && !g8(0x2220));
        }
        if (g8(0x21FC) || !g8(0x22C6)) {
            sg32(0x238C, g32(0x238C) + 1);
            if ((int16_t)g16(0x217C) >= 2) {
                uint16_t t = g16(0x217C);
                while (g16(0x217C) == t) mz_idle();
            } else {
                while ((int16_t)g16(0x217C) < 2) mz_idle();
            }
            sg16(0x2174, g16(0x217C)); sg16(0x217C, 0);
            sg16(0x217A, g16(0x2182)); sg16(0x2182, 0);
            sg16(0x2178, g16(0x2180)); sg16(0x2180, 0);
            sg16(0x2176, g16(0x217E)); sg16(0x217E, 0);
            if ((int16_t)g16(0x2174) > 8) {
                sg16(0x2174, 8);
                sg16(0x217A, (uint16_t)clamp8((int16_t)g16(0x217A)));
                sg16(0x2178, (uint16_t)clamp8((int16_t)g16(0x2178)));
                sg16(0x2176, (uint16_t)clamp8((int16_t)g16(0x2176)));
            }
            for (int16_t i = 0; i < (int16_t)g16(0x2174); i++) {
                sg16(0x20BA, (uint16_t)(g16(0x20BA) + 1));
                if (g16(0x20BA) & 1) continue;
                fn_S8_0E4C(&r);
                if (!g8(0x21FC)) {
                    sg16(0x20BE, (uint16_t)(g16(0x20BE) + 1));
                    if (!g16(0x20D8)) fn_S8_0E52(&r);
                }
                if (!g8(0x21FC)) fn_S8_0E68(&r);
            }
            sg32(0x4E2C, g32(0x20D2)); sg32(0x4E28, g32(0x20CE));
            if (!g16(0x20D8)) fn_S8_0E80();
            sg32(0x20C8, per_tick(g32(0x20D2) - g32(0x4E2C), g16(0x2174)));
            sg32(0x20C4, per_tick(g32(0x20CE) - g32(0x4E28), g16(0x2174)));
            if (g8(0x2172) && (int16_t)g16(0x20F8) > 0) {
                sg16(0x20F8, (uint16_t)(g16(0x20F8) - 1));
                fn_S8_0F0C(&r);
                sg8(0x2172, 0);
            }
            for (int16_t i = 0; i < (int16_t)g16(0x2174); i++) {
                sg16(0x20B8, (uint16_t)(g16(0x20B8) + 1));
                if (g16(0x20B8) & 1) continue;
                sg16(0x20BC, (uint16_t)(g16(0x20BC) + 1));
                mz_seg(2), fn_S2_0376(&r);
            }
            mz_seg(2);
            fn_S2_4058(); fn_S2_41A6(); fn_S2_3A36();
            fn_S2_4F1C(g32(0x1DB0));
            w32(args + 4, g32(0x1DB0)); w32(args + 8, g32(0x1DEC));
            r.a[7] = args; fn_S2_3692(&r);
            fn_S2_5810(&r);
            if (g16(0x20D8)) fn_S8_0CFE(&r);
            if (g16(0x2102) == 0x2B) fn_S8_0F90();
            if (g16(0x2102) == 0x2C) fn_S8_1004();
            if (g16(0x2102) == 0x2D) fn_S8_1078();
            if (!g8(0x2393) || !(g32(0x238C) & 1))
                mz_seg(4), fn_S4_0000((int32_t)g32(0x20D2), (int32_t)g32(0x20CE), g32(0x1DC4), g32(0x1DCC), g32(0x22C0));
            fn_S2_040A();
            mz_seg(4), fn_S4_3F02();
            fn_S2_42A8(); fn_S2_44AA(); fn_S2_46BC(); fn_S2_48D2(); fn_S2_4A98(); fn_S2_4BF8();
            sg16(0x20DC, (uint16_t)(g16(0x20DC) + (uint16_t)(g16(0x2174) << 3) - 0x10));
            if ((g32(0x238C) & 3) == 3) {
                sg16(0x20DC, (uint16_t)((int16_t)g16(0x20DC) >> 2));
                fn_S2_4C86();
                sg16(0x20DC, 0);
            }
        }
        if (g8(0x21FC)) sg8(0x2220, 1);
    }
}

/* ---------- a play session ---------- */

/* fn_S8_1814 is the VBL task (input polling); its jump-table entry is at A5 + $72. */
#define VBL_TASK_ENTRY (MZ_A5 + 0x72)

/* fn_S8_172A: centres the mouse on the main screen G_22BC (G_2186 / G_2184 = the centre; the old
 * position kept in G_4E02; low-memory pointers G_4E0E, G_4E12, G_4E0A, G_4E06) and installs the
 * input VBL task G_2198 (every tick, fn_S8_1814, with the A5 world in G_218A). A failure is fatal
 * (alert G_4DFE). */
void fn_S8_172A(void) {
    sg32(0x4E02, r32(g32(0x4E0E)));
    uint32_t r = deref(g32(0x22BC)) + 0x22;                 /* gdRect */
    int16_t top = (int16_t)r16(r), left = (int16_t)r16(r + 2), bottom = (int16_t)r16(r + 4), right = (int16_t)r16(r + 6);
    sg16(0x2186, (uint16_t)(int16_t)((int16_t)(right + left) / 2));
    sg16(0x2184, (uint16_t)(int16_t)((int16_t)(top + bottom) / 2));
    w16(g32(0x4E12) + 2, g16(0x2186));
    w16(g32(0x4E12), g16(0x2184));
    w32(g32(0x4E0E), r32(g32(0x4E12)));
    w8(g32(0x4E0A), r8(g32(0x4E06)));
    sg16(0x2194, 1);                                        /* vType */
    sg32(0x2192, VBL_TASK_ENTRY);                           /* vblAddr */
    sg16(0x218E, 1);                                        /* vblCount */
    sg16(0x218C, 0);                                        /* vblPhase */
    sg32(0x218A, MZ_A5);
    int16_t err = mac_VInstall(GADDR(0x2198));
    if (err) mz_seg(2), fn_S2_03A0(GADDR(0x4DFE), err, 1);
}

/* fn_S8_17D0: removes the input VBL task (fatal alert G_4DD0 on failure) and puts the mouse back. */
void fn_S8_17D0(void) {
    int16_t err = mac_VRemove(GADDR(0x2198));
    if (err) mz_seg(2), fn_S2_03A0(GADDR(0x4DD0), err, 1);
    w32(g32(0x4E12), g32(0x4E02));
    w32(g32(0x4E0E), r32(g32(0x4E12)));
    w8(g32(0x4E0A), r8(g32(0x4E06)));
}

/* fn_S8_0206: the level's 'm2cv' resources: those no longer wanted (G_4E68[i] clear) are disposed
 * of, the wanted ones not yet loaded are loaded (id 128 + i, detached, into G_1EBC[i]), their total
 * size in G_22AA. Returns 1, or 0 if one is missing. */
uint8_t fn_S8_0206(void) {
    for (int16_t i = 0; i < 0x32; i++) {
        uint32_t slot = GADDR(0x1EBC) + 4u * (uint32_t)i;
        if (!r8(GADDR(0x4E68) + (uint32_t)i) && r32(slot)) { mac_DisposeHandle(r32(slot)); w32(slot, 0); }
    }
    sg32(0x22AA, 0);
    for (int16_t i = 0; i < 0x32; i++) {
        uint32_t slot = GADDR(0x1EBC) + 4u * (uint32_t)i;
        if (!r8(GADDR(0x4E68) + (uint32_t)i) || r32(slot)) continue;
        uint32_t h = mac_GetResource(0x6D326376, (int16_t)(0x80 + i));   /* 'm2cv' */
        int16_t err = mac_ResError();
        if (err || !h) return 0;
        mac_DetachResource(h);
        w32(slot, h);
        sg32(0x22AA, g32(0x22AA) + (uint32_t)(mz_seg(3), fn_S3_020E(h)));
    }
    return 1;
}

/* fn_S8_0136: marks the graphics every level needs (fn_S8_01CE, fn_S8_01F2 for each) and loads
 * them (fn_S8_0206). */
uint8_t fn_S8_0136(int16_t unused) {
    static const int16_t always[] = {3, 5, 7, 8, 9, 0xA, 0xB, 0xC, 0x14, 0x15, 0x16, 0x1B, 0x1C, 0x22, 0x24, 0x28};
    (void)unused;
    fn_S8_01CE();
    for (unsigned k = 0; k < sizeof always / sizeof *always; k++) fn_S8_01F2(always[k]);
    return fn_S8_0206();
}

/* fn_S8_03AE: the game palette for the screen G_22BC ('clut' 202 at 1 bit, 200 for a colour
 * device, 201 for grays), set on the windows G_2232 and (unless G_21FC) G_222E and the offscreen
 * worlds, replacing G_2224. Returns 0 if it cannot be made. */
uint8_t fn_S8_03AE(void) {
    uint32_t pal;
    mz_seg(2);
    if (r16(deref(r32(deref(g32(0x22BC)) + 0x16)) + 0x20) == 1) pal = fn_S2_01FA(0xCA);
    else if (mac_TestDeviceAttribute(g32(0x22BC), 0)) pal = fn_S2_01FA(0xC8);   /* gdDevType: colour */
    else pal = fn_S2_01FA(0xC9);
    if (!pal) return 0;
    mac_NSetPalette(g32(0x2232), pal, 0x100);
    if (!g8(0x21FC)) mac_NSetPalette(g32(0x222E), pal, 0x100);
    if (g32(0x22C4)) mac_NSetPalette(g32(0x22C4), pal, 0x100);
    if (g32(0x22C0)) mac_NSetPalette(g32(0x22C0), pal, 0x100);
    mac_DisposePalette(g32(0x2224));
    sg32(0x2224, pal);
    return 1;
}

/* fn_S8_0000: plays games until G_221F: the input VBL task runs throughout (not in the other window,
 * G_21FC). A new game (G_220C) seeds the random table from the clock (G_0082 / G_4E34), sets up the
 * level and the player. Each pass loads the graphics, palette and offscreen worlds, locks
 * everything, opens the sound channels and runs the game loop, then releases it all. Returns 1,
 * or 0 if something could not be set up. */
uint8_t fn_S8_0000(void) {
    MZ_FRAME;
    Regs r = {{0}, {0}, 0};
    uint8_t ok = 1;
    if (!g8(0x21FC)) fn_S8_172A();
    sg8(0x221F, 0);
    while (!g8(0x221F)) {
        if (g8(0x220C)) {
            sg32(0x4E34, r32(0x020C));                      /* Time */
            sg32(0x0082, g32(0x4E34));
            mz_seg(2), fn_S2_3502();
            fn_S8_083A(&r);
            mz_seg(4), fn_S4_1898();
            fn_S8_0946(&r);
            sg32(0x20D2, g32(0x2206)); sg32(0x20CE, g32(0x2202));
            sg16(0x20D4, g16(0x2208)); sg16(0x20EC, g16(0x220A));
            sg8(0x220C, 0); sg16(0x20D8, 0);
        }
        if (!fn_S8_0136(0) || !fn_S8_03AE()) { ok = 0; break; }
        fn_S8_4D68(GADDR(0x22C4), GADDR(0x22C0), g32(0x22BC));
        if (!g32(0x22C4)) { ok = 0; break; }
        if (g8(0x21FC)) {
            uint32_t rc = mz_local(8);
            mac_SetPort(g32(0x2232));
            w32(rc, r32(g32(0x2232) + 0x10)); w32(rc + 4, r32(g32(0x2232) + 0x14));
            mac_ClipRect(rc); mac_ValidRect(rc);
        }
        fn_S8_083A(&r);
        fn_S8_02F0();
        mz_seg(4), fn_S4_3B10(); fn_S4_3BF8();
        fn_S8_09FC(g32(0x1DE8));
        fn_S8_04C8(&r);
        fn_S8_0A24();
        fn_S8_0946(&r);
        fn_S8_0360();
        mz_seg(4), fn_S4_3CE8(); fn_S4_3B9A();
        if (g8(0x21FC)) sg8(0x221F, 1);
    }
    if (!g8(0x21FC)) fn_S8_17D0();
    return ok;
}

/* fn_S8_1814 (JT 10): the input VBL task (A0 = the task, its A5 world at +$E), every tick: counts
 * the tick (G_217C) and accumulates movement from the keyboard (GetKeys: bit k of long n = key code
 * 32n + k, little-endian within each long's bytes as QuickDraw stores KeyMap): turning G_217E,
 * forward / back G_2180, sideways G_2182, each from the arrow keys, keypad and letter keys; the fire
 * keys arm G_2171 when released, and pressing one then sets G_2172 (one shot per press). The task
 * re-arms itself (vblCount 1). D0-D2 are left holding the key map. */
void fn_S8_1814(Regs *r) {
    MZ_FRAME;
    uint32_t task = r->a[0], km = mz_local(16);
    mac_GetKeys(km);
    uint32_t k0 = r32(km), k1 = r32(km + 4), k2 = r32(km + 8);
#define KEY(k, n) (((k) >> (n)) & 1)
    sg16(0x217C, (uint16_t)(g16(0x217C) + 1));
    if (KEY(k0, 20) || KEY(k1, 24) || KEY(k2, 1))  sg16(0x217E, (uint16_t)(g16(0x217E) - 1));
    if (KEY(k0, 21) || KEY(k1, 26) || KEY(k2, 3))  sg16(0x2180, (uint16_t)(g16(0x2180) + 1));
    if (KEY(k0, 22) || KEY(k0, 7)  || KEY(k2, 4))  sg16(0x217E, (uint16_t)(g16(0x217E) + 1));
    if (KEY(k0, 24) || KEY(k1, 30) || KEY(k2, 14)) sg16(0x2182, (uint16_t)(g16(0x2182) - 1));
    if (KEY(k0, 26) || KEY(k1, 29) || KEY(k2, 0))  sg16(0x2182, (uint16_t)(g16(0x2182) + 1));
    if (KEY(k0, 31) || KEY(k1, 19) || KEY(k2, 12)) sg16(0x2180, (uint16_t)(g16(0x2180) - 1));
    if (KEY(k1, 28) || KEY(k2, 20) || KEY(k1, 9) || KEY(k1, 0)) {
        if (g8(0x2171)) { sg8(0x2172, 1); sg8(0x2171, 0); }
    } else sg8(0x2171, 1);
#undef KEY
    w16(task + 0xA, 1);
    r->d[0] = k0; r->d[1] = k1; r->d[2] = k2;
    r->ccr = (uint8_t)(r->ccr & CCR_X);                     /* move.w #1: N, Z, V, C clear */
}
