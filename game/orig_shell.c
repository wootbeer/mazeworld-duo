/* Mazeworld Original's shell (CODE segment 8, see build/disasm-original/seg8.s): its own menu
 * commands, game files and screen layouts, named by Original's offsets. Its menus: Apple (About,
 * Read Instructions, Print Order Form, Register), File (Play Game, New Game, Load Game, Save Game
 * As, Quit), Edit, Window Size (Tiny .. 13" Monitor) and Options (Fast Graphics / Mac Friendly).
 * The game in progress is the 'mazr' resource (G_54E4), saved and loaded whole as an 'Mzws' file;
 * G_57CE holds the file's FSSpec (G_57CF: there is one), the window G_5804 shows its name. */
#include "mzasm.h"
#include "fns.h"

static void alert(uint16_t msg, int16_t err, int8_t fatal) { mz_seg_of(2, 0x03A0), fn_S2_03A0(OADDR(msg), err, fatal); }
static void inval_window(void) { mac_SetPort(og32(0x5804)); mac_InvalRect(og32(0x5804) + 0x10); }
/* "a game is going on: really?" (alert 132 with the texts ^0..^3; item 2 is Cancel) */
static int confirmed(uint16_t t0, uint16_t t1, uint16_t t2, uint16_t t3) {
    if (!og8(0x5784) || og8(0x5787)) return 1;
    mac_ParamText(OADDR(t0), OADDR(t1), OADDR(t2), OADDR(t3));
    return mac_CautionAlert(0x84, 0) != 2;
}

/* fo_S8_2AE2: the new game at start-up: 'mazr' 199 (alerts G_6E2E / G_6E0C / G_6DE8 on failure),
 * detached; no file. */
void fo_S8_2AE2(void) {
    sog32(0x54E4, mac_GetResource(0x6D617A72, 0xC7));
    int16_t err = mac_ResError();
    if (err) alert(0x6E2E, err, 1);
    if (!og32(0x54E4)) alert(0x6E0C, 0, 1);
    mac_DetachResource(og32(0x54E4));
    err = mac_ResError();
    if (err) alert(0x6DE8, err, 1);
    sog8(0x5788, 1); sog8(0x5787, 0); sog8(0x5784, 0); sog8(0x57CF, 0);
}
/* fo_S8_2BA8: a new game: the old one disposed of, 'mazr' 199 again (alerts G_6D2C / G_6D0A), the
 * window titled G_6CE6 and redrawn; no file. */
void fo_S8_2BA8(void) {
    mac_DisposeHandle(og32(0x54E4));
    sog32(0x54E4, mac_GetResource(0x6D617A72, 0xC7));
    int16_t err = mac_ResError();
    if (err) alert(0x6D2C, err, 1);
    if (!og32(0x54E4)) alert(0x6D0A, 0, 1);
    mac_DetachResource(og32(0x54E4));
    inval_window();
    mac_SetWTitle(og32(0x5804), OADDR(0x6CE6));
    sog8(0x5788, 1); sog8(0x5787, 0); sog8(0x5784, 0); sog8(0x57CF, 0);
}
/* fo_S8_2B68: File > New Game (asking first while a game is going on) */
void fo_S8_2B68(void) { if (confirmed(0x6DC6, 0x6D32, 0x6D30, 0x6D2E)) fo_S8_2BA8(); }
/* fo_S8_31FA: File > Quit (asking first while a game is going on): ends the shell loop (G_580E) */
void fo_S8_31FA(void) { if (confirmed(0x6B34, 0x6AA2, 0x6AA0, 0x6A9E)) sog8(0x580E, 1); }

/* fo_S8_305A: "could not load" (alert 129): ^0 the file's name, ^1 the error's text, ^2 its number */
void fo_S8_305A(uint32_t name, int16_t err) {
    MZ_FRAME;
    uint32_t num = mz_local(0x100);
    mz_seg_of(3, 0x038E), fn_S3_038E((int32_t)err, num);
    uint32_t text = fn_S8_3822(err);
    mac_ParamText(OADDR(0x6CBE), num, text, name);
    mac_StopAlert(0x81, 0);
}
/* fo_S8_2F34: loads the game file `spec` (fn_S8_36F4): it becomes the game G_54E4 and the file
 * G_57CE (the window takes its name). Returns the error. */
