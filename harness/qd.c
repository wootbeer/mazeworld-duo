/* Color QuickDraw, Palette Manager and Window Manager wrappers for the harness.
 * Records live in emulated memory with their real 68k layouts (the game reads them
 * directly); the pixel and colour work is done by mactb. */
#include "qd_int.h"
#include "m68k.h"
#include <string.h>
#include <stdlib.h>

#define A7 M68K_REG_A7
#define ARGW()   ((int16_t)pop16())
#define ARGL()   (pop32())
#define RETW(v)  wr16(reg(A7), (uint16_t)(v))
#define RETL(v)  wr32(reg(A7), (uint32_t)(v))
#define RETB(v)  wr16(reg(A7), (uint16_t)((v) ? 0x0100 : 0))

/* ---------- record layouts ---------- */
enum { GD_PMap = 22, GD_Flags = 20, GD_Type = 4, GD_ITable = 6, GD_ResPref = 10, GD_NextGD = 30, GD_Rect = 34, GD_Mode = 42, GD_SIZE = 62 };
enum { PM_Base = 0, PM_RowBytes = 4, PM_Bounds = 6, PM_Version = 14, PM_HRes = 22, PM_VRes = 26, PM_PixelType = 30, PM_PixelSize = 32,
       PM_CmpCount = 34, PM_CmpSize = 36, PM_Table = 42, PM_SIZE = 50 };
#define GW_DEVICE (CP_SIZE)            /* private: GDHandle of a GWorld */
#define GW_SIZE   (CP_SIZE + 8)

static uint32_t main_gd, cur_gd;
static uint32_t window_list;           /* front-most window */

/* ---------- colour tables ---------- */
static void ctab_read(uint32_t h, MtbCTab *t) {
    uint32_t p = rd32(h);
    t->seed = rd32(p); t->count = (int16_t)rd16(p + 6) + 1;
    if (t->count > 256) t->count = 256;
    for (int i = 0; i < t->count; i++) {
        uint32_t e = p + 8 + 8 * i;
        int idx = (rd16(p + 4) & 0x8000) ? i : rd16(e);   /* device tables: position is the index */
        if (idx < 0 || idx > 255) idx = i;
        t->c[idx] = (MtbRGB){rd16(e + 2), rd16(e + 4), rd16(e + 6)};
    }
}
static void ctab_write(uint32_t h, const MtbCTab *t, int device) {
    mm_set_handle_size(h, 8 + 8 * t->count);
    uint32_t p = rd32(h);
    wr32(p, t->seed); wr16(p + 4, device ? 0x8000 : 0); wr16(p + 6, (uint16_t)(t->count - 1));
    for (int i = 0; i < t->count; i++) {
        uint32_t e = p + 8 + 8 * i;
        wr16(e, (uint16_t)i); wr16(e + 2, t->c[i].red); wr16(e + 4, t->c[i].green); wr16(e + 6, t->c[i].blue);
    }
}
static uint32_t ctab_new(const MtbCTab *t, int device) { uint32_t h = mm_new_handle(8, 1); ctab_write(h, t, device); return h; }

/* ---------- devices ---------- */
static uint32_t new_pixmap(uint32_t base, int rowbytes, MtbRect b, uint32_t ctab) {
    uint32_t h = mm_new_handle(PM_SIZE, 1), p = rd32(h);
    wr32(p + PM_Base, base); wr16(p + PM_RowBytes, (uint16_t)(rowbytes | 0x8000)); wr_rect(p + PM_Bounds, b);
    wr32(p + PM_HRes, 72 << 16); wr32(p + PM_VRes, 72 << 16);
    wr16(p + PM_PixelSize, 8); wr16(p + PM_CmpCount, 1); wr16(p + PM_CmpSize, 8); wr32(p + PM_Table, ctab);
    return h;
}

void qd_init(void) {
    MtbCTab c8; mtb_std_clut(8, &c8);
    uint32_t ct = ctab_new(&c8, 1);
    MtbRect sr = {0, 0, SCREEN_H, SCREEN_W};
    uint32_t pm = new_pixmap(SCREEN_BASE, SCREEN_W, sr, ct);
    main_gd = mm_new_handle(GD_SIZE, 1);
    uint32_t g = rd32(main_gd);
    wr16(g + GD_Type, 0); wr32(g + GD_ITable, mm_new_handle(4, 1)); wr16(g + GD_ResPref, 4);
    wr16(g + GD_Flags, (1 << 0) | (1 << 10) | (1 << 11) | (1 << 12) | (1 << 13) | (1 << 15));
    wr32(g + GD_PMap, pm); wr_rect(g + GD_Rect, sr); wr32(g + GD_Mode, 0x83);
    cur_gd = main_gd;
    wr32(LM_MainDevice, main_gd); wr32(LM_DeviceList, main_gd); wr32(LM_TheGDevice, main_gd);
    { hx_track(SCREEN_BASE, (uint32_t)(SCREEN_W * SCREEN_H)); memset(g_mem + SCREEN_BASE, 0, SCREEN_W * SCREEN_H); }
}

static uint32_t gd_ctab(uint32_t gd) { return rd32(rd32(rd32(rd32(gd) + GD_PMap)) + PM_Table); }  /* GDHandle -> GDevice -> PixMapHandle -> PixMap -> pmTable */

/* ---------- ports ---------- */
static uint32_t qd_globals(void) { return rd32(rd32(LM_CurrentA5)); }
static uint32_t the_port(void) { return rd32(qd_globals()); }
static void set_port(uint32_t p) { wr32(qd_globals(), p); }
static int is_cport(uint32_t port) { return (rd16(port + CP_Version) & 0xC000) == 0xC000; }

static uint32_t port_pixmap(uint32_t port) { return rd32(rd32(port + CP_PixMap)); }   /* PixMap pointer */
static MtbPix8 pix_of(uint32_t pm) {
    MtbPix8 v; v.base = g_mem + rd32(pm + PM_Base); v.rowbytes = rd16(pm + PM_RowBytes) & 0x3FFF; v.bounds = rd_rect(pm + PM_Bounds);
    return v;
}
/* report the pixels of rectangle r (clipped to clip and the pixmap) as written, for -subcheck */
static void track_pix(const MtbPix8 *pm, MtbRect r, const MtbRect *clip) {
    MtbRect o;
    if (clip && !mtb_sect_rect(&r, clip, &r)) return;
    if (!mtb_sect_rect(&r, &pm->bounds, &o)) return;
    uint32_t base = (uint32_t)(pm->base - g_mem);
    for (int y = o.top; y < o.bottom; y++)
        hx_track(base + (uint32_t)((y - pm->bounds.top) * pm->rowbytes + (o.left - pm->bounds.left)), (uint32_t)(o.right - o.left));
}
static MtbRect rgn_box(uint32_t rh) { return rh && rd32(rh) ? rd_rect(rd32(rh) + 2) : (MtbRect){-32768, -32768, 32767, 32767}; }
static MtbRect port_clip(uint32_t port) {
    MtbRect a = rd_rect(port + CP_PortRect), b = rgn_box(rd32(port + CP_ClipRgn)), c = rgn_box(rd32(port + CP_VisRgn)), o;
    if (!mtb_sect_rect(&a, &b, &o) || !mtb_sect_rect(&o, &c, &o)) o = (MtbRect){0, 0, 0, 0};
    return o;
}
static void port_ctab(uint32_t port, MtbCTab *t) { ctab_read(rd32(port_pixmap(port) + PM_Table), t); }

