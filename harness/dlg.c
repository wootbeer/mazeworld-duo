/* Dialog Manager and Control Manager subset for the harness: modal dialogs built from DLOG/DITL,
 * push buttons, check boxes, static and edit text, driven by the input script.
 *
 * Records keep their real 68k layouts (the game reads item lists and control records through
 * GetDialogItem). Drawing is mactb's (shared with the port).
 *
 * ModalDialog waits like the rest of the time model: when no event is due it lets one tick pass
 * and re-runs its A-line instruction, so VBL tasks and sound callbacks keep running while the
 * dialog is up. Keys go to the edit field; Return/Enter press the default button (item 1) when it
 * is active, holding it highlighted for 8 ticks as the standard filter does.
 * A filter procedure sees each event first (update events of other windows, mouse and key events)
 * and may answer it; user items are drawn by their procedures, through hx_call_pascal.
 * FIDELITY: no text selection, caret or mouse positioning inside edit text; Return with the
 * default button inactive is ignored; the filter is not called with null events. */
#include "qd_int.h"
#include "m68k.h"
#include <string.h>

#define A7 M68K_REG_A7
#define ARGW()   ((int16_t)pop16())
#define ARGL()   (pop32())
#define RETW(v)  wr16(reg(A7), (uint16_t)(v))
#define RETL(v)  wr32(reg(A7), (uint32_t)(v))

#define HX_SCRATCH 0x00FFE000u   /* harness-private EventRecord for ModalDialog */

enum { D_Items = 156, D_TextH = 160, D_EditField = 164, D_EditOpen = 166, D_ADefItem = 168, D_SIZE = 170 };
enum { C_Next = 0, C_Owner = 4, C_Rect = 8, C_Vis = 16, C_Hilite = 17, C_Value = 18, C_Min = 20, C_Max = 22,
       C_DefProc = 24, C_Data = 28, C_Action = 32, C_RefCon = 36, C_Title = 40 };

int  hx_trap_retry(void);
const uint8_t *hx_param_text(int i);
void vbl_advance(uint32_t n);
uint32_t hx_ticks(void);
void input_poll(uint32_t tick);
int  input_pending(void);
int  input_get_event(uint32_t ev, uint16_t mask, int remove);

static struct { uint32_t d; int allocated; } dialogs[8];

static int is_dialog(uint32_t w) { return w && (int16_t)rd16(w + W_Kind) == WK_DIALOG; }

/* ---------- item list: count-1, then {handle.l, rect, type.b, len.b, data[len], pad to even} ---------- */
static int n_items(uint32_t d) { uint32_t h = rd32(d + D_Items); return h ? (int16_t)rd16(rd32(h)) + 1 : 0; }
static uint32_t item_at(uint32_t d, int no) {   /* address of item `no` (1-based), or 0 */
    uint32_t h = rd32(d + D_Items); if (!h || no < 1 || no > n_items(d)) return 0;
    uint32_t p = rd32(h) + 2;
    for (int i = 1; i < no; i++) { int len = rd8(p + 13); p += 14 + ((len + 1) & ~1); }
    return p;
}
static int is_control(uint8_t kind) { int k = kind & 0x7F; return k >= 4 && k <= 7; }

static void colours(MtbDColors *c) {
    MtbCTab t; qd_device_ctab(&t);
    c->black = (uint8_t)mtb_color_to_index(&t, (MtbRGB){0, 0, 0});
    c->white = (uint8_t)mtb_color_to_index(&t, (MtbRGB){0xFFFF, 0xFFFF, 0xFFFF});
    c->grey  = (uint8_t)mtb_color_to_index(&t, (MtbRGB){0x7FFF, 0x7FFF, 0x7FFF});
}

/* static text with ^0..^3 replaced by the ParamText strings */
static int expand_param(const uint8_t *s, int n, uint8_t *out, int max) {
    int o = 0;
    for (int i = 0; i < n && o < max; i++) {
        if (s[i] == '^' && i + 1 < n && s[i + 1] >= '0' && s[i + 1] <= '3') {
            const uint8_t *p = hx_param_text(s[++i] - '0');
            for (int k = 0; k < p[0] && o < max; k++) out[o++] = p[1 + k];
        } else out[o++] = s[i];
    }
    return o;
}

