/* CODE segment 8: windows, panels and screens drawn with the Toolbox (see build/disasm/seg8.s). */
#include "mz.h"
#include "fns.h"

static void set_box(uint32_t r, int16_t top, int16_t left, int16_t bottom, int16_t right) {
    w16(r, (uint16_t)top); w16(r + 2, (uint16_t)left); w16(r + 4, (uint16_t)bottom); w16(r + 6, (uint16_t)right);
}
static void box_cols(uint32_t r, int16_t left, int16_t right) { w16(r + 2, (uint16_t)left); w16(r + 6, (uint16_t)right); }

/* a number in `box` (TETextBox justification just); s: a Str255 to build it in */
static void box_number(int32_t n, uint32_t box, int16_t just, uint32_t s) {
    mz_seg_of(3, 0x038E), fn_S3_038E(n, s);
    mac_TETextBox(s + 1, r8(s), box, just);
}
/* a C string in `box` */
static void box_cstring(uint32_t text, uint32_t box, int16_t just) {
    mac_TETextBox(text, (int32_t)(mz_seg_of(2, 0x0252), fn_S2_0252(text)), box, just);
}

enum { JUST_LEFT = 0, JUST_CENTER = 1, JUST_RIGHT = -1 };

/* fn_S8_3FB2: the high-score table at (G_210C, G_210E) (left, top), 12-point text in colour 15 on
 * 0: the title (C string G_47A4, copied), column heads (G_3E76 / G_3E70 / G_3E6A: name, level,
 * score), a rule, the ten entries of G_2242 ($46 bytes each from +$118: Pascal name, level word at
 * +$158, score long at +$15A; the one just entered, G_22A6, marked when G_2388), then this game's
 * line: G_3E62 with level G_20EC and score G_20E6, or (game over, G_21B4 / G_21B3) G_3E50 with the
 * entered level G_2264 and score G_2262 when G_2388 (marked), else G_3E44 / G_3E42. A second rule
 * and, in 9-point, the C string G_4AD2 below. */
void fn_S8_3FB2(void) {
    MZ_FRAME;
    uint32_t title = mz_local(12), box = mz_local(8), num = mz_local(0x100);
    for (uint32_t k = 0; k < 12; k += 4) w32(title + k, r32(GADDR(0x47A4) + k));
    mac_HLock(g32(0x2242));
    uint32_t tbl = deref(g32(0x2242));
    mac_PmForeColor(15); mac_PmBackColor(0); mac_TextSize(12);
    int16_t top = (int16_t)g16(0x210E), left = (int16_t)g16(0x210C);

    set_box(box, top, left, (int16_t)(top + 15), (int16_t)(left + 0x136));
    box_cstring(title, box, JUST_CENTER);

    set_box(box, (int16_t)(top + 0x19), left, (int16_t)(top + 0x19 + 15), (int16_t)(left + 0xCD));
    mac_TETextBox(GADDR(0x3E76), 4, box, JUST_LEFT);
    box_cols(box, (int16_t)(left + 0xCD), (int16_t)(left + 0xF5));
    mac_TETextBox(GADDR(0x3E70), 5, box, JUST_CENTER);
    box_cols(box, (int16_t)(left + 0xF5), (int16_t)(left + 0x136));
    mac_TETextBox(GADDR(0x3E6A), 5, box, JUST_RIGHT);
    mac_MoveTo(left, (int16_t)(top + 0x28)); mac_Line(0x136, 0);

    for (int16_t i = 0; i < 10; i++) {
        int16_t t = (int16_t)(15 * i + top + 0x2D);
        w16(box, (uint16_t)t); w16(box + 4, (uint16_t)(t + 15));
        if (g8(0x2388) && (int16_t)g16(0x22A6) == i) {
            box_cols(box, (int16_t)(left - 10), left);
            mac_TETextBox(GADDR(0x3E64), 1, box, JUST_CENTER);
        }
        uint32_t e = tbl + (uint32_t)(int32_t)(0x46 * i);
        box_cols(box, left, (int16_t)(left + 0xCD));
        mac_TETextBox(e + 0x119, r8(e + 0x118), box, JUST_LEFT);
        box_cols(box, (int16_t)(left + 0xCD), (int16_t)(left + 0xF5));
        box_number((int16_t)r16(e + 0x158), box, JUST_CENTER, num);
        box_cols(box, (int16_t)(left + 0xF5), (int16_t)(left + 0x136));
        box_number((int32_t)r32(e + 0x15A), box, JUST_RIGHT, num);
    }

    set_box(box, (int16_t)(top + 0xC9), left, (int16_t)(top + 0xC9 + 15), (int16_t)(left + 0xCD));
    if (!g8(0x21B4) && !g8(0x21B3)) {
        mac_TETextBox(GADDR(0x3E62), 0x10, box, JUST_LEFT);
        box_cols(box, (int16_t)(left + 0xCD), (int16_t)(left + 0xF5));
        box_number((int16_t)g16(0x20EC), box, JUST_CENTER, num);
        box_cols(box, (int16_t)(left + 0xF5), (int16_t)(left + 0x136));
        box_number((int32_t)g32(0x20E6), box, JUST_RIGHT, num);
    } else {
        mac_TETextBox(GADDR(0x3E50), 0xB, box, JUST_LEFT);
        box_cols(box, (int16_t)(left + 0xCD), (int16_t)(left + 0xF5));
        if (g8(0x2388)) box_number((int16_t)g16(0x2264), box, JUST_CENTER, num);
        else mac_TETextBox(GADDR(0x3E44), 1, box, JUST_CENTER);
        box_cols(box, (int16_t)(left + 0xF5), (int16_t)(left + 0x136));
        if (g8(0x2388)) box_number((int32_t)g32(0x2262), box, JUST_RIGHT, num);
        else mac_TETextBox(GADDR(0x3E42), 1, box, JUST_RIGHT);
        if (g8(0x2388)) {
            box_cols(box, (int16_t)(left - 10), left);
            mac_TETextBox(GADDR(0x3E40), 1, box, JUST_CENTER);
        }
    }
    mac_MoveTo(left, (int16_t)(top + 0xDC)); mac_Line(0x136, 0);
    mac_TextSize(9);
    set_box(box, (int16_t)(top + 0xE5), left, (int16_t)(top + 0xE5 + 0x3C), (int16_t)(left + 0x136));
    box_cstring(GADDR(0x4AD2), box, JUST_LEFT);
    mac_HUnlock(g32(0x2242));
}

