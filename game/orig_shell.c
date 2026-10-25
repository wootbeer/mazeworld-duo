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

/* ---------- the preferences ---------- */

/* fo_S8_2008: the preferences ($114 bytes: the window size and screen, the mode) from the file
 * named by 'STR#' 128 #1 in the Preferences folder, if `load`; else, or if missing or the wrong
 * size, the defaults (fo_S8_20EA). Sets G_5894 from it. Returns the handle (0 if out of memory). */
uint32_t fo_S8_2008(uint8_t load) {
    MZ_FRAME;
    uint32_t h = mz_local(4), name = mz_local(0x100), vref = mz_local(2), dir = mz_local(4), spec = mz_local(0x46);
    w32(h, 0);
    int ok = 0;
    if (load) {
        mz_seg_of(3, 0x03BA), fn_S3_03BA(name, 0x80, 1);
        int16_t err = fn_S3_0400((int16_t)0x8000, 0x70726566, 0, vref, dir);   /* kOnSystemDisk, 'pref' */
        if (!err) err = mac_FSMakeFSSpec((int16_t)r16(vref), (int32_t)r32(dir), name, spec);
        if (!err) err = fn_S8_36F4(spec, h);
        ok = !err && r32(h) && (mz_seg_of(3, 0x020E), fn_S3_020E(r32(h))) == 0x114;
    }
    if (!ok) {
        if (r32(h)) mac_DisposeHandle(r32(h));
        w32(h, mac_NewHandle(0x114));
        if (r32(h)) fo_S8_20EA(r32(h));
    }
    if (r32(h)) {
        mac_HLock(r32(h));
        sog8(0x5894, r8(deref(r32(h)) + 0xA));
        mac_HUnlock(r32(h));
    }
    return r32(h);
}
/* fo_S8_210C: saves the preferences in h (the mode, the window size and screen) to the file named
 * by 'STR#' 128 #1 in the Preferences folder. Returns the error. */
int16_t fo_S8_210C(uint32_t h) {
    MZ_FRAME;
    mac_HLock(h);
    uint32_t p = deref(h);
    w8(p + 0xA, og8(0x5894));
    w16(p, og16(0x5892));
    uint32_t g = deref(og32(0x5898));
    w32(p + 2, r32(g + 0x22)); w32(p + 6, r32(g + 0x26));
    mac_HUnlock(h);
    uint32_t name = mz_local(0x100), vref = mz_local(2), dir = mz_local(4), spec = mz_local(0x46);
    mz_seg_of(3, 0x03BA), fn_S3_03BA(name, 0x80, 1);
    int16_t err = fn_S3_0400((int16_t)0x8000, 0x70726566, 0, vref, dir);
    if (!err) {
        err = mac_FSMakeFSSpec((int16_t)r16(vref), (int32_t)r32(dir), name, spec);
        if (err == -43) err = 0;                            /* fnfErr: a new file */
    }
    if (!err) err = fn_S8_3522(spec, h, 0x4D7A776C, 0x50524546);          /* 'Mzwl', 'PREF' */
    return err;
}

/* ---------- registration ---------- */

/* fo_S8_6760: is the 8-character registration code (its first four characters a seed, its last four
 * a check) right for the name (a Pascal string)? 1 / 0 */
uint8_t fo_S8_6760(uint32_t code, uint32_t name) {
    uint8_t buf[0x100], acc[4];
    int16_t n = r8(name), i;
    for (i = 0; i < n; i++) buf[i] = r8(name + 1 + (uint32_t)i);
    buf[i] = 0x0A; n++;
    for (i = 0; i < 4; i++) acc[i] = r8(code + (uint32_t)i);
    for (i = 0; i < n; i++) {
        int lower = (int8_t)buf[i] >= 'a' && (int8_t)buf[i] <= 'z';
        acc[i % 4] = (uint8_t)(acc[i % 4] + buf[i] + i % 4 + (lower ? 0xE0 : 0));
    }
    for (i = 0; i < 4; i++) { uint8_t v = (uint8_t)(acc[i] % 36); acc[i] = (uint8_t)(v > 9 ? v - 10 + 'A' : v + '0'); }
    for (i = 0; i < 4; i++) if (acc[i] != r8(code + 4 + (uint32_t)i)) return 0;
    return 1;
}
/* fo_S8_6668: the registration (record G_5814: code at +$C, name at +$14): 1 and, where asked, the
 * code (as a Pascal string) and the name; or 0 and the texts G_0018 / G_000E */