static void draw_item(uint32_t d, int no) {
    uint32_t it = item_at(d, no);
    if (!it || !rd8(d + W_Visible)) return;
    uint32_t h = rd32(it); uint8_t kind = rd8(it + 12);
    MtbDItem di = {kind, rd_rect(it + 4), NULL, 0, 0, 0};
    static uint8_t buf[1024];
    if (is_control(kind) && h) {
        uint32_t c = rd32(h);
        if (!rd8(c + C_Vis)) return;
        di.text = g_mem + c + C_Title + 1; di.len = rd8(c + C_Title);
        di.value = (int16_t)rd16(c + C_Value); di.hilite = rd8(c + C_Hilite);
    } else if (((kind & 0x7F) == MTB_DI_STATIC || (kind & 0x7F) == MTB_DI_EDIT) && h) {
        int n = mm_handle_size(h); if (n > 1000) n = 1000;
        if ((kind & 0x7F) == MTB_DI_STATIC) { di.len = expand_param(g_mem + rd32(h), n, buf, sizeof buf); di.text = buf; }
        else { di.text = g_mem + rd32(h); di.len = n; }
    } else if ((kind & 0x7F) == 0) {                     /* user item: its procedure draws it */
        if (h) {
            uint32_t a[2] = {d, (uint32_t)no}; int sz[2] = {4, 2};
            hx_call_pascal(h, 0, 2, a, sz);
        }
        return;
    } else if ((kind & 0x7F) != MTB_DI_STATIC) {
        static int warned; if (!warned++) hx_log("dialog item kind %d not drawn (icons, pictures and user items are not supported yet)", kind & 0x7F);
        return;
    }
    MtbPix8 pm = qd_port_pix(d); MtbRect clip = qd_port_clip(d); MtbDColors col; colours(&col);
    mtb_dialog_item8(&pm, &clip, &di, &col);
}
static void draw_dialog(uint32_t d) {
    if (!rd8(d + W_Visible)) return;
    MtbPix8 scr = {g_mem + SCREEN_BASE, SCREEN_W, {0, 0, SCREEN_H, SCREEN_W}};
    MtbRect content = rd_rect(rd32(rd32(d + W_ContRgn)) + 2), all = scr.bounds;
    MtbDColors col; colours(&col);
    mtb_dialog_frame8(&scr, &all, &content, &col);
    for (int i = 1; i <= n_items(d); i++) draw_item(d, i);
}
static uint32_t owner_of_text(uint32_t h, int *no) {
    for (int k = 0; k < 8; k++) {
        uint32_t d = dialogs[k].d; if (!d) continue;
        for (int i = 1; i <= n_items(d); i++) if (rd32(item_at(d, i)) == h) { *no = i; return d; }
    }
    return 0;
}