/* One offscreen GWorld *pgw of size bounds at the depth (and colour table) of device gdh, 8 bits for
 * a depth QuickDraw does not support: made, or updated if it exists; on failure it is disposed of
 * and cleared. A good one gets the game's palette G_2224 and is erased to colour 0. Returns the
 * error. */
static int16_t make_gworld(uint32_t pgw, uint32_t gdh, uint32_t bounds) {
    MZ_FRAME;
    uint32_t pm = deref(r32(deref(gdh) + 0x16));
    uint32_t ctab = r32(pm + 0x2A);
    int16_t depth = (int16_t)r16(pm + 0x20);
    if (depth != 8 && depth != 4 && depth != 16 && depth != 32 && depth != 1) { depth = 8; ctab = 0; }
    int16_t err;
    if (!r32(pgw)) err = mac_NewGWorld(pgw, depth, bounds, ctab, 0, 0);
    else {
        uint32_t flags = mac_UpdateGWorld(pgw, depth, bounds, ctab, 0, 0);
        err = (flags & 0x80000000u) ? (int16_t)(flags & 0xFFFF) : 0;   /* gwFlagErr: the error in the low word */
    }
    if (!r32(pgw) && !err) err = -108;                     /* memFullErr */
    if (err) {
        if (r32(pgw)) mac_DisposeGWorld(r32(pgw));
        w32(pgw, 0);
    }
    if (r32(pgw)) {
        mac_NSetPalette(r32(pgw), g32(0x2224), 0x100);
        uint32_t px = mac_GetGWorldPixMap(r32(pgw));
        if (mac_LockPixels(px)) {
            uint32_t saved = mz_local(8), r = mz_local(8);
            mac_GetGWorld(saved, saved + 4);
            mac_SetGWorld(r32(pgw), 0);
            w32(r, r32(r32(pgw) + 0x10)); w32(r + 4, r32(r32(pgw) + 0x14));   /* portRect */
            mac_ClipRect(r);
            mac_PmBackColor(0);
            mac_EraseRect(r);
            mac_SetGWorld(r32(saved), r32(saved + 4));
            mac_UnlockPixels(px);
        }
    }
    return err;
}

/* fn_S8_4D68: (re)makes the offscreen worlds for device gdh: the 3D view *pview (G_2156 x G_2150)
 * and the automap *pmap (160 x 160). If either cannot be made, both are disposed of. Returns the
 * error. */
int16_t fn_S8_4D68(uint32_t pview, uint32_t pmap, uint32_t gdh) {
    MZ_FRAME;
    uint32_t bounds = mz_local(8);
    w16(bounds, 0); w16(bounds + 2, 0); w16(bounds + 4, g16(0x2150)); w16(bounds + 6, g16(0x2156));
    int16_t err = make_gworld(pview, gdh, bounds);
    if (!r32(pview)) {
        if (r32(pmap)) mac_DisposeGWorld(r32(pmap));
        w32(pmap, 0);
        return err;
    }
    w16(bounds + 4, 0xA0); w16(bounds + 6, 0xA0);
    err = make_gworld(pmap, gdh, bounds);
    if (!r32(pmap)) {
        if (r32(pview)) mac_DisposeGWorld(r32(pview));
        w32(pview, 0);
    }
    return err;
}

/* fn_S8_50FC: disposes of both offscreen worlds (*pmap first) and clears them. */
void fn_S8_50FC(uint32_t pview, uint32_t pmap) {
    if (r32(pmap)) mac_DisposeGWorld(r32(pmap));
    w32(pmap, 0);
    if (r32(pview)) mac_DisposeGWorld(r32(pview));
    w32(pview, 0);
}

/* ---------- start-up: memory and resources ---------- */

/* A detached resource 'mazr' id into global g: the Resource Manager error, 2 if missing, 0 if loaded */
static int16_t load_mazr(int16_t id, uint32_t g) {
    sg32(g, mac_GetResource(0x6D617A72, id));               /* 'mazr' */
    int16_t err = mac_ResError();
    if (err) return err;
    if (!g32(g)) return 2;
    mac_DetachResource(g32(g));
    return mac_ResError();
}

/* fn_S8_04FA: allocates the renderer's buffers (handles G_1F08 ... G_1EC0, the tables G_1EF8 /
 * G_1EF4 / G_1EF0, G_1EE0 and G_1ED8) and loads the 'mazr' resources 128-134 (G_1EEC, G_1EE8, G_1EE4,
 * G_1ED4, G_1ED0, G_1ECC, G_1EC8), then clears the 50 longs G_1EBC[]. Returns 1 if memory runs out,
 * a resource error, or 0. */
int16_t fn_S8_04FA(void) {
    static const struct { uint16_t g; int32_t size; } bufs[] = {
        {0x1F08, 1}, {0x1F04, 1}, {0x1F00, 1}, {0x1EFC, 1}, {0x1EDC, 1}, {0x1EC4, 1}, {0x1EC0, 1},
        {0x1EF8, 0x6600}, {0x1EF4, 0x19800}, {0x1EF0, 0x6600},
    };
    for (unsigned k = 0; k < sizeof bufs / sizeof *bufs; k++) {
        sg32(bufs[k].g, mac_NewHandle(bufs[k].size));
        if (!g32(bufs[k].g)) return 1;
    }
    int16_t err;
    if ((err = load_mazr(0x80, 0x1EEC))) return err;
    if ((err = load_mazr(0x81, 0x1EE8))) return err;
    if ((err = load_mazr(0x82, 0x1EE4))) return err;
    sg32(0x1EE0, mac_NewHandle(0x8000)); if (!g32(0x1EE0)) return 1;
    sg32(0x1ED8, mac_NewHandle(mz_original ? 0x1E00 : 0x3200)); if (!g32(0x1ED8)) return 1;   /* Original: $1E00 */
    if ((err = load_mazr(0x83, 0x1ED4))) return err;
    if ((err = load_mazr(0x84, 0x1ED0))) return err;
    if ((err = load_mazr(0x85, 0x1ECC))) return err;
    if ((err = load_mazr(0x86, 0x1EC8))) return err;
    for (int16_t i = 0; i < 0x32; i++) w32(GADDR(0x1EBC) + 4u * (uint32_t)i, 0);
    return 0;
}

