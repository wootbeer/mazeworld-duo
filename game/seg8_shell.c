/* CODE segment 8: the program's main routine, the shell's event loop, the menu commands' actions
 * and saving the preferences (see build/disasm/seg8.s and docs/SHELL.md). Menu tracking and mouse
 * clicks are left to the native shell (shell_menu_key / shell_mouse_down): the port draws its own
 * menu and calls the actions below. */
#include "mzasm.h"
#include "fns.h"

/* ---------- the menu commands' actions ---------- */

/* fn_S8_3476: File > New Game: resets the player (fn_S8_3492) and redraws the high-score window. */
void fn_S8_3476(void) {
    fn_S8_3492();
    mac_SetPort(g32(0x2232));
    mac_InvalRect(g32(0x2232) + 0x10);
}

/* fn_S8_394A: File > Clear High Scores: after confirmation (alert 134, item 2) the ten entries become
 * empty (the name G_47E4, level 0, score 0) and the window is redrawn. G_22A6 (the place to mark)
 * is cleared either way. */
void fn_S8_394A(void) {
    MZ_FRAME;
    uint32_t name = mz_local(0x40);
    for (uint32_t k = 0; k < 0x40; k += 4) w32(name + k, r32(GADDR(0x47E4) + k));
    mac_HLock(g32(0x2242));
    uint32_t tbl = deref(g32(0x2242));
    if (mac_Alert(0x86, 0) == 2) {
        for (int16_t i = 0; i < 10; i++) {
            uint32_t e = tbl + (uint32_t)(0x46 * i);
            mz_seg_of(2, 0x0332), fn_S2_0332(name, e + 0x118);
            w16(e + 0x158, 0); w32(e + 0x15A, 0);
        }
        mac_SetPort(g32(0x2232));
        mac_InvalRect(g32(0x2232) + 0x10);
    }
    sg16(0x22A6, 0xFFFF);
    mac_HUnlock(g32(0x2242));
}

/* fn_S8_39EE: File > Quit: ends the shell loop. */
void fn_S8_39EE(void) { sg8(0x223C, 1); }

/* fn_S8_33E2: Sound > item 1-4: G_238E = item - 1 (No / Reduced / Normal / Full); the sounds are
 * unloaded at 0 or loaded if missing, and the channels reopened (4 for Full). */
void fn_S8_33E2(int16_t item) {
    if (item >= 1 && item <= 4) sg16(0x238E, (uint16_t)(item - 1));
    mz_seg(4);
    if (!g16(0x238E)) { if (fn_S4_38E6()) fn_S4_3AB2(); }
    else if (!fn_S4_38E6()) fn_S8_237C();
    fn_S4_3896(g16(0x238E) == 3 ? 4 : (int16_t)g16(0x238E));
}

/* fn_S8_4ACE: View > size 1-4: rebuilds everything for that size; if it cannot, goes back to the
 * previous size (alert G_3B32) or to size 1 (alert G_3B94); the window is resized and recentred.
 * Returns 0 if not even size 1 works (the caller quits). */
uint8_t fn_S8_4ACE(int16_t size) {
    mz_seg_of(2, 0x3872), fn_S2_3872(GADDR(0x22C4));
    if (!mz_original) fn_S2_3872(GADDR(0x22C0));            /* Original (fn_S8_4216) has no automap world */
    uint8_t reserved = fn_S8_61AA();
    if (!reserved) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3BF4), 0, 0);
    mac_SetCursor(deref(mac_GetCursor(4)));                 /* watch */
    int16_t old = (int16_t)g16(0x22B6);
    int outcome = 0;                                        /* 1: fell back to size 1, 2: kept the old size */
    if (!fn_S8_527A(size)) {
        if (fn_S8_527A(old)) outcome = 2;
        else if (fn_S8_527A(1)) outcome = 1;
        else {
            if (reserved) { Regs r = {{0}, {0}, 0}; fn_S8_61FE(&r); }
            mac_InitCursor();
            mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3BCC), 0, 0);
            return 0;
        }
    }
    if (reserved) { Regs r = {{0}, {0}, 0}; fn_S8_61FE(&r); }
    if (outcome != 2) {
        uint32_t w = g32(0x2232);
        mac_HideWindow(w);
        mac_SizeWindow(w, (int16_t)g16(0x2166), (int16_t)g16(0x2164), 1);
        fn_S8_4BE2(1);
        mac_ShowWindow(w);
        mac_SelectWindow(w);
    }
    mac_InitCursor();
    if (outcome == 1) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3B94), 0, 0);
    if (outcome == 2) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3B32), 0, 0);
    return 1;
}