/* ---------- GetNewDialog ---------- */
static uint32_t new_control(uint32_t d, uint32_t it) {
    int len = rd8(it + 13);
    uint32_t h = mm_new_handle(C_Title + 1 + (uint32_t)len, 1), c = rd32(h);
    wr32(c + C_Owner, d); wr_rect(c + C_Rect, rd_rect(it + 4)); wr8(c + C_Vis, 0xFF);
    wr16(c + C_Max, 1); wr32(c + C_DefProc, (uint32_t)(rd8(it + 12) & 3));   /* the CDEF variation; no real defproc */
    wr8(c + C_Title, (uint8_t)len); { hx_track(c + C_Title + 1, (uint32_t)((size_t)len)); memcpy(g_mem + c + C_Title + 1, g_mem + it + 14, (size_t)len); }
    return h;
}
/* a dialog record with its items from DITL handle dr (GetNewDialog, Alert) */
static uint32_t build_dialog(uint32_t storage, MtbRect rect, uint32_t dr, int visible, int goaway, uint32_t refcon, uint32_t behind) {
    uint32_t d = qd_new_window(storage, D_SIZE, rect, 8, visible, goaway, refcon, behind, WK_DIALOG);
    /* the item list is a copy of the DITL with each item's handle filled in */
    uint32_t n = (uint32_t)mm_handle_size(dr), ih = mm_new_handle(n, 0);
    { hx_track(rd32(ih), (uint32_t)(n)); memmove(g_mem + rd32(ih), g_mem + rd32(dr), n); }
    wr32(d + D_Items, ih); wr16(d + D_EditField, 0xFFFF); wr16(d + D_ADefItem, 1);
    for (int i = 1; i <= n_items(d); i++) {
        uint32_t it = item_at(d, i); uint8_t kind = rd8(it + 12); int len = rd8(it + 13);
        uint32_t h = 0;
        if (is_control(kind)) { h = new_control(d, it); it = item_at(d, i); }
        else if ((kind & 0x7F) == MTB_DI_STATIC || (kind & 0x7F) == MTB_DI_EDIT) {
            h = mm_new_handle((uint32_t)len, 0);
            it = item_at(d, i);   /* re-derived after the allocation, in case the list moved */
            { hx_track(rd32(h), (uint32_t)((size_t)len)); memmove(g_mem + rd32(h), g_mem + it + 14, (size_t)len); }
            if ((kind & 0x7F) == MTB_DI_EDIT && (int16_t)rd16(d + D_EditField) < 0) wr16(d + D_EditField, (uint16_t)(i - 1));
        }
        wr32(it, h);
    }
    for (int k = 0; k < 8; k++) if (!dialogs[k].d) { dialogs[k].d = d; dialogs[k].allocated = storage == 0; break; }
    if (rd8(d + W_Visible)) qd_post_update(d);
    return d;
}
static void t_GetNewDialog(uint16_t w) {
    (void)w; uint32_t behind = ARGL(), storage = ARGL(); int16_t id = ARGW();
    uint32_t r = rs_get(FOURCC('D','L','O','G'), id, 0);
    if (!r) { hx_log("GetNewDialog(%d) at %s: no DLOG", id, hx_where()); RETL(0); return; }
    uint32_t a = rd32(r);
    int16_t proc = (int16_t)rd16(a + 8), ditl = (int16_t)rd16(a + 18);
    uint32_t dr = rs_get(FOURCC('D','I','T','L'), ditl, 0);
    if (!dr) { hx_log("GetNewDialog(%d) at %s: no DITL %d", id, hx_where(), ditl); RETL(0); return; }
    if (rs_get(FOURCC('d','c','t','b'), id, 0)) hx_log("GetNewDialog(%d): dctb colour table ignored", id);
    if (proc != 1) hx_log("GetNewDialog(%d): window definition %d drawn as a modal dialog box", id, proc);
    uint32_t d = build_dialog(storage, qd_auto_position(rd_rect(a), qd_template_position(r, 20)), dr, rd8(a + 10), rd8(a + 12), rd32(a + 14), behind);
    hx_log("GetNewDialog(%d) at %s: %d items, DITL %d", id, hx_where(), n_items(d), ditl);
    RETL(d);
}

/* ---------- items ---------- */
static void t_GetDialogItem(uint16_t w) {
    (void)w; uint32_t pbox = ARGL(), ph = ARGL(), pkind = ARGL(); int16_t no = ARGW(); uint32_t d = ARGL();
    uint32_t it = item_at(d, no);
    if (!it) { hx_log("GetDialogItem(%d) at %s: no such item", no, hx_where()); return; }
    wr16(pkind, rd8(it + 12)); wr32(ph, rd32(it)); wr_rect(pbox, rd_rect(it + 4));
}
static void t_SetDialogItem(uint16_t w) {
    (void)w; uint32_t pbox = ARGL(), h = ARGL(); int16_t kind = ARGW(), no = ARGW(); uint32_t d = ARGL();
    uint32_t it = item_at(d, no);
    if (!it) return;
    wr32(it, h); wr8(it + 12, (uint8_t)kind); wr_rect(it + 4, rd_rect(pbox));
}
static void t_GetDialogItemText(uint16_t w) {
    (void)w; uint32_t s = ARGL(), h = ARGL();
    int n = h && rd32(h) ? mm_handle_size(h) : 0; if (n > 255) n = 255;
    wr8(s, (uint8_t)n); if (n) { hx_track(s + 1, (uint32_t)((size_t)n)); memmove(g_mem + s + 1, g_mem + rd32(h), (size_t)n); }
}
static void t_SetDialogItemText(uint16_t w) {
    (void)w; uint32_t s = ARGL(), h = ARGL();
    int n = rd8(s);
    mm_set_handle_size(h, (uint32_t)n); if (n) { hx_track(rd32(h), (uint32_t)((size_t)n)); memmove(g_mem + rd32(h), g_mem + s + 1, (size_t)n); }
    int no; uint32_t d = owner_of_text(h, &no); if (d) draw_item(d, no);
}
static void t_SelectDialogItemText(uint16_t w) {
    (void)w; ARGW(); ARGW(); int16_t no = ARGW(); uint32_t d = ARGL();
    uint32_t it = item_at(d, no);
    if (it && (rd8(it + 12) & 0x7F) == MTB_DI_EDIT) wr16(d + D_EditField, (uint16_t)(no - 1));
}
static void t_DrawDialog(uint16_t w) { (void)w; draw_dialog(ARGL()); }

