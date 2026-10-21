/* CODE segment 2: putting a rendered frame on screen (see build/disasm/seg2.s, $0000-$0D98).
 *
 * The 3D view is drawn in two layers. Walls and sprites go into an offscreen GWorld (G_22C4) through
 * the edge rasterizer; the floor and ceiling are described per screen row by two "edge lists", one
 * for this frame (G_1DEC) and one for the previous frame (G_1DE8), swapped once drawn. A list has
 * $CC bytes per row: words whose low 6 bits are a colour index and whose upper 10 bits are the x
 * where that colour ends ($FFFF ends the row). fn_S2_0986 compares the two lists and fills only the
 * runs whose colour changed, either into the GWorld or straight into screen memory. */
#include "mz.h"
#include "fns.h"

/* fn_S2_0000 (JT 104): fills G_1CE0[16] with the 16 palette entries as solid fill patterns for the
 * depth of the device that port / gdh select: the index replicated across a long (1-bit: entry 0 is
 * all ones, the others zero). The current GWorld is put back afterwards. */
void fn_S2_0000(uint32_t port, uint32_t gdh) {
    MZ_FRAME;
    uint32_t saved = mz_local(8);                 /* the original's -$A (port) and -$E (device) */
    mac_GetGWorld(saved + 4, saved);
    mac_SetGWorld(port, gdh);
    uint32_t pm = r32(deref(mac_GetGDevice()) + 0x16);
    int16_t depth = (int16_t)r16(deref(pm) + 0x20);
    uint32_t tbl = GADDR(0x1CE0);
    switch (depth) {
    case 1:
        w32(tbl, 0xFFFFFFFFu);
        for (int16_t i = 1; i < 0x10; i++) w32(tbl + 4u * (uint32_t)i, 0);
        break;
    case 4: case 8: case 16: case 32:
        for (int16_t i = 0; i < 0x10; i++) {
            uint32_t v = (uint32_t)mac_Entry2Index(i);
            if (depth == 4)  { v &= 0xF; v |= v << 4; v |= v << 8; v |= v << 16; }
            if (depth == 8)  { v &= 0xFF; v |= v << 8; v |= v << 16; }
            if (depth == 16) { v &= 0xFFFF; v |= v << 16; }
            w32(tbl + 4u * (uint32_t)i, v);
        }
        break;
    default: break;
    }
    mac_SetGWorld(r32(saved + 4), r32(saved));
}

/* ---------- fn_S2_0986: floor and ceiling runs, 8 bits per pixel ---------- */

/* A run as the fill loop reads it: the address of its first pixel, the fill pattern, and a long
 * with bit 24 = starts on an odd address (one byte first), bits 16-23 = the low byte of the
 * remaining width (bit 17: a word at the end, bit 16: a byte), low word = that width (longs: >> 2). */
static uint32_t put_run(uint32_t out, uint32_t row, uint16_t x, uint16_t w, uint32_t pattern) {
    uint32_t addr = row + x;
    w32(out, addr); w32(out + 4, pattern);
    if (addr & 1) w--;
    w32(out + 8, (addr & 1) << 24 | (uint32_t)(w & 0xFF) << 16 | w);
    return out + 12;
}

/* One row: walks this frame's runs (list `now`, colour ending at each x) against the previous
 * frame's (`was`), and emits a run for every stretch whose colour differs, from the last x covered
 * (`*x`) to where the stretch ends. `end` is the row's right edge. */
static uint32_t diff_row(uint32_t out, uint32_t row, uint32_t now, uint32_t was, uint16_t end) {
    uint32_t tbl = GADDR(0x1CE0);
    uint16_t x = 4;
    uint16_t c_was = r16(was) & 0x3F; was += 2;
    uint16_t c_now = r16(now) & 0x3F; now += 2;
    uint32_t pattern = r32(tbl + 4u * c_now);
    for (int last = 0; !last; ) {
        uint16_t e_now;
        if (r16(now) == 0xFFFF) { e_now = end; last = 1; }
        else e_now = r16(now) >> 6;
        /* the previous frame's runs that end inside this one */
        for (;;) {
            uint16_t e_was = r16(was);
            if (e_was == 0xFFFF) break;
            e_was >>= 6;
            if (e_was >= e_now) break;
            if (c_was != c_now) {
                if (e_was > x) { out = put_run(out, row, x, (uint16_t)(e_was - x), pattern); x = e_was; }
            } else if (e_was > x) x = e_was;
            c_was = r16(was) & 0x3F; was += 2;
        }
        /* the rest of this run */
        if (c_was != c_now) {
            if (e_now > x) { out = put_run(out, row, x, (uint16_t)(e_now - x), pattern); x = e_now; }
        } else if (e_now > x) x = e_now;
        if (last) break;
        c_now = r16(now) & 0x3F; now += 2;
        pattern = r32(tbl + 4u * c_now);
    }
    return out;
}