static uint32_t new_rgn_rect(MtbRect r) { uint32_t h = mm_new_handle(10, 1); wr16(rd32(h), 10); wr_rect(rd32(h) + 2, r); return h; }

static void init_cport(uint32_t port, uint32_t pmh, MtbRect portRect) {
    { hx_track(port, (uint32_t)(CP_SIZE)); memset(g_mem + port, 0, CP_SIZE); }
    wr32(port + CP_PixMap, pmh); wr16(port + CP_Version, 0xC000);
    wr_rect(port + CP_PortRect, portRect);
    wr32(port + CP_VisRgn, new_rgn_rect(portRect));
    wr32(port + CP_ClipRgn, new_rgn_rect((MtbRect){-32767, -32767, 32767, 32767}));
    wr16(port + CP_PnSize, 1); wr16(port + CP_PnSize + 2, 1); wr16(port + CP_PnMode, 8); /* patCopy */
    wr16(port + CP_TxSize, 0); wr32(port + CP_FgColor, 255); wr32(port + CP_BkColor, 0);
    wr16(port + CP_RGBBk, 0xFFFF); wr16(port + CP_RGBBk + 2, 0xFFFF); wr16(port + CP_RGBBk + 4, 0xFFFF);
}

/* ---------- palettes (host-side model mirrored into 68k records) ---------- */
#define MAXPAL 32
static struct { uint32_t h; MtbPalette p; uint32_t win; } pals[MAXPAL];
static int active_pal = -1;
static int pal_index(uint32_t h) { for (int i = 0; i < MAXPAL; i++) if (pals[i].h == h && h) return i; return -1; }
static void pal_mirror(int i) {
    MtbPalette *p = &pals[i].p; uint32_t h = pals[i].h;
    mm_set_handle_size(h, 16 + 16 * p->count);
    uint32_t a = rd32(h); { hx_track(a, (uint32_t)(16 + 16 * p->count)); memset(g_mem + a, 0, 16 + 16 * p->count); }
    wr16(a, (uint16_t)p->count);
    for (int k = 0; k < p->count; k++) {
        uint32_t e = a + 16 + 16 * k;
        wr16(e, p->e[k].rgb.red); wr16(e + 2, p->e[k].rgb.green); wr16(e + 4, p->e[k].rgb.blue);
        wr16(e + 6, p->e[k].usage); wr16(e + 8, p->e[k].tolerance);
    }
}
static int pal_new(void) { for (int i = 0; i < MAXPAL; i++) if (!pals[i].h) { memset(&pals[i], 0, sizeof pals[i]); pals[i].h = mm_new_handle(16, 1); return i; } hx_fault("too many palettes"); return 0; }
static void pal_activate(int i) {
    MtbCTab dev; uint32_t dh = gd_ctab(main_gd);
    ctab_read(dh, &dev);
    if (mtb_pal_activate(&pals[i].p, &dev)) ctab_write(dh, &dev, 1);
    active_pal = i;
}
static int pal_for_port(uint32_t port) {
    for (int i = 0; i < MAXPAL; i++) if (pals[i].h && pals[i].win == port) return i;
    return active_pal;
}
static void set_fg_from_rgb(uint32_t port, MtbRGB c, int devidx, int bk) {
    MtbCTab pt, dev; port_ctab(port, &pt); ctab_read(gd_ctab(main_gd), &dev);
    int idx = (devidx >= 0 && pt.seed == dev.seed) ? devidx : mtb_color_to_index(&pt, c);
    uint32_t rf = port + (bk ? CP_RGBBk : CP_RGBFg);
    wr16(rf, c.red); wr16(rf + 2, c.green); wr16(rf + 4, c.blue);
    wr32(port + (bk ? CP_BkColor : CP_FgColor), (uint32_t)idx);
}

/* ---------- traps: devices and colour tables ---------- */
static void t_GetMainDevice(uint16_t w) { (void)w; RETL(main_gd); }
static void t_GetNextDevice(uint16_t w) { (void)w; ARGL(); RETL(0); }
static void t_GetGDevice(uint16_t w) { (void)w; RETL(cur_gd); }
static void t_SetGDevice(uint16_t w) { (void)w; cur_gd = ARGL(); wr32(LM_TheGDevice, cur_gd); }
static void t_TestDeviceAttribute(uint16_t w) { (void)w; int16_t a = ARGW(); uint32_t gd = ARGL(); RETB((rd16(rd32(gd) + GD_Flags) >> a) & 1); }
static void t_GetCTable(uint16_t w) {
    (void)w; int16_t id = ARGW();
    uint32_t r = rs_get(FOURCC('c','l','u','t'), id, 0);
    if (r) {
        uint32_t n = (uint32_t)mm_handle_size(r), h = mm_new_handle(n, 0);
        { hx_track(rd32(h), (uint32_t)(n)); memmove(g_mem + rd32(h), g_mem + rd32(r), n); }
        RETL(h); return;
    }
    MtbCTab t;
    if (mtb_std_clut(id, &t) == 0) { RETL(ctab_new(&t, 0)); return; }
    hx_log("GetCTable(%d) at %s: no such table", id, hx_where());
    RETL(0);
}
static void t_DisposeCTable(uint16_t w) { (void)w; uint32_t h = ARGL(); if (h) mm_dispose_handle(h); }