/* fn_S8_338A: View > item: 1-4 the view size (fn_S8_4ACE; quits if nothing works; after a game
 * interrupted by the change, G_221E, play resumes), 6-8 the resolution (G_2392 = 3 / 2 / 1). */
void fn_S8_338A(int16_t item) {
    if (item >= 1 && item <= 4) {
        if (!fn_S8_4ACE(item)) mac_ExitToShell();
        if (g8(0x221E)) sg8(0x4798, 1);
    } else if (item == 6) sg16(0x2392, 3);
    else if (item == 7) sg16(0x2392, 2);
    else if (item == 8) sg16(0x2392, 1);
}

/* fn_S8_2D26: a click in the high-score window w: after a game over (G_21B3, unless G_21B2) it
 * starts a new game (fn_S8_3492); otherwise it starts play (G_4798), a click in the window's
 * top-right 4x4 pixels first toggling G_22C9 (the order of the floor renderer's passes). */
void fn_S8_2D26(uint32_t w, uint32_t ev) {
    MZ_FRAME;
    if (w != g32(0x2232)) return;
    if (g8(0x21B3) && !g16(0x21B2)) { fn_S8_3492(); return; }
    uint32_t pt = mz_local(4);
    mac_SetPort(g32(0x2232));
    w32(pt, r32(ev + 0xA));
    mac_GlobalToLocal(pt);
    int16_t v = (int16_t)r16(pt), h = (int16_t)r16(pt + 2);
    if ((int16_t)(g16(0x2166) - 4) < h && v < 4) sg8(0x22C9, g8(0x22C9) ? 0 : 1);
    sg8(0x4798, 1);
}

/* ---------- the shell's events ---------- */

/* fn_S8_2B86 / 2B5E: suspend / resume (operating-system event): G_22C6 = suspended. */
void fn_S8_2B86(uint32_t ev) {
    if (r8(ev + 5) & 1) { sg8(0x22C6, 0); mac_InitCursor(); }
    else sg8(0x22C6, 1);
}
void fn_S8_2B5E(uint32_t ev) { if (((r32(ev + 2) >> 24) & 0xFF) == 1) fn_S8_2B86(ev); }

/* fn_S8_2C36: a key in the shell: with the command key, a menu command (the native shell's);
 * space or Return (not repeats) start play (G_4798). */
void fn_S8_2C36(uint32_t ev) {
    uint8_t ch = r8(ev + 5);
    if (r8(ev + 0xE) & 1) { shell_menu_key(ev); return; }   /* cmdKey */
    if ((ch == ' ' || ch == '\r') && r16(ev) != 5) sg8(0x4798, 1);
}

/* fn_S8_2D12: a high-level event: the Apple Event Manager calls the installed handlers (the
 * native shell's: on a Mac the handlers for 'oapp', 'odoc' and 'quit', fn_S8_3A44 / 3A72 / 3B6E). */
void fn_S8_2D12(uint32_t ev) { shell_high_level_event(ev); }

/* fn_S8_2AB8: dispatches a shell event: mouse-down (the native shell), keys (fn_S8_2C36), update
 * (fn_S8_2CAC), operating-system (fn_S8_2B5E) and high-level events (fn_S8_2D12). Disk-inserted
 * events (the Disk Initialization package, fn_S8_2CD0) and activate events (fn_S8_2C8C, which does
 * nothing) have no Windows counterpart. */
void fn_S8_2AB8(uint32_t ev) {
    switch (r16(ev)) {
    case 1: shell_mouse_down(ev); break;
    case 3: case 5: fn_S8_2C36(ev); break;
    case 6: fn_S8_2CAC(r32(ev + 2)); break;
    case 15: fn_S8_2B5E(ev); break;
    case 23: fn_S8_2D12(ev); break;
    default: break;
    }
}