uint8_t fo_S8_6668(uint32_t pcode, uint32_t pname) {
    uint32_t h = og32(0x5814);
    mac_HLock(h);
    uint32_t p = deref(h);
    uint8_t ok = fo_S8_6760(p + 0xC, p + 0x14);
    if (!ok) {
        if (pcode) mz_seg_of(2, 0x0332), fn_S2_0332(OADDR(0x18), pcode);
        if (pname) mz_seg_of(2, 0x0332), fn_S2_0332(OADDR(0x0E), pname);
    } else {
        if (pcode) { w8(pcode, 8); for (uint32_t k = 0; k < 8; k++) w8(pcode + 1 + k, r8(deref(h) + 0xC + k)); }
        if (pname) for (uint32_t k = 0; k <= r8(deref(h) + 0x14); k++) w8(pname + k, r8(deref(h) + 0x14 + k));
    }
    mac_HUnlock(h);
    return ok;
}
/* fo_S8_68FA: Apple > Register: dialog 302 (items 3 the name, 4 the code) until Cancel (2) or an OK
 * (1) with a right code (8 characters, upper-cased), which is kept in G_5814; a wrong one is alert 133 */
void fo_S8_68FA(void) {
    MZ_FRAME;
    uint32_t port = mz_local(4), kind = mz_local(2), box = mz_local(8), hit = mz_local(2);
    uint32_t hname = mz_local(4), hcode = mz_local(4), hx = mz_local(4);
    uint32_t name = mz_local(0x100), code = mz_local(0x100);
    mac_GetPort(port);
    uint32_t d = mac_GetNewDialog(0x12E, 0, 0xFFFFFFFFu);
    if (!d) return;
    mac_SetPort(d);
    mac_ShowWindow(d);
    mac_GetDialogItem(d, 1, kind, hx, box);
    mac_GetDialogItem(d, 2, kind, hx, box);
    mac_GetDialogItem(d, 3, kind, hname, box);
    mac_GetDialogItem(d, 4, kind, hcode, box);
    for (int done = 0; !done; ) {
        mac_ModalDialog(0, hit);
        int16_t item = (int16_t)r16(hit);
        if (item == 2) { done = 1; continue; }
        if (item != 1) continue;
        mac_GetDialogItemText(r32(hname), name);
        mac_GetDialogItemText(r32(hcode), code);
        for (uint32_t k = 1; k <= r8(code); k++) { uint8_t c = r8(code + k); if (c >= 'a' && c <= 'z') w8(code + k, (uint8_t)(c - 0x20)); }
        if (r8(code) != 8 || !fo_S8_6760(code + 1, name)) { mac_Alert(0x85, 0); continue; }
        uint32_t p = deref(og32(0x5814));
        for (uint32_t k = 0; k < 8; k++) w8(p + 0xC + k, r8(code + 1 + k));
        for (uint32_t k = 0; k < (uint32_t)r8(name) + 1; k++) w8(deref(og32(0x5814)) + 0x14 + k, r8(name + k));
        done = 1;
    }
    mac_SetPort(r32(port));
    mac_DisposeDialog(d);
}

/* ---------- the main window's screens ---------- */

/* fo_S8_3C0E: centred text (a C string) in the main window G_5804, on lines `line` to line + 2 of a
 * 32-line grid over its height G_5746 */
void fo_S8_3C0E(int16_t line, uint32_t text) {
    MZ_FRAME;
    uint32_t r = mz_local(8);
    mac_TextFont((int16_t)og16(0x5816));
    w16(r, (uint16_t)(int16_t)((int32_t)(int16_t)og16(0x5746) * line / 32));
    w16(r + 4, (uint16_t)(int16_t)((int32_t)(int16_t)(line + 2) * (int16_t)og16(0x5746) / 32));
    w16(r + 2, 0); w16(r + 6, og16(0x5748));
    mac_TETextBox(text, (int32_t)(mz_seg_of(2, 0x0252), fn_S2_0252(text)), r, 1);
}
/* the window black, white text of size G_56F2 */
static void screen_begin(void) {
    MZ_FRAME;
    uint32_t r = mz_local(8), w = og32(0x5804);
    mac_SetPort(w);
    w32(r, r32(w + 0x10)); w32(r + 4, r32(w + 0x14));
    mac_ClipRect(r);
    mac_PmForeColor(0); mac_PaintRect(r);
    mac_PmForeColor(0xF); mac_PmBackColor(0);
    mac_TextSize((int16_t)og16(0x56F2));
}
static void lines(const uint16_t *l, int n) { for (int i = 0; i < n; i += 2) fo_S8_3C0E((int16_t)l[i], OADDR(l[i + 1])); }
/* fo_S8_39AA: the title screen: with a registration, its texts and the owner's name; else the
 * shareware texts */