/* Waits for the next tick when asked to (G_2394 for direct drawing): on the slot VBL counter
 * G_21AE when one was installed (G_22B7), otherwise on the Ticks counter G_241E points at. */
static void wait_tick(void) {
    if (g8(0x22B7)) {
        uint32_t t = g32(0x21AE);
        while (g32(0x21AE) == t) mz_idle();
    } else {
        uint32_t a = g32(0x241E), t = r32(a);
        while (r32(a) == t) mz_idle();
    }
}

/* fn_S2_0986: draws the floor and ceiling runs that changed into pixmap pm (8 bits per pixel), the
 * view's top-left at global (x, y). Rows come from G_2150 (with `mode` 2 or 3 each list row is drawn
 * 2 or 3 times, for the lower-resolution views), the right edge from G_2156 + 4. The run list is
 * built in G_1DF4 (at most 2 * G_2150 - 10 rows' worth), the two edge lists are swapped, then (after
 * an optional wait for the next tick) the runs are filled with MMU 32-bit mode on. */
void fn_S2_0986(uint32_t pm, int16_t x, int16_t y, uint32_t now, uint32_t was, uint8_t wait, int16_t mode) {
    uint32_t base = mac_GetPixBaseAddr(pm);
    uint16_t rowbytes = r16(deref(pm) + 4) & 0x3FFF;
    int16_t top = (int16_t)r16(deref(pm) + 6), left = (int16_t)r16(deref(pm) + 8);
    uint32_t out = g32(0x1DF4);
    int16_t full = (int16_t)(g16(0x2150) * 2 - 10);
    uint32_t limit = out + (uint32_t)(uint16_t)(full < 0 ? 0 : full) * 0xCC;
    uint16_t end = (uint16_t)(g16(0x2156) + 4);
    int16_t rows = (int16_t)g16(0x2150);
    uint32_t row = base + (uint32_t)(uint16_t)(y - top) * rowbytes + (uint32_t)(uint16_t)(x - left) - 4;
    int16_t step = (mode == 2 || mode == 3) ? mode : 1;
    uint32_t a = now + 2, b = was + 2;
    for (;;) {
        out = diff_row(out, row, a, b, end);
        rows = (int16_t)(rows - step);
        if (step == 1 ? rows == 0 : rows < step) break;
        if (out >= limit) break;
        a += 0xCCu * (uint32_t)step; b += 0xCCu * (uint32_t)step;
        row += (uint32_t)rowbytes * (uint32_t)step;
    }
    w32(out, 0);
    uint32_t t = g32(0x1DEC); sg32(0x1DEC, g32(0x1DE8)); sg32(0x1DE8, t);

    uint32_t list = (mz_seg(3), fn_S3_03A0(g32(0x1DF4)));
    uint8_t mmu = mac_SwapMMUMode(1);
    if (wait) wait_tick();
    uint32_t copies[2] = { rowbytes, 2u * rowbytes };
    int extra = step - 1;                        /* the same pixels again one and two rows down */
    for (uint32_t p = list; r32(p); p += 12) {
        uint32_t d = r32(p), pat = r32(p + 4), n = r32(p + 8);
        if (n & 0x1000000) { for (int k = 0; k < extra; k++) w8(d + copies[k], (uint8_t)pat); w8(d, (uint8_t)pat); d++; }
        for (uint16_t i = (uint16_t)(n & 0xFFFF) >> 2; i; i--) { for (int k = 0; k < extra; k++) w32(d + copies[k], pat); w32(d, pat); d += 4; }
        if (n & 0x20000)   { for (int k = 0; k < extra; k++) w16(d + copies[k], (uint16_t)pat); w16(d, (uint16_t)pat); d += 2; }
        if (n & 0x10000)   { for (int k = 0; k < extra; k++) w8(d + copies[k], (uint8_t)pat); w8(d, (uint8_t)pat); d++; }
    }
    mac_SwapMMUMode(mmu);
}

/* The same for the other pixel depths: never used by the port (its screen is 8 bits deep). */
static void draw_runs(int16_t depth, uint32_t pm, int16_t x, int16_t y, uint8_t wait) {
    uint32_t now = g32(0x1DEC), was = g32(0x1DE8);
    int16_t mode = (int16_t)g16(0x2392);
    switch (depth) {
    case 8: fn_S2_0986(pm, x, y, now, was, wait, mode); break;
    case 4: mz_unsupported("4-bit floor runs (fn_S2_0D98)"); break;
    case 16: mz_unsupported("16-bit floor runs (fn_S2_1276)"); break;
    case 32: mz_unsupported("32-bit floor runs (fn_S2_15FC)"); break;
    case 1: mz_unsupported("1-bit floor runs (fn_S2_195C)"); break;
    default: break;
    }
}