/* ---------- traps: palettes ---------- */
static void t_NewPalette(uint16_t w) {
    (void)w; int16_t tol = ARGW(), usage = ARGW(); uint32_t ct = ARGL(); int16_t n = ARGW();
    int i = pal_new(); MtbPalette *p = &pals[i].p;
    p->count = n;
    MtbCTab t = {0}; if (ct) ctab_read(ct, &t);
    for (int k = 0; k < n; k++) { p->e[k].rgb = k < t.count ? t.c[k] : (MtbRGB){0, 0, 0}; p->e[k].usage = (uint16_t)usage; p->e[k].tolerance = (uint16_t)tol; p->e[k].devindex = -1; }
    pal_mirror(i); hx_log("NewPalette(%d entries, usage %d, tol %d) #%d at %s", n, usage, tol, i, hx_where()); RETL(pals[i].h);
}
static void t_GetNewPalette(uint16_t w) {
    (void)w; int16_t id = ARGW();
    uint32_t r = rs_get(FOURCC('p','l','t','t'), id, 0);
    if (!r) { RETL(0); return; }
    int i = pal_new(); MtbPalette *p = &pals[i].p; uint32_t a = rd32(r);
    p->count = rd16(a);
    for (int k = 0; k < p->count && k < 256; k++) {
        uint32_t e = a + 16 + 16 * k;
        p->e[k].rgb = (MtbRGB){rd16(e), rd16(e + 2), rd16(e + 4)}; p->e[k].usage = rd16(e + 6); p->e[k].tolerance = rd16(e + 8); p->e[k].devindex = -1;
    }
    pal_mirror(i); RETL(pals[i].h);
}
static void t_DisposePalette(uint16_t w) { (void)w; uint32_t h = ARGL(); int i = pal_index(h); if (i >= 0) { mm_dispose_handle(h); pals[i].h = 0; if (active_pal == i) active_pal = -1; } }
static void t_NSetPalette(uint16_t w) {
    (void)w; ARGW(); uint32_t ph = ARGL(), win = ARGL();
    for (int i = 0; i < MAXPAL; i++) if (pals[i].h && pals[i].win == win) pals[i].win = 0;
    int i = pal_index(ph); if (i < 0) return;
    pals[i].win = win;
    if (win == window_list || win == 0) pal_activate(i);
}
static void t_ActivatePalette(uint16_t w) { (void)w; uint32_t win = ARGL(); for (int i = 0; i < MAXPAL; i++) if (pals[i].h && pals[i].win == win) pal_activate(i); }
static void t_GetPalette(uint16_t w) { (void)w; uint32_t win = ARGL(); for (int i = 0; i < MAXPAL; i++) if (pals[i].h && pals[i].win == win) { RETL(pals[i].h); return; } RETL(0); }
static void t_PaletteDispatch(uint16_t w) {
    (void)w; int sel = reg(M68K_REG_D0) & 0xFF;
    if (sel != 0) { hx_fault("PaletteDispatch selector %d at %s", sel, hx_where()); return; }
    int16_t entry = ARGW();   /* Entry2Index */
    int i = pal_for_port(the_port());
    MtbCTab dev; ctab_read(gd_ctab(main_gd), &dev);
    int r = i >= 0 ? mtb_pal_entry_index(&pals[i].p, &dev, entry) : 0;
    static int logged; if (logged++ < 40) hx_log("Entry2Index(%d) port %06X palette #%d (%d entries) -> %d at %s", entry, the_port(), i, i >= 0 ? pals[i].p.count : 0, r, hx_where());
    RETL(r);
}
static void pm_color(int bk) {
    int16_t entry = ARGW(); uint32_t port = the_port();
    int i = pal_for_port(port);
    if (i < 0 || entry >= pals[i].p.count) return;
    MtbCTab dev; ctab_read(gd_ctab(main_gd), &dev);
    set_fg_from_rgb(port, pals[i].p.e[entry].rgb, mtb_pal_entry_index(&pals[i].p, &dev, entry), bk);
}
static void t_PmForeColor(uint16_t w) { (void)w; pm_color(0); }
static void t_PmBackColor(uint16_t w) { (void)w; pm_color(1); }
static void t_RGBForeColor(uint16_t w) { (void)w; uint32_t c = ARGL(); set_fg_from_rgb(the_port(), (MtbRGB){rd16(c), rd16(c + 2), rd16(c + 4)}, -1, 0); }
static void t_RGBBackColor(uint16_t w) { (void)w; uint32_t c = ARGL(); set_fg_from_rgb(the_port(), (MtbRGB){rd16(c), rd16(c + 2), rd16(c + 4)}, -1, 1); }
static MtbRGB classic_rgb(int32_t c) {
    switch (c) {
    case 33: return (MtbRGB){0, 0, 0};             case 30: return (MtbRGB){0xFFFF, 0xFFFF, 0xFFFF};
    case 205: return (MtbRGB){0xDD6B, 0x08C2, 0x06A2}; case 341: return (MtbRGB){0x0000, 0x8000, 0x11B0};
    case 409: return (MtbRGB){0x0000, 0x0000, 0xD400}; case 273: return (MtbRGB){0x0241, 0xAB54, 0xEAFF};
    case 137: return (MtbRGB){0xF2D7, 0x0856, 0x84EC}; case 69: return (MtbRGB){0xFC00, 0xF37D, 0x052F};
    }
    return (MtbRGB){0, 0, 0};
}
static void t_ForeColor(uint16_t w) { (void)w; int32_t c = (int32_t)ARGL(); set_fg_from_rgb(the_port(), classic_rgb(c), -1, 0); }
static void t_BackColor(uint16_t w) { (void)w; int32_t c = (int32_t)ARGL(); set_fg_from_rgb(the_port(), classic_rgb(c), -1, 1); }

/* ---------- traps: ports and drawing ---------- */
static void t_SetPort(uint16_t w) { (void)w; set_port(ARGL()); }
static void t_GetPort(uint16_t w) { (void)w; wr32(ARGL(), the_port()); }
static void t_ClipRect(uint16_t w) {
    (void)w; MtbRect r = rd_rect(ARGL()); uint32_t port = the_port(), rh = rd32(port + CP_ClipRgn);
    if (rh) wr_rect(rd32(rh) + 2, r);
}
static void draw_rect(int mode, uint32_t rp) {   /* 0 paint, 1 frame, 2 erase */
    uint32_t port = the_port(); if (!is_cport(port)) { hx_log("drawing into a non-colour port at %s ignored", hx_where()); return; }
    MtbRect r = rd_rect(rp), clip = port_clip(port);
    MtbPix8 pm = pix_of(port_pixmap(port));
    uint8_t fg = (uint8_t)rd32(port + CP_FgColor), bk = (uint8_t)rd32(port + CP_BkColor);
    track_pix(&pm, r, &clip);
    if (mode == 0) mtb_fill_rect8(&pm, &r, &clip, fg);
    else if (mode == 1) mtb_frame_rect8(&pm, &r, &clip, (int16_t)rd16(port + CP_PnSize + 2), (int16_t)rd16(port + CP_PnSize), fg);
    else mtb_fill_rect8(&pm, &r, &clip, bk);
}
static void t_PaintRect(uint16_t w) { (void)w; draw_rect(0, ARGL()); }
static void t_FrameRect(uint16_t w) { (void)w; draw_rect(1, ARGL()); }
static void t_EraseRect(uint16_t w) { (void)w; draw_rect(2, ARGL()); }
static void t_MoveTo(uint16_t w) { (void)w; int16_t v = ARGW(), h = ARGW(); uint32_t p = the_port(); wr16(p + CP_PnLoc, v); wr16(p + CP_PnLoc + 2, h); }
static void t_Move(uint16_t w) { (void)w; int16_t dv = ARGW(), dh = ARGW(); uint32_t p = the_port(); wr16(p + CP_PnLoc, rd16(p + CP_PnLoc) + dv); wr16(p + CP_PnLoc + 2, rd16(p + CP_PnLoc + 2) + dh); }
static void line_to(int h1, int v1) {
    uint32_t p = the_port();
    int v0 = (int16_t)rd16(p + CP_PnLoc), h0 = (int16_t)rd16(p + CP_PnLoc + 2);
    if ((int16_t)rd16(p + CP_PnVis) >= 0 && is_cport(p)) {
        MtbRect clip = port_clip(p); MtbPix8 pm = pix_of(port_pixmap(p));
        track_pix(&pm, (MtbRect){(int16_t)(v0 < v1 ? v0 : v1), (int16_t)(h0 < h1 ? h0 : h1), (int16_t)((v0 > v1 ? v0 : v1) + 1), (int16_t)((h0 > h1 ? h0 : h1) + 1)}, &clip);
        mtb_line8(&pm, h0, v0, h1, v1, &clip, (uint8_t)rd32(p + CP_FgColor));
    }
    wr16(p + CP_PnLoc, (uint16_t)v1); wr16(p + CP_PnLoc + 2, (uint16_t)h1);
}
static void t_LineTo(uint16_t w) { (void)w; int16_t v = ARGW(), h = ARGW(); line_to(h, v); }
static void t_Line(uint16_t w) { (void)w; int16_t dv = ARGW(), dh = ARGW(); uint32_t p = the_port(); line_to((int16_t)rd16(p + CP_PnLoc + 2) + dh, (int16_t)rd16(p + CP_PnLoc) + dv); }
static void t_PenNormal(uint16_t w) { (void)w; uint32_t p = the_port(); wr16(p + CP_PnSize, 1); wr16(p + CP_PnSize + 2, 1); wr16(p + CP_PnMode, 8); }
static void t_GetPenState(uint16_t w) {   /* PenState: pnLoc, pnSize, pnMode, pnPat(8) */
    (void)w; uint32_t s = ARGL(), p = the_port();
    wr32(s, rd32(p + CP_PnLoc)); wr32(s + 4, rd32(p + CP_PnSize)); wr16(s + 8, rd16(p + CP_PnMode)); { hx_track(s + 10, (uint32_t)(8)); memset(g_mem + s + 10, 0xFF, 8); }
}
static void t_SetPenState(uint16_t w) { (void)w; uint32_t s = ARGL(), p = the_port(); wr32(p + CP_PnLoc, rd32(s)); wr32(p + CP_PnSize, rd32(s + 4)); wr16(p + CP_PnMode, rd16(s + 8)); }
static void t_TextFont(uint16_t w) { (void)w; wr16(the_port() + CP_TxFont, (uint16_t)ARGW()); }
static void t_TextSize(uint16_t w) { (void)w; wr16(the_port() + CP_TxSize, (uint16_t)ARGW()); }
static void t_TextFace(uint16_t w) { (void)w; wr8(the_port() + CP_TxFace, (uint8_t)ARGW()); }
static void t_TextMode(uint16_t w) { (void)w; wr16(the_port() + CP_TxMode, (uint16_t)ARGW()); }
static void t_GetFNum(uint16_t w) {
    (void)w; uint32_t pnum = ARGL(), pname = ARGL();
    char n[64]; int l = rd8(pname); memcpy(n, g_mem + pname + 1, l); n[l] = 0;
    int16_t num = !strcmp(n, "Geneva") ? 3 : !strcmp(n, "Monaco") ? 4 : !strcmp(n, "Chicago") ? 0 : !strcmp(n, "New York") ? 2 : 0;
    wr16(pnum, (uint16_t)num);
}

