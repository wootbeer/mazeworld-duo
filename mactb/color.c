/* Colour tables, colour matching, Palette Manager model, QuickDraw Random. */
#include "mactb.h"
#include <string.h>

static const uint16_t ramp[10] = {0xEEEE, 0xDDDD, 0xBBBB, 0xAAAA, 0x8888, 0x7777, 0x5555, 0x4444, 0x2222, 0x1111};

/* Apple's standard 16-colour table (clut 4). */
static const MtbRGB clut4[16] = {
    {0xFFFF, 0xFFFF, 0xFFFF}, {0xFC00, 0xF37D, 0x052F}, {0xFFFF, 0x648A, 0x028C}, {0xDD6B, 0x08C2, 0x06A2},
    {0xF2D7, 0x0856, 0x84EC}, {0x46E3, 0x0000, 0xA53E}, {0x0000, 0x0000, 0xD400}, {0x0241, 0xAB54, 0xEAFF},
    {0x1F21, 0xB793, 0x1431}, {0x0000, 0x64AF, 0x11B0}, {0x5600, 0x2C9D, 0x0524}, {0x90D7, 0x7160, 0x3A34},
    {0xC000, 0xC000, 0xC000}, {0x8000, 0x8000, 0x8000}, {0x4000, 0x4000, 0x4000}, {0x0000, 0x0000, 0x0000},
};

int mtb_std_clut(int id, MtbCTab *t) {
    memset(t, 0, sizeof *t);
    t->seed = (uint32_t)id;
    switch (id) {
    case 1: case 33:
        t->count = 2; t->c[0] = (MtbRGB){0xFFFF, 0xFFFF, 0xFFFF}; return 0;
    case 2: case 34:
        t->count = 4;
        for (int i = 0; i < 4; i++) { uint16_t v = (uint16_t)(0xFFFF - i * 0x5555); t->c[i] = (MtbRGB){v, v, v}; }
        if (id == 2) { t->c[1] = (MtbRGB){0xAC00, 0xAC00, 0xFFFF}; t->c[2] = (MtbRGB){0x3000, 0x3000, 0x9C00}; }  /* clut 2 has two blues */
        return 0;
    case 4: t->count = 16; memcpy(t->c, clut4, sizeof clut4); return 0;
    case 36:
        t->count = 16;
        for (int i = 0; i < 16; i++) { uint16_t v = (uint16_t)(0xFFFF - i * 0x1111); t->c[i] = (MtbRGB){v, v, v}; }
        return 0;
    case 8: {
        t->count = 256;
        for (int i = 0; i < 215; i++) {
            t->c[i].red = (uint16_t)(0xFFFF - (i / 36) * 0x3333);
            t->c[i].green = (uint16_t)(0xFFFF - ((i / 6) % 6) * 0x3333);
            t->c[i].blue = (uint16_t)(0xFFFF - (i % 6) * 0x3333);
        }
        for (int k = 0; k < 10; k++) {
            t->c[215 + k] = (MtbRGB){ramp[k], 0, 0};
            t->c[225 + k] = (MtbRGB){0, ramp[k], 0};
            t->c[235 + k] = (MtbRGB){0, 0, ramp[k]};
            t->c[245 + k] = (MtbRGB){ramp[k], ramp[k], ramp[k]};
        }
        t->c[255] = (MtbRGB){0, 0, 0};
        return 0; }
    case 40:
        t->count = 256;
        for (int i = 0; i < 256; i++) { uint16_t v = (uint16_t)((255 - i) * 0x0101); t->c[i] = (MtbRGB){v, v, v}; }
        return 0;
    }
    return -1;
}

static uint32_t next_seed = 1024;
uint32_t mtb_new_ctseed(void) { return ++next_seed; }

static uint64_t dist2(MtbRGB a, MtbRGB b) {
    int64_t dr = (int64_t)a.red - b.red, dg = (int64_t)a.green - b.green, db = (int64_t)a.blue - b.blue;
    return (uint64_t)(dr * dr + dg * dg + db * db);
}

int mtb_color_to_index(const MtbCTab *t, MtbRGB c) {
    int best = 0; uint64_t bd = UINT64_MAX;
    for (int i = 0; i < t->count; i++) { uint64_t d = dist2(t->c[i], c); if (d < bd) { bd = d; best = i; } }
    return best;
}

static int within(MtbRGB a, MtbRGB b, uint16_t tol) {
    int dr = a.red > b.red ? a.red - b.red : b.red - a.red;
    int dg = a.green > b.green ? a.green - b.green : b.green - a.green;
    int db = a.blue > b.blue ? a.blue - b.blue : b.blue - a.blue;
    return dr <= tol && dg <= tol && db <= tol;
}

int mtb_pal_activate(MtbPalette *p, MtbCTab *dev) {
    int changed = 0;
    uint8_t claimed[256] = {0};
    for (int i = 0; i < p->count; i++) {
        MtbPalEntry *e = &p->e[i];
        e->devindex = -1;
        if (e->usage & MTB_PM_EXPLICIT) { e->devindex = i % dev->count; continue; }
        if (!(e->usage & (MTB_PM_TOLERANT | MTB_PM_ANIMATED))) continue;    /* courteous: matched on use */
        if (e->usage & MTB_PM_TOLERANT) {
            for (int k = 0; k < dev->count; k++) if (!claimed[k] && within(dev->c[k], e->rgb, e->tolerance)) { e->devindex = k; break; }
            if (e->devindex >= 0) { claimed[e->devindex] = 1; continue; }
        }
        for (int k = dev->count - 2; k >= 1; k--) if (!claimed[k]) {
            int used_exact = 0;  /* keep entries that another palette entry already matched */
            for (int j = 0; j < i; j++) if (p->e[j].devindex == k) used_exact = 1;
            if (used_exact) continue;
            claimed[k] = 1; e->devindex = k;
            if (dev->c[k].red != e->rgb.red || dev->c[k].green != e->rgb.green || dev->c[k].blue != e->rgb.blue) { dev->c[k] = e->rgb; changed = 1; }
            break;
        }
    }
    if (changed) dev->seed = mtb_new_ctseed();
    return changed;
}

int mtb_pal_entry_index(const MtbPalette *p, const MtbCTab *dev, int entry) {
    if (entry < 0 || entry >= p->count) return 0;
    if (p->e[entry].devindex >= 0) return p->e[entry].devindex;
    return mtb_color_to_index(dev, p->e[entry].rgb);
}

int16_t mtb_qd_random(uint32_t *seed) {
    uint64_t s = *seed ? *seed : 1;
    s = (s * 16807u) % 0x7FFFFFFFu;
    *seed = (uint32_t)s;
    uint16_t r = (uint16_t)(s & 0xFFFF);
    return (int16_t)(r == 0x8000 ? 0 : r);
}
