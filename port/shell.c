/* The port's shell: the original's menus on a native menu bar (see docs/SHELL.md).
 *
 * A menu choice reaches the game as it would on a Mac: posted as a click on the menu bar, which
 * the MenuSelect trap answers with that item (harness/menu.c). Command keys arrive as key events
 * and go through MenuKey. Either way the game's own handlers run the command, so the result is
 * the same as on a Mac. The test rig's scripts pick menu items the same way.
 *
 * Greying and check marks come from the game's own rules (fn_S8_2E4C / 2FB4 / 3190 / 3278,
 * in shell_item_state). Catacombs' Options dialog ("More Power B'Elanna!") becomes three
 * check-mark items under Options > Legacy. */
#include "../harness/hx.h"
#include "../game/mz.h"
#include "../game/fns.h"
#include "../mactb/mactb.h"
#include "shell.h"

uint32_t qd_front_window(void);
MtbRect qd_window_global(uint32_t win);
void input_live_event(uint16_t what, uint32_t msg, uint32_t where);
void input_live_menu(int menu, int item);


/* is the high-score window (the shell's) in front? (fn_S8_2E10: a desk accessory or a window
 * with a reference constant, i.e. the game's, disables everything but Quit, fn_S8_2E4C) */
static int shell_in_front(void) {
    uint32_t w = qd_front_window();
    if (!w) return 0;
    return (int16_t)rd16(w + 0x6C) >= 0 && rd32(w + 0x98) == 0;
}

/* the game window in front (a window with a reference constant): play, or the game paused in it */
int shell_playing(void) {
    uint32_t w = qd_front_window();
    return w && (int16_t)rd16(w + 0x6C) >= 0 && rd32(w + 0x98) != 0 && !g8(0x22C6);
}

/* The part of the Mac screen the window shows (x, y, w, h).
 * Catacombs: the whole screen. Original: only the game's area at its window size (G_5748 x
 * G_5746): in play where the full-screen window draws it (the screen's centre), paused where the
 * main window G_5804 is. That keeps the picture still when play pauses, and hides the Mac desktop
 * around a small window. Returns 1 for the game's area, 0 for the whole screen. */
int shell_view(int *x, int *y, int *w, int *h) {
    *x = 0; *y = 0; *w = SCREEN_W; *h = SCREEN_H;
    uint32_t main = mz_original ? og32(0x5804) : 0, front = qd_front_window();
    int vw = (int16_t)og16(0x5748), vh = (int16_t)og16(0x5746);
    if (!main || !front || vw < 64 || vh < 64 || vw > SCREEN_W || vh > SCREEN_H) return 0;
    MtbRect f = qd_window_global(front);
    if (front != main && f.top <= 0 && f.left <= 0 && f.right >= SCREEN_W && f.bottom >= SCREEN_H) { *x = (SCREEN_W - vw) / 2; *y = (SCREEN_H - vh) / 2; }
    else { MtbRect m = qd_window_global(main); *x = m.left; *y = m.top; }
    *w = vw; *h = vh;
    return 1;
}

/* Mazeworld Original's menus, by its rules (fn_S8_259A: in the main window fn_S8_2716, else
 * everything off, fn_S8_25D6; the marks always: Window Size G_5892, fn_S8_28EA; Options G_5894,
 * fn_S8_2972). Quit stays on: outside the main window it is the 'quit' Apple event. */
static int orig_item_state(int menu, int item, int *checked) {
    *checked = 0;
    if (menu == MENU_O_SIZE) *checked = (int16_t)og16(0x5892) == item;
    if (menu == MENU_O_OPTIONS) *checked = (item == 2) == (og8(0x5894) != 0);
    if (menu == MENU_FILE && item == 8) return 1;
    if (!shell_in_front()) return 0;
    switch (menu) {
    case MENU_APPLE: return item >= 1 && item <= 4;
    case MENU_FILE:
        if (item == 1) return !og8(0x5787);
        if (item == 3) return !og8(0x5788);
        if (item == 6) return !og8(0x5788) && !og8(0x5787) && og8(0x5784);
        return item == 4;
    case MENU_O_SIZE: return item >= 1 && item <= 4 && (int16_t)og16(0x574A) >= item;
    case MENU_O_OPTIONS: return item == 1 || item == 2;
    }
    return 0;
}

