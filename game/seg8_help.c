/* CODE segment 8: the instructions (Apple menu > About, or Command-/): dialog 300 with a title page
 * and 23 pages of text, Previous / Next / Exit buttons and arrow keys (see build/disasm/seg8.s). */
#include "mz.h"
#include "fns.h"

#define HELP_DIALOG   g32(0x4BC2)   /* the dialog */
#define HELP_PAGE     0x4BC4        /* the page shown, 1..24 */
#define HELP_PAGES    0x18

/* fn_S8_1A9C: shows page `page`: Previous (item 2) hidden on the first page, Next (item 3) on the last. */
void fn_S8_1A9C(int16_t page) {
    MZ_FRAME;
    uint32_t k2 = mz_local(2), h2 = mz_local(4), b2 = mz_local(8);
    uint32_t k3 = mz_local(2), h3 = mz_local(4), b3 = mz_local(8);
    mac_GetDialogItem(HELP_DIALOG, 2, k2, h2, b2);
    mac_GetDialogItem(HELP_DIALOG, 3, k3, h3, b3);
    if (page == 1) { mac_HideControl(r32(h2)); mac_ShowControl(r32(h3)); }
    else if (page == HELP_PAGES) { mac_ShowControl(r32(h2)); mac_HideControl(r32(h3)); }
    else { mac_ShowControl(r32(h2)); mac_ShowControl(r32(h3)); }
    sg16(HELP_PAGE, (uint16_t)page);
}

/* a centred text box of the title page: rows top..bottom of `r`, the C string at global g */
static void title_line(uint32_t r, int16_t top, int16_t bottom, uint16_t g) {
    w16(r, (uint16_t)top); w16(r + 4, (uint16_t)bottom);
    mac_TETextBox(GADDR(g), (int32_t)(mz_seg(2), fn_S2_0252(GADDR(g))), r, 1);
}

/* fn_S8_1B0E (JT 16): the user item (item 4) drawing procedure: the page in white on black.
 * Page 1: picture 129 centred in the top 100 rows of the dialog, and four centred lines
 * (G_4DA4, G_4D86, G_4C5E, G_4C9A). Pages 2-24: the heading G_4B32 with the page number - 1,
 * underlined, and the text of 'TEXT' 126 + page. */
void fn_S8_1B0E(uint32_t dlg, int16_t item) {
    (void)dlg; (void)item;                                  /* it always draws item 4 of G_4BC2 */
    MZ_FRAME;
    uint32_t kind = mz_local(2), h = mz_local(4), box = mz_local(8), port = mz_local(4);
    uint32_t pen = mz_local(18), tr = mz_local(8);
    mac_GetDialogItem(HELP_DIALOG, 4, kind, h, box);
    mac_GetPort(port);
    mac_SetPort(HELP_DIALOG);
    mac_GetPenState(pen);
    mac_PenNormal();
    mac_ForeColor(33);                                      /* blackColor */
    mac_PaintRect(box);
    mac_ForeColor(30);                                      /* whiteColor */
    mac_BackColor(33);
    mac_TextFont((int16_t)g16(0x2244));
    int16_t page = (int16_t)g16(HELP_PAGE);
    if (page >= 1 && page <= HELP_PAGES) {
        if (page == 1) {
            uint32_t pic = mac_GetPicture(0x81);
            if (pic) {
                uint32_t pr = mz_local(8), dr = mz_local(8);
                w32(dr, r32(HELP_DIALOG + 0x10)); w32(dr + 4, r32(HELP_DIALOG + 0x14));   /* portRect */
                w16(dr + 4, 0x64);                                                        /* bottom 100 */
                w32(pr, r32(deref(pic) + 2)); w32(pr + 4, r32(deref(pic) + 6));           /* picFrame */
                mac_OffsetRect(pr, (int16_t)(r16(dr + 2) - r16(pr + 2)), (int16_t)(r16(dr) - r16(pr)));
                mac_OffsetRect(pr, (int16_t)((int16_t)(r16(dr + 6) - r16(pr + 6)) / 2),
                                   (int16_t)((int16_t)(r16(dr + 4) - r16(pr + 4)) / 2));
                mac_DrawPicture(pic, pr);
            }
            mac_TextSize(12);
            w32(tr, r32(box)); w32(tr + 4, r32(box + 4));
            title_line(tr, 0x5F, 0x6E, 0x4DA4);
            title_line(tr, 0x7D, 0xAF, 0x4D86);
            title_line(tr, 0x11C, 0x12E, 0x4C9A);
            title_line(tr, 0xB4, 0x11D, 0x4C5E);
        } else {
            uint32_t text = mac_GetResource(0x54455854u /* 'TEXT' */, (int16_t)(0x7E + page));
            if (text) {
                uint32_t num = mz_local(256);
                mac_HLock(text);
                mac_TextSize(12);
                w32(tr, r32(box)); w32(tr + 4, r32(box + 4));
                mac_InsetRect(tr, 10, 10);
                mac_MoveTo((int16_t)r16(tr + 2), 0x16);
                mac_TextFace(4);                            /* underline */
                mac_DrawString(GADDR(0x4B32));
                mz_seg(3), fn_S3_038E((int32_t)page - 1, num);
                mac_DrawString(num);
                mac_TextFace(0);
                w16(tr, 0x2A); w16(tr + 4, 0x11A);
                uint32_t p = deref(text);
                mac_TETextBox(p, (mz_seg(3), fn_S3_020E(text)), tr, 0);
                mac_HUnlock(text);
                mac_ReleaseResource(text);
            }
        }
    }
    mac_SetPenState(pen);
    mac_SetPort(r32(port));
}

