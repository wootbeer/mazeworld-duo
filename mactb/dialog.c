/* Dialog Manager drawing: the modal dialog frame and the standard item kinds
 * (push button, check box, radio button, static text, edit text), in the system font.
 *
 * FIDELITY: shapes follow the System 7 black-and-white look (rounded push buttons,
 * 12x12 check boxes, double-line modal frame), drawn with this project's font. Exact
 * pixel shapes of Apple's CDEF/WDEF are modelled, not copied. No text caret is drawn. */
#include "mactb.h"

static void hline(const MtbPix8 *pm, const MtbRect *clip, int h0, int h1, int v, uint8_t c) {
    MtbRect r = {(int16_t)v, (int16_t)h0, (int16_t)(v + 1), (int16_t)(h1 + 1)};
    mtb_fill_rect8(pm, &r, clip, c);
}
static void vline(const MtbPix8 *pm, const MtbRect *clip, int h, int v0, int v1, uint8_t c) {
    MtbRect r = {(int16_t)v0, (int16_t)h, (int16_t)(v1 + 1), (int16_t)(h + 1)};
    mtb_fill_rect8(pm, &r, clip, c);
}

/* Rounded frame with corner radius rad (1-pixel pen), corners drawn by the midpoint circle rule. */
static void frame_round8(const MtbPix8 *pm, const MtbRect *clip, const MtbRect *r, int rad, uint8_t c) {
    int w = r->right - r->left, h = r->bottom - r->top;
    if (rad * 2 > w) rad = w / 2;
    if (rad * 2 > h) rad = h / 2;
    int L = r->left, T = r->top, R = r->right - 1, B = r->bottom - 1;
    hline(pm, clip, L + rad, R - rad, T, c); hline(pm, clip, L + rad, R - rad, B, c);
    vline(pm, clip, L, T + rad, B - rad, c); vline(pm, clip, R, T + rad, B - rad, c);
    int x = rad, y = 0, err = 1 - rad;
    while (x >= y) {   /* one octant, mirrored into the four corners */
        int pts[2][2] = {{x, y}, {y, x}};
        for (int k = 0; k < 2; k++) {
            int dx = pts[k][0], dy = pts[k][1];
            MtbRect p;
            int hs[4] = {L + rad - dx, R - rad + dx, L + rad - dx, R - rad + dx};
            int vs[4] = {T + rad - dy, T + rad - dy, B - rad + dy, B - rad + dy};
            for (int q = 0; q < 4; q++) { p = (MtbRect){(int16_t)vs[q], (int16_t)hs[q], (int16_t)(vs[q] + 1), (int16_t)(hs[q] + 1)}; mtb_fill_rect8(pm, &p, clip, c); }
        }
        y++;
        if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
}

void mtb_dialog_frame8(const MtbPix8 *pm, const MtbRect *clip, const MtbRect *content, const MtbDColors *col) {
    MtbRect r = {(int16_t)(content->top - 8), (int16_t)(content->left - 8), (int16_t)(content->bottom + 8), (int16_t)(content->right + 8)};
    mtb_fill_rect8(pm, &r, clip, col->white);
    mtb_frame_rect8(pm, &r, clip, 1, 1, col->black);
    MtbRect in = {(int16_t)(r.top + 2), (int16_t)(r.left + 2), (int16_t)(r.bottom - 2), (int16_t)(r.right - 2)};
    mtb_frame_rect8(pm, &in, clip, 3, 3, col->black);
}

static int baseline_centred(const MtbRect *r, int size) {
    int a, d, l, wm; mtb_font_metrics(size, &a, &d, &l, &wm);
    return r->top + (r->bottom - r->top - (a + d)) / 2 + a;
}

void mtb_dialog_item8(const MtbPix8 *pm, const MtbRect *clip, const MtbDItem *it, const MtbDColors *col) {
    const int size = 12;   /* system font */
    MtbRect r = it->rect, c;
    if (!mtb_sect_rect(clip, &pm->bounds, &c)) return;
    int dim = it->hilite == 255;
    uint8_t ink = dim ? col->grey : col->black;
    switch (it->kind & 0x7F) {
    case MTB_DI_BUTTON: {
        int pressed = it->hilite > 0 && it->hilite < 254;   /* part code 1..253: drawn inverted */
        mtb_fill_rect8(pm, &r, &c, col->white);
        frame_round8(pm, &c, &r, 8, col->black);
        if (pressed) {   /* FIDELITY: the inside is filled to the frame, corners included */
            MtbRect in = {(int16_t)(r.top + 1), (int16_t)(r.left + 1), (int16_t)(r.bottom - 1), (int16_t)(r.right - 1)};
            mtb_fill_rect8(pm, &in, &c, col->black); ink = col->white;
        }
        int tw = mtb_text_width(size, 0, it->text, it->len);
        int h = r.left + (r.right - r.left - tw) / 2, v = baseline_centred(&r, size);
        MtbRect tc; MtbRect inner = {(int16_t)(r.top + 1), (int16_t)(r.left + 1), (int16_t)(r.bottom - 1), (int16_t)(r.right - 1)};
        if (mtb_sect_rect(&c, &inner, &tc)) mtb_draw_text8(pm, &tc, h, v, it->text, it->len, size, 0, 1, ink, col->white);
        break; }
    case MTB_DI_CHECKBOX: case MTB_DI_RADIO: {
        mtb_fill_rect8(pm, &r, &c, col->white);
        int v0 = r.top + (r.bottom - r.top - 12) / 2;
        MtbRect box = {(int16_t)v0, (int16_t)(r.left + 2), (int16_t)(v0 + 12), (int16_t)(r.left + 14)};
        if ((it->kind & 0x7F) == MTB_DI_CHECKBOX) {
            mtb_frame_rect8(pm, &box, &c, 1, 1, ink);
            if (it->value) {
                mtb_line8(pm, box.left, box.top, box.right - 1, box.bottom - 1, &c, ink);
                mtb_line8(pm, box.right - 1, box.top, box.left, box.bottom - 1, &c, ink);
            }
        } else {
            frame_round8(pm, &c, &box, 6, ink);
            if (it->value) { MtbRect dot = {(int16_t)(box.top + 3), (int16_t)(box.left + 3), (int16_t)(box.bottom - 3), (int16_t)(box.right - 3)}; mtb_fill_rect8(pm, &dot, &c, ink); }
        }
        mtb_draw_text8(pm, &c, r.left + 17, baseline_centred(&r, size), it->text, it->len, size, 0, 1, ink, col->white);
        break; }
    case MTB_DI_STATIC:
        mtb_textbox8(pm, &c, &r, it->text, it->len, size, 0, 1, col->black, col->white, 0);
        break;
    case MTB_DI_EDIT: {
        MtbRect f = {(int16_t)(r.top - 3), (int16_t)(r.left - 3), (int16_t)(r.bottom + 3), (int16_t)(r.right + 3)};
        mtb_fill_rect8(pm, &f, &c, col->white);
        mtb_frame_rect8(pm, &f, &c, 1, 1, col->black);
        mtb_textbox8(pm, &c, &r, it->text, it->len, size, 0, 1, col->black, col->white, 0);
        break; }
    default:   /* icons, pictures, user items: drawn by their owners */
        break;
    }
}