/* ---------- screens and windows ---------- */

enum { gdScreenActive = 13, gdMainScreen = 15 };      /* TestDeviceAttribute bits the game checks */
static int usable_screen(uint32_t gd) { return mac_TestDeviceAttribute(gd, gdScreenActive) && mac_TestDeviceAttribute(gd, gdMainScreen); }
static uint32_t gd_rect(uint32_t gd) { return deref(gd) + 0x22; }

/* fn_S8_606C: the screen showing most of global rectangle r (the main device if none does) */
uint32_t fn_S8_606C(uint32_t r) {
    MZ_FRAME;
    uint32_t best = 0, sect = mz_local(8);
    int32_t best_area = 0;
    for (uint32_t gd = mac_GetDeviceList(); gd; gd = mac_GetNextDevice(gd)) {
        if (!usable_screen(gd) || !mac_SectRect(gd_rect(gd), r, sect)) continue;
        int32_t area = (int32_t)(int16_t)(r16(sect + 4) - r16(sect)) * (int16_t)(r16(sect + 6) - r16(sect + 2));
        if (area > best_area) { best_area = area; best = gd; }
    }
    return best ? best : mac_GetMainDevice();
}

/* fn_S8_610C: the screen whose rectangle is exactly r, or 0 */
uint32_t fn_S8_610C(uint32_t r) {
    for (uint32_t gd = mac_GetDeviceList(); gd; gd = mac_GetNextDevice(gd)) {
        if (!usable_screen(gd)) continue;
        uint32_t g = gd_rect(gd);
        if (r16(g + 2) == r16(r + 2) && r16(g + 6) == r16(r + 6) && r16(g) == r16(r) && r16(g + 4) == r16(r + 4)) return gd;
    }
    return 0;
}

/* fn_S8_513C: the largest view size any screen allows: 0 (under 352x320), 2 (464x384), 3 (640x480)
 * or 4. Original: 0 (under 240x200), 1 (352x270), 2 (512x384), 3 (640x480) or 4. */
int16_t fn_S8_513C(void) {
    int16_t best = 0;
    for (uint32_t gd = mac_GetDeviceList(); gd; gd = mac_GetNextDevice(gd)) {
        if (!usable_screen(gd)) continue;
        uint32_t g = gd_rect(gd);
        int16_t w = (int16_t)(r16(g + 6) - r16(g + 2)), h = (int16_t)(r16(g + 4) - r16(g));
        int16_t size = (w < 0x160 || h < 0x140) ? 0 : (w < 0x1D0 || h < 0x180) ? 2 : (w < 0x280 || h < 0x1E0) ? 3 : 4;
        if (mz_original)                                    /* Original (fn_S8_45C4): five sizes */
            size = (w < 0xF0 || h < 0xC8) ? 0 : (w < 0x160 || h < 0x10E) ? 1 : (w < 0x200 || h < 0x180) ? 2 : (w < 0x280 || h < 0x1E0) ? 3 : 4;
        if (size > best) best = size;
    }
    return best;
}

/* fn_S8_61AA: reserves the memory the game will need (200000 bytes less the graphics loaded, G_22AA)
 * as handle G_3C9C, so it is known to be there. Returns 1, or 0 (with an alert if the budget is
 * out of range). */
uint8_t fn_S8_61AA(void) {
    int32_t n = 200000 - (int32_t)g32(0x22AA);
    if (n <= 0) { mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3AA2), 0, 0); return 0; }
    if (n > 200000) { mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3A8A), 0, 0); return 0; }
    sg32(0x3C9C, mac_NewHandle(n));
    return g32(0x3C9C) ? 1 : 0;
}

/* fn_S8_4CF0: *pgd = the screen showing most of window G_2232 (its title bar included); fatal alert
 * G_3AFE if G_22BC is then empty. */
void fn_S8_4CF0(uint32_t pgd) {
    MZ_FRAME;
    uint32_t w = g32(0x2232), b = deref(r32(w + 2)) + 6;   /* portPixMap bounds */
    uint32_t r = mz_local(8);
    w32(r, r32(w + 0x10)); w32(r + 4, r32(w + 0x14));
    w16(r, (uint16_t)(r16(r) - r16(b)));         w16(r + 4, (uint16_t)(r16(r + 4) - r16(b)));
    w16(r + 2, (uint16_t)(r16(r + 2) - r16(b + 2))); w16(r + 6, (uint16_t)(r16(r + 6) - r16(b + 2)));
    w16(r, (uint16_t)(r16(r) - 0x14));
    w32(pgd, fn_S8_606C(r));
    if (!g32(0x22BC)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3AFE), 0, 1);
}

/* fn_S8_4BE2: centres window G_2232 (G_2166 x G_2164) on screen G_22BC, x on a multiple of 8; with
 * `keep_visible`, below the menu bar (20 pixels, plus the menu bar's height on the main screen) and
 * not off the left. The offset is kept in G_2162 / G_2160. Returns whether it fits on the screen. */
uint8_t fn_S8_4BE2(uint8_t keep_visible) {
    uint32_t g = gd_rect(g32(0x22BC));
    int16_t top = (int16_t)r16(g), left = (int16_t)r16(g + 2), bottom = (int16_t)r16(g + 4), right = (int16_t)r16(g + 6);
    int16_t x = (int16_t)((int16_t)(right - left - g16(0x2166)) / 2);
    int16_t y = (int16_t)((int16_t)(bottom - top - g16(0x2164)) / 2);
    x = mz_original ? (int16_t)((x >> 2) << 2) : (int16_t)((x >> 3) << 3);   /* Original (fn_S8_4322): multiple of 4 */
    if (keep_visible) {
        int16_t min_y = mac_GetMainDevice() == g32(0x22BC) ? (int16_t)(0x14 + r16(0x0BAA)) : 0x14;   /* MBarHeight */
        if (y < min_y) y = min_y;
        if (x < 0) x = 0;
    }
    int16_t wl = (int16_t)(left + x), wt = (int16_t)(top + y);
    int16_t wr = (int16_t)(wl + g16(0x2166)), wb = (int16_t)(wt + g16(0x2164));
    mac_MoveWindow(g32(0x2232), wl, wt, 0);
    sg16(0x2162, (uint16_t)x); sg16(0x2160, (uint16_t)y);
    return wl >= left && wr <= right && wt >= top && wb <= bottom;
}