/* fn_S8_2A4A: the shell loop, until Quit (G_223C): waits for events (sleeping a tick), handles
 * them, and starts play when asked (G_4798, fn_S8_3C0C). An event that ended play (G_221E) is
 * handled again here. */
void fn_S8_2A4A(void) {
    sg8(0x223C, 0); sg8(0x22C6, 0); sg8(0x221E, 0);
    while (!g8(0x223C)) {
        int got = g8(0x221E) ? 1 : mac_WaitNextEvent((int16_t)0xFFFF, GADDR(0x221C), 1, 0);
        sg8(0x4798, 0);
        if (got) fn_S8_2AB8(GADDR(0x221C));
        sg8(0x221E, 0);
        if (g8(0x4798)) fn_S8_3C0C();
    }
}

/* ---------- saving the preferences ---------- */

/* fn_S8_3822: the text for a File Manager error code (in the alerts of fn_S8_37E6). */
uint32_t fn_S8_3822(int16_t err) {
    static const struct { int16_t err; uint16_t g; } texts[] = {
        {1, 0x427A}, {2, 0x4268}, {-33, 0x4252}, {-34, 0x423E}, {-35, 0x4232}, {-36, 0x421E},
        {-37, 0x4216}, {-39, 0x41F8}, {-42, 0x41E2}, {-43, 0x41CC}, {-44, 0x41BC}, {-45, 0x41A4},
        {-46, 0x4192}, {-47, 0x417C}, {-53, 0x416E}, {-54, 0x415C}, {-61, 0x414E}, {-5000, 0x413A},
    };
    for (unsigned k = 0; k < sizeof texts / sizeof *texts; k++) if (texts[k].err == err) return GADDR(texts[k].g);
    return GADDR(0x4104);
}

/* fn_S8_37E6: "could not save" alert (130): ^0 = what, ^1 = the error's text, ^2 = its number. */
void fn_S8_37E6(uint32_t what, int16_t err) {
    MZ_FRAME;
    uint32_t num = mz_local(0x100);
    mz_seg_of(3, 0x038E), fn_S3_038E(err, num);
    mac_ParamText(what, fn_S8_3822(err), num, GADDR(0x427C));
    mac_StopAlert(0x82, 0);
}

/* fn_S8_3658: writes handle h as the whole data fork of open file ref, then flushes its volume;
 * G_21B0 (unsaved changes) is cleared on success. Returns the error. */
int16_t fn_S8_3658(int16_t ref, uint32_t h) {
    MZ_FRAME;
    uint32_t n = mz_local(4), vref = mz_local(2);
    mz_seg(3);
    w32(n, (uint32_t)fn_S3_020E(h));
    int16_t err = fn_S3_030E(ref, 1, 0);                    /* fsFromStart */
    if (!err) err = fn_S3_0268(ref, n, deref(h));
    if (!err) err = fn_S3_02EE(ref, (int32_t)r32(n));
    if (!err) err = fn_S3_0334(ref, vref);
    if (!err) err = fn_S3_02AC(0, (int16_t)r16(vref));
    if (!err) sg8(0x21B0, 0);
    return err;
}

/* fn_S8_3522: saves handle h as file `spec` safely: written to a temporary file (named after the
 * time, in the Temporary Items folder of the same volume), which then replaces the file. Returns
 * the error. */
int16_t fn_S8_3522(uint32_t spec, uint32_t h, uint32_t creator, uint32_t type) {
    MZ_FRAME;
    uint32_t name = mz_local(0x100), vref = mz_local(2), dir = mz_local(4), tmp = mz_local(0x46), ref = mz_local(2);
    mz_seg_of(3, 0x038E), fn_S3_038E((int32_t)r32(0x020C), name);
    int16_t err = fn_S3_0400((int16_t)r16(spec), 0x74656D70, 1, vref, dir);   /* 'temp', create */
    if (!err) err = mac_FSMakeFSSpec((int16_t)r16(vref), (int32_t)r32(dir), name, tmp);
    if (!err || err == -43) err = mac_FSpCreate(tmp, 0x74727368, 0x74727368, -1);   /* 'trsh' */
    if (!err) err = mac_FSpOpenDF(tmp, 3, ref);
    if (!err) err = fn_S8_3658((int16_t)r16(ref), h);
    if (!err) err = fn_S3_024A((int16_t)r16(ref));
    if (!err) { mac_FSpDelete(spec); err = mac_FSpCreate(spec, creator, type, -1); }
    if (!err) mac_FSpExchangeFiles(tmp, spec);
    if (!err) mac_FSpDelete(tmp);
    return err;
}

