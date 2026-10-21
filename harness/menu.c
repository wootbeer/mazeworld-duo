/* Menu Manager, as much as the game uses: the menu bar from 'MBAR' / 'MENU' resources, enabling,
 * disabling and checking items, and MenuKey (command-key equivalents). Nothing is drawn: the test rig
 * has no menu bar on screen (the port has a native one).
 *
 * A menu is its 'MENU' resource handle, as on a Mac: menuID +0, enableFlags +10 (bit 0 the whole
 * menu, bit n item n), the title (Pascal string) at +14, then the items: Pascal string, icon, key
 * equivalent, mark, style; a zero byte ends them. The menu list (GetNewMBar's result) is a handle
 * holding the count and then the menu handles. */
#include "hx.h"
#include "m68k.h"

#define A7 M68K_REG_A7
#define ARGW()   ((int16_t)pop16())
#define ARGL()   (pop32())
#define RETL(v)  wr32(reg(A7), (uint32_t)(v))

typedef void (*TrapFn)(uint16_t w);
void traps_register_tb(uint16_t n, TrapFn f);

static uint32_t menu_list;   /* the current menu bar (SetMenuBar) */

static int nmenus(uint32_t list) { return list ? (int16_t)rd16(rd32(list)) : 0; }
static uint32_t menu_at(uint32_t list, int i) { return rd32(rd32(list) + 2 + 4u * (uint32_t)i); }

/* the address of item n's 4 bytes after its text (icon, key, mark, style), or 0 */
static uint32_t item_info(uint32_t menu, int n) {
    uint32_t p = rd32(menu);
    p += 14 + 1 + rd8(p + 14);                               /* past the title */
    for (int i = 1; rd8(p); i++) {
        uint32_t info = p + 1 + rd8(p);
        if (i == n) return info;
        p = info + 4;
    }
    return 0;
}

static void t_GetNewMBar(uint16_t w) {
    (void)w; int16_t id = ARGW();
    uint32_t mbar = rs_get(FOURCC('M','B','A','R'), id, 0);
    if (!mbar) { RETL(0); return; }
    int n = (int16_t)rd16(rd32(mbar));
    uint32_t list = mm_new_handle(2 + 4u * (uint32_t)n, 1);
    wr16(rd32(list), (uint16_t)n);
    for (int i = 0; i < n; i++) {
        int16_t mid = (int16_t)rd16(rd32(mbar) + 2 + 2u * (uint32_t)i);
        wr32(rd32(list) + 2 + 4u * (uint32_t)i, rs_get(FOURCC('M','E','N','U'), mid, 0));
    }
    RETL(list);
}
static void t_SetMenuBar(uint16_t w) { (void)w; menu_list = ARGL(); }
static void t_GetMenuHandle(uint16_t w) {
    (void)w; int16_t id = ARGW();
    for (int i = 0; i < nmenus(menu_list); i++) {
        uint32_t m = menu_at(menu_list, i);
        if (m && (int16_t)rd16(rd32(m)) == id) { RETL(m); return; }
    }
    RETL(0);
}
static void set_enabled(uint32_t menu, int item, int on) {
    uint32_t f = rd32(menu) + 10, bit = item < 32 ? 1u << item : 0;
    wr32(f, on ? (rd32(f) | bit) : (rd32(f) & ~bit));
}
static void t_EnableItem(uint16_t w)  { (void)w; int16_t item = ARGW(); uint32_t m = ARGL(); if (m) set_enabled(m, item, 1); }
static void t_DisableItem(uint16_t w) { (void)w; int16_t item = ARGW(); uint32_t m = ARGL(); if (m) set_enabled(m, item, 0); }
static void t_CheckItem(uint16_t w) {
    (void)w; uint16_t on = pop16(); int16_t item = ARGW(); uint32_t m = ARGL();
    uint32_t info = m ? item_info(m, item) : 0;
    if (info) wr8(info + 2, (on >> 8) ? 0x12 : 0);             /* checkMark */
}
/* MenuKey(ch): the enabled item whose key equivalent is ch (letters in either case), as
 * menuID << 16 | item, or 0 */
static void t_MenuKey(uint16_t w) {
    (void)w; uint8_t ch = (uint8_t)pop16();
    if (ch >= 'a' && ch <= 'z') ch = (uint8_t)(ch - 32);
    for (int i = 0; i < nmenus(menu_list); i++) {
        uint32_t m = menu_at(menu_list, i); if (!m) continue;
        uint32_t flags = rd32(rd32(m) + 10);
        if (!(flags & 1)) continue;
        for (int n = 1; ; n++) {
            uint32_t info = item_info(m, n); if (!info) break;
            uint8_t k = rd8(info + 1);
            if (k >= 'a' && k <= 'z') k = (uint8_t)(k - 32);
            if (k && k == ch && n < 32 && (flags & (1u << n))) { RETL((uint32_t)rd16(rd32(m)) << 16 | (uint32_t)n); return; }
        }
    }
    RETL(0);
}
static void t_HiliteMenu(uint16_t w) { (void)w; ARGW(); }

void menu_register(void) {
    traps_register_tb(0x1C0, t_GetNewMBar); traps_register_tb(0x13C, t_SetMenuBar);
    traps_register_tb(0x149, t_GetMenuHandle); traps_register_tb(0x139, t_EnableItem);
    traps_register_tb(0x13A, t_DisableItem); traps_register_tb(0x145, t_CheckItem);
    traps_register_tb(0x13E, t_MenuKey); traps_register_tb(0x138, t_HiliteMenu);
}