void fo_S8_39AA(void) {
    MZ_FRAME;
    static const uint16_t reg[] = {4, 0x661A, 7, 0x65F8, 0x11, 0x65E8, 0x18, 0x65BE};
    static const uint16_t unreg[] = {3, 0x6590, 8, 0x656E, 0xA, 0x6540, 0xC, 0x6510, 0xE, 0x64E0, 0x10, 0x64B2, 0x12, 0x6486, 0x18, 0x645C, 0x1C, 0x6440};
    uint32_t name = mz_local(0x100);
    screen_begin();
    if (fo_S8_6668(0, name)) {
        lines(reg, 8);
        w8(name + 1 + r8(name), 0);
        fo_S8_3C0E(0x1A, name + 1);
    } else lines(unreg, 18);
}
/* fo_S8_3AB8: the level's end (G_5786: 0 one text pair, 1 another) */
void fo_S8_3AB8(void) {
    static const uint16_t l0[] = {0xC, 0x641C, 0x16, 0x6402}, l1[] = {0xA, 0x63E0, 0xC, 0x63AE};
    screen_begin();
    if (og16(0x5786) == 0) lines(l0, 4);
    else if (og16(0x5786) == 1) lines(l1, 4);
}
/* fo_S8_3B3E / fo_S8_3BB8: the paused game's screen; the screen when the game cannot be shown */
void fo_S8_3B3E(void) {
    static const uint16_t l[] = {0xA, 0x638C, 0xC, 0x635E, 0xE, 0x632E, 0x10, 0x6306, 0x12, 0x62DE};
    screen_begin(); lines(l, 10);
}
void fo_S8_3BB8(void) {
    static const uint16_t l[] = {6, 0x62B4, 8, 0x628C};
    screen_begin(); lines(l, 4);
}

/* fo_S8_05CC: the palette of colour set n ('clut' 200 + n on a colour screen, else 210 + n) for the
 * main and game windows and the offscreen world; it replaces G_57F6. 0 if it cannot be made. */
uint8_t fo_S8_05CC(int16_t n) {
    int colour = mac_TestDeviceAttribute(og32(0x5898), 0);
    uint32_t pal = (mz_seg_of(2, 0x01FA), fn_S2_01FA)((int16_t)(n + (colour ? 0xC8 : 0xD2)));
    if (!pal) return 0;
    mac_NSetPalette(og32(0x5804), pal, 0x100);
    if (!og8(0x57D0)) mac_NSetPalette(og32(0x5800), pal, 0x100);
    if (og32(0x589C)) mac_NSetPalette(og32(0x589C), pal, 0x100);
    mac_DisposePalette(og32(0x57F6));
    sog32(0x57F6, pal);
    return 1;
}

/* ---------- the instructions ---------- */

/* fo_S8_57DE (JT 10): the instructions dialog's (G_080A) user item 4, the page G_080C in white on
 * black: 1 picture 129 and the title texts (with the owner's name when registered), 2-19 headings,
 * underlined / bold lines and text boxes, as listed below. */