/* ---------- text ---------- */
static int txmode(uint32_t port) {
    int m = (int16_t)rd16(port + CP_TxMode);
    if (m == 0) return 0;          /* srcCopy */
    if (m != 1) { static int warned; if (!warned++) hx_log("text mode %d drawn as srcOr at %s", m, hx_where()); }
    return 1;
}
static void draw_text(const uint8_t *text, int len);
static void t_DrawString(uint16_t w) { (void)w; uint32_t sp = ARGL(); draw_text(g_mem + sp + 1, rd8(sp)); }
static void t_DrawChar(uint16_t w) { (void)w; uint8_t c = (uint8_t)pop16(); draw_text(&c, 1); }
/* text at the pen in the port's font, size, face and mode; the pen moves past it */
static void draw_text(const uint8_t *text, int len) {
    uint32_t port = the_port();
    if (!is_cport(port)) return;
    MtbRect clip = port_clip(port); MtbPix8 pm = pix_of(port_pixmap(port));
    int v = (int16_t)rd16(port + CP_PnLoc), h = (int16_t)rd16(port + CP_PnLoc + 2);
    {   int asc, desc, lead, wmax, sz = (int16_t)rd16(port + CP_TxSize);
        mtb_font_metrics(sz, &asc, &desc, &lead, &wmax);
        int tw = mtb_text_width(sz, rd8(port + CP_TxFace), text, len);
        track_pix(&pm, (MtbRect){(int16_t)(v - asc), (int16_t)h, (int16_t)(v + desc + 2), (int16_t)(h + tw)}, &clip); }
    h = mtb_draw_text8(&pm, &clip, h, v, text, len, (int16_t)rd16(port + CP_TxSize), rd8(port + CP_TxFace),
                       txmode(port), (uint8_t)rd32(port + CP_FgColor), (uint8_t)rd32(port + CP_BkColor));
    wr16(port + CP_PnLoc + 2, (uint16_t)h);
}
static void t_TETextBox(uint16_t w) {
    (void)w; int16_t just = ARGW(); uint32_t box = ARGL(); int32_t len = (int32_t)ARGL(); uint32_t text = ARGL();
    uint32_t port = the_port();
    if (!is_cport(port) || len <= 0) return;
    MtbRect clip = port_clip(port), b = rd_rect(box); MtbPix8 pm = pix_of(port_pixmap(port));
    track_pix(&pm, b, &clip);
    mtb_textbox8(&pm, &clip, &b, g_mem + text, len, (int16_t)rd16(port + CP_TxSize), rd8(port + CP_TxFace), txmode(port),
                 (uint8_t)rd32(port + CP_FgColor), (uint8_t)rd32(port + CP_BkColor), just);
}

/* ---------- pictures ---------- */
static void t_GetPicture(uint16_t w) { (void)w; int16_t id = ARGW(); RETL(rs_get(FOURCC('P','I','C','T'), id, 0)); }
static void t_DrawPicture(uint16_t w) {
    (void)w; uint32_t dr = ARGL(), pic = ARGL();
    uint32_t port = the_port();
    if (!pic || !rd32(pic) || !is_cport(port)) return;
    MtbRect clip = port_clip(port), d = rd_rect(dr); MtbPix8 pm = pix_of(port_pixmap(port));
    MtbCTab dev; port_ctab(port, &dev);
    track_pix(&pm, d, &clip);
    char err[128];
    if (mtb_pict_draw8(g_mem + rd32(pic), (uint32_t)mm_handle_size(pic), &d, &pm, &clip, &dev, err, sizeof err))
        hx_fault("DrawPicture at %s: %s not supported", hx_where(), err);
}