/* fn_S8_486C: the number of Toolbox trap entries: $200 on an old 64K-ROM-style table (_InitGraf and
 * _ReadDateTime's Toolbox twin at the same address), else $400 */
int16_t fn_S8_486C(void) {
    mz_seg(3);
    return fn_S3_021C((int16_t)0xA86E, 1) == fn_S3_021C((int16_t)0xAA6E, 1) ? 0x200 : 0x400;
}

/* fn_S8_4802: whether trap `trap` is implemented (its address differs from _Unimplemented's) */
uint8_t fn_S8_4802(int16_t trap) {
    int16_t type = fn_S8_4858((int8_t)((uint16_t)trap >> 8));
    if ((type & 0xFF) == 1) {                               /* ToolTrap */
        int16_t n = fn_S8_486C();
        trap = (int16_t)(trap & 0x7FF);
        if (!(n > trap)) return 0;
    }
    mz_seg(3);
    uint32_t unimpl = fn_S3_021C((int16_t)0xA89F, 1);
    return fn_S3_021C(trap, (uint8_t)type) != unimpl;
}

/* fn_S8_47E0 (JT 14): the slot VBL task (A0 = the task): counts the screen's refreshes in G_21AE and
 * re-arms itself. */
void fn_S8_47E0(Regs *r) {
    uint32_t n = g32(0x21AE);
    sg32(0x21AE, n + 1);
    w16(r->a[0] + 0xA, 1);
    r->ccr = n == 0xFFFFFFFFu ? CCR_X : 0;                  /* addq.l's X; move.w #1 clears the rest */
}

/* the slot of screen G_22BC (its driver's dCtlSlot) */
static int16_t screen_slot(void) {
    uint32_t dce = (mz_seg_of(3, 0x0230), fn_S3_0230((int16_t)r16(deref(g32(0x22BC)))));
    return (int8_t)r8(deref(dce) + 0x28);
}

/* fn_S8_471C: installs the slot VBL task G_21AA (fn_S8_47E0, every refresh) for screen G_22BC when
 * SlotVInstall exists; G_22B7 = whether it is running. */
void fn_S8_471C(void) {
    if (!fn_S8_4802((int16_t)0xA06F)) { sg8(0x22B7, 0); return; }
    int16_t slot = screen_slot();
    sg16(0x21A6, 1);                                        /* qType */
    sg32(0x21A4, mz_jt(0x92));                             /* vblAddr: fn_S8_47E0's jump-table entry */
    sg16(0x21A0, 1); sg16(0x219E, 0);                       /* vblCount, vblPhase */
    sg32(0x219C, MZ_A5);
    sg8(0x22B7, mac_SlotVInstall(GADDR(0x21AA), slot) == 0);
}

/* fn_S8_4790: removes the slot VBL task (fatal alert G_3CC8 on failure). */
void fn_S8_4790(void) {
    if (!g8(0x22B7)) return;
    int16_t err = mac_SlotVRemove(GADDR(0x21AA), screen_slot());
    if (err) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3CC8), err, 1);
}

/* fn_S8_4526: opens the full-screen game window G_222E ('WIND' 129, storage G_2236) on screen G_22BC,
 * hiding the menu bar (its height kept in G_2226, the screen's menu bar strip added to the window's
 * visible region), black. Fatal alert G_3D18 if it cannot. */
void fn_S8_4526(void) {
    MZ_FRAME;
    sg16(0x2226, r16(0x0BAA)); w16(0x0BAA, 0);
    sg32(0x222E, mac_GetNewCWindow(0x81, g32(0x2236), 0xFFFFFFFFu));
    if (!g32(0x222E)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3D18), 0, 1);
    uint32_t g = gd_rect(g32(0x22BC)), w = g32(0x222E);
    int16_t top = (int16_t)r16(g), left = (int16_t)r16(g + 2), bottom = (int16_t)r16(g + 4), right = (int16_t)r16(g + 6);
    mac_SizeWindow(w, (int16_t)(right - left), (int16_t)(bottom - top), 1);
    mac_MoveWindow(w, left, top, 1);
    mac_SetPort(w); mac_PmBackColor(0); mac_ShowWindow(w);
    uint32_t rgn = mac_NewRgn(), r = mz_local(8);
    /* the menu bar: QuickDraw's screenBits.bounds (top, left, right) with the saved height */
    mac_SetRect(r, (int16_t)g16(0x76), (int16_t)g16(0x78), (int16_t)g16(0x72), (int16_t)(g16(0x78) + g16(0x2226)));
    mac_RectRgn(rgn, r);
    mac_UnionRgn(r32(w + 0x18), rgn, r32(w + 0x18));        /* visRgn */
    mac_DisposeRgn(rgn);
    mac_SetPort(w); mac_PmForeColor(0);
    mac_PaintRect(w + 0x10);
}

/* fn_S8_4620: covers every other active screen with a black window ('WIND' 129), chained through
 * their refCons from G_222A (fatal alert G_3CF2 if one cannot be made). */
void fn_S8_4620(void) {
    uint32_t prev = 0;
    for (uint32_t gd = mac_GetDeviceList(); gd; gd = mac_GetNextDevice(gd)) {
        if (!mac_TestDeviceAttribute(gd, gdScreenActive) || r32(gd) == r32(g32(0x22BC))) continue;
        uint32_t w = mac_GetNewCWindow(0x81, 0, 0xFFFFFFFFu);
        if (!w) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3CF2), 0, 1);
        uint32_t g = gd_rect(gd);
        int16_t top = (int16_t)r16(g), left = (int16_t)r16(g + 2), bottom = (int16_t)r16(g + 4), right = (int16_t)r16(g + 6);
        mac_SizeWindow(w, (int16_t)(right - left), (int16_t)(bottom - top), 0);
        mac_MoveWindow(w, left, top, 1);
        mac_ShowWindow(w);
        mac_SetWRefCon(w, prev);
        prev = w;
    }
    sg32(0x222A, prev);
}