static void set_rect(uint32_t r, int16_t top, int16_t left, int16_t bottom, int16_t right) {
    w16(r, (uint16_t)top); w16(r + 2, (uint16_t)left); w16(r + 4, (uint16_t)bottom); w16(r + 6, (uint16_t)right);
}

/* Copies the GWorld (its portRect) to dst in window port `win` (white background, black foreground) */
static void copy_view(uint32_t pm, uint32_t win, uint32_t src_rect, uint32_t dst_rect) {
    mac_ForeColor(33);                           /* blackColor */
    mac_BackColor(30);                           /* whiteColor */
    mac_CopyBits(deref(pm), win + 2, src_rect, dst_rect, 0, 0);
}

/* fn_S2_040A (JT 76): puts the frame on screen. The view rectangle is G_215A..G_2158 (top, bottom)
 * by G_215E..G_215C (left, right), in window G_222E at its offset G_2160 / G_2162 on the screen
 * device G_22BC.
 *  - When the screen can be drawn into directly (G_22B8 clear, depth 1/4/8/16/32), the floor and
 *    ceiling runs go straight to screen memory; walls and sprites were drawn there already.
 *  - Otherwise the runs go into the GWorld G_22C4, which is then copied to the window (after
 *    waiting for the next tick when G_2394 asks for it).
 *  - G_21FC selects the other window, G_2232 (the runs go into the GWorld, which is copied there).
 * Last, the automap GWorld G_22C0 (128x128 at 16,16) is copied to G_2110 / G_2112 in the window,
 * every frame or (with G_2393) every other frame (G_238C odd). */
void fn_S2_040A(void) {
    MZ_FRAME;
    uint32_t r = mz_local(8), src = mz_local(8);
    int16_t gw_depth = 0;
    if (g32(0x22C4)) {
        uint32_t dev = mac_GetGWorldDevice(g32(0x22C4));
        gw_depth = (int16_t)r16(deref(r32(deref(dev) + 0x16)) + 0x20);
    }
    if (g8(0x21FC)) {
        if (!g32(0x22C4)) return;
        fn_S2_0000(g32(0x22C4), 0);
        uint32_t pm = mac_GetGWorldPixMap(g32(0x22C4));
        if (!mac_LockPixels(pm)) return;
        draw_runs(gw_depth, pm, 0, 0, 0);
        uint32_t w = g32(0x2232);
        set_rect(r, (int16_t)(r16(w + 0x10) + g16(0x215A)), (int16_t)(r16(w + 0x12) + g16(0x215E)),
                 (int16_t)(r16(w + 0x10) + g16(0x2158)), (int16_t)(r16(w + 0x12) + g16(0x215C)));
        copy_view(pm, g32(0x2232), g32(0x22C4) + 0x10, r);
        mac_UnlockPixels(pm);
        return;
    }
    int16_t screen_depth = (int16_t)r16(deref(r32(deref(g32(0x22BC)) + 0x16)) + 0x20);
    int direct = !g8(0x22B8) && (screen_depth == 8 || screen_depth == 4 || screen_depth == 16 ||
                                 screen_depth == 32 || screen_depth == 1);
    if (direct) {
        fn_S2_0000(g32(0x222E), g32(0x22BC));
        draw_runs(screen_depth, r32(deref(g32(0x22BC)) + 0x16), (int16_t)(g16(0x2162) + g16(0x215E)),
                  (int16_t)(g16(0x2160) + g16(0x215A)), g8(0x2394));
    } else if (g32(0x22C4)) {
        fn_S2_0000(g32(0x22C4), 0);
        uint32_t pm = mac_GetGWorldPixMap(g32(0x22C4));
        if (mac_LockPixels(pm)) {
            draw_runs(gw_depth, pm, 0, 0, 0);
            set_rect(r, (int16_t)(g16(0x215A) + g16(0x2160)), (int16_t)(g16(0x215E) + g16(0x2162)),
                     (int16_t)(g16(0x2158) + g16(0x2160)), (int16_t)(g16(0x215C) + g16(0x2162)));
            mac_ForeColor(33); mac_BackColor(30);
            if (g8(0x2394)) wait_tick();
            mac_CopyBits(deref(pm), g32(0x222E) + 2, g32(0x22C4) + 0x10, r, 0, 0);
            mac_UnlockPixels(pm);
        }
    }
    /* the automap */
    if (g8(0x2393) && !(g32(0x238C) & 1)) return;
    uint32_t pm = mac_GetGWorldPixMap(g32(0x22C0));
    if (!mac_LockPixels(pm)) return;
    set_rect(src, 0x10, 0x10, 0x90, 0x90);
    int16_t mt = (int16_t)(g16(0x2110) + g16(0x2160)), ml = (int16_t)(g16(0x2112) + g16(0x2162));
    set_rect(r, mt, ml, (int16_t)(mt + 0x80), (int16_t)(ml + 0x80));
    copy_view(pm, g32(0x222E), src, r);
    mac_UnlockPixels(pm);
}
