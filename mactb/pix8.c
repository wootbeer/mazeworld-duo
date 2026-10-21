/* 8-bit QuickDraw primitives. Rectangles follow QuickDraw: top/left inclusive, bottom/right exclusive. */
#include "mactb.h"
#include <string.h>
#include <stdlib.h>

int mtb_sect_rect(const MtbRect *a, const MtbRect *b, MtbRect *o) {
    MtbRect r;
    r.top = a->top > b->top ? a->top : b->top;
    r.left = a->left > b->left ? a->left : b->left;
    r.bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    r.right = a->right < b->right ? a->right : b->right;
    if (r.top >= r.bottom || r.left >= r.right) { *o = (MtbRect){0, 0, 0, 0}; return 0; }
    *o = r; return 1;
}

static inline uint8_t *px(const MtbPix8 *pm, int h, int v) {
    return pm->base + (int32_t)(v - pm->bounds.top) * pm->rowbytes + (h - pm->bounds.left);
}

void mtb_fill_rect8(const MtbPix8 *pm, const MtbRect *r, const MtbRect *clip, uint8_t index) {
    MtbRect a, b;
    if (!mtb_sect_rect(r, &pm->bounds, &a)) return;
    if (clip && !mtb_sect_rect(&a, clip, &b)) return;
    if (!clip) b = a;
    for (int v = b.top; v < b.bottom; v++) memset(px(pm, b.left, v), index, (size_t)(b.right - b.left));
}

void mtb_frame_rect8(const MtbPix8 *pm, const MtbRect *r, const MtbRect *clip, int pw, int ph, uint8_t index) {
    if (r->bottom - r->top <= 0 || r->right - r->left <= 0) return;
    if (2 * pw >= r->right - r->left || 2 * ph >= r->bottom - r->top) { mtb_fill_rect8(pm, r, clip, index); return; }
    MtbRect t = {r->top, r->left, (int16_t)(r->top + ph), r->right};
    MtbRect bo = {(int16_t)(r->bottom - ph), r->left, r->bottom, r->right};
    MtbRect l = {(int16_t)(r->top + ph), r->left, (int16_t)(r->bottom - ph), (int16_t)(r->left + pw)};
    MtbRect rr = {(int16_t)(r->top + ph), (int16_t)(r->right - pw), (int16_t)(r->bottom - ph), r->right};
    mtb_fill_rect8(pm, &t, clip, index); mtb_fill_rect8(pm, &bo, clip, index);
    mtb_fill_rect8(pm, &l, clip, index); mtb_fill_rect8(pm, &rr, clip, index);
}

void mtb_line8(const MtbPix8 *pm, int h0, int v0, int h1, int v1, const MtbRect *clip, uint8_t index) {
    MtbRect c;
    if (clip) { if (!mtb_sect_rect(clip, &pm->bounds, &c)) return; } else c = pm->bounds;
    int dh = abs(h1 - h0), dv = abs(v1 - v0), sh = h0 < h1 ? 1 : -1, sv = v0 < v1 ? 1 : -1;
    int err = dh - dv;
    for (;;) {
        if (h0 >= c.left && h0 < c.right && v0 >= c.top && v0 < c.bottom) *px(pm, h0, v0) = index;
        if (h0 == h1 && v0 == v1) break;
        int e2 = 2 * err;
        if (e2 > -dv) { err -= dv; h0 += sh; }
        if (e2 < dh) { err += dh; v0 += sv; }
    }
}

void mtb_copybits8(const MtbPix8 *src, const MtbRect *sr, const MtbPix8 *dst, const MtbRect *dr,
                   const MtbRect *clip, const uint8_t *xlat) {
    int sw = sr->right - sr->left, shh = sr->bottom - sr->top;
    int dw = dr->right - dr->left, dh = dr->bottom - dr->top;
    if (sw <= 0 || shh <= 0 || dw <= 0 || dh <= 0) return;
    MtbRect area;
    if (!mtb_sect_rect(dr, &dst->bounds, &area)) return;
    if (clip && !mtb_sect_rect(&area, clip, &area)) return;
    for (int v = area.top; v < area.bottom; v++) {
        int sv = sr->top + (int)((int64_t)(v - dr->top) * shh / dh);
        if (sv < src->bounds.top || sv >= src->bounds.bottom) continue;
        uint8_t *d = px(dst, area.left, v);
        if (sw == dw && !xlat && area.left - dr->left + sr->left >= src->bounds.left && area.right - dr->left + sr->left <= src->bounds.right) {
            memmove(d, px(src, sr->left + (area.left - dr->left), sv), (size_t)(area.right - area.left));
            continue;
        }
        for (int h = area.left; h < area.right; h++, d++) {
            int sh = sr->left + (int)((int64_t)(h - dr->left) * sw / dw);
            if (sh < src->bounds.left || sh >= src->bounds.right) continue;
            uint8_t p = *px(src, sh, sv);
            *d = xlat ? xlat[p] : p;
        }
    }
}