/* the window's portRect, cleared to palette entry 0 (black) */
static void paint_port(uint32_t w, uint32_t r) {
    w32(r, r32(w + 0x10)); w32(r + 4, r32(w + 0x14));
    mac_ClipRect(r); mac_PmForeColor(0); mac_PaintRect(r);
}

/* fn_S8_3F6A: update of the high-score window G_2232: black, then the table (fn_S8_3FB2). */
void fn_S8_3F6A(void) {
    MZ_FRAME;
    uint32_t w = g32(0x2232), r = mz_local(8);
    mac_BeginUpdate(w); mac_SetPort(w);
    paint_port(w, r);
    fn_S8_3FB2();
    mac_EndUpdate(w);
}

/* fn_S8_2CAC: an update event for window w: the high-score window is redrawn, others just validated. */
void fn_S8_2CAC(uint32_t w) {
    if (w == g32(0x2232)) fn_S8_3F6A();
    else { mac_BeginUpdate(w); mac_EndUpdate(w); }
}

/* fn_S8_3E88: asks for the player's name (dialog 301: item 1 OK, enabled once the edit text, item 2,
 * is not empty) and stores it (at most 63 characters) as a Pascal string at `name` (empty if the
 * dialog cannot be shown). */
void fn_S8_3E88(uint32_t name) {
    MZ_FRAME;
    uint32_t saved = mz_local(4), type = mz_local(2), edit = mz_local(4), ok = mz_local(4), box = mz_local(8);
    uint32_t item = mz_local(2), text = mz_local(0x100);
    mac_GetPort(saved);
    uint32_t dlg = mac_GetNewDialog(0x12D, 0, 0xFFFFFFFFu);
    if (!dlg) { w8(name, 0); return; }
    mac_SetPort(dlg); mac_ShowWindow(dlg);
    mac_GetDialogItem(dlg, 2, type, edit, box);
    mac_GetDialogItem(dlg, 1, type, ok, box);
    for (int done = 0; !done; ) {
        mac_GetDialogItemText(r32(edit), text);
        mac_HiliteControl(r32(ok), r8(text) ? 0 : 0xFF);
        mac_ModalDialog(0, item);
        if ((int16_t)r16(item) == 1) done = 1;
    }
    mac_GetDialogItemText(r32(edit), text);
    if (r8(text) > 0x3F) w8(text, 0x3F);
    mz_seg_of(2, 0x0332), fn_S2_0332(text, name);
    mac_SetPort(r32(saved));
    mac_DisposeDialog(dlg);
}

/* fn_S8_3D66: game over: if the score G_20E6 beats an entry of the high-score table G_2242, asks
 * for a name (fn_S8_3E88) and inserts it there (G_20EC the level); G_2388 / G_22A6 (the place, or
 * -1) / G_2262 / G_2264 for the table's display, which is invalidated. */
void fn_S8_3D66(void) {
    MZ_FRAME;
    mac_HLock(g32(0x2242));
    uint32_t tbl = deref(g32(0x2242));
#define ENTRY(i) (tbl + (uint32_t)(int32_t)(0x46 * (i)))
    int16_t i = 0;
    while (i < 10 && (int32_t)r32(ENTRY(i) + 0x15A) >= (int32_t)g32(0x20E6)) i++;
    int16_t place = i < 10 ? i : -1;
    if (place >= 0) {
        uint32_t name = mz_local(0x50);
        fn_S8_3E88(name);
        for (int16_t k = 9; k > place; k--)
            for (uint32_t b = 0; b < 0x46; b += 2) w16(ENTRY(k) + 0x118 + b, r16(ENTRY(k - 1) + 0x118 + b));
        mz_seg_of(2, 0x0332), fn_S2_0332(name, ENTRY(place) + 0x118);
        w32(ENTRY(place) + 0x15A, g32(0x20E6));
        w16(ENTRY(place) + 0x158, g16(0x20EC));
    }
#undef ENTRY
    sg8(0x2388, 1);
    sg16(0x22A6, (uint16_t)place);
    sg32(0x2262, g32(0x20E6)); sg16(0x2264, g16(0x20EC));
    uint32_t r = mz_local(8);
    mac_SetPort(g32(0x2232));
    w32(r, r32(g32(0x2232) + 0x10)); w32(r + 4, r32(g32(0x2232) + 0x14));
    mac_InvalRect(r);
    mac_HUnlock(g32(0x2242));
}

/* fn_S8_3C0C: starts playing from the high-score window: finds its screen (fn_S8_4CF0), makes the
 * offscreen worlds (alert G_403C if it cannot), and, unless G_21B3, checks the screen (depth: alert
 * G_4004 but go on; size at least G_2166 x G_2164: else alert G_3F10 and stop). Then the game
 * window, black windows on the other screens and the slot VBL task, plays (fn_S8_0000), and puts it
 * all back; after a game over (G_21B3) enters the score (fn_S8_3D66) and saves (fn_S8_3492). */