/* fn_S8_1D5C (JT 17): the dialog's filter. An update of the high-score window G_2232 redraws it
 * (fn_S8_2CAC) and is taken (the item is left as it was). Escape or Command-. press Exit (item 1),
 * the left arrow Previous (2), the right arrow Next (3): the button is shown pressed for 8 ticks. */
uint8_t fn_S8_1D5C(uint32_t dlg, uint32_t ev, uint32_t pitem) {
    MZ_FRAME;
    uint16_t what = r16(ev);
    if (what == 6) {
        if (r32(ev + 2) != g32(0x2232)) return 0;
        uint32_t port = mz_local(4);
        mac_GetPort(port);
        fn_S8_2CAC(r32(ev + 2));
        mac_SetPort(r32(port));
        return 1;
    }
    if (what != 3 && what != 5) return 0;
    uint8_t ch = r8(ev + 5);
    int16_t item;
    if (ch == 0x1B || ((r8(ev + 0xE) & 1) && ch == '.')) item = 1;
    else if (ch == 0x1C) item = 2;
    else if (ch == 0x1D) item = 3;
    else return 0;
    uint32_t kind = mz_local(2), h = mz_local(4), box = mz_local(8);
    mac_GetDialogItem(dlg, item, kind, h, box);
    mac_HiliteControl(r32(h), 10);
    mac_Delay(8);
    mac_HiliteControl(r32(h), 0);
    w16(pitem, (uint16_t)item);
    return 1;
}

/* the main screen's rectangle, its bottom replaced by the saved menu bar height G_2226 */
static void menubar_rect(uint32_t r) {
    uint32_t gd = deref(mac_GetMainDevice());
    w32(r, r32(gd + 0x22)); w32(r + 4, r32(gd + 0x26));
    w16(r + 4, g16(0x2226));
}

/* fn_S8_1ED0: hides the menu bar (on screens under 384 rows): MBarHeight ($BAA) saved in G_2226 and
 * set to 0, the gray region ($9EE) saved in G_4BBE and extended over the bar, which is repainted. */