int16_t fo_S8_2F34(uint32_t spec) {
    MZ_FRAME;
    uint32_t h = mz_local(4);
    int16_t err = fn_S8_36F4(spec, h);
    if (err) return err;
    mac_DisposeHandle(og32(0x54E4));
    sog32(0x54E4, r32(h));
    for (uint32_t k = 0; k < 0x46; k += 2) w16(OADDR(0x57CE) + k, r16(spec + k));
    sog8(0x57CF, 1);
    mac_SetWTitle(og32(0x5804), spec + 6);
    inval_window();
    sog8(0x5788, 0); sog8(0x5787, 0); sog8(0x5784, 0);
    return 0;
}
/* fo_S8_2EC6: File > Load Game: an 'Mzws' file (Standard File), loaded (fo_S8_2F34) */
void fo_S8_2EC6(void) {
    MZ_FRAME;
    uint32_t reply = mz_local(0x58), types = mz_local(4), where = mz_local(4);
    w32(types, 0x4D7A7773); w32(where, 0xFFFFFFFFu);
    mac_CustomGetFile(1, types, reply, r32(where), mz_jt(0x8A));
    mac_InitCursor();
    if (!r8(reply)) return;
    int16_t err = fo_S8_2F34(reply + 6);
    if (err) fo_S8_305A(reply + 0xC, err);
}
/* fo_S8_2C2E: File > Save Game As: the game G_54E4 saved as an 'Mzws' file (fn_S8_3522; suggesting
 * the current file's name, or G_6CC8), which becomes the current file; alert fn_S8_37E6 on error */
void fo_S8_2C2E(void) {
    MZ_FRAME;
    uint32_t reply = mz_local(0x58);
    if (og8(0x57CF)) mac_CustomPutFile(OADDR(0x6CDC), OADDR(0x57C8), reply, 0xFFFFFFFFu, mz_jt(0x8A));
    else mac_CustomPutFile(OADDR(0x6CD2), OADDR(0x6CC8), reply, 0xFFFFFFFFu, mz_jt(0x8A));
    mac_InitCursor();
    if (!r8(reply)) return;
    int16_t err = fn_S8_3522(reply + 6, og32(0x54E4), 0x4D7A776C, 0x4D7A7773);   /* 'Mzwl', 'Mzws' */
    if (err) { fn_S8_37E6(reply + 0xC, err); return; }
    mac_SetWTitle(og32(0x5804), reply + 0xC);
    for (uint32_t k = 0; k < 0x46; k += 2) w16(OADDR(0x57CE) + k, r16(reply + 6 + k));
    sog8(0x57CF, 1);
}
/* fo_S8_2A2A: File > item: 1 Play Game (G_71AC), 3 New Game, 4 Load Game, 6 Save Game As, 8 Quit */
void fo_S8_2A2A(int16_t item) {
    switch (item) {
    case 1: sog8(0x71AC, 1); break;
    case 3: fo_S8_2B68(); break;
    case 4: fo_S8_2EC6(); break;
    case 6: fo_S8_2C2E(); break;
    case 8: fo_S8_31FA(); break;
    }
}
/* fo_S8_2A78: Window Size > item 1-4 (fn_S8_4ACE; quits if nothing works); play resumes after a game
 * interrupted by the menu (G_57F0) */
void fo_S8_2A78(int16_t item) {
    if (item <= 0 || item >= 5) return;
    if (!fn_S8_4ACE(item)) mac_ExitToShell();
    if (og8(0x57F0)) sog8(0x71AC, 1);
}
/* fo_S8_2AA8: Options > 1 Fast Graphics Mode / 2 Mac Friendly Mode (G_5894); play resumes as above */
void fo_S8_2AA8(int16_t item) {
    if (item != 1 && item != 2) return;
    sog8(0x5894, item == 2);
    if (og8(0x57F0)) sog8(0x71AC, 1);
}
/* fo_S8_2972: Options menu m: the check mark on the mode G_5894 */
void fo_S8_2972(uint32_t m) {
    mac_CheckItem(m, 1, !og8(0x5894));
    mac_CheckItem(m, 2, og8(0x5894) != 0);
}

/* fo_S8_20EA: default preferences in h: view size 0, Fast Graphics */
void fo_S8_20EA(uint32_t h) {
    mac_HLock(h);
    uint32_t p = deref(h);
    w16(p, 0); w8(p + 0xA, 0);
    mac_HUnlock(h);
}

/* ---------- the screen layouts ---------- */

/* fo_S8_44A8: (re)makes the offscreen world *pgw (G_5738 x G_5732) for device gdh (8-bit if the
 * screen's depth is not 4, 8, 16 or 32), with the palette G_57F6. Returns the error. */
int16_t fo_S8_44A8(uint32_t pgw, uint32_t gdh) {
    MZ_FRAME;
    uint32_t bounds = mz_local(8);
    w16(bounds, 0); w16(bounds + 2, 0); w16(bounds + 4, og16(0x5732)); w16(bounds + 6, og16(0x5738));
    uint32_t pm = deref(r32(deref(gdh) + 0x16));
    uint32_t ctab = r32(pm + 0x2A);
    int16_t depth = (int16_t)r16(pm + 0x20);
    if (depth != 8 && depth != 4 && depth != 16 && depth != 32) { depth = 8; ctab = 0; }
    int16_t err;
    if (!r32(pgw)) err = mac_NewGWorld(pgw, depth, bounds, ctab, 0, 0);
    else {
        uint32_t flags = mac_UpdateGWorld(pgw, depth, bounds, ctab, 0, 0);
        err = (flags & 0x80000000u) ? (int16_t)(flags & 0xFFFF) : 0;
        if (err) {
            if (r32(pgw)) mac_DisposeGWorld(r32(pgw));
            w32(pgw, 0);
        }
    }
    if (r32(pgw)) mac_NSetPalette(r32(pgw), og32(0x57F6), 0x100);
    if (!r32(pgw) && !err) err = -108;                     /* memFullErr */
    return err;
}
/* fo_S8_4D26: rebuilds the drawing tables (as Catacombs' fn_S8_5884 starts) */
void fo_S8_4D26(void) { fn_S8_5962(); fn_S8_5C80(); fn_S8_5E7A(); fn_S8_6036(); }