/* ---------- traps: rectangles and regions ---------- */
static void t_SetRect(uint16_t w) { (void)w; int16_t b = ARGW(), r = ARGW(), t = ARGW(), l = ARGW(); uint32_t p = ARGL(); wr_rect(p, (MtbRect){t, l, b, r}); }
static void t_OffsetRect(uint16_t w) { (void)w; int16_t dv = ARGW(), dh = ARGW(); uint32_t p = ARGL(); MtbRect r = rd_rect(p); r.top += dv; r.bottom += dv; r.left += dh; r.right += dh; wr_rect(p, r); }
static void t_InsetRect(uint16_t w) { (void)w; int16_t dv = ARGW(), dh = ARGW(); uint32_t p = ARGL(); MtbRect r = rd_rect(p); r.top += dv; r.bottom -= dv; r.left += dh; r.right -= dh; if (r.top >= r.bottom || r.left >= r.right) r = (MtbRect){0, 0, 0, 0}; wr_rect(p, r); }
static void t_SectRect(uint16_t w) { (void)w; uint32_t d = ARGL(), b = ARGL(), a = ARGL(); MtbRect ra = rd_rect(a), rb = rd_rect(b), o; int ok = mtb_sect_rect(&ra, &rb, &o); wr_rect(d, o); RETB(ok); }
static void t_SetPt(uint16_t w) { (void)w; int16_t v = ARGW(), h = ARGW(); uint32_t p = ARGL(); wr16(p, v); wr16(p + 2, h); }
static void t_NewRgn(uint16_t w) { (void)w; RETL(new_rgn_rect((MtbRect){0, 0, 0, 0})); }
static void t_DisposeRgn(uint16_t w) { (void)w; uint32_t h = ARGL(); if (h) mm_dispose_handle(h); }
static void t_RectRgn(uint16_t w) { (void)w; MtbRect r = rd_rect(ARGL()); uint32_t h = ARGL(); if (r.top >= r.bottom || r.left >= r.right) r = (MtbRect){0, 0, 0, 0}; wr16(rd32(h), 10); wr_rect(rd32(h) + 2, r); }
static void t_CopyRgn(uint16_t w) { (void)w; uint32_t d = ARGL(), s = ARGL(); mm_set_handle_size(d, 10); wr16(rd32(d), 10); wr_rect(rd32(d) + 2, rd_rect(rd32(s) + 2)); }
static void t_UnionRgn(uint16_t w) {
    (void)w; uint32_t d = ARGL(), b = ARGL(), a = ARGL();
    MtbRect ra = rd_rect(rd32(a) + 2), rb = rd_rect(rd32(b) + 2), o;
    int ea = ra.top >= ra.bottom || ra.left >= ra.right, eb = rb.top >= rb.bottom || rb.left >= rb.right;
    if (ea) o = rb; else if (eb) o = ra;
    else {
        o.top = ra.top < rb.top ? ra.top : rb.top; o.left = ra.left < rb.left ? ra.left : rb.left;
        o.bottom = ra.bottom > rb.bottom ? ra.bottom : rb.bottom; o.right = ra.right > rb.right ? ra.right : rb.right;
        hx_log("UnionRgn of two rectangles approximated by their bounding box at %s", hx_where());
    }
    mm_set_handle_size(d, 10); wr16(rd32(d), 10); wr_rect(rd32(d) + 2, o);
}

/* ---------- CopyBits ---------- */
static uint32_t resolve_bits(uint32_t bm) {   /* -> PixMap pointer */
    uint16_t rb = rd16(bm + 4);
    if ((rb & 0xC000) == 0xC000) return rd32(rd32(bm));   /* &cport->portBits: portPixMap handle */
    if (rb & 0x8000) return bm;
    return 0;
}
static void t_CopyBits(uint16_t w) {
    (void)w; uint32_t mask = ARGL(); ARGW(); uint32_t dr = ARGL(), sr = ARGL(), dst = ARGL(), src = ARGL();
    uint32_t spm = resolve_bits(src), dpm = resolve_bits(dst);
    if (!spm || !dpm) { hx_log("CopyBits with a 1-bit BitMap at %s ignored", hx_where()); return; }
    MtbPix8 s = pix_of(spm), d = pix_of(dpm);
    MtbRect rs = rd_rect(sr), rd = rd_rect(dr), clip = port_clip(the_port());
    if (mask && rd32(mask)) { MtbRect m = rgn_box(mask); mtb_sect_rect(&clip, &m, &clip); }
    MtbCTab cs, cd; ctab_read(rd32(spm + PM_Table), &cs); ctab_read(rd32(dpm + PM_Table), &cd);
    uint8_t xl[256]; const uint8_t *x = NULL;
    if (cs.seed != cd.seed) { for (int i = 0; i < 256; i++) xl[i] = (uint8_t)mtb_color_to_index(&cd, cs.c[i < cs.count ? i : 0]); x = xl; }
    track_pix(&d, rd, &clip);
    mtb_copybits8(&s, &rs, &d, &rd, &clip, x);
    extern int g_shot_every; extern const char *g_outdir;
    static unsigned frames;
    if (g_shot_every && rd32(dpm + PM_Base) == SCREEN_BASE && (++frames % (unsigned)g_shot_every) == 0) {
        char p[512]; snprintf(p, sizeof p, "%s/shots/f%05u.ppm", g_outdir, frames);
        void qd_dump_screen(const char *path); qd_dump_screen(p);
    }
    if (rd32(dpm + PM_Base) == SCREEN_BASE && getenv("MZ_FRAMEHASH")) {   /* a hash of every frame shown, for comparing runs */
        static unsigned nf; uint64_t hsh = 1469598103934665603ull;
        for (uint32_t i = 0; i < SCREEN_W * SCREEN_H; i++) hsh = (hsh ^ g_mem[SCREEN_BASE + i]) * 1099511628211ull;
        hx_log("frame %u hash %016llx", ++nf, (unsigned long long)hsh);
    }
}

/* ---------- GWorlds ---------- */
static uint32_t gworlds[32]; static int n_gworlds;
static uint32_t gw_new(int depth, MtbRect b, uint32_t ctab) {
    if (depth != 0 && depth != 8) hx_fault("NewGWorld depth %d not supported at %s", depth, hx_where());
    int w = b.right - b.left, h = b.bottom - b.top;
    int rowbytes = ((w * 8 + 31) / 32) * 4;
    uint32_t pix = mm_new_ptr((uint32_t)(rowbytes * h + 4), 1);
    MtbCTab t;
    if (ctab) ctab_read(ctab, &t);
    else if (depth == 0) ctab_read(gd_ctab(main_gd), &t);
    else mtb_std_clut(8, &t);
    uint32_t pmh = new_pixmap(pix, rowbytes, b, ctab_new(&t, 0));
    uint32_t port = mm_new_ptr(GW_SIZE, 1);
    init_cport(port, pmh, b);
    wr32(port + GW_DEVICE, main_gd);
    if (n_gworlds < 32) gworlds[n_gworlds++] = port;
    hx_log("NewGWorld %dx%d depth %d at %s -> port %06X pixels %06X", w, h, depth, hx_where(), port, pix);
    return port;
}
static void t_QDExtensions(uint16_t w) {
    (void)w; uint32_t sel = reg(M68K_REG_D0);
    switch (sel) {
    case 0x160000: { /* NewGWorld(VAR gw, depth, bounds*, cTable, aGDevice, flags) */
        ARGL(); ARGL(); uint32_t ct = ARGL(), br = ARGL(); int16_t depth = ARGW(); uint32_t pgw = ARGL();
        wr32(pgw, gw_new(depth, rd_rect(br), ct)); RETW(0); break; }
    case 0x160003: { /* UpdateGWorld(VAR gw, depth, bounds*, cTable, aGDevice, flags): GWorldFlags */
        ARGL(); ARGL(); uint32_t ct = ARGL(), br = ARGL(); int16_t depth = ARGW(); uint32_t pgw = ARGL();
        uint32_t gw = rd32(pgw); MtbRect nb = rd_rect(br), ob = rd_rect(gw + CP_PortRect);
        if (nb.bottom - nb.top != ob.bottom - ob.top || nb.right - nb.left != ob.right - ob.left) {
            wr32(pgw, gw_new(depth, nb, ct)); hx_log("UpdateGWorld reallocated at %s", hx_where()); RETL(1 << 2 /* reallocPix */);
            break;
        }
        uint32_t flags = 0;
        if (ct) {   /* new colour table: adopt it (seed included) and remap the pixels by colour */
            uint32_t pm = port_pixmap(gw), oh = rd32(pm + PM_Table);
            MtbCTab oldt, newt; ctab_read(oh, &oldt); ctab_read(ct, &newt);
            if (oldt.seed != newt.seed) {
                uint8_t map[256];
                for (int k = 0; k < 256; k++) map[k] = (uint8_t)mtb_color_to_index(&newt, oldt.c[k < oldt.count ? k : 0]);
                MtbPix8 v = pix_of(pm);
                track_pix(&v, v.bounds, NULL);
                for (int y = 0; y < v.bounds.bottom - v.bounds.top; y++)
                    for (int x = 0; x < v.bounds.right - v.bounds.left; x++) v.base[y * v.rowbytes + x] = map[v.base[y * v.rowbytes + x]];
                ctab_write(oh, &newt, 0);
                flags |= 1 << 1;   /* mapPix */
                hx_log("UpdateGWorld adopted colour table seed %u (was %u) at %s", newt.seed, oldt.seed, hx_where());
            }
        }
        RETL(flags);
        break; }
    case 0x40001: ARGL(); RETB(1); break;                      /* LockPixels */
    case 0x40002: ARGL(); break;                                /* UnlockPixels */
    case 0x40004: ARGL(); break;                                /* DisposeGWorld: memory left allocated */
    case 0x80005: { uint32_t pgd = ARGL(), pp = ARGL(); wr32(pp, the_port()); wr32(pgd, cur_gd); break; }   /* GetGWorld */
    case 0x80006: { uint32_t gd = ARGL(), port = ARGL(); set_port(port); if (gd) cur_gd = gd; break; }    /* SetGWorld */
    case 0x4000F: { uint32_t pmh = ARGL(); RETL(rd32(rd32(pmh) + PM_Base)); break; }                       /* GetPixBaseAddr */
    case 0x40012: { uint32_t gw = ARGL(); RETL(rd32(gw + GW_DEVICE) ? rd32(gw + GW_DEVICE) : main_gd); break; } /* GetGWorldDevice */
    case 0x40017: { uint32_t gw = ARGL(); RETL(rd32(gw + CP_PixMap)); break; }                             /* GetGWorldPixMap */
    default: hx_fault("QDExtensions selector %X at %s", sel, hx_where());
    }
}