enum { END, SZ, BX, IN, MV, FC, DS, TP, BT, TXL, TXC };
#define TX(just) ((just) ? TXC : TXL)
static const uint16_t page_ops[] = {
    /*  2 */ SZ, 0xC, BX, IN, 0xA, TX(0), 0x3666, END,
    /*  3 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x77A, FC, 0x0, TP, 0x2A, BT, 0x8A, TX(0), 0x33EE, MV, 0xA6, FC, 0x1, DS, 0x714, FC, 0x0, TP, 0xBA, BT, 0x11A, TX(0), 0x323A, END,
    /*  4 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x706, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x6A0, FC, 0x0, TP, 0x4A, BT, 0xEA, TX(0), 0x3104, END,
    /*  5 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x68C, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x626, FC, 0x0, TP, 0x3A, BT, 0x9A, TX(0), 0x2E34, MV, 0xB6, FC, 0x1, DS, 0x612, FC, 0x0, TP, 0xBA, BT, 0x11A, TX(0), 0x2CAE, END,
    /*  6 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x604, FC, 0x0, MV, 0x26, FC, 0x1, DS, 0x59E, FC, 0x0, TP, 0x2A, BT, 0x11A, TX(0), 0x2B28, END,
    /*  7 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x590, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x52A, FC, 0x0, TP, 0x3A, BT, 0xAA, TX(0), 0x26DA, TP, 0xAA, BT, 0x12A, TX(0), 0x253A, END,
    /*  8 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x51A, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x4B4, FC, 0x0, TP, 0x4A, BT, 0xBA, TX(0), 0x22F0, END,
    /*  9 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x4A8, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x442, FC, 0x0, TP, 0x3A, BT, 0x11A, TX(0), 0x2118, END,
    /* 10 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x428, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x3C2, FC, 0x0, TP, 0x3A, BT, 0x6A, TX(0), 0x1D44, MV, 0x86, FC, 0x1, DS, 0x3AA, FC, 0x0, TP, 0x8A, BT, 0x12A, TX(0), 0x1CA2, END,
    /* 11 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x39C, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x336, FC, 0x0, TP, 0x4A, BT, 0xFA, TX(0), 0x1A58, END,
    /* 12 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x316, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x2B0, FC, 0x0, TP, 0x4A, BT, 0xCA, TX(0), 0x17A0, END,
    /* 13 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x29E, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x238, FC, 0x0, TP, 0x4A, BT, 0x11A, TX(0), 0x15C8, END,
    /* 14 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x218, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x1B2, FC, 0x0, TP, 0x4A, BT, 0x12A, TX(0), 0x1380, END,
    /* 15 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0x19A, FC, 0x0, MV, 0x36, FC, 0x1, DS, 0x134, FC, 0x0, TP, 0x4A, BT, 0x12A, TX(0), 0x1074, END,
    /* 16 */ SZ, 0xA, BX, IN, 0xA, MV, 0x16, FC, 0x5, DS, 0x112, FC, 0x0, TP, 0x25, BT, 0x10A, TX(0), 0x5F4A, SZ, 0xA, MV, 0x11A, FC, 0x1, DS, 0x102, END,
    /* 17 */ SZ, 0xA, BX, IN, 0xA, MV, 0x16, FC, 0x5, DS, 0xE2, FC, 0x0, TP, 0x25, BT, 0x12A, TX(0), 0x5C0A, END,
    /* 18 */ SZ, 0xA, BX, IN, 0xA, MV, 0x16, FC, 0x4, DS, 0xB2, FC, 0x0, TP, 0x2A, BT, 0x12A, TX(0), 0x0D3A, END,
    /* 19 */ SZ, 0xC, BX, IN, 0xA, MV, 0x16, FC, 0x1, DS, 0x96, FC, 0x0, TP, 0x1A, BT, 0x11A, TX(0), 0x08B6, END,
};
static const uint16_t page_start[] = {0, 8, 42, 70, 112, 140, 174, 202, 230, 272, 300, 328, 356, 384, 412, 440, 460, 480};
static void text_box(uint32_t r, uint16_t g, int16_t just) {
    mac_TETextBox(OADDR(g), (int32_t)(mz_seg_of(2, 0x0252), fn_S2_0252(OADDR(g))), r, just);
}
void fo_S8_57DE(uint32_t dlg, int16_t item) {
    (void)dlg; (void)item;                                  /* it always draws item 4 of G_080A */
    MZ_FRAME;
    uint32_t kind = mz_local(2), h = mz_local(4), box = mz_local(8), port = mz_local(4);
    uint32_t pen = mz_local(18), r = mz_local(8), d = og32(0x080A);
    mac_GetDialogItem(d, 4, kind, h, box);
    mac_GetPort(port);
    mac_SetPort(d);
    mac_GetPenState(pen);
    mac_PenNormal();
    mac_ForeColor(33);                                      /* blackColor */
    mac_PaintRect(box);
    mac_ForeColor(30);                                      /* whiteColor */
    mac_BackColor(33);
    mac_TextFont((int16_t)og16(0x5816));
    int16_t page = (int16_t)og16(0x080C);
    if (page == 1) {
        uint32_t pic = mac_GetPicture(0x81);
        if (pic) {
            uint32_t pr = mz_local(8), dr = mz_local(8);
            w32(dr, r32(d + 0x10)); w32(dr + 4, r32(d + 0x14));
            w16(dr + 4, 0x64);
            w32(pr, r32(deref(pic) + 2)); w32(pr + 4, r32(deref(pic) + 6));
            mac_OffsetRect(pr, (int16_t)(r16(dr + 2) - r16(pr + 2)), (int16_t)(r16(dr) - r16(pr)));
            mac_OffsetRect(pr, (int16_t)((int16_t)(r16(dr + 6) - r16(pr + 6)) / 2),
                               (int16_t)((int16_t)(r16(dr + 4) - r16(pr + 4)) / 2));
            mac_DrawPicture(pic, pr);
        }
        mac_TextSize(9);
        w32(r, r32(box)); w32(r + 4, r32(box + 4));
        w16(r, 0x64); w16(r + 4, 0x78); text_box(r, 0x3740, 1);
        w16(r, 0xAF); w16(r + 4, 0xEB); text_box(r, 0x372C, 1);
        uint32_t name = mz_local(0x100);
        if (fo_S8_6668(0, name)) {
            w16(r, 0x104); w16(r + 4, 0x110); text_box(r, 0x3692, 1);
            w16(r, 0x112); w16(r + 4, 0x11E);
            mac_TETextBox(name + 1, r8(name), r, 1);
        } else { w16(r, 0x104); w16(r + 4, 0x110); text_box(r, 0x3682, 1); }
    } else if (page >= 2 && page <= 0x13) {
        for (const uint16_t *op = page_ops + page_start[page - 2]; *op != END; ) {
            switch (*op++) {
            case SZ: mac_TextSize((int16_t)*op++); break;
            case BX: w32(r, r32(box)); w32(r + 4, r32(box + 4)); break;
            case IN: mac_InsetRect(r, (int16_t)*op, (int16_t)*op); op++; break;
            case MV: mac_MoveTo((int16_t)r16(r + 2), (int16_t)*op++); break;
            case FC: mac_TextFace((int16_t)*op++); break;
            case DS: mac_DrawString(OADDR(*op++)); break;
            case TP: w16(r, *op++); break;
            case BT: w16(r + 4, *op++); break;
            case TXL: text_box(r, *op++, 0); break;
            case TXC: text_box(r, *op++, 1); break;
            }
        }
    }
    mac_SetPenState(pen);
    mac_SetPort(r32(port));
}