/* fo_S8_472E: switches to window size `size` (1-4; anything else is a fatal alert, G_6026): sets the
 * layout globals, the run-list sizes G_572A / G_5726, reallocates the buffers (fn_S8_577A),
 * rebuilds the tables (fo_S8_4D26) and the offscreen world G_589C. G_5892 = the size, or -1 on
 * failure (returns 0). */
static const uint16_t layout_g[17] = {0x5748, 0x5746, 0x5740, 0x573C, 0x573E, 0x573A, 0x5738, 0x5736,
                                      0x5734, 0x5732, 0x5730, 0x5722, 0x5720, 0x571E, 0x571C, 0x571A, 0x5710};
static const struct { uint16_t v[17]; uint32_t scale; uint8_t frame, b1, x, y, b2; } layout[4] = {
    {{0xF0, 0xC8, 0x10, 4, 0xE0, 0x92, 0xD0, 0x68, 0xFA, 0x8E, 0x47, 3, 2, 8, 0x92, 9, 9}, 0x3F800000, 5, 7, 7, 8, 0x21},
    {{0x160, 0x10E, 0x10, 6, 0x150, 0xE0, 0x140, 0xA0, 0x12C, 0xDA, 0x6D, 3, 4, 8, 0xDE, 9, 9}, 0x3FC51EB8, 5, 7, 7, 8, 0x1A},
    {{0x200, 0x180, 0x18, 8, 0x1E8, 0x144, 0x1D0, 0xE8, 0x12C, 0x13C, 0x9E, 5, 6, 0xC, 0x140, 0xC, 0xA}, 0x400EB852, 9, 0xC, 9, 0xA, 0x23},
    {{0x280, 0x1E0, 0x20, 0xA, 0x260, 0x194, 0x240, 0x120, 0x190, 0x18A, 0xC5, 7, 8, 0x10, 0x18E, 0x10, 0xC}, 0x403147AE, 0xA, 0xF, 0xA, 0xB, 0x28},
};
static const uint16_t text_size[4] = {9, 0xC, 0xE, 0x12};
uint8_t fo_S8_472E(int16_t size) {
    if (size >= 1 && size <= 4) {
        const int s = size - 1;
        for (int k = 0; k < 17; k++) sog16(layout_g[k], layout[s].v[k]);
        sog32(0x572E, layout[s].scale);
        sog16(0x5716, (uint16_t)(og16(0x5740) - 2)); sog16(0x5712, (uint16_t)(og16(0x573E) + 2));
        sog16(0x5718, (uint16_t)(og16(0x573A) + 3)); sog16(0x5714, (uint16_t)(og16(0x5746) - 1));
        sog16(0x570E, (uint16_t)(og16(0x5722) + (s < 2 ? 1 : s == 2 ? 4 : 6)));
        sog16(0x570C, (uint16_t)(og16(0x5748) - og16(0x5722) - layout[s].frame));
        uint16_t b1 = (uint16_t)(og16(0x5718) + layout[s].b1), b2 = (uint16_t)(og16(0x5718) + layout[s].b2), x = layout[s].x, y = layout[s].y;
        sog16(0x570A, b1); sog16(0x5708, (uint16_t)(b1 + x)); sog16(0x5706, (uint16_t)(b1 + x + y));
        sog16(0x5704, b1); sog16(0x5702, (uint16_t)(b1 + x)); sog16(0x5700, (uint16_t)(b1 + x + y));
        sog16(0x56FE, b2); sog16(0x56FC, (uint16_t)(b2 + y)); sog16(0x56FA, (uint16_t)(b2 + y + x));
        sog16(0x56F8, b2); sog16(0x56F6, (uint16_t)(b2 + x)); sog16(0x56F4, (uint16_t)(b2 + 2 * x));
        sog16(0x56F2, text_size[s]);
    } else alert(0x6026, 0, 1);
    sog32(0x572A, (uint32_t)(0xCC * (int32_t)(int16_t)og16(0x5732)));
    sog32(0x5726, (uint32_t)((int32_t)og32(0x572A) >> 1));
    if (!fn_S8_577A()) { sog16(0x5892, 0xFFFF); return 0; }
    fo_S8_4D26();
    if (fo_S8_44A8(OADDR(0x589C), og32(0x5898))) { sog16(0x5892, 0xFFFF); return 0; }
    sog16(0x5892, (uint16_t)size);
    return 1;
}