/* ---------- Window Manager ---------- */
/* windows needing an update event (shown or invalidated) */
static uint32_t upd_pending[16]; static int n_upd;
static void need_update(uint32_t win) { for (int i = 0; i < n_upd; i++) if (upd_pending[i] == win) return; if (n_upd < 16) upd_pending[n_upd++] = win; }
uint32_t qd_take_update(void) {   /* called by the event code: next window with a pending update, or 0 */
    if (!n_upd) return 0;
    uint32_t w = upd_pending[0];
    memmove(upd_pending, upd_pending + 1, sizeof upd_pending[0] * (size_t)--n_upd);
    return w;
}
int qd_take_update_for(uint32_t win) {
    for (int i = 0; i < n_upd; i++) if (upd_pending[i] == win) { memmove(upd_pending + i, upd_pending + i + 1, sizeof upd_pending[0] * (size_t)(n_upd - i - 1)); n_upd--; return 1; }
    return 0;
}
uint32_t qd_new_window(uint32_t storage, uint32_t size, MtbRect b, int frame, int visible, int goaway,
                       uint32_t refcon, uint32_t behind, int kind) {
    uint32_t win = storage ? storage : mm_new_ptr(size, 1);
    MtbRect pr = {0, 0, (int16_t)(b.bottom - b.top), (int16_t)(b.right - b.left)};
    MtbRect pb = {(int16_t)-b.top, (int16_t)-b.left, (int16_t)(SCREEN_H - b.top), (int16_t)(SCREEN_W - b.left)};
    uint32_t pmh = new_pixmap(SCREEN_BASE, SCREEN_W, pb, gd_ctab(main_gd));
    init_cport(win, pmh, pr);
    { hx_track(win + CP_SIZE, (uint32_t)(size - CP_SIZE)); memset(g_mem + win + CP_SIZE, 0, size - CP_SIZE); }
    wr16(win + W_Kind, (uint16_t)kind); wr8(win + W_Visible, visible ? 0xFF : 0); wr8(win + W_GoAway, goaway ? 0xFF : 0);
    wr32(win + W_RefCon, refcon);
    MtbRect sb = {(int16_t)(b.top - frame), (int16_t)(b.left - frame), (int16_t)(b.bottom + frame), (int16_t)(b.right + frame)};
    wr32(win + W_StrucRgn, new_rgn_rect(sb)); wr32(win + W_ContRgn, new_rgn_rect(b)); wr32(win + W_UpdateRgn, new_rgn_rect((MtbRect){0, 0, 0, 0}));
    if (behind == 0xFFFFFFFFu || !window_list) { wr32(win + W_Next, window_list); window_list = win; }
    else { wr32(win + W_Next, 0); uint32_t x = window_list; while (rd32(x + W_Next)) x = rd32(x + W_Next); wr32(x + W_Next, win); }
    return win;
}
/* Removing a window uncovers its structure rectangle: the desktop (index 0) is repainted there,
 * then each window still showing through gets its background colour and an update event.
 * FIDELITY: the desktop is plain white here; a real Mac paints the desktop pattern. */
void qd_remove_window(uint32_t win) {
    if (window_list == win) window_list = rd32(win + W_Next);
    else { uint32_t x = window_list; while (x && rd32(x + W_Next) != win) x = rd32(x + W_Next); if (x) wr32(x + W_Next, rd32(win + W_Next)); }
    qd_take_update_for(win);
    if (!rd8(win + W_Visible)) return;
    MtbRect gone = rgn_box(rd32(win + W_StrucRgn));
    MtbPix8 scr = {g_mem + SCREEN_BASE, SCREEN_W, {0, 0, SCREEN_H, SCREEN_W}};
    mtb_fill_rect8(&scr, &gone, NULL, 0);
    uint32_t order[64]; int n = 0;
    for (uint32_t x = window_list; x && n < 64; x = rd32(x + W_Next)) order[n++] = x;
    for (int i = n - 1; i >= 0; i--) {   /* back to front */
        uint32_t x = order[i]; MtbRect s = rgn_box(rd32(x + W_StrucRgn)), o;
        if (!rd8(x + W_Visible) || !mtb_sect_rect(&s, &gone, &o)) continue;
        mtb_fill_rect8(&scr, &o, NULL, (uint8_t)rd32(x + CP_BkColor));
        need_update(x);
    }
}
/* System 7 automatic positioning word after a WIND/DLOG/ALRT template's title (0 = none).
 * FIDELITY: centring uses the content rectangle on the area below a 20-pixel menu bar;
 * "alert position" puts a third of the free space above. Parent-window variants use the main screen. */