void fn_S8_3C0C(void) {
    MZ_FRAME;
    fn_S8_4CF0(GADDR(0x22BC));
    fn_S8_4D68(GADDR(0x22C4), GADDR(0x22C0), g32(0x22BC));
    if (!g32(0x22C4)) { mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x403C), 0, 0); return; }
    int16_t depth = (int16_t)r16(deref(r32(deref(g32(0x22BC)) + 0x16)) + 0x20);
    if (g8(0x21B3)) return;
    if (depth != 8 && depth != 4 && depth != 16 && depth != 32 && depth != 1) {
        fn_S8_4BE2(1);
        mz_seg_of(2, 0x03DC), fn_S2_03DC(GADDR(0x4004), 0);
    }
    uint32_t g = gd_rect(g32(0x22BC));
    if ((int16_t)(r16(g + 6) - r16(g + 2)) < (int16_t)g16(0x2166) || (int16_t)(r16(g + 4) - r16(g)) < (int16_t)g16(0x2164)) {
        fn_S8_4BE2(1);
        mz_seg_of(2, 0x03DC), fn_S2_03DC(GADDR(0x3F10), 0);
        return;
    }
    sg8(0x21B4, 0); sg8(0x21B0, 1); sg8(0x21FC, 0);
    fn_S8_4BE2(0);
    mac_HideCursor();
    fn_S8_4526(); fn_S8_4620(); fn_S8_471C();
    uint32_t w = g32(0x222E), r = mz_local(8);
    mac_SelectWindow(w); mac_SetPort(w);
    paint_port(w, r);
    mac_ValidRect(r);
    { Regs regs = {{0}, {0}, 0}; mz_seg_of(2, 0x39F8), fn_S2_39F8(&regs); }
    fn_S8_0000();
    fn_S8_4790(); fn_S8_46EE(); fn_S8_460C();
    mac_SetPort(g32(0x2232));
    mac_ShowCursor();
    fn_S8_4BE2(1);
    if (g8(0x21B3)) { fn_S8_3D66(); fn_S8_3492(); }
}

/* ---------- view sizes ---------- */

/* fn_S8_577A: (re)allocates the renderer's per-frame buffers for the current view size: the run
 * lists G_1F08 (twice G_2148), G_1F04 / G_1F00 / G_1EFC (G_2148 each), the row table G_1EDC
 * (G_2150 longs), the column table G_1EC4 (G_2156 words + 20 bytes) and the shading ramp G_1EC0
 * (G_214E + 1 words). Returns 0 if memory runs out. */
uint8_t fn_S8_577A(void) {
    const struct { uint16_t g; int32_t size; } bufs[] = {
        {0x1F08, (int32_t)g32(0x2148) * 2}, {0x1F04, (int32_t)g32(0x2148)}, {0x1F00, (int32_t)g32(0x2148)},
        {0x1EFC, (int32_t)g32(0x2148)}, {0x1EDC, (int32_t)(int16_t)g16(0x2150) * 4},
        {0x1EC4, (int32_t)(int16_t)g16(0x2156) * 2 + 0x14}, {0x1EC0, (int32_t)(int16_t)g16(0x214E) * 2 + 2},
    };
    for (unsigned k = 0; k < sizeof bufs / sizeof *bufs; k++) {
        if (g32(bufs[k].g)) mac_DisposeHandle(g32(bufs[k].g));
        sg32(bufs[k].g, 0);
        sg32(bufs[k].g, mac_NewHandle(bufs[k].size));
        if (!g32(bufs[k].g)) return 0;
    }
    return 1;
}

/* The globals each view size sets directly (1 and 2: a 352x320 window, the view 240x130 or
 * 344x186; 3: 464x384; 4: 640x480), then the scale G_214C (a single). */
static const uint16_t layout_globals[24] = {
    0x2166, 0x2164, 0x215E, 0x215C, 0x215A, 0x2158, 0x2156, 0x2154, 0x2152, 0x2150, 0x214E,
    0x213A, 0x213C, 0x2136, 0x2132, 0x212C, 0x212E, 0x2128, 0x2124, 0x211C, 0x2140, 0x2112, 0x2110, 0x210E,
};
static const uint16_t layout_values[4][24] = {
    {0x160, 0x140, 0x38, 0x128, 0x1E, 0xA0, 0xF0, 0x78, 0xC8, 0x82, 0x41,
     0x8, 0xCA, 0x8, 0x68, 0xF8, 0xCA, 0xF8, 0x158, 0x8, 0x9, 0x70, 0xBE, 0xF},
    {0x160, 0x140, 0x4, 0x15C, 0x2, 0xBC, 0x158, 0xAC, 0x12C, 0xBA, 0x5D,
     0x8, 0xCA, 0x8, 0x68, 0xF8, 0xCA, 0xF8, 0x158, 0x8, 0xC, 0x70, 0xBE, 0xF},
    {0x1D0, 0x180, 0x4, 0x1CC, 0x2, 0xFC, 0x1C8, 0xE8, 0x12C, 0xFA, 0x7D,
     0x20, 0x10A, 0x20, 0x80, 0x148, 0x10A, 0x148, 0x1A8, 0x20, 0xE, 0xA8, 0xFE, 0x1E},
    {0x280, 0x1E0, 0x4, 0x27C, 0x2, 0x15C, 0x278, 0x13C, 0x190, 0x15A, 0xAD,
     0x49, 0x16A, 0x49, 0xA9, 0x1C9, 0x16A, 0x1C9, 0x229, 0x49, 0x12, 0x100, 0x15E, 0x2D},
};
static const uint32_t layout_scale[4] = { 0x3F866666, 0x3FC147AE, 0x40000000, 0x403147AE };   /* 1.05, 1.51, 2, 2.77 */

/* fn_S8_527A: switches to view size `size` (1-4; anything else is a fatal alert, G_3AD8): sets
 * every layout global, the run-list sizes G_2148 / G_2144, reallocates the buffers (fn_S8_577A),
 * rebuilds the tables (fn_S8_5884) and the offscreen worlds. G_22B6 = the size, or -1 on failure
 * (returns 0). */