/* ---------- play ---------- */

/* a routine taking the level as its word argument on the stack (fo_S8_0510, 0756, 07D4) */
static void with_level(Regs *r, void (*f)(Regs *), int16_t level) {
    MZ_FRAME;
    uint32_t s = mz_local(8);
    w16(s + 4, (uint16_t)level);
    r->a[7] = s;
    f(r);
}
/* fo_S8_0E2A: the game loop, until G_57F2. Each pass handles the pending events (unless only the
 * window is being redrawn, G_57D0), then, unless suspended (G_589E), plays one frame: the step
 * taken from G_56D8 / G_56D4 (G_56CE / G_56CA), the mouse's movement (G_5756 / G_5754, into
 * G_7200 / G_7202) steering the player (fo_S8_1152, not while a full-view picture shows), the
 * objects (fn_S2_031C), then the drawing: panel, walls (fo_S2_41E8), floor, sprites, full-view
 * pictures (fo_S8_0F5A), effects, the frame onto the window (which waits for the frame's time),
 * gauges, message box and labels. A redraw (G_57D0) is one pass. */
void fo_S8_0E2A(void) {
    MZ_FRAME;
    Regs r = {{0}, {0}, 0};
    uint32_t args = mz_local(0x40) + 0x30;                  /* stack arguments for fn_S2_2C72 */
    sog8(0x71F8, 0); sog16(0x71F0, 0); sog16(0x71F2, 0);
    if (!og8(0x57D0)) sog32(0x7206, r32(r32(og32(0x5800) + 0x68) + 0x20));
    sog8(0x57F2, 0);
    while (!og8(0x57F2)) {
        if (!og8(0x57D0)) {
            int got;
            do {
                got = mac_WaitNextEvent((int16_t)0xFFFF, OADDR(0x57EE), 0, 0);
                if (got) fn_S8_10EC(OADDR(0x57EE));
            } while (got && !og8(0x57F2));
        }
        if (og8(0x57D0) || !og8(0x589E)) {
            sog32(0x56CE, og32(0x56D8)); sog32(0x56CA, og32(0x56D4));
            fn_S8_0E4C(&r);
            if (!og8(0x57D0)) {
                sog16(0x7200, og16(0x5756)); sog16(0x7202, og16(0x5754));
                sog16(0x5756, 0); sog16(0x5754, 0);
                if (!og16(0x56E0)) fo_S8_1152();
            }
            mz_seg_of(2, 0x0376), fn_S2_0376(&r);
            mz_seg(2);
            fo_S2_35FC();
            fn_S2_3A36();
            fo_S2_41E8(og32(0x53B4));
            w32(args + 4, og32(0x53B4)); w32(args + 8, og32(0x53F4));
            r.a[7] = args; fn_S2_3692(&r);
            fn_S2_5810(&r);
            if (og16(0x56E0)) fo_S8_0F5A();
            if (og16(0x56EE) == 0x2B) fn_S8_0F90();
            if (og16(0x56EE) == 0x2C) fn_S8_1004();
            if (og16(0x56EE) == 0x2D) fn_S8_1078();
            mz_seg(2);
            fn_S2_040A();
            fo_S2_36C2(); fo_S2_3A0E(); fo_S2_3D6E(); fo_S2_3E04();
        }
        if (og8(0x57D0)) sog8(0x57F2, 1);
    }
}
/* fo_S8_0000: plays levels until G_57F1: the mouse's VBL task runs throughout (not for a redraw,
 * G_57D0). Each pass sets up the level G_56DC (G_720C): its objects and the player (fo_S8_065A),
 * graphics (fn_S8_0136), colours (fo_S8_05CC), offscreen world, map (fo_S8_0510), objects'
 * places (fo_S8_0756), locks it all and runs the game loop; then (not for a redraw) stores the
 * level (fo_S8_07D4) and, on a level change (G_57DE), puts the player at the new level's arrival
 * point. Returns 1, or 0 if something could not be set up. */