MtbRect qd_auto_position(MtbRect b, uint16_t pos) {
    int w = b.right - b.left, h = b.bottom - b.top, mbar = 20, free_v = SCREEN_H - mbar - h;
    int top = b.top, left = b.left;
    switch (pos & 0x3FFF) {
    case 0x280A: left = (SCREEN_W - w) / 2; top = mbar + free_v / 2; break;   /* centre */
    case 0x300A: left = (SCREEN_W - w) / 2; top = mbar + free_v / 3; break;   /* alert position */
    case 0: return b;
    default: hx_log("window position %04X not modelled; template rectangle used", pos); return b;
    }
    return (MtbRect){(int16_t)top, (int16_t)left, (int16_t)(top + h), (int16_t)(left + w)};
}
/* position word of a template whose title starts at offset `title` (0 if the template has none) */
uint16_t qd_template_position(uint32_t h, int title) {
    uint32_t a = rd32(h); int end = title + 1 + rd8(a + (uint32_t)title); end = (end + 1) & ~1;
    return mm_handle_size(h) >= end + 2 ? rd16(a + (uint32_t)end) : 0;
}
uint32_t qd_front_window(void) { return window_list; }   /* for the port's shell */
/* a window's content in screen coordinates (its port rectangle less its pixel map's origin) */
MtbRect qd_window_global(uint32_t win) {
    MtbRect pr = rd_rect(win + CP_PortRect), b = rd_rect(port_pixmap(win) + PM_Bounds);
    return (MtbRect){(int16_t)(pr.top - b.top), (int16_t)(pr.left - b.left), (int16_t)(pr.bottom - b.top), (int16_t)(pr.right - b.left)};
}
/* FindWindow(pt, VAR window): which part of which window a global point is in: inMenuBar (1) above
 * MBarHeight, else the front-most visible window whose structure holds it: inContent (3), or
 * inSysWindow (2) for a desk accessory, or inDrag (4) on its frame; else inDesk (0).
 * FIDELITY: windows here have no title bars or close boxes, so a frame click is reported as inDrag;
 * the menu bar is the top 20 rows (nothing is drawn there). */
static void t_FindWindow(uint16_t w) {
    (void)w; uint32_t pwin = ARGL(), pt = ARGL();
    int16_t v = (int16_t)(pt >> 16), h = (int16_t)pt;
    wr32(pwin, 0);
    if (v < 20) { RETW(1); return; }
    for (uint32_t x = window_list; x; x = rd32(x + W_Next)) {
        if (!rd8(x + W_Visible)) continue;
        MtbRect sr = rgn_box(rd32(x + W_StrucRgn)), cr = rgn_box(rd32(x + W_ContRgn));
        if (v < sr.top || v >= sr.bottom || h < sr.left || h >= sr.right) continue;
        wr32(pwin, x);
        if ((int16_t)rd16(x + W_Kind) < 0) { RETW(2); return; }
        RETW(v >= cr.top && v < cr.bottom && h >= cr.left && h < cr.right ? 3 : 4);
        return;
    }
    RETW(0);
}
void qd_post_update(uint32_t win) { need_update(win); }
MtbPix8 qd_port_pix(uint32_t port) { return pix_of(port_pixmap(port)); }
MtbRect qd_port_clip(uint32_t port) { return port_clip(port); }
void qd_device_ctab(MtbCTab *t) { ctab_read(gd_ctab(main_gd), t); }
static void t_GetNewCWindow(uint16_t w) {
    (void)w; uint32_t behind = ARGL(), storage = ARGL(); int16_t id = ARGW();
    uint32_t r = rs_get(FOURCC('W','I','N','D'), id, 0);
    if (!r) { RETL(0); return; }
    uint32_t a = rd32(r);
    MtbRect b = qd_auto_position(rd_rect(a), qd_template_position(r, 18));
    uint32_t win = qd_new_window(storage, W_SIZE, b, 0, rd8(a + 10), rd8(a + 12), rd32(a + 14), behind, WK_USER);
    set_port(win);
    hx_log("GetNewCWindow(%d) at %s: %dx%d at (%d,%d)", id, hx_where(), b.right - b.left, b.bottom - b.top, b.left, b.top);
    RETL(win);
}
static void t_ShowHide(uint16_t w) {
    uint32_t win = ARGL(); int show = (w & 0x3FF) == 0x115;
    if (show && !rd8(win + W_Visible)) need_update(win);
    wr8(win + W_Visible, show ? 0xFF : 0);
}
static void t_InvalRect(uint16_t w) { (void)w; ARGL(); need_update(the_port()); }
static void t_SelectWindow(uint16_t w) {
    (void)w; uint32_t win = ARGL();
    if (win == window_list) return;
    uint32_t x = window_list; while (x && rd32(x + W_Next) != win) x = rd32(x + W_Next);
    if (x) { wr32(x + W_Next, rd32(win + W_Next)); wr32(win + W_Next, window_list); window_list = win; }
    for (int i = 0; i < MAXPAL; i++) if (pals[i].h && pals[i].win == win) pal_activate(i);
}
static void t_FrontWindow(uint16_t w) { (void)w; RETL(window_list); }
/* SetWTitle(window, title): the title handle (W_TitleHandle) gets the new Pascal string; the
 * rig's windows have no title bars, so nothing is drawn */
static void t_SetWTitle(uint16_t w) {
    (void)w; uint32_t title = ARGL(), win = ARGL();
    uint32_t h = rd32(win + W_TitleHandle), n = rd8(title) + 1u;
    if (!h) { h = mm_new_handle(n, 0); wr32(win + W_TitleHandle, h); } else mm_set_handle_size(h, n);
    if (h && rd32(h)) { hx_track(rd32(h), n); memmove(g_mem + rd32(h), g_mem + title, n); }
}
static void t_GetWRefCon(uint16_t w) { (void)w; RETL(rd32(ARGL() + W_RefCon)); }
static void t_SetWRefCon(uint16_t w) { (void)w; uint32_t d = ARGL(), win = ARGL(); wr32(win + W_RefCon, d); }
static void t_MoveWindow(uint16_t w) {
    (void)w; ARGW(); int16_t v = ARGW(), h = ARGW(); uint32_t win = ARGL();
    MtbRect pr = rd_rect(win + CP_PortRect);
    uint32_t pm = rd32(rd32(win + CP_PixMap));
    wr_rect(pm + PM_Bounds, (MtbRect){(int16_t)(-v + pr.top), (int16_t)(-h + pr.left), (int16_t)(SCREEN_H - v + pr.top), (int16_t)(SCREEN_W - h + pr.left)});
    MtbRect b = {v, h, (int16_t)(v + pr.bottom - pr.top), (int16_t)(h + pr.right - pr.left)};
    wr_rect(rd32(rd32(win + W_StrucRgn)) + 2, b); wr_rect(rd32(rd32(win + W_ContRgn)) + 2, b);
}
static void t_SizeWindow(uint16_t w) {
    (void)w; ARGW(); int16_t hgt = ARGW(), wid = ARGW(); uint32_t win = ARGL();
    MtbRect pr = rd_rect(win + CP_PortRect); pr.bottom = pr.top + hgt; pr.right = pr.left + wid;
    wr_rect(win + CP_PortRect, pr); wr_rect(rd32(rd32(win + CP_VisRgn)) + 2, pr);
}
static void t_LocalGlobal(uint16_t w) {
    uint32_t pt = ARGL(), port = the_port();
    MtbRect b = rd_rect(port_pixmap(port) + PM_Bounds);
    int sign = (w & 0x3FF) == 0x070 ? -1 : 1;   /* LocalToGlobal subtracts bounds.topLeft */
    wr16(pt, (uint16_t)((int16_t)rd16(pt) + sign * b.top)); wr16(pt + 2, (uint16_t)((int16_t)rd16(pt + 2) + sign * b.left));
}
static void t_Pop4(uint16_t w) { (void)w; ARGL(); }
static void t_Pop8(uint16_t w) { (void)w; ARGL(); ARGL(); }
static void t_HiWord(uint16_t w) { (void)w; uint32_t x = ARGL(); RETW(x >> 16); }
static void t_LoWord(uint16_t w) { (void)w; uint32_t x = ARGL(); RETW(x & 0xFFFF); }
static void t_GetCursor(uint16_t w) { (void)w; int16_t id = ARGW(); RETL(rs_get(FOURCC('C','U','R','S'), id, 0)); }