void fn_S8_1ED0(void) {
    MZ_FRAME;
    if (g8(0x4DA6)) return;
    sg16(0x2226, r16(0x0BAA));
    w16(0x0BAA, 0);
    sg32(0x4BBE, mac_NewRgn());
    mac_CopyRgn(r32(0x09EE), g32(0x4BBE));
    uint32_t rgn = mac_NewRgn(), r = mz_local(8);
    menubar_rect(r);
    mac_RectRgn(rgn, r);
    mac_UnionRgn(r32(0x09EE), rgn, r32(0x09EE));
    mac_PaintBehind(mac_FrontWindow(), rgn);
    mac_CalcVisBehind(mac_FrontWindow(), rgn);
    mac_DisposeRgn(rgn);
    sg8(0x4DA6, 1);
}

/* fn_S8_1F54: shows the menu bar again (undoes fn_S8_1ED0). */
void fn_S8_1F54(void) {
    MZ_FRAME;
    if (!g8(0x4DA6)) return;
    w16(0x0BAA, g16(0x2226));
    mac_CopyRgn(g32(0x4BBE), r32(0x09EE));
    mac_DisposeRgn(g32(0x4BBE));
    uint32_t rgn = mac_NewRgn(), r = mz_local(8);
    menubar_rect(r);
    mac_RectRgn(rgn, r);
    mac_PaintBehind(mac_FrontWindow(), rgn);
    mac_CalcVisBehind(mac_FrontWindow(), rgn);
    mac_DisposeRgn(rgn);
    mac_DrawMenuBar();
    sg8(0x4DA6, 0);
}

/* fn_S8_190C: the instructions dialog, opened at page `page`: on screens under 342 x 512 an alert
 * first (G_4BBA); under 384 rows the menu bar is hidden meanwhile (fn_S8_1ED0 / 1F54). Palette 300;
 * item 4 drawn by fn_S8_1B0E; keys by the filter fn_S8_1D5C; until Exit. */
void fn_S8_190C(int16_t page) {
    MZ_FRAME;
    uint32_t gd = deref(mac_GetMainDevice());
    int16_t top = (int16_t)r16(gd + 0x22), left = (int16_t)r16(gd + 0x24);
    int16_t bottom = (int16_t)r16(gd + 0x26), right = (int16_t)r16(gd + 0x28);
    if ((int16_t)(bottom - top) < 0x156 && (int16_t)(right - left) < 0x200) mz_seg(2), fn_S2_03A0(GADDR(0x4BBA), 0, 0);
    if ((int16_t)(bottom - top) < 0x180) fn_S8_1ED0();
    uint32_t port = mz_local(4), item = mz_local(2), h = mz_local(4), box = mz_local(8);
    mac_GetPort(port);
    sg32(0x4BC2, mac_GetNewDialog(300, 0, 0xFFFFFFFFu));
    if (!HELP_DIALOG) return;
    mac_SetPort(HELP_DIALOG);
    uint32_t pal = mac_GetNewPalette(300);
    if (!pal) return;                                        /* (the original leaves the dialog open) */
    mac_NSetPalette(HELP_DIALOG, pal, 0x100);
    mac_GetDialogItem(HELP_DIALOG, 4, item, h, box);         /* item 4's kind goes in the item-hit word */
    mac_SetDialogItem(HELP_DIALOG, 4, (int16_t)r16(item), MZ_A5 + 0xA2, box);   /* JT 16: fn_S8_1B0E */
    fn_S8_1A9C(page);
    mac_ShowWindow(HELP_DIALOG);
    for (int done = 0; !done; ) {
        mac_ModalDialog(MZ_A5 + 0xAA, item);                  /* JT 17: fn_S8_1D5C */
        int16_t hit = (int16_t)r16(item);
        if (hit <= 0 || hit > 3) continue;
        if (hit == 1) { done = 1; continue; }
        int16_t p = (int16_t)g16(HELP_PAGE);
        if (hit == 3) { if (p < HELP_PAGES) fn_S8_1A9C((int16_t)(p + 1)); }
        else if (p > 1) fn_S8_1A9C((int16_t)(p - 1));
        mac_GetDialogItem(HELP_DIALOG, 4, item, h, box);
        mac_InvalRect(box);
    }
    mac_SetPort(r32(port));
    mac_DisposeDialog(HELP_DIALOG);
    if (pal) mac_DisposePalette(pal);
    fn_S8_1F54();
}