uint8_t fn_S8_527A(int16_t size) {
    if (mz_original) return fo_S8_472E(size);              /* Original: its own layouts */
    if (size >= 1 && size <= 4) {
        for (int k = 0; k < 24; k++) sg16(layout_globals[k], layout_values[size - 1][k]);
        sg32(0x214C, layout_scale[size - 1]);
        sg16(0x213E, 9);
        sg16(0x2138, (uint16_t)(g16(0x213C) + 10)); sg16(0x2134, (uint16_t)(g16(0x2138) + 8));
        sg16(0x2130, 0x26);
        sg16(0x212A, (uint16_t)(g16(0x213C) + 6)); sg16(0x2126, (uint16_t)(g16(0x2138) + 15));
        sg16(0x2122, 0x26); sg16(0x2120, 0xC);
        sg16(0x211E, (uint16_t)(g16(0x2164) - 6)); sg16(0x2118, g16(0x212C)); sg16(0x211A, (uint16_t)(g16(0x2164) - 6));
        sg16(0x2114, (uint16_t)(g16(0x2166) - 0x3A)); sg16(0x2116, (uint16_t)(g16(0x212E) + 2));
        sg16(0x210C, (uint16_t)(int16_t)((int16_t)(g16(0x2166) - 0x136) / 2));
    } else {
        mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3AD8), 0, 1);
    }
    sg32(0x2148, (uint32_t)(0xCC * (int32_t)(int16_t)g16(0x2150)));
    sg32(0x2144, (uint32_t)((int32_t)g32(0x2148) >> 1));
    if (!fn_S8_577A()) { sg16(0x22B6, 0xFFFF); return 0; }
    fn_S8_5884();
    if (fn_S8_4D68(GADDR(0x22C4), GADDR(0x22C0), g32(0x22BC))) { sg16(0x22B6, 0xFFFF); return 0; }
    sg16(0x22B6, (uint16_t)size);
    return 1;
}

/* fn_S8_48A2: opens the main (high-score) window at view size `size` on the screen whose rectangle
 * is *rect (the preferences'; the main screen if size is out of range or no screen matches),
 * limited to what that screen allows (fn_S8_521A): screens too small for the game (fn_S8_513C) or
 * too little memory (fn_S8_61AA) are alerts; if the size cannot be set up, smaller ones are tried
 * (and an alert, G_3C56, says so). The window ('WIND' 128, storage G_223A) is centred, x on a
 * multiple of 4, below the menu bar. Returns 1, or 0 if the game cannot run. */
uint8_t fn_S8_48A2(int16_t size, uint32_t rect) {
    mac_SetCursor(deref(mac_GetCursor(4)));                 /* watch */
    sg16(0x2168, (uint16_t)fn_S8_513C());
    if (!g16(0x2168)) { mac_InitCursor(); mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3C98), 0, 0); return 0; }
    int16_t use;
    if (size <= 0 || size > 4) {
        sg32(0x22BC, mac_GetMainDevice());
        if (!g32(0x22BC)) mac_ExitToShell();
        use = fn_S8_521A(g32(0x22BC));
    } else {
        sg32(0x22BC, fn_S8_610C(rect));
        if (!g32(0x22BC)) sg32(0x22BC, mac_GetMainDevice());
        if (!g32(0x22BC)) mac_ExitToShell();
        int16_t most = fn_S8_521A(g32(0x22BC));
        if (most < size) size = most;
        use = size;
    }
    uint8_t reserved = fn_S8_61AA();
    if (!reserved) { mac_InitCursor(); mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x2386), 0, 1); return 0; }
    int fell_back = 0;
    if (!fn_S8_527A(use)) {
        uint8_t ok = 0;
        for (int16_t k = (int16_t)(use - 1); k > 0 && !ok; k--) ok = fn_S8_527A(k);
        if (!ok) {
            if (reserved) { Regs r = {{0}, {0}, 0}; fn_S8_61FE(&r); }
            mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x2386), 0, 0);
            return 0;
        }
        fell_back = 1;
    }
    if (reserved) { Regs r = {{0}, {0}, 0}; fn_S8_61FE(&r); }
    uint32_t g = gd_rect(g32(0x22BC));
    int16_t top = (int16_t)r16(g), left = (int16_t)r16(g + 2), bottom = (int16_t)r16(g + 4), right = (int16_t)r16(g + 6);
    int16_t x = (int16_t)((int16_t)(right - left - g16(0x2166)) / 2);
    int16_t y = (int16_t)((int16_t)(bottom - top - g16(0x2164)) / 2);
    x = (int16_t)((x >> 2) << 2);
    x = (int16_t)(x + left); y = (int16_t)(y + top);
    if ((int16_t)(r16(0x0BAA) + 0x14) > y) y = (int16_t)(r16(0x0BAA) + 0x14);     /* below the menu bar */
    sg32(0x2232, mac_GetNewCWindow(0x80, g32(0x223A), 0xFFFFFFFFu));
    if (!g32(0x2232)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3C72), 1, 1);
    uint32_t w = g32(0x2232);
    mac_SizeWindow(w, (int16_t)g16(0x2166), (int16_t)g16(0x2164), 1);
    mac_MoveWindow(w, x, y, 1);
    mac_SetPort(w); mac_PmBackColor(0);
    mac_NSetPalette(w, g32(0x2224), 0x100);
    mac_ShowWindow(w);
    mac_InitCursor();
    if (fell_back) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x3C56), 0, 0);
    return 1;
}

/* ---------- start-up checks ---------- */

/* fn_S8_2480: the system must have Gestalt (else fatal alert G_44F2), System 7 (G_44B0), Color
 * QuickDraw 2.0 (G_4488), the Folder Manager (G_444A), FSSpec calls (G_440A) and the Standard File
 * package 7 routines (G_43CA); each missing one is a fatal alert. */
void fn_S8_2480(void) {
    MZ_FRAME;
    uint32_t resp = mz_local(4);
    if (!fn_S8_4802((int16_t)(mz_original ? 0xA0AD : 0xA1AD))) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x44F2), 0, 1);
    mz_seg(3);
    if (fn_S3_0000(0x73797376, resp) || (int32_t)(r32(resp) & 0xF00) < 0x700) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x44B0), 0, 1);     /* 'sysv' */
    if (fn_S3_0000(0x71642020, resp) || (int32_t)(r32(resp) & 0xFFFF) < 0x200) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x4488), 0, 1);   /* 'qd  ' */
    if (fn_S3_0000(0x666F6C64, resp) || !(r8(resp + 3) & 1)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x444A), 0, 1);                       /* 'fold' */
    if (fn_S3_0000(0x66732020, resp) || !(r8(resp + 3) & 2)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x440A), 0, 1);                       /* 'fs  ' */
    if (fn_S3_0000(0x73746466, resp) || !(r8(resp + 3) & 1)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x43CA), 0, 1);                       /* 'stdf' */
}

