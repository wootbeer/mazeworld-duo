/* Bitmap font strikes (generated data in font_data.c). */
#ifndef MTB_FONT_DATA_H
#define MTB_FONT_DATA_H
#include <stdint.h>

typedef struct { uint8_t width, advance; uint16_t row; } MtbGlyph;   /* advance 0 = glyph missing */
typedef struct {
    int size, ascent, descent, leading;
    const MtbGlyph *glyphs;     /* 256 entries, indexed by Mac Roman code */
    const uint32_t *rows;       /* ascent+descent rows per glyph, bit 31 = leftmost pixel */
} MtbFont;

extern const MtbFont mtb_font9, mtb_font12, mtb_font14, mtb_font18;
#endif
