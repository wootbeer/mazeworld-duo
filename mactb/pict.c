/* QuickDraw pictures ('PICT', version 2), as far as the games' pictures need: header, clip
 * region, comments, and PackBitsRect images (8-bit with a colour table, or 1-bit), drawn with
 * srcCopy into an 8-bit pixmap. The picture frame is mapped onto the destination rectangle (scaled
 * by nearest pixel when the sizes differ), as DrawPicture does. */
#include "mactb.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static unsigned be16(const uint8_t *p) { return (unsigned)p[0] << 8 | p[1]; }
static int16_t s16(const uint8_t *p) { return (int16_t)be16(p); }
static MtbRect rect_at(const uint8_t *p) { return (MtbRect){s16(p), s16(p + 2), s16(p + 4), s16(p + 6)}; }

/* a point of the picture frame mapped onto dst */
static int map_h(int x, const MtbRect *f, const MtbRect *d) { return d->left + (x - f->left) * (d->right - d->left) / (f->right - f->left); }
static int map_v(int y, const MtbRect *f, const MtbRect *d) { return d->top + (y - f->top) * (d->bottom - d->top) / (f->bottom - f->top); }

/* one PackBits row of rowbytes bytes; returns bytes of input used, or -1 */
static int unpack_row(const uint8_t *in, int avail, uint8_t *out, int rowbytes) {
    int i = 0, o = 0;
    while (o < rowbytes) {
        if (i >= avail) return -1;
        int n = (int8_t)in[i++];
        if (n >= 0) { n++; if (i + n > avail || o + n > rowbytes) return -1; memcpy(out + o, in + i, (size_t)n); i += n; o += n; }
        else if (n != -128) { n = 1 - n; if (i >= avail || o + n > rowbytes) return -1; memset(out + o, in[i++], (size_t)n); o += n; }
    }
    return i;
}

int mtb_pict_draw8(const uint8_t *pic, uint32_t len, const MtbRect *dst, const MtbPix8 *pm, const MtbRect *clip,
                   const MtbCTab *dev, char *err, int errlen) {
    if (len < 12) { snprintf(err, (size_t)errlen, "picture too short"); return -1; }
    MtbRect frame = rect_at(pic + 2);
    if (frame.right <= frame.left || frame.bottom <= frame.top) return 0;
    uint32_t p = 10;
    if (be16(pic + 10) != 0x0011 || be16(pic + 12) != 0x02FF) { snprintf(err, (size_t)errlen, "not a version 2 picture"); return -1; }
    p = 14;
    while (p + 2 <= len) {
        if (p & 1) p++;
        unsigned op = be16(pic + p); p += 2;
        switch (op) {
        case 0x0000: case 0x001E: break;                           /* NOP, DefHilite */
        case 0x0C00: p += 24; break;                               /* header */
        case 0x0001: p += be16(pic + p); break;                    /* clip region: the frame is the clip */
        case 0x00A0: p += 2; break;                                /* short comment */
        case 0x00A1: p += 4 + be16(pic + p + 2); break;            /* long comment */
        case 0x00FF: return 0;                                     /* end */
        case 0x0098: {                                             /* PackBitsRect */
            unsigned rb = be16(pic + p); int pixmap = (rb & 0x8000) != 0; rb &= 0x3FFF;
            MtbRect bounds = rect_at(pic + p + 2);
            int depth = pixmap ? (int)be16(pic + p + 2 + 8 + 18) : 1;   /* pixelSize */
            p += 10;
            MtbCTab ct; ct.count = 0;
            if (pixmap) {
                p += 36;                                           /* rest of the PixMap */
                unsigned n = be16(pic + p + 6) + 1u; p += 8;
                ct.count = (int)n;
                for (unsigned k = 0; k < n && k < 256; k++) {
                    const uint8_t *e = pic + p + 8 * k;   /* value, red, green, blue (the value field is ignored) */
                    ct.c[k] = (MtbRGB){(uint16_t)be16(e + 2), (uint16_t)be16(e + 4), (uint16_t)be16(e + 6)};
                }
                p += 8 * n;
            }
            MtbRect sr = rect_at(pic + p), dr = rect_at(pic + p + 8);
            unsigned mode = be16(pic + p + 16); p += 18;
            if (mode != 0) { snprintf(err, (size_t)errlen, "transfer mode %u", mode); return -1; }
            if (depth != 8 && depth != 1) { snprintf(err, (size_t)errlen, "pixel depth %d", depth); return -1; }
            int rows = bounds.bottom - bounds.top, cols = bounds.right - bounds.left;
            uint8_t *row = malloc(rb ? rb : 1), *pix = malloc((size_t)rows * (size_t)cols + 1);
            for (int y = 0; y < rows; y++) {
                if (rb < 8) { memcpy(row, pic + p, rb); p += rb; }
                else {
                    int cnt = rb > 250 ? (int)be16(pic + p) : pic[p]; p += rb > 250 ? 2 : 1;
                    if (p + (uint32_t)cnt > len || unpack_row(pic + p, cnt, row, (int)rb) < 0) {
                        free(row); free(pix); snprintf(err, (size_t)errlen, "bad packed row %d", y); return -1;
                    }
                    p += (uint32_t)cnt;
                }
                for (int x = 0; x < cols; x++)
                    pix[(size_t)y * (size_t)cols + (size_t)x] = depth == 8 ? row[x] : ((row[x >> 3] >> (7 - (x & 7))) & 1);
            }
            uint8_t xl[256];
            if (depth == 8) for (int i = 0; i < 256; i++) xl[i] = (uint8_t)mtb_color_to_index(dev, ct.c[i < ct.count ? i : 0]);
            else {
                xl[0] = (uint8_t)mtb_color_to_index(dev, (MtbRGB){0xFFFF, 0xFFFF, 0xFFFF});
                xl[1] = (uint8_t)mtb_color_to_index(dev, (MtbRGB){0, 0, 0});
            }
            MtbPix8 src = {pix, cols, bounds};
            MtbRect to = {(int16_t)map_v(dr.top, &frame, dst), (int16_t)map_h(dr.left, &frame, dst),
                          (int16_t)map_v(dr.bottom, &frame, dst), (int16_t)map_h(dr.right, &frame, dst)};
            MtbRect c = *clip, fr = {(int16_t)map_v(frame.top, &frame, dst), (int16_t)map_h(frame.left, &frame, dst),
                                     (int16_t)map_v(frame.bottom, &frame, dst), (int16_t)map_h(frame.right, &frame, dst)};
            mtb_sect_rect(&c, &fr, &c);
            mtb_copybits8(&src, &sr, pm, &to, &c, xl);
            free(row); free(pix);
            break;
        }
        default:
            snprintf(err, (size_t)errlen, "opcode %04X", op); return -1;
        }
    }
    return 0;
}