/* fn_S8_295A: saves the preferences in h (the settings, the view size and screen) to the file
 * named by 'STR#' 128 #1 in the Preferences folder. Returns the error. */
int16_t fn_S8_295A(uint32_t h) {
    MZ_FRAME;
    mac_HLock(h);
    uint32_t p = deref(h);
    w8(p + 0xA, g8(0x22B8)); w8(p + 0xB, g8(0x2394)); w8(p + 0xC, g8(0x2391)); w8(p + 0xD, g8(0x2393));
    w16(p + 0xE, g16(0x238E));
    w16(p, g16(0x22B6));
    uint32_t g = deref(g32(0x22BC));
    w32(p + 2, r32(g + 0x22)); w32(p + 6, r32(g + 0x26));
    mac_HUnlock(h);
    uint32_t name = mz_local(0x100), vref = mz_local(2), dir = mz_local(4), spec = mz_local(0x46);
    mz_seg_of(3, 0x03BA), fn_S3_03BA(name, 0x80, 1);
    int16_t err = fn_S3_0400((int16_t)0x8000, 0x70726566, 0, vref, dir);   /* kOnSystemDisk, 'pref' */
    if (!err) {
        err = mac_FSMakeFSSpec((int16_t)r16(vref), (int32_t)r32(dir), name, spec);
        if (err == -43) err = 0;                            /* fnfErr: a new file */
    }
    if (!err) err = fn_S8_3522(spec, h, 0x4D7A7743, 0x50524546);          /* 'MzwC', 'PREF' */
    return err;
}

/* ---------- main ---------- */

/* fn_S8_21AA (JT 15): the program: Toolbox set-up, menus, system checks, Apple events, the font
 * (G_4796 into G_2244; alert G_478E if missing), window storage (G_223A, G_2236), preferences
 * (G_2242), the palette (G_2224), the renderer's memory, a new game, the sounds, the main window;
 * then the shell loop; on Quit the preferences are saved (alert on error) and everything freed.
 * Each failure on the way is a fatal alert. */
void fn_S8_21AA(void) {
    MZ_FRAME;
    Regs r = {{0}, {0}, 0};
    fn_S8_25A8(); fn_S8_26A2(); fn_S8_2480(); fn_S8_25BE();
    mac_GetFNum(GADDR(0x4796), GADDR(0x2244));
    /* (a segment is loaded where the original first calls into it: that allocates its code) */
    if (!g16(0x2244)) mz_seg_of(2, 0x03DC), fn_S2_03DC(GADDR(0x478E), 0);
    sg32(0x223A, mac_NewPtr(0x9C)); if (!g32(0x223A)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x4708), 0, 1);
    sg32(0x2236, mac_NewPtr(0x9C)); if (!g32(0x2236)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x46EE), 0, 1);
    sg32(0x2242, fn_S8_26D6()); if (!g32(0x2242)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x46D4), 0, 1);
    uint32_t main_gd = mac_GetMainDevice();
    if (r16(deref(r32(deref(main_gd) + 0x16)) + 0x20) == 1) sg32(0x2224, (mz_seg_of(2, 0x01FA), fn_S2_01FA(0xCA)));
    else if (mac_TestDeviceAttribute(mac_GetMainDevice(), 0)) sg32(0x2224, (mz_seg_of(2, 0x01FA), fn_S2_01FA(0xC8)));
    else sg32(0x2224, (mz_seg_of(2, 0x01FA), fn_S2_01FA(0xC9)));
    if (!g32(0x2224)) fn_S2_03A0(GADDR(0x468C), 0, 1);
    sg32(0x22C4, 0); sg32(0x22C0, 0);
    int16_t err = fn_S8_04FA();
    if (err) fn_S2_03A0(GADDR(0x2386), err, 1);
    fn_S8_3492();
    fn_S8_0136(1);
    mz_seg_of(4, 0x38B8), fn_S4_38B8();
    if (g16(0x238E)) fn_S8_237C();
    fn_S4_3896(g16(0x238E) == 3 ? 4 : (int16_t)g16(0x238E));
    fn_S8_2A32(&r);
    mz_seg_of(2, 0x39B8), fn_S2_39B8(&r);
    int16_t size = (int16_t)r16(deref(g32(0x2242)));
    if (!size) size = 2;
    if (!fn_S8_48A2(size, deref(g32(0x2242)) + 2)) mac_ExitToShell();
    sg32(0x0082, r32(0x020C));                              /* seed from the clock (Time) */
    fn_S8_2A4A();
    err = fn_S8_295A(g32(0x2242));
    if (err) {
        uint32_t what = mz_local(0x100);
        mz_seg_of(3, 0x03BA), fn_S3_03BA(what, 0x80, 1);
        fn_S8_37E6(what, err);
    }
    fn_S8_50FC(GADDR(0x22C4), GADDR(0x22C0));
    mz_seg_of(4, 0x3AB2), fn_S4_3AB2();
}