uint8_t fo_S8_0000(void) {
    MZ_FRAME;
    Regs r = {{0}, {0}, 0};
    uint32_t rc = mz_local(8);
    int fail = 0;
    if (!og8(0x57D0)) fn_S8_172A();
    sog8(0x57F1, 0);
    while (!og8(0x57F1)) {
        fn_S8_083A(&r);
        fo_S8_065A(&r);
        sog16(0x720C, og16(0x56DC));
        fn_S8_0946(&r);
        int16_t level = (int16_t)og16(0x720C);
        if (!fn_S8_0136(level) || !fo_S8_05CC(level)) { fail = 1; break; }
        fo_S8_44A8(OADDR(0x589C), og32(0x5898));
        if (!og32(0x589C)) { fail = 1; break; }
        fn_S8_083A(&r);
        with_level(&r, fo_S8_0510, level);
        if (!(uint8_t)r.d[0]) { fail = 2; break; }
        if (og8(0x57D0)) {
            uint32_t w = og32(0x5804);
            mac_SetPort(w);
            w32(rc, r32(w + 0x10)); w32(rc + 4, r32(w + 0x14));
            mac_ClipRect(rc); mac_ValidRect(rc);
        }
        with_level(&r, fo_S8_0756, level);
        fn_S8_0946(&r);
        fn_S8_083A(&r);
        fn_S8_02F0();
        fn_S8_09FC(og32(0x53F0));
        fn_S8_04C8(&r);
        fo_S8_0E2A();
        fn_S8_0946(&r);
        fn_S8_0360();
        if (og8(0x57D0)) { sog8(0x57F1, 1); continue; }
        fn_S8_083A(&r);
        with_level(&r, fo_S8_07D4, level);
        if (og8(0x57DE)) {
            sog32(0x56D8, og32(0x57D8)); sog32(0x56D4, og32(0x57D4));
            sog16(0x56DC, og16(0x57DC)); sog16(0x56DA, og16(0x57DA));
            sog8(0x57DE, 0); sog16(0x56E0, 0);
        }
        fo_S8_06DC(&r);
        fn_S8_0946(&r);
    }
    if (!og8(0x57D0)) fn_S8_17D0();
    if (fail == 2) fn_S8_0946(&r);
    return !fail;
}
/* fo_S8_3796: starts playing from the main window: finds its screen (fn_S8_4CF0) and makes the
 * offscreen world (alert G_68E0 if it cannot); unless the game is over (G_5787) checks the screen
 * (fewer than 4 bits per pixel: alert G_68A8; another unusual depth: G_67A8; both go on; smaller than
 * the window: alert G_66B4 and stop). Then the game window, black windows on the other screens,
 * plays (fo_S8_0000) and puts it all back. */
