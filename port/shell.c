/* The port's shell: what the original's menus do (docs/SHELL.md), for the native menu bar.
 *
 * Menu commands reach the game the way a Mac's do: as an event the shell loop (fn_S8_2A4A) takes
 * from the event queue. A command chosen in the native menu is posted as a mouse-down event that
 * carries the menu and item (the original's MenuSelect result); a command-key press arrives as a
 * key event (fn_S8_2C36) and is looked up like the original's MenuKey. Either way the command then
 * runs inside the shell loop, through the game's own handlers (fn_S8_3344, fn_S8_338A, ...), so the
 * game is exactly as it would be after the same choice on a Mac.
 *
 * Before a command the game's own menu records are brought up to date (fn_S8_2E10, as the original
 * does) and only an item enabled there runs. The native menu shows the same state, computed by
 * shell_item_state from the same rules (fn_S8_2E4C / 2FB4 / 3190 / 3278) without touching the game.
 * The Options dialog (fn_S8_1FC4, "More Power B'Elanna!") becomes three check-mark items in the
 * Options menu, with the same effect as its check boxes and OK. */
#include "../harness/hx.h"
#include "../game/mz.h"
#include "../game/fns.h"
#include "shell.h"

uint32_t qd_front_window(void);
void input_live_event(uint16_t what, uint32_t msg, uint32_t where);

#define MENU_CMD 0x80000000u   /* marks a mouse-down event as a native menu command */

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

int shell_item_state(int menu, int item, int *checked) {
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
    if (menu == MENU_FILE && item == 7) { shell_request_quit(); return; }
    input_live_event(1, MENU_CMD | (uint32_t)menu << 16 | (uint32_t)item, 0xFFFFFFFFu);
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

/* is the item enabled in the game's MENU record (what MenuSelect would allow)? */
static int record_enabled(int menu, int item) {
    uint32_t m = mac_GetMenuHandle((int16_t)menu);
    if (!m) return 0;
    uint32_t flags = r32(deref(m) + 10);
    if (menu == MENU_OPTIONS) item = 1;                     /* the dialog's item stands for its check boxes */
    return (flags & 1) && item < 32 && (flags >> item & 1);
}

/* fn_S8_2C36 with the command key: the menus brought up to date (fn_S8_2E10), then MenuKey finds the
 * enabled item with that key equivalent */
void shell_menu_key(uint32_t ev) {
    fn_S8_2E10();
    dispatch(mac_MenuKey((int16_t)(int8_t)r8(ev + 5)));
}

/* fn_S8_2BA8: a mouse-down: a native menu command, or a click in the high-score window's content
 * (fn_S8_2D26). Its frame (inDrag: DragWindow) and the rest of the screen do nothing in the port. */
void shell_mouse_down(uint32_t ev) {
    uint32_t msg = r32(ev + 2);
    if (msg & MENU_CMD) {                                   /* as MenuSelect: after fn_S8_2E10, enabled items only */
        int menu = (int)(msg >> 16 & 0x7FFF), item = (int)(msg & 0xFFFF);
        fn_S8_2E10();
        dispatch(record_enabled(menu, item) ? (uint32_t)menu << 16 | (uint32_t)item : 0);
        return;
    }
    uint32_t w = qd_front_window();
    if (!w || w != g32(0x2232)) return;
    int16_t v = (int16_t)r16(ev + 10), h = (int16_t)r16(ev + 12);
    /* FindWindow's inContent: the window's content region (global coordinates), its bounding box */
    uint32_t box = deref(r32(w + 0x76)) + 2;
    int16_t top = (int16_t)r16(box), left = (int16_t)r16(box + 2), bottom = (int16_t)r16(box + 4), right = (int16_t)r16(box + 6);
    if (v >= top && v < bottom && h >= left && h < right) fn_S8_2D26(w, ev);
}

/* fn_S8_3B6E: the 'quit' Apple event's handler; the others ('oapp', 'odoc') do nothing here */
void shell_high_level_event(uint32_t ev) {
    if (r32(ev + 2) == FOURCC('a','e','v','t') && r32(ev + 10) == FOURCC('q','u','i','t')) sg8(0x223C, 1);
}