/* ---------- enabling and checking the menu items ---------- */
/* (The port draws its own menu bar from the same rules, port/shell.c; these keep the game's MENU
 * records as the original does, before every menu command.) */

static void able(uint32_t m, int16_t item, int on) { if (on) mac_EnableItem(m, item); else mac_DisableItem(m, item); }

/* fn_S8_3190: View menu m: check marks on the view size (G_22B6, items 1-4) and the resolution
 * (G_2392 = 1 / 2 / 3: items 8 / 7 / 6). */
void fn_S8_3190(uint32_t m) {
    for (int16_t i = 1; i <= 4; i++) mac_CheckItem(m, i, (int16_t)g16(0x22B6) == i);
    if (mz_original) return;                                  /* Original (fn_S8_28EA): no resolutions */
    for (int16_t r = 1; r <= 3; r++) mac_CheckItem(m, (int16_t)(9 - r), (int16_t)g16(0x2392) == r);
}

/* fn_S8_3278: Sound menu m: the check mark on the sound level G_238E (items 1-4 = 0..3). */
void fn_S8_3278(uint32_t m) {
    if (mz_original) {                                        /* Original (fn_S8_2972): On / Off by G_22B8 */
        mac_CheckItem(m, 1, !g8(0x22B8)); mac_CheckItem(m, 2, g8(0x22B8) != 0);
        return;
    }
    for (int16_t i = 1; i <= 4; i++) mac_CheckItem(m, i, (int16_t)g16(0x238E) == i - 1);
}

/* fn_S8_2E4C: everything disabled but Quit (a desk accessory or the game window in front). */
static void menus_original(int on);
void fn_S8_2E4C(void) {
    uint32_t m;
    if (mz_original) { menus_original(0); return; }
    if (!(m = mac_GetMenuHandle(0x80))) return;
    mac_DisableItem(m, 1);
    if (!(m = mac_GetMenuHandle(0x81))) return;
    mac_DisableItem(m, 1); mac_DisableItem(m, 3); mac_DisableItem(m, 5); mac_EnableItem(m, 7);
    if (!(m = mac_GetMenuHandle(0x82))) return;
    mac_DisableItem(m, 1); mac_DisableItem(m, 3); mac_DisableItem(m, 4); mac_DisableItem(m, 5); mac_DisableItem(m, 6);
    if (!(m = mac_GetMenuHandle(0x83))) return;
    mac_DisableItem(m, 1); mac_DisableItem(m, 2); mac_DisableItem(m, 3); mac_DisableItem(m, 4);
    mac_DisableItem(m, 8); mac_DisableItem(m, 7); mac_DisableItem(m, 6);
    fn_S8_3190(m);
    if (!(m = mac_GetMenuHandle(0x84))) return;
    mac_DisableItem(m, 1); mac_DisableItem(m, 2); mac_DisableItem(m, 3); mac_DisableItem(m, 4);
    fn_S8_3278(m);
    if (!(m = mac_GetMenuHandle(0x85))) return;
    mac_DisableItem(m, 1);
}

