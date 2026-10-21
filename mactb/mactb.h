/* mactb: native C implementation of the small part of the classic Mac Toolbox that
 * Mazeworld Catacombs uses. Plain C99, no 68k, no emulation: the Windows port links it
 * directly, and the test harness calls it from its trap wrappers so both sides share
 * one implementation.
 *
 * Fidelity notes are marked FIDELITY: where behaviour is modelled rather than taken
 * from Apple's code. Those points affect what is on screen, not game logic, and are
 * identical in the port and in the harness. */
#ifndef MACTB_H
#define MACTB_H
#include <stdint.h>

typedef struct { int16_t top, left, bottom, right; } MtbRect;
typedef struct { uint16_t red, green, blue; } MtbRGB;

/* ---------- colour tables ---------- */
typedef struct { int count; uint32_t seed; MtbRGB c[256]; } MtbCTab;

/* Standard system colour tables: 1, 2, 4, 8 (colour) and 33, 34, 36, 40 (grey).
 * Returns 0 if `id` is a standard table. */
int  mtb_std_clut(int id, MtbCTab *out);
/* Index of the closest colour (squared distance on 16-bit components, lowest index wins ties).
 * FIDELITY: Color QuickDraw uses a 4-bit inverse table; results can differ for colours not in the table. */
int  mtb_color_to_index(const MtbCTab *t, MtbRGB c);
/* Seeds for new tables: unique and increasing, like Color QuickDraw's GetCTSeed. */
uint32_t mtb_new_ctseed(void);

/* ---------- Palette Manager model ---------- */
enum { MTB_PM_COURTEOUS = 0, MTB_PM_TOLERANT = 2, MTB_PM_ANIMATED = 4, MTB_PM_EXPLICIT = 8 };
typedef struct { MtbRGB rgb; uint16_t usage, tolerance; int devindex; } MtbPalEntry;
typedef struct { int count; MtbPalEntry e[256]; } MtbPalette;

/* Activate a palette on an indexed device. Tolerant entries are matched against the device
 * table within tolerance; misses claim device entries from 254 downward, never 0 (white) or
 * 255 (black). Returns 1 if the device table changed.
 * FIDELITY: Apple's allocation order is not documented; this order is fixed and deterministic. */
int  mtb_pal_activate(MtbPalette *p, MtbCTab *device);
/* Device index for a palette entry after activation (courteous entries: closest colour). */
int  mtb_pal_entry_index(const MtbPalette *p, const MtbCTab *device, int entry);

/* ---------- QuickDraw arithmetic ---------- */
/* QuickDraw Random(): seed = seed * 16807 mod (2^31 - 1); result = low word, with 0x8000 -> 0.
 * FIDELITY: published algorithm; to be confirmed against a ROM trace. */
int16_t mtb_qd_random(uint32_t *seed);

/* ---------- 8-bit pixel operations ---------- */
typedef struct {
    uint8_t *base;      /* pixel (bounds.left, bounds.top) */
    int32_t rowbytes;
    MtbRect bounds;     /* coordinate system of the pixmap */
} MtbPix8;

int  mtb_sect_rect(const MtbRect *a, const MtbRect *b, MtbRect *out);   /* 1 if non-empty */
void mtb_fill_rect8(const MtbPix8 *pm, const MtbRect *r, const MtbRect *clip, uint8_t index);
void mtb_frame_rect8(const MtbPix8 *pm, const MtbRect *r, const MtbRect *clip, int penw, int penh, uint8_t index);
/* Bresenham line with a 1x1 pen, both end points drawn as QuickDraw does for 1x1 pens. */
void mtb_line8(const MtbPix8 *pm, int h0, int v0, int h1, int v1, const MtbRect *clip, uint8_t index);
/* srcCopy CopyBits between 8-bit pixmaps with optional colour translation map (NULL = identity),
 * scaling by nearest pixel when the rectangles differ in size. */
void mtb_copybits8(const MtbPix8 *src, const MtbRect *sr, const MtbPix8 *dst, const MtbRect *dr,
                   const MtbRect *clip, const uint8_t *xlat);
/* A version 2 picture drawn onto dst in an 8-bit pixmap (header, clip, comments and PackBitsRect
 * images, srcCopy). dev: the destination's colour table. Returns 0, or -1 with err set. */