/* ---------- controls ---------- */
static void redraw_control(uint32_t ch) {
    uint32_t d = rd32(rd32(ch) + C_Owner);
    if (!is_dialog(d)) return;
    for (int i = 1; i <= n_items(d); i++) if (rd32(item_at(d, i)) == ch) { draw_item(d, i); return; }
}
static void t_HiliteControl(uint16_t w) {
    (void)w; int16_t v = ARGW(); uint32_t ch = ARGL();
    if (rd8(rd32(ch) + C_Hilite) == (uint8_t)v) return;
    wr8(rd32(ch) + C_Hilite, (uint8_t)v); redraw_control(ch);
}
static void t_SetControlValue(uint16_t w) {
    (void)w; int16_t v = ARGW(); uint32_t ch = ARGL(), c = rd32(ch);
    int16_t lo = (int16_t)rd16(c + C_Min), hi = (int16_t)rd16(c + C_Max);
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    if ((int16_t)rd16(c + C_Value) == v) return;
    wr16(c + C_Value, (uint16_t)v); redraw_control(ch);
}
static void t_GetControlValue(uint16_t w) { (void)w; uint32_t ch = ARGL(); RETW(rd16(rd32(ch) + C_Value)); }
/* ShowControl draws the control; HideControl erases its rectangle to the dialog's white */
static void t_ShowControl(uint16_t w) {
    (void)w; uint32_t ch = ARGL(), c = rd32(ch);
    if (rd8(c + C_Vis)) return;
    wr8(c + C_Vis, 0xFF); redraw_control(ch);
}
static void t_HideControl(uint16_t w) {
    (void)w; uint32_t ch = ARGL(), c = rd32(ch);
    if (!rd8(c + C_Vis)) return;
    wr8(c + C_Vis, 0);
    uint32_t d = rd32(c + C_Owner);
    if (!is_dialog(d) || !rd8(d + W_Visible)) return;
    MtbPix8 pm = qd_port_pix(d); MtbRect clip = qd_port_clip(d), r = rd_rect(c + C_Rect); MtbDColors col; colours(&col);
    mtb_fill_rect8(&pm, &r, &clip, col.white);
}

/* ---------- closing ---------- */
static void close_dialog(uint32_t d, int dispose) {
    qd_remove_window(d);
    int k; for (k = 0; k < 8 && dialogs[k].d != d; k++) ;
    if (dispose) {
        for (int i = 1; i <= n_items(d); i++) {
            uint32_t it = item_at(d, i), h = rd32(it); uint8_t kind = rd8(it + 12);
            if (h && (is_control(kind) || (kind & 0x7F) == MTB_DI_STATIC || (kind & 0x7F) == MTB_DI_EDIT)) mm_dispose_handle(h);
        }
        mm_dispose_handle(rd32(d + D_Items)); wr32(d + D_Items, 0);
        if (k < 8 && dialogs[k].allocated) mm_dispose_ptr(d);
    }
    if (k < 8) dialogs[k].d = 0;
}
static void t_DisposeDialog(uint16_t w) { uint32_t d = ARGL(); close_dialog(d, (w & 0x3FF) == 0x183); }
static void t_CloseWindow(uint16_t w) {
    uint32_t win = ARGL();
    if (is_dialog(win)) { close_dialog(win, (w & 0x3FF) == 0x114); return; }
    qd_remove_window(win);
    hx_log("%s at %s: window record left allocated", (w & 0x3FF) == 0x114 ? "DisposeWindow" : "CloseWindow", hx_where());
}

/* ---------- ModalDialog ---------- */
static int track_item;   /* control under a mouse press, waiting for the release */