/* fn_S8_2FB4: the high-score window in front: About; Play Game unless the game is over (G_21B3),
 * New Game while a game is in progress (G_21B4), Clear High Scores, Quit; Edit disabled; the view
 * sizes the screen allows (G_2168) and the resolutions; the sound levels; Options. */
void fn_S8_2FB4(void) {
    uint32_t m;
    if (mz_original) { menus_original(1); return; }
    if (!(m = mac_GetMenuHandle(0x80))) return;
    mac_EnableItem(m, 1);
    if (!(m = mac_GetMenuHandle(0x81))) return;
    able(m, 1, !g8(0x21B3));
    able(m, 3, !g8(0x21B4));
    mac_EnableItem(m, 5); mac_EnableItem(m, 7);
    if (!(m = mac_GetMenuHandle(0x82))) return;
    mac_DisableItem(m, 1); mac_DisableItem(m, 3); mac_DisableItem(m, 4); mac_DisableItem(m, 5); mac_DisableItem(m, 6);
    if (!(m = mac_GetMenuHandle(0x83))) return;
    for (int16_t i = 1; i <= 4; i++) able(m, i, (int16_t)g16(0x2168) >= i);
    mac_EnableItem(m, 8); mac_EnableItem(m, 7); mac_EnableItem(m, 6);
    fn_S8_3190(m);
    if (!(m = mac_GetMenuHandle(0x84))) return;
    mac_EnableItem(m, 1); mac_EnableItem(m, 2); mac_EnableItem(m, 3); mac_EnableItem(m, 4);
    fn_S8_3278(m);
    if (!(m = mac_GetMenuHandle(0x85))) return;
    mac_EnableItem(m, 1);
}

/* Mazeworld Original's menus (fn_S8_25D6 all off, fn_S8_2716 for the high-score window): Apple
 * 1-4; File 1 Play Game unless over (G_21B3), 3 New Game unless in progress (G_21B4), 4, 6 only
 * with G_21B0 and neither of those, 8; Edit off; View sizes allowed (G_2168); Sound On / Off. */
static void menus_original(int on) {
    uint32_t m;
    if (!(m = mac_GetMenuHandle(0x80))) return;
    for (int16_t i = 1; i <= 4; i++) able(m, i, on);
    if (!(m = mac_GetMenuHandle(0x81))) return;
    if (on) {
        able(m, 1, !g8(0x21B3)); able(m, 3, !g8(0x21B4)); mac_EnableItem(m, 4);
        able(m, 6, !g8(0x21B4) && !g8(0x21B3) && g8(0x21B0)); mac_EnableItem(m, 8);
    } else { mac_DisableItem(m, 1); mac_DisableItem(m, 3); mac_DisableItem(m, 4); mac_DisableItem(m, 6); mac_DisableItem(m, 8); }
    if (!(m = mac_GetMenuHandle(0x82))) return;
    mac_DisableItem(m, 1); mac_DisableItem(m, 3); mac_DisableItem(m, 4); mac_DisableItem(m, 5); mac_DisableItem(m, 6);
    if (!(m = mac_GetMenuHandle(0x83))) return;
    for (int16_t i = 1; i <= 4; i++) able(m, i, on && (int16_t)g16(0x2168) >= i);
    fn_S8_3190(m);
    if (!(m = mac_GetMenuHandle(0x84))) return;
    able(m, 1, on); able(m, 2, on);
    fn_S8_3278(m);
}

/* fn_S8_2E10: before a menu command: fn_S8_2E4C if a desk accessory (negative window kind) or a
 * window with a reference constant (the game's) is in front, else fn_S8_2FB4. */
void fn_S8_2E10(void) {
    uint32_t w = mac_FrontWindow();
    if ((int16_t)r16(w + 0x6C) < 0) fn_S8_2E4C();
    else if (mac_GetWRefCon(w)) fn_S8_2E4C();
    else fn_S8_2FB4();
}