int shell_item_state(int menu, int item, int *checked) {
    if (mz_original) return orig_item_state(menu, item, checked);
    *checked = 0;
    switch (menu) {
    case MENU_VIEW:
        if (item >= 1 && item <= 4) *checked = (int16_t)g16(0x22B6) == item;
        if (item >= 6 && item <= 8) *checked = (int16_t)g16(0x2392) == 9 - item;
        break;
    case MENU_SOUND:
        if (item >= 1 && item <= 4) *checked = (int16_t)g16(0x238E) == item - 1;
        break;
    case MENU_OPTIONS:
        if (item == OPT_DIRECT) *checked = !g8(0x22B8);
        if (item == OPT_NOSYNC) *checked = !g8(0x2394);
        if (item == OPT_DISTRIBUTE) *checked = g8(0x2393) != 0;
        break;
    }
    if (!shell_in_front()) return menu == MENU_FILE && item == 7;
    switch (menu) {
    case MENU_APPLE: return item == 1;
    case MENU_FILE:
        if (item == 1) return !g8(0x21B3);
        if (item == 3) return !g8(0x21B4);
        return item == 5 || item == 7;
    case MENU_EDIT: return 0;
    case MENU_VIEW:
        if (item >= 1 && item <= 4) return (int16_t)g16(0x2168) >= item;
        return item >= 6 && item <= 8;
    case MENU_SOUND: return item >= 1 && item <= 4;
    case MENU_OPTIONS: return item == OPT_DIRECT || item == OPT_NOSYNC || item == OPT_DISTRIBUTE;
    }
    return 0;
}

/* ---------- from the front end ---------- */

void shell_choose(int menu, int item) {
    if (menu == MENU_FILE && item == (mz_original ? 8 : 7) && (!mz_original || !shell_in_front())) { shell_request_quit(); return; }
    input_live_menu(menu, item);                            /* a click on the menu bar; MenuSelect answers it */
}
/* Quit, from anywhere (also the window's close box): the 'quit' Apple event, which also ends play
 * (fn_S8_10EC) and is then handled by the shell loop (fn_S8_2D12) */
void shell_request_quit(void) { input_live_event(23, FOURCC('a','e','v','t'), FOURCC('q','u','i','t')); }
/* the application going to the background or coming back: suspend / resume events */
void shell_focus(int active) { input_live_event(15, 0x01000000u | (active ? 1u : 0u), 0); }

/* ---------- from the game ---------- */

/* fn_S8_2D86: runs a menu command (menu << 16 | item; 0 = none), then unhighlights the menu */
static void dispatch(uint32_t cmd) {
    if (!cmd) return;
    int menu = (int)(cmd >> 16), item = (int)(cmd & 0xFFFF);
    if (mz_original) { void fo_menu(int16_t menu, int16_t item); fo_menu((int16_t)menu, (int16_t)item); mac_HiliteMenu(0); return; }
    switch (menu) {
    case MENU_APPLE: if (item == 1) fn_S8_190C(1); break;  /* fn_S8_32FE: the instructions (desk accessories: none) */
    case MENU_FILE:                                         /* fn_S8_3344 */
        if (item == 1) sg8(0x4798, 1);
        else if (item == 3) fn_S8_3476();
        else if (item == 5) fn_S8_394A();
        else if (item == 7) fn_S8_39EE();
        break;
    case MENU_VIEW: fn_S8_338A((int16_t)item); break;
    case MENU_SOUND: fn_S8_33E2((int16_t)item); break;
    case MENU_OPTIONS:                                      /* fn_S8_1FC4's three check boxes, as menu items */
        if (item == OPT_DIRECT) sg8(0x22B8, g8(0x22B8) ? 0 : 1);
        if (item == OPT_NOSYNC) sg8(0x2394, g8(0x2394) ? 0 : 1);
        if (item == OPT_DISTRIBUTE) sg8(0x2393, g8(0x2393) ? 0 : 1);
        break;
    }
    mac_HiliteMenu(0);
}

/* fn_S8_2C36 with the command key: the menus brought up to date (fn_S8_2E10), then MenuKey finds the
 * enabled item with that key equivalent */
void shell_menu_key(uint32_t ev) {
    fn_S8_2E10();
    dispatch(mac_MenuKey((int16_t)(int8_t)r8(ev + 5)));
}

/* fn_S8_2BA8: a mouse-down: FindWindow; on the menu bar the menus are brought up to date
 * (fn_S8_2E10) and MenuSelect's choice runs; in the high-score window's content, fn_S8_2D26. A desk
 * accessory (SystemClick) and the window frame (DragWindow) have nothing to do in the port. */
void shell_mouse_down(uint32_t ev) {
    MZ_FRAME;
    uint32_t pw = mz_local(4);
    int16_t part = mac_FindWindow(r32(ev + 10), pw);
    if (part == 1) { fn_S8_2E10(); dispatch(mac_MenuSelect(r32(ev + 10))); }
    else if (part == 3) fn_S8_2D26(r32(pw), ev);
}

/* fn_S8_3B6E: the 'quit' Apple event's handler (Original: fn_S8_36F8, G_580E); the others ('oapp',
 * 'odoc', 'pdoc') do nothing here */
void shell_high_level_event(uint32_t ev) {
    if (r32(ev + 2) == FOURCC('a','e','v','t') && r32(ev + 10) == FOURCC('q','u','i','t')) sg8(0x223C, 1);
}