void fo_S8_3796(void) {
    MZ_FRAME;
    uint32_t rc = mz_local(8);
    fn_S8_4CF0(OADDR(0x5898));
    fo_S8_44A8(OADDR(0x589C), og32(0x5898));
    if (!og32(0x589C)) { mz_seg_of(2, 0x03A0), fn_S2_03A0(OADDR(0x68E0), 0, 0); return; }
    int16_t depth = (int16_t)r16(deref(r32(deref(og32(0x5898)) + 0x16)) + 0x20);
    if (og8(0x5787)) return;
    if (depth < 4) { fn_S8_4BE2(1); mz_seg_of(2, 0x03DC), fn_S2_03DC(OADDR(0x68A8), 0); }
    else if (depth != 8 && depth != 4 && depth != 16 && depth != 32) { fn_S8_4BE2(1); mz_seg_of(2, 0x03DC), fn_S2_03DC(OADDR(0x67A8), 0); }
    uint32_t g = deref(og32(0x5898)) + 0x22;
    if ((int16_t)(r16(g + 6) - r16(g + 2)) < (int16_t)og16(0x5748) || (int16_t)(r16(g + 4) - r16(g)) < (int16_t)og16(0x5746)) {
        fn_S8_4BE2(1); mz_seg_of(2, 0x03DC), fn_S2_03DC(OADDR(0x66B4), 0);
        return;
    }
    sog8(0x5788, 0); sog8(0x5784, 1); sog8(0x57D0, 0);
    fn_S8_4BE2(0);
    mac_HideCursor();
    fn_S8_4526(); fn_S8_4620(); fn_S8_471C();
    uint32_t w = og32(0x5800);
    mac_SelectWindow(w); mac_SetPort(w);
    w32(rc, r32(w + 0x10)); w32(rc + 4, r32(w + 0x14));
    mac_ClipRect(rc); mac_PmForeColor(0); mac_PaintRect(rc); mac_ValidRect(rc);
    { Regs regs = {{0}, {0}, 0}; mz_seg_of(2, 0x39F8), fn_S2_39F8(&regs); }
    fo_S8_0000();
    fn_S8_4790(); fn_S8_46EE(); fn_S8_460C();
    mac_SetPort(og32(0x5804));
    mac_ShowCursor();
    fn_S8_4BE2(1);
}
/* fo_S8_38EE: an update of the main window G_5804: what it shows (G_57D0 set: drawn, not played):
 * no offscreen world: fo_S8_3BB8; the title (G_5788): fo_S8_39AA; the level's end (G_5787):
 * fo_S8_3AB8; else the game's view (one pass of fo_S8_0000; the pause screen fo_S8_3B3E if it cannot) */