static int find_item(uint32_t d, int16_t gv, int16_t gh) {
    MtbRect content = rd_rect(rd32(rd32(d + W_ContRgn)) + 2);
    int v = gv - content.top, h = gh - content.left;
    for (int i = 1; i <= n_items(d); i++) {
        uint32_t it = item_at(d, i); MtbRect r = rd_rect(it + 4);
        if (v >= r.top && v < r.bottom && h >= r.left && h < r.right) return i;
    }
    return 0;
}
static int item_active(uint32_t d, int no) {
    uint32_t it = item_at(d, no); if (!it) return 0;
    uint8_t kind = rd8(it + 12);
    if (kind & MTB_DI_DISABLED) return 0;
    if (is_control(kind) && rd32(it) && rd8(rd32(rd32(it)) + C_Hilite) == 255) return 0;
    return 1;
}
static void flash_button(uint32_t d, int no) {   /* the standard filter's simulated click */
    uint32_t ch = rd32(item_at(d, no));
    wr8(rd32(ch) + C_Hilite, 1); draw_item(d, no);
    vbl_advance(8);
    wr8(rd32(ch) + C_Hilite, 0); draw_item(d, no);
}
static int edit_key(uint32_t d, uint8_t ch) {   /* returns the edit item number, or 0 */
    int16_t ef = (int16_t)rd16(d + D_EditField);
    if (ef < 0) return 0;
    uint32_t h = rd32(item_at(d, ef + 1));
    int n = mm_handle_size(h);
    if (ch == '\t') {                                     /* Tab: the next edit text item, round */
        int k = ef + 1;
        for (int i = 1; i <= n_items(d); i++) {
            int no = (ef + i) % n_items(d) + 1;
            if ((rd8(item_at(d, no) + 12) & 0x7F) == MTB_DI_EDIT) { k = no; break; }
        }
        wr16(d + D_EditField, (uint16_t)(k - 1));
        return 0;
    }
    if (ch == 0x08) { if (n > 0) mm_set_handle_size(h, (uint32_t)(n - 1)); }
    else if (ch >= 0x20 && ch != 0x7F) { mm_set_handle_size(h, (uint32_t)(n + 1)); wr8(rd32(h) + (uint32_t)n, ch); }
    else return 0;   /* arrows, tab and other control keys: no edit */
    draw_item(d, ef + 1);
    return ef + 1;
}
/* one pass of a modal dialog d: draws it if needed, then takes the events due (the filter first,
 * if any): the item hit, -1 if the filter answered (the item is in *pitem), or 0 (nothing yet) */
static int modal_events(uint32_t d, uint32_t filter, uint32_t pitem) {
    if (qd_take_update_for(d)) draw_dialog(d);
    input_poll(hx_ticks());
    uint32_t ev = HX_SCRATCH; int hit = 0;
    /* the filter's answer: 1 = it handled the event (the item number is in *pitem) */
    #define FILTER() (filter && (hx_call_pascal(filter, 2, 3, (uint32_t[]){d, ev, pitem}, (int[]){4, 4, 4}) & 0xFF00))
    if (filter) {   /* updates of the windows behind come first, as GetNextEvent returns them */
        uint32_t uw;
        while (!hit && (uw = qd_take_update()) != 0) {
            if (uw == d) { draw_dialog(d); continue; }
            for (int i = 0; i < 16; i++) wr8(ev + i, 0);
            wr16(ev, 6); wr32(ev + 2, uw); wr32(ev + 6, hx_ticks());
            if (FILTER()) hit = -1;
        }
    }
    while (!hit && input_get_event(ev, (1 << 1) | (1 << 2) | (1 << 3) | (1 << 5), 1)) {
        if (FILTER()) { hit = -1; break; }
        uint16_t what = rd16(ev); uint32_t msg = rd32(ev + 2);
        int16_t gv = (int16_t)rd16(ev + 10), gh = (int16_t)rd16(ev + 12);
        if (what == 1) {
            int no = find_item(d, gv, gh);
            if (!no || !item_active(d, no)) continue;
            if (is_control(rd8(item_at(d, no) + 12))) {
                track_item = no;
                if ((rd8(item_at(d, no) + 12) & 0x7F) == MTB_DI_BUTTON) { wr8(rd32(rd32(item_at(d, no))) + C_Hilite, 1); draw_item(d, no); }
            } else hit = no;
        } else if (what == 2) {
            if (!track_item) continue;
            int no = track_item; track_item = 0;
            uint32_t it = item_at(d, no);
            if ((rd8(it + 12) & 0x7F) == MTB_DI_BUTTON) { wr8(rd32(rd32(it)) + C_Hilite, 0); draw_item(d, no); }
            if (find_item(d, gv, gh) == no) hit = no;
        } else {
            uint8_t ch = (uint8_t)msg;
            if (ch == '\r' || ch == 0x03) {
                int def = (int16_t)rd16(d + D_ADefItem);
                if (item_active(d, def) && is_control(rd8(item_at(d, def) + 12))) { flash_button(d, def); hit = def; }
                continue;
            }
            int no = edit_key(d, ch);
            if (no && !(rd8(item_at(d, no) + 12) & MTB_DI_DISABLED)) hit = no;
        }
    }
    #undef FILTER
    return hit;
}
/* nothing to do yet: a tick passes and the trap runs again */
static void modal_wait(const char *what) {
    if (!input_pending()) {
        g_stop = 1; snprintf(g_stop_reason, sizeof g_stop_reason, "waiting in %s at %s, input script done", what, hx_where());
        return;
    }
    vbl_advance(1);
    if (!hx_trap_retry()) hx_fault("%s called by address at %s: cannot wait", what, hx_where());
}
static void t_ModalDialog(uint16_t w) {
    (void)w;
    uint32_t sp = reg(A7), pitem = rd32(sp), filter = rd32(sp + 4);
    uint32_t d = qd_front_window();
    if (!is_dialog(d)) { hx_fault("ModalDialog at %s with no dialog in front", hx_where()); return; }
    int hit = modal_events(d, filter, pitem);
    if (hit) {
        ARGL(); ARGL();
        if (hit > 0) wr16(pitem, (uint16_t)hit);
        else hit = (int16_t)rd16(pitem);                   /* answered by the filter */
        hx_log("ModalDialog at %s: item %d", hx_where(), hit);
        return;
    }
    modal_wait("ModalDialog");
}