typedef void (*TrapFn)(uint16_t w);
void traps_register_tb(uint16_t n, TrapFn f);

void qd_register(void) {
    traps_register_tb(0x22A, t_GetMainDevice); traps_register_tb(0x229, t_GetMainDevice); traps_register_tb(0x22B, t_GetNextDevice);
    traps_register_tb(0x232, t_GetGDevice); traps_register_tb(0x231, t_SetGDevice); traps_register_tb(0x22C, t_TestDeviceAttribute);
    traps_register_tb(0x218, t_GetCTable); traps_register_tb(0x224, t_DisposeCTable);
    traps_register_tb(0x291, t_NewPalette); traps_register_tb(0x292, t_GetNewPalette); traps_register_tb(0x293, t_DisposePalette);
    traps_register_tb(0x295, t_NSetPalette); traps_register_tb(0x294, t_ActivatePalette); traps_register_tb(0x296, t_GetPalette);
    traps_register_tb(0x2A2, t_PaletteDispatch); traps_register_tb(0x297, t_PmForeColor); traps_register_tb(0x298, t_PmBackColor);
    traps_register_tb(0x214, t_RGBForeColor); traps_register_tb(0x215, t_RGBBackColor);
    traps_register_tb(0x062, t_ForeColor); traps_register_tb(0x063, t_BackColor);
    traps_register_tb(0x073, t_SetPort); traps_register_tb(0x074, t_GetPort); traps_register_tb(0x07B, t_ClipRect);
    traps_register_tb(0x0A2, t_PaintRect); traps_register_tb(0x0A1, t_FrameRect); traps_register_tb(0x0A3, t_EraseRect);
    traps_register_tb(0x093, t_MoveTo); traps_register_tb(0x094, t_Move); traps_register_tb(0x091, t_LineTo); traps_register_tb(0x092, t_Line);
    traps_register_tb(0x09E, t_PenNormal); traps_register_tb(0x098, t_GetPenState); traps_register_tb(0x099, t_SetPenState);
    traps_register_tb(0x087, t_TextFont); traps_register_tb(0x08A, t_TextSize); traps_register_tb(0x088, t_TextFace); traps_register_tb(0x089, t_TextMode);
    traps_register_tb(0x100, t_GetFNum); traps_register_tb(0x084, t_DrawString); traps_register_tb(0x1CE, t_TETextBox);
    traps_register_tb(0x0A7, t_SetRect); traps_register_tb(0x0A8, t_OffsetRect); traps_register_tb(0x0A9, t_InsetRect); traps_register_tb(0x0AA, t_SectRect);
    traps_register_tb(0x080, t_SetPt); traps_register_tb(0x0D8, t_NewRgn); traps_register_tb(0x0D9, t_DisposeRgn); traps_register_tb(0x0DF, t_RectRgn);
    traps_register_tb(0x0DC, t_CopyRgn); traps_register_tb(0x0E5, t_UnionRgn); traps_register_tb(0x0EC, t_CopyBits);
    traps_register_tb(0x31D, t_QDExtensions);
    traps_register_tb(0x246, t_GetNewCWindow); traps_register_tb(0x115, t_ShowHide); traps_register_tb(0x116, t_ShowHide);
    traps_register_tb(0x11F, t_SelectWindow); traps_register_tb(0x124, t_FrontWindow); traps_register_tb(0x12C, t_FindWindow); traps_register_tb(0x11A, t_SetWTitle); traps_register_tb(0x083, t_DrawChar); traps_register_tb(0x1BC, t_GetPicture); traps_register_tb(0x0F6, t_DrawPicture); traps_register_tb(0x117, t_GetWRefCon);
    traps_register_tb(0x118, t_SetWRefCon); traps_register_tb(0x11B, t_MoveWindow); traps_register_tb(0x11D, t_SizeWindow);
    traps_register_tb(0x070, t_LocalGlobal); traps_register_tb(0x071, t_LocalGlobal);
    traps_register_tb(0x128, t_InvalRect); traps_register_tb(0x12A, t_Pop4); traps_register_tb(0x122, t_Pop4); traps_register_tb(0x123, t_Pop4); /* Inval/ValidRect, Begin/EndUpdate */
    traps_register_tb(0x10D, t_Pop8); traps_register_tb(0x10A, t_Pop8);   /* PaintBehind, CalcVisBehind */
    void dlg_register(void); dlg_register();
    traps_register_tb(0x06A, t_HiWord); traps_register_tb(0x06B, t_LoWord); traps_register_tb(0x1B9, t_GetCursor);
}

/* GWorld dumps for inspection */
void qd_dump_gworlds(const char *dir) {
    for (int i = 0; i < n_gworlds; i++) {
        uint32_t pm = port_pixmap(gworlds[i]);
        MtbPix8 v = pix_of(pm); MtbCTab t; ctab_read(rd32(pm + PM_Table), &t);
        int w = v.bounds.right - v.bounds.left, h = v.bounds.bottom - v.bounds.top;
        char p[512]; snprintf(p, sizeof p, "%s/gworld%d.ppm", dir, i);
        FILE *f = fopen(p, "wb"); if (!f) continue;
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            MtbRGB c = t.c[v.base[y * v.rowbytes + x]];
            uint8_t px[3] = {(uint8_t)(c.red >> 8), (uint8_t)(c.green >> 8), (uint8_t)(c.blue >> 8)}; fwrite(px, 1, 3, f);
        }
        fclose(f);
    }
}

/* the screen as 0xAARRGGBB pixels through the main device's colour table (the port's display) */
void qd_screen_argb(uint32_t *out) {
    MtbCTab t; ctab_read(gd_ctab(main_gd), &t);
    uint32_t pal[256];
    for (int i = 0; i < 256; i++) pal[i] = 0xFF000000u | (uint32_t)(t.c[i].red >> 8) << 16 | (uint32_t)(t.c[i].green >> 8) << 8 | (t.c[i].blue >> 8);
    const uint8_t *s = g_mem + SCREEN_BASE;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) out[i] = pal[s[i]];
}

/* screen dump for inspection: 8-bit PPM via the main device colour table */
void qd_dump_screen(const char *path) {
    MtbCTab t; ctab_read(gd_ctab(main_gd), &t);
    FILE *f = fopen(path, "wb"); if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        MtbRGB c = t.c[g_mem[SCREEN_BASE + i]];
        uint8_t px[3] = {(uint8_t)(c.red >> 8), (uint8_t)(c.green >> 8), (uint8_t)(c.blue >> 8)};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}