void fo_S8_38EE(void) {
    MZ_FRAME;
    uint32_t rc = mz_local(8), w = og32(0x5804);
    sog8(0x57D0, 1);
    fn_S8_4CF0(OADDR(0x5898));
    fo_S8_44A8(OADDR(0x589C), og32(0x5898));
    mac_BeginUpdate(w);
    if (!og32(0x589C)) fo_S8_3BB8();
    else if (og8(0x5788)) fo_S8_39AA();
    else if (og8(0x5787)) fo_S8_3AB8();
    else {
        mac_SetPort(w);
        w32(rc, r32(w + 0x10)); w32(rc + 4, r32(w + 0x14));
        mac_ClipRect(rc); mac_PmForeColor(0); mac_PaintRect(rc);
        { Regs regs = {{0}, {0}, 0}; mz_seg_of(2, 0x39F8), fn_S2_39F8(&regs); }
        if (!fo_S8_0000()) fo_S8_3B3E();
        if (og8(0x5787)) {
            mac_SetPort(w);
            w32(rc, r32(w + 0x10)); w32(rc + 4, r32(w + 0x14));
            mac_ClipRect(rc); mac_InvalRect(rc);
        }
    }
    mac_EndUpdate(w);
}

/* Original's menus (fn_S8_2524): Apple 128 (fn_S8_29AE: About = the instructions from page 1, Read
 * Instructions from page 3, Print Order Form, Register; desk accessories), File 129 (fo_S8_2A2A),
 * Window Size 131 (fo_S8_2A78), Options 132 (fo_S8_2AA8). Print Order Form is left out: the port
 * has no Mac printing (the original prints the form through the Printing Manager). */
void fo_menu(int16_t menu, int16_t item) {
    switch (menu) {
    case 0x80:
        if (item == 1 || item == 2) fn_S8_190C(item == 1 ? 1 : 3);
        else if (item == 4) { fo_S8_68FA(); inval_window(); }
        break;
    case 0x81: fo_S8_2A2A(item); break;
    case 0x83: fo_S8_2A78(item); break;
    case 0x84: fo_S8_2AA8(item); break;
    }
}

/* fo_S8_1C0C (JT 19): the program: Toolbox set-up, menus, system checks, Apple events, the font
 * (G_71AA into G_5816; alert G_71A2 if missing), window storage (G_580C, G_5808), preferences
 * (G_5814, fo_S8_2008), the palette (G_57F6), the renderer's memory, the game (fo_S8_2AE2), level
 * 1's graphics, the panel's starting values, the main window at the preferences' size and screen;
 * then the shell loop; on Quit the preferences are saved (alert on error). */
void fo_S8_1C0C(void) {
    MZ_FRAME;
    Regs r = {{0}, {0}, 0};
    fn_S8_25A8(); fn_S8_26A2(); fn_S8_2480(); fn_S8_25BE();
    mac_GetFNum(OADDR(0x71AA), OADDR(0x5816));
    if (!og16(0x5816)) mz_seg_of(2, 0x03DC), fn_S2_03DC(OADDR(0x71A2), 0);
    sog32(0x580C, mac_NewPtr(0x9C)); if (!og32(0x580C)) alert(0x711C, 0, 1);
    sog32(0x5808, mac_NewPtr(0x9C)); if (!og32(0x5808)) alert(0x7102, 0, 1);
    sog32(0x5814, fo_S8_2008(1)); if (!og32(0x5814)) alert(0x70E8, 0, 1);
    int colour = mac_TestDeviceAttribute(mac_GetMainDevice(), 0);
    sog32(0x57F6, (mz_seg_of(2, 0x01FA), fn_S2_01FA(colour ? 0xC8 : 0xD2)));
    if (!og32(0x57F6)) alert(0x70A0, 0, 1);
    sog32(0x589C, 0);
    int16_t err = fn_S8_04FA();
    if (err) alert(0x595E, err, 1);
    fo_S8_2AE2();
    fn_S8_0136(1);
    fn_S8_2A32(&r);
    mz_seg_of(2, 0x39F8), fn_S2_39F8(&r);                   /* fn_S2_2F98: the panel's starting values */
    sog16(0x56EE, 0); sog16(0x56EA, 0x8E); sog16(0x56E6, 0x8E); sog16(0x56E2, 3);
    uint32_t p = deref(og32(0x5814));
    if (!fn_S8_48A2((int16_t)r16(p), p + 2)) mac_ExitToShell();
    sog32(0x37C4, r32(0x020C));                             /* seed from the clock (Time) */
    fn_S8_2A4A();
    err = fo_S8_210C(og32(0x5814));
    if (err) {
        uint32_t what = mz_local(0x100);
        mz_seg_of(3, 0x03BA), fn_S3_03BA(what, 0x80, 1);
        fn_S8_37E6(what, err);
    }
}