/* Alert / StopAlert / NoteAlert / CautionAlert(id, filter): a modal dialog from 'ALRT' id (its
 * rectangle, DITL, and stage 1's default button: item 2 if its bold bit is set, else 1), shown
 * until a button is hit; then disposed, and the item returned.
 * FIDELITY: the alert icon is not drawn, the alert sound is not played, every alert is stage 1. */
static uint32_t alert_d;
static void t_Alert(uint16_t w) {
    uint32_t sp = reg(A7), filter = rd32(sp); int16_t id = (int16_t)rd16(sp + 4);
    if (!alert_d) {
        uint32_t r = rs_get(FOURCC('A','L','R','T'), id, 0), dr = r ? rs_get(FOURCC('D','I','T','L'), (int16_t)rd16(rd32(r) + 8), 0) : 0;
        if (!dr) { ARGL(); ARGW(); hx_log("Alert(%d) at %s: no ALRT / DITL", id, hx_where()); RETW(-1); return; }
        uint32_t a = rd32(r);
        uint16_t pos = mm_handle_size(r) >= 14 ? rd16(a + 12) : 0;
        alert_d = build_dialog(0, qd_auto_position(rd_rect(a), pos ? pos : 0x300A), dr, 1, 0, 0, 0xFFFFFFFFu);
        wr16(alert_d + D_ADefItem, (rd16(a + 10) & 8) ? 2 : 1);
        hx_log("Alert(%d) at %s", id, hx_where());
    }
    int hit = modal_events(alert_d, filter, HX_SCRATCH + 0x40);
    if (hit) {
        if (hit < 0) hit = (int16_t)rd16(HX_SCRATCH + 0x40);
        ARGL(); ARGW();
        close_dialog(alert_d, 1); alert_d = 0;
        hx_log("Alert(%d): item %d", id, hit);
        RETW(hit);
        return;
    }
    modal_wait("Alert");
}

typedef void (*TrapFn)(uint16_t w);
void traps_register_tb(uint16_t n, TrapFn f);
void dlg_register(void) {
    traps_register_tb(0x17C, t_GetNewDialog); traps_register_tb(0x191, t_ModalDialog);
    traps_register_tb(0x18D, t_GetDialogItem); traps_register_tb(0x18E, t_SetDialogItem);
    traps_register_tb(0x190, t_GetDialogItemText); traps_register_tb(0x18F, t_SetDialogItemText);
    traps_register_tb(0x17E, t_SelectDialogItemText); traps_register_tb(0x181, t_DrawDialog);
    traps_register_tb(0x15D, t_HiliteControl); traps_register_tb(0x163, t_SetControlValue); traps_register_tb(0x160, t_GetControlValue);
    traps_register_tb(0x185, t_Alert); traps_register_tb(0x186, t_Alert); traps_register_tb(0x187, t_Alert); traps_register_tb(0x188, t_Alert);
    traps_register_tb(0x157, t_ShowControl); traps_register_tb(0x158, t_HideControl);
    traps_register_tb(0x183, t_DisposeDialog); traps_register_tb(0x182, t_DisposeDialog);
    traps_register_tb(0x12D, t_CloseWindow); traps_register_tb(0x114, t_CloseWindow);
}