int mtb_pict_draw8(const uint8_t *pic, uint32_t len, const MtbRect *dst, const MtbPix8 *pm, const MtbRect *clip,
                   const MtbCTab *dev, char *err, int errlen);
/* ---------- text (original bitmap font standing in for Geneva; see tools/fontgen.py) ---------- */
void mtb_font_metrics(int size, int *ascent, int *descent, int *leading, int *widmax);
int  mtb_char_width(int size, int face, uint8_t c);
int  mtb_text_width(int size, int face, const uint8_t *s, int n);
/* mode 0 = srcCopy (cells filled with bk), 1 = srcOr; face bit 2 = underline. Returns new pen h. */
int  mtb_draw_text8(const MtbPix8 *pm, const MtbRect *clip, int h, int v, const uint8_t *s, int n,
                    int size, int face, int mode, uint8_t fg, uint8_t bk);
/* TETextBox: erase box, word-wrap, CR breaks lines; just 0 left, 1 centre, -1 right. */
void mtb_textbox8(const MtbPix8 *pm, const MtbRect *clip, const MtbRect *box, const uint8_t *s, int n,
                  int size, int face, int mode, uint8_t fg, uint8_t bk, int just);

/* ---------- Dialog Manager drawing (dialog.c) ---------- */
enum { MTB_DI_USER = 0, MTB_DI_BUTTON = 4, MTB_DI_CHECKBOX = 5, MTB_DI_RADIO = 6, MTB_DI_STATIC = 8,
       MTB_DI_EDIT = 16, MTB_DI_ICON = 32, MTB_DI_PICT = 64, MTB_DI_DISABLED = 128 };
typedef struct { uint8_t black, white, grey; } MtbDColors;
typedef struct {
    uint8_t kind;              /* DITL item type, disabled bit included */
    MtbRect rect;              /* local coordinates of the dialog */
    const uint8_t *text; int len;
    int value, hilite;         /* controls: 0/1, 255 = inactive */
} MtbDItem;
/* Modal dialog window frame (dBoxProc), drawn around the content rectangle (8 pixels wide). */
void mtb_dialog_frame8(const MtbPix8 *pm, const MtbRect *clip, const MtbRect *content, const MtbDColors *col);
void mtb_dialog_item8(const MtbPix8 *pm, const MtbRect *clip, const MtbDItem *it, const MtbDColors *col);

/* ---------- Sound Manager model (sampled sound channels) ---------- */
#define MTB_SND_QLEN 128
typedef struct { uint16_t cmd; int16_t p1; uint32_t p2; const uint8_t *hdr; } MtbSndCmd;
typedef struct {
    int used, playing;
    MtbSndCmd q[MTB_SND_QLEN]; int qh, qt;
    const uint8_t *data; uint32_t frames, rate; uint64_t pos;   /* current buffer, rate in Fixed, pos 16.16 */
    uint8_t amp;
} MtbSndChan;
void mtb_snd_init(MtbSndChan *c);
/* Queue a command (SndDoCommand). hdr: sampled sound header for bufferCmd/soundCmd. Returns 0 or queueFull. */
int  mtb_snd_command(MtbSndChan *c, uint16_t cmd, int16_t p1, uint32_t p2, const uint8_t *hdr);
/* SndDoImmediate. Returns 1 if a callBackCmd must be delivered now. */
int  mtb_snd_immediate(MtbSndChan *c, uint16_t cmd, int16_t p1, uint32_t p2, const uint8_t *hdr);
/* Advance one tick: mixes into mix[0..nmix), returns callbacks reached (their param1/param2). */
int  mtb_snd_tick(MtbSndChan *c, int32_t *mix, int nmix, int16_t *cb_p1, uint32_t *cb_p2, int maxcb);
int  mtb_snd_samples_for_tick(uint32_t tick);   /* output samples at 22254.5454 Hz for this tick */

/* ---------- SANE (FP68K) ---------- */
/* Operands are in 68k memory layout. Returns 0, or -1 if the operation is not implemented. */
int mtb_fp68k(uint16_t opword, uint8_t *dst, const uint8_t *src, uint8_t *ccr);
int mtb_sane_pop_bytes(uint16_t opword);       /* parameter bytes FP68K removes, opword included */
int mtb_sane_operand_size(uint16_t opword);    /* bytes of the source operand's format */
#endif