/* fn_S8_25BE: Apple events must be available (Gestalt 'evnt': fatal alerts G_4388 / G_436C); installs
 * the handlers for Open Application (fn_S8_3A44), Open Documents (fn_S8_3A72) and Quit (fn_S8_3B6E),
 * by their jump-table entries (fatal alerts G_4352 / G_4324 / G_42F8 on failure). */
void fn_S8_25BE(void) {
    MZ_FRAME;
    uint32_t resp = mz_local(4);
    if (mz_seg_of(3, 0x0000), fn_S3_0000(0x65766E74, resp)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x4388), 0, 1);       /* 'evnt' */
    else if (!(r8(resp + 3) & 1)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x436C), 0, 1);
    if (mac_AEInstallEventHandler(0x61657674, 0x6F617070, mz_jt(0x7A), 0, 0)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x4352), 0, 1);   /* 'aevt' 'oapp' */
    if (mz_original && mac_AEInstallEventHandler(0x61657674, 0x6F646F63, MZ_A5 + 0x9A, 0, 0))   /* Original: 'odoc' (fn_S8_3482) */
        mz_seg_of(2, 0x03A0), fn_S2_03A0(OADDR(0x6EB4), 0, 1);
    if (mac_AEInstallEventHandler(0x61657674, 0x70646F63, mz_jt(0x82), 0, 0)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x4324), 0, 1);   /* 'pdoc' */
    if (mac_AEInstallEventHandler(0x61657674, 0x71756974, mz_jt(0x8A), 0, 0)) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x42F8), 0, 1);   /* 'quit' */
}

/* fn_S8_237C: loads the sounds from the sound file (named G_45F2) in the application's folder (found
 * through the Process Manager); a missing file is an alert (G_45D2), other errors too (G_466E,
 * G_4632, G_4580, G_455C for a file without sounds). Any failure turns sound off (G_238E = 0). */
void fn_S8_237C(void) {
    MZ_FRAME;
    uint32_t psn = mz_local(8), info = mz_local(0x3C), spec = mz_local(0x46);
    int16_t err = mac_GetCurrentProcess(psn);
    if (err) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x466E), err, 0);
    if (!err) {
        w32(info, 0x3C);                                    /* processInfoLength */
        w32(info + 4, 0);                                   /* processName */
        w32(info + 0x38, spec);                             /* processAppSpec */
        err = mac_GetProcessInformation(psn, info);
        if (err) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x4632), err, 0);
    }
    if (!err) {
        mz_seg_of(2, 0x0332), fn_S2_0332(GADDR(0x45F2), spec + 6);    /* the sound file's name, same folder */
        int16_t ref = mac_FSpOpenResFile(spec, 1);          /* fsRdPerm */
        err = mac_ResError();
        if (err == -43) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x45D2), 0, 0);   /* fnfErr */
        else if (err) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x4580), err, 0);
        if (!err) {
            err = (mz_seg_of(4, 0x38F4), fn_S4_38F4(ref));
            if (err == 1) mz_seg_of(2, 0x03A0), fn_S2_03A0(GADDR(0x455C), 0, 0);
            mac_CloseResFile(ref);
        }
    }
    if (err) sg16(0x238E, 0);
}

/* ---------- the preferences file ---------- */

/* fn_S8_36F4: reads the whole data fork of file `spec` into a new handle *ph (left 0 if memory runs
 * out: error 1). Returns the File Manager error. */
int16_t fn_S8_36F4(uint32_t spec, uint32_t ph) {
    MZ_FRAME;
    uint32_t ref = mz_local(2), eof = mz_local(4);
    int16_t err = mac_FSpOpenDF(spec, 3, ref);              /* fsRdWrPerm */
    mz_seg(3);
    if (!err) err = fn_S3_030E((int16_t)r16(ref), 1, 0);   /* fsFromStart */
    if (!err) err = fn_S3_02CC((int16_t)r16(ref), eof);
    if (!err) {
        w32(ph, mac_NewHandle((int32_t)r32(eof)));
        if (!r32(ph)) err = 1;
    }
    mac_HLock(r32(ph));
    if (!err) err = fn_S3_0264((int16_t)r16(ref), eof, deref(r32(ph)));
    mac_HUnlock(r32(ph));
    if (!err) err = fn_S3_024A((int16_t)r16(ref));
    return err;
}

/* fn_S8_26D6: loads the preferences ($3D4 bytes: the view size and screen, options, the high-score
 * table) from the file named by 'STR#' 128 #1 in the Preferences folder, or makes the defaults
 * (fn_S8_27CC) if it is missing or the wrong size. Sets G_22B8, G_2394, G_2392, G_2393 and G_238E
 * from it. Returns the handle (0 if out of memory). */
uint32_t fn_S8_26D6(void) {
    MZ_FRAME;
    uint32_t h = mz_local(4), name = mz_local(0x100), vref = mz_local(2), dir = mz_local(4), spec = mz_local(0x46);
    w32(h, 0);
    mz_seg_of(3, 0x03BA), fn_S3_03BA(name, 0x80, 1);
    int16_t err = fn_S3_0400((int16_t)0x8000, 0x70726566, 0, vref, dir);   /* kOnSystemDisk, 'pref' */
    if (!err) err = mac_FSMakeFSSpec((int16_t)r16(vref), (int32_t)r32(dir), name, spec);
    if (!err) err = fn_S8_36F4(spec, h);
    if (err || !r32(h) || (mz_seg_of(3, 0x020E), fn_S3_020E(r32(h))) != 0x3D4) {
        if (r32(h)) mac_DisposeHandle(r32(h));
        w32(h, mac_NewHandle(0x3D4));
        if (r32(h)) fn_S8_27CC(r32(h));
    }
    if (r32(h)) {
        mac_HLock(r32(h));
        uint32_t p = deref(r32(h));
        sg8(0x22B8, r8(p + 0xA));
        sg8(0x2394, r8(p + 0xB));
        sg16(0x2392, r8(p + 0xC));
        sg8(0x2393, r8(p + 0xD));
        sg16(0x238E, r16(p + 0xE));
        mac_HUnlock(r32(h));
    }
    return r32(h);
}
