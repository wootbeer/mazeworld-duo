/* QuickDraw / Window Manager internals shared by qd.c and dlg.c (record layouts and helpers). */
#ifndef QD_INT_H
#define QD_INT_H
#include "hx.h"
#include "../mactb/mactb.h"

enum { CP_Device = 0, CP_PixMap = 2, CP_Version = 6, CP_PortRect = 16, CP_VisRgn = 24, CP_ClipRgn = 28, CP_RGBFg = 36, CP_RGBBk = 42,
       CP_PnLoc = 48, CP_PnSize = 52, CP_PnMode = 56, CP_PnVis = 66, CP_TxFont = 68, CP_TxFace = 70, CP_TxMode = 72, CP_TxSize = 74,
       CP_FgColor = 80, CP_BkColor = 84, CP_SIZE = 108 };
enum { W_Kind = 108, W_Visible = 110, W_Hilited = 111, W_GoAway = 112, W_StrucRgn = 114, W_ContRgn = 118, W_UpdateRgn = 122,
       W_TitleHandle = 134, W_Next = 144, W_RefCon = 152, W_SIZE = 156 };
enum { WK_DIALOG = 2, WK_USER = 8 };

static inline MtbRect rd_rect(uint32_t a) { MtbRect r = {(int16_t)rd16(a), (int16_t)rd16(a + 2), (int16_t)rd16(a + 4), (int16_t)rd16(a + 6)}; return r; }
static inline void wr_rect(uint32_t a, MtbRect r) { wr16(a, (uint16_t)r.top); wr16(a + 2, (uint16_t)r.left); wr16(a + 4, (uint16_t)r.bottom); wr16(a + 6, (uint16_t)r.right); }

const char *hx_where(void);
MtbPix8  qd_port_pix(uint32_t port);           /* the port's pixels, in its local coordinates */
MtbRect  qd_port_clip(uint32_t port);
void     qd_device_ctab(MtbCTab *t);           /* the main screen's colour table */
/* New window record (storage 0 = allocate `size` bytes) on the main screen. frame = pixels of
 * structure outside the content (8 for a modal dialog box). In front when behind is -1, else at the back. */
uint32_t qd_new_window(uint32_t storage, uint32_t size, MtbRect content, int frame, int visible, int goaway,
                       uint32_t refcon, uint32_t behind, int kind);
/* Take the window out of the window list; uncovered screen is erased and the windows behind get updates. */
void     qd_remove_window(uint32_t win);
MtbRect  qd_auto_position(MtbRect content, uint16_t pos);
uint16_t qd_template_position(uint32_t h, int title_offset);
uint32_t qd_front_window(void);
MtbRect qd_window_global(uint32_t win);
void     qd_post_update(uint32_t win);        /* queue an update event for win */
int      qd_take_update_for(uint32_t win);     /* 1 if an update was pending for win (and is now taken) */
uint32_t qd_take_update(void);
#endif
