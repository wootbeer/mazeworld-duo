/* Text: DrawString-style drawing and TETextBox layout on 8-bit pixmaps. */
#include "mactb.h"
#include "font_data.h"
#include <string.h>

const MtbFont *mtb_font_for_size(int size) {
    if (size <= 0) size = 12;                       /* size 0 = the application font's default */
    if (size <= 10) return &mtb_font9;
    if (size <= 13) return &mtb_font12;
    if (size <= 16) return &mtb_font14;
    return &mtb_font18;
}

void mtb_font_metrics(int size, int *ascent, int *descent, int *leading, int *widmax) {
    const MtbFont *f = mtb_font_for_size(size);
    int wm = 0; for (int i = 0; i < 256; i++) if (f->glyphs[i].advance > wm) wm = f->glyphs[i].advance;
    if (ascent) *ascent = f->ascent;
    if (descent) *descent = f->descent;
    if (leading) *leading = f->leading;
    if (widmax) *widmax = wm;
}

static const MtbGlyph *glyph(const MtbFont *f, uint8_t c) {
    const MtbGlyph *g = &f->glyphs[c];
    if (!g->advance) g = &f->glyphs['?'];             /* missing glyph */
    return g;
}

int mtb_char_width(int size, int face, uint8_t c) {
    (void)face;
    return glyph(mtb_font_for_size(size), c)->advance;
}

int mtb_text_width(int size, int face, const uint8_t *s, int n) {
    int w = 0; for (int i = 0; i < n; i++) w += mtb_char_width(size, face, s[i]);
    return w;
}

/* Draws n bytes with the baseline at v, starting at h. mode: 0 srcCopy (cell background in bk),
 * 1 srcOr (ink only). face bit 2 = underline. Returns the new pen h. */
int mtb_draw_text8(const MtbPix8 *pm, const MtbRect *clip, int h, int v, const uint8_t *s, int n,
                   int size, int face, int mode, uint8_t fg, uint8_t bk) {
    const MtbFont *f = mtb_font_for_size(size);
    MtbRect c;
    if (clip) { if (!mtb_sect_rect(clip, &pm->bounds, &c)) return h + mtb_text_width(size, face, s, n); }
    else c = pm->bounds;
    int h0 = h;
    for (int i = 0; i < n; i++) {
        const MtbGlyph *g = glyph(f, s[i]);
        for (int y = 0; y < f->ascent + f->descent; y++) {
            int py = v - f->ascent + y;
            if (py < c.top || py >= c.bottom) continue;
            uint32_t bits = f->rows[g->row + y];
            uint8_t *row = pm->base + (int32_t)(py - pm->bounds.top) * pm->rowbytes - pm->bounds.left;
            for (int x = 0; x < g->advance; x++) {
                int px = h + x;
                if (px < c.left || px >= c.right) continue;
                int ink = x < g->width && (bits >> (31 - x)) & 1;
                if (ink) row[px] = fg; else if (mode == 0) row[px] = bk;
            }
        }
        h += g->advance;
    }
    if (face & 4) {   /* underline one pixel below the baseline */
        int py = v + 1;
        if (py >= c.top && py < c.bottom)
            for (int px = h0; px < h; px++) if (px >= c.left && px < c.right) pm->base[(int32_t)(py - pm->bounds.top) * pm->rowbytes + px - pm->bounds.left] = fg;
    }
    return h;
}

/* TETextBox: word-wraps text into box, honours CR, justification 0 left, 1 centre, -1 right.
 * The box is erased to bk first, as TETextBox does. */
void mtb_textbox8(const MtbPix8 *pm, const MtbRect *clip, const MtbRect *box, const uint8_t *s, int n,
                  int size, int face, int mode, uint8_t fg, uint8_t bk, int just) {
    MtbRect cb;
    if (!mtb_sect_rect(box, clip ? clip : &pm->bounds, &cb)) return;
    mtb_fill_rect8(pm, box, clip, bk);
    const MtbFont *f = mtb_font_for_size(size);
    int lh = f->ascent + f->descent + f->leading, maxw = box->right - box->left;
    int v = box->top + f->ascent, i = 0;
    while (i < n && v - f->ascent < box->bottom) {
        /* find the end of this line */
        int j = i, w = 0, lastbreak = -1;
        while (j < n && s[j] != '\r') {
            int cw = mtb_char_width(size, face, s[j]);
            if (w + cw > maxw && j > i) break;
            if (s[j] == ' ') lastbreak = j;
            w += cw; j++;
        }
        int end = j, next = j;
        if (j < n && s[j] != '\r' && lastbreak >= i) { end = lastbreak; next = lastbreak + 1; }
        else if (j < n && s[j] == '\r') next = j + 1;
        int tw = mtb_text_width(size, face, s + i, end - i);
        while (end > i && s[end - 1] == ' ') { tw -= mtb_char_width(size, face, ' '); end--; }   /* trailing blanks don't count */
        int h = box->left + (just == 1 ? (maxw - tw) / 2 : just == -1 ? maxw - tw : 0);
        mtb_draw_text8(pm, &cb, h, v, s + i, end - i, size, face, mode, fg, bk);
        v += lh; i = next;
    }
}
