/* Toolbox glue for the game's C code (game/mz.h, game/mzasm.h): each mac_ call pushes its Pascal
 * arguments, dispatches the trap to the Toolbox implementation and pops the result, exactly as the
 * original's call did; plus the C code's runtime (segments, idling, scratch memory). Shared by the
 * harness (with the per-call checker, sub.c) and the port (port/rt.c), which provide the rt_ hooks. */
#include "hx.h"
#include "m68k.h"
#include "../game/mzasm.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
int mtb_sane_pop_bytes(uint16_t op);   /* mactb/sane.c */
const char *hx_where(void);

uint8_t *M;                        /* game code's view of memory */
uint32_t g_sub_pc;                 /* entry of the C function now running (for logs) */
static uint32_t local_top;         /* mz_local scratch */
void     mz_local_reset(void) { local_top = 0; }
uint32_t mz_local_get(void) { return local_top; }
void     mz_local_set(uint32_t t) { local_top = t; }

/* hooks */
int  rt_in_check_phase(void);
void rt_skip_compare(int why);
void rt_abandon(int why);
void rt_idle_tick(void);
void rt_run_pending(void);
static void dispatch(uint16_t trap);

/* ---------- registers ---------- */
void sub_regs_load(Regs *r) {
    for (int i = 0; i < 8; i++) { r->d[i] = reg(M68K_REG_D0 + i); r->a[i] = reg(M68K_REG_A0 + i); }
    r->ccr = (uint8_t)(reg(M68K_REG_SR) & 0x1F);
}
void sub_regs_store(const Regs *r) {
    for (int i = 0; i < 8; i++) { setreg(M68K_REG_D0 + i, r->d[i]); setreg(M68K_REG_A0 + i, r->a[i]); }
    setreg(M68K_REG_SR, (reg(M68K_REG_SR) & ~0x1Fu) | r->ccr);
}

/* Calling through the jump table into a segment that is not loaded yet loads it (allocating its
 * code handle), as the original's first call does. In the C phase of a check this cannot be undone,
 * so the call is not compared (counted as skipped) and the original runs alone. */
int seg_is_loaded(int n);
void mz_seg(int n) {
    if (seg_is_loaded(n)) return;
    if (rt_in_check_phase()) { rt_skip_compare(n); return; }
    if (seg_load(n)) hx_fault("loading segment %d for C code failed", n);
}

/* Where the original spins until an interrupt changes something, the C code calls mz_idle once per
 * pass. Running normally, a tick passes and its VBL tasks run (vbl_idle_tick). In the C phase of a
 * check the call cannot be compared (the time would pass twice), so the C version is abandoned, its
 * writes undone, and the original runs alone. If the run's tick limit arrives here, the C version is
 * abandoned too: the original stops at this very point, inside its loop. */
void mz_idle(void) {
    if (rt_in_check_phase()) { rt_skip_compare(-1); rt_abandon(1); }
    /* the interrupt's stack must stay clear of the C code's scratch below the stack pointer */
    uint32_t sp = reg(M68K_REG_A7), lt = local_top, pc = g_sub_pc;
    if (lt && lt < sp) setreg(M68K_REG_A7, lt & ~3u);
    rt_idle_tick();
    setreg(M68K_REG_A7, sp); local_top = lt; g_sub_pc = pc;
    if (g_stop) rt_abandon(2);
}

/* ---------- Toolbox glue for C game code: push Pascal arguments, dispatch the trap, pop the result
 * (dispatched as a call by address: such a trap cannot be retried by re-running an A-line word).
 * Check mode undoes a C call's memory writes, so glue is only for traps whose whole effect is in
 * emulated memory (Random: the seed in the QuickDraw globals). ---------- */
uint8_t mac_FP68K(uint16_t op, uint32_t dst, uint32_t src) {
    if (mtb_sane_pop_bytes(op) == 10) push32(src);
    push32(dst); push16(op); dispatch(0xA9EB);
    return (uint8_t)(reg(M68K_REG_SR) & 0x1F);
}
int16_t mac_Random(void) { push16(0); dispatch(0xA861); return (int16_t)pop16(); }

/* Pascal-convention Toolbox traps: result space, then the arguments in order ('w' = 2 bytes,
 * 'l' = 4 bytes); the trap pops the arguments and leaves the result. */
/* Dispatches a trap for C glue. A trap that waits (ModalDialog with nothing to do yet) has let a
 * tick pass and asks to run again with the same arguments: its VBL processing runs now, and the trap
 * again. In the C phase of a check the call is abandoned instead (as for mz_idle). */
extern int g_trap_wait;
/* Ticks that passed during a trap (a sleep, a wait) have their VBL processing run before the C code
 * goes on, as the original's would at its next instruction; in the C phase of a check the call is
 * abandoned instead (as for mz_idle). */
int vbl_pending_ticks(void);
static void run_ticks(void) {
    if (rt_in_check_phase()) { rt_skip_compare(-1); rt_abandon(1); }
    if (g_stop) rt_abandon(2);
    uint32_t sp = reg(M68K_REG_A7), lt = local_top, pc = g_sub_pc;
    if (lt && lt < sp) setreg(M68K_REG_A7, lt & ~3u);    /* the interrupt's stack below the scratch */
    rt_run_pending();
    setreg(M68K_REG_A7, sp); local_top = lt; g_sub_pc = pc;
    if (g_stop) rt_abandon(2);
}
static void dispatch(uint16_t trap) {
    trap_dispatch(trap, g_sub_pc, 1);
    if (vbl_pending_ticks()) run_ticks();
}
static void glue_dispatch(uint16_t trap) {
    for (;;) {
        g_trap_wait = 0;
        trap_dispatch(trap, g_sub_pc, 1);
        if (!g_trap_wait) { if (vbl_pending_ticks()) run_ticks(); return; }
        g_trap_wait = 0;
        run_ticks();                       /* the trap's arguments stay above the interrupt's stack */
    }
}
static uint32_t pascal_trap(uint16_t trap, int retbytes, const char *sig, ...) {
    va_list ap; va_start(ap, sig);
    if (retbytes == 2) push16(0); else if (retbytes == 4) push32(0);
    for (const char *c = sig; *c; c++) { uint32_t v = va_arg(ap, uint32_t); if (*c == 'w') push16((uint16_t)v); else push32(v); }
    va_end(ap);
    glue_dispatch(trap);
    return retbytes == 2 ? pop16() : retbytes == 4 ? pop32() : 0;
}
void     mac_InitGraf(uint32_t globals)            { pascal_trap(0xA86E, 0, "l", globals); }
void     mac_InitFonts(void)                       { pascal_trap(0xA8FE, 0, ""); }
void     mac_InitWindows(void)                     { pascal_trap(0xA912, 0, ""); }
void     mac_InitMenus(void)                       { pascal_trap(0xA930, 0, ""); }
void     mac_TEInit(void)                          { pascal_trap(0xA9CC, 0, ""); }
void     mac_InitDialogs(uint32_t resume)          { pascal_trap(0xA97B, 0, "l", resume); }
void     mac_InitCursor(void)                      { pascal_trap(0xA850, 0, ""); }
void     mac_ShowCursor(void)                      { pascal_trap(0xA853, 0, ""); }
void     mac_HideCursor(void)                      { pascal_trap(0xA852, 0, ""); }
void     mac_CloseWindow(uint32_t w)               { pascal_trap(0xA92D, 0, "l", w); }
void     mac_DisposeWindow(uint32_t w)             { pascal_trap(0xA914, 0, "l", w); }
uint32_t mac_GetWRefCon(uint32_t w)                { return pascal_trap(0xA917, 4, "l", w); }
uint32_t mac_FrontWindow(void)                     { return pascal_trap(0xA924, 4, ""); }
void     mac_BeginUpdate(uint32_t w)               { pascal_trap(0xA922, 0, "l", w); }
void     mac_EndUpdate(uint32_t w)                 { pascal_trap(0xA923, 0, "l", w); }
void     mac_SetPort(uint32_t port)                { pascal_trap(0xA873, 0, "l", port); }
/* QDExtensions (_AB1D): routine selector in D0, Pascal arguments */
static uint32_t qdx(uint32_t sel, int retbytes, const char *sig, uint32_t a) { setreg(M68K_REG_D0, sel); return pascal_trap(0xAB1D, retbytes, sig, a); }
void     mac_GetGWorld(uint32_t pport, uint32_t pgdh) { setreg(M68K_REG_D0, 0x80005); pascal_trap(0xAB1D, 0, "ll", pport, pgdh); }
void     mac_SetGWorld(uint32_t port, uint32_t gdh)   { setreg(M68K_REG_D0, 0x80006); pascal_trap(0xAB1D, 0, "ll", port, gdh); }
uint32_t mac_GetGDevice(void)                       { return pascal_trap(0xAA32, 4, ""); }
int32_t  mac_Entry2Index(int16_t entry)             { setreg(M68K_REG_D0, 0); return (int32_t)pascal_trap(0xAAA2, 4, "w", (uint32_t)(uint16_t)entry); }
int16_t  mac_CurResFile(void)                      { return (int16_t)pascal_trap(0xA994, 2, ""); }
void     mac_UseResFile(int16_t file)              { pascal_trap(0xA998, 0, "w", (uint32_t)(uint16_t)file); }
uint32_t mac_GetResource(uint32_t type, int16_t id) { return pascal_trap(0xA9A0, 4, "lw", type, (uint32_t)(uint16_t)id); }
int16_t  mac_ResError(void)                        { return (int16_t)pascal_trap(0xA9AF, 2, ""); }
void     mac_DetachResource(uint32_t h)            { pascal_trap(0xA992, 0, "l", h); }
int16_t  mac_SndNewChannel(uint32_t pchan, int16_t synth, int32_t init, uint32_t proc) { return (int16_t)pascal_trap(0xA807, 2, "lwll", pchan, (uint32_t)(uint16_t)synth, (uint32_t)init, proc); }
int16_t  mac_SndDisposeChannel(uint32_t chan, int quiet)   { return (int16_t)pascal_trap(0xA801, 2, "lw", chan, quiet ? 0x100u : 0u); }
int16_t  mac_SndDoCommand(uint32_t chan, uint32_t cmd, int nowait) { return (int16_t)pascal_trap(0xA803, 2, "llw", chan, cmd, nowait ? 0x100u : 0u); }
int16_t  mac_SndDoImmediate(uint32_t chan, uint32_t cmd)   { return (int16_t)pascal_trap(0xA804, 2, "ll", chan, cmd); }
void     mac_TETextBox(uint32_t text, int32_t len, uint32_t box, int16_t just) { pascal_trap(0xA9CE, 0, "lllw", text, (uint32_t)len, box, (uint32_t)(uint16_t)just); }
uint32_t mac_GetCTable(int16_t id)                 { return pascal_trap(0xAA18, 4, "w", (uint32_t)(uint16_t)id); }
void     mac_DisposeCTable(uint32_t ct)            { pascal_trap(0xAA24, 0, "l", ct); }
uint32_t mac_NewPalette(int16_t n, uint32_t ct, int16_t usage, int16_t tol) {
    return pascal_trap(0xAA91, 4, "wlww", (uint32_t)(uint16_t)n, ct, (uint32_t)(uint16_t)usage, (uint32_t)(uint16_t)tol);
}
void     mac_DisposeGWorld(uint32_t gw)            { qdx(0x40004, 0, "l", gw); }
/* Gestalt (_A1AD): selector in D0, response in A0, result code in D0 */
int16_t  mac_Gestalt(uint32_t sel, uint32_t presp) {
    setreg(M68K_REG_D0, sel); dispatch(0xA1AD);
    w32(presp, reg(M68K_REG_A0)); return (int16_t)reg(M68K_REG_D0);
}
/* NGetTrapAddress (_A746 Toolbox / _A346 OS): trap number in D0.w, address back in A0 */
uint32_t mac_NGetTrapAddress(uint16_t trap, int toolbox) {
    setreg(M68K_REG_D0, (reg(M68K_REG_D0) & 0xFFFF0000u) | trap); dispatch(toolbox ? 0xA746 : 0xA346);
    return reg(M68K_REG_A0);
}
/* BlockMove (_A02E): A0 source, A1 destination, D0 count */
void     mac_BlockMove(uint32_t src, uint32_t dst, int32_t n) {
    setreg(M68K_REG_A0, src); setreg(M68K_REG_A1, dst); setreg(M68K_REG_D0, (uint32_t)n); dispatch(0xA02E);
}
int16_t  mac_FindFolder(int16_t vref, uint32_t type, int create, uint32_t pvref, uint32_t pdir) {
    setreg(M68K_REG_D0, 0);
    return (int16_t)pascal_trap(0xA823, 2, "wlwll", (uint32_t)(uint16_t)vref, type, create ? 0x100u : 0u, pvref, pdir);
}
int16_t  mac_NewGWorld(uint32_t pgw, int16_t depth, uint32_t rect, uint32_t ctab, uint32_t gdh, uint32_t flags) {
    setreg(M68K_REG_D0, 0x160000);
    return (int16_t)pascal_trap(0xAB1D, 2, "lwllll", pgw, (uint32_t)(uint16_t)depth, rect, ctab, gdh, flags);
}
uint32_t mac_UpdateGWorld(uint32_t pgw, int16_t depth, uint32_t rect, uint32_t ctab, uint32_t gdh, uint32_t flags) {
    setreg(M68K_REG_D0, 0x160003);
    return pascal_trap(0xAB1D, 4, "lwllll", pgw, (uint32_t)(uint16_t)depth, rect, ctab, gdh, flags);
}
void     mac_NSetPalette(uint32_t win, uint32_t pal, int16_t updates) { pascal_trap(0xAA95, 0, "llw", win, pal, (uint32_t)(uint16_t)updates); }
void     mac_ClipRect(uint32_t r)                  { pascal_trap(0xA87B, 0, "l", r); }
int      mac_WaitNextEvent(int16_t mask, uint32_t ev, uint32_t sleep, uint32_t rgn) {
    return (pascal_trap(0xA860, 2, "wlll", (uint32_t)(uint16_t)mask, ev, sleep, rgn) >> 8) & 0xFF;
}
static uint32_t os_a0(uint16_t w, uint32_t a0);
int      mac_TestDeviceAttribute(uint32_t gdh, int16_t attr) { return (pascal_trap(0xAA2C, 2, "lw", gdh, (uint32_t)(uint16_t)attr) >> 8) & 0xFF; }
void     mac_DisposePalette(uint32_t pal)          { pascal_trap(0xAA93, 0, "l", pal); }
void     mac_ValidRect(uint32_t r)                 { pascal_trap(0xA92A, 0, "l", r); }
int16_t  mac_VInstall(uint32_t task)               { return (int16_t)os_a0(0xA033, task); }
int16_t  mac_VRemove(uint32_t task)                { return (int16_t)os_a0(0xA034, task); }
void     mac_GetKeys(uint32_t keymap)            { pascal_trap(0xA976, 0, "l", keymap); }
#define W(x) ((uint32_t)(uint16_t)(x))
uint32_t mac_GetNewCWindow(int16_t id, uint32_t storage, uint32_t behind) { return pascal_trap(0xAA46, 4, "wll", W(id), storage, behind); }
void mac_SizeWindow(uint32_t w, int16_t width, int16_t height, int update) { pascal_trap(0xA91D, 0, "lwww", w, W(width), W(height), update ? 0x100u : 0u); }
void mac_MoveWindow(uint32_t w, int16_t h, int16_t v, int front) { pascal_trap(0xA91B, 0, "lwww", w, W(h), W(v), front ? 0x100u : 0u); }
void mac_ShowWindow(uint32_t w) { pascal_trap(0xA915, 0, "l", w); }
void mac_HideWindow(uint32_t w) { pascal_trap(0xA916, 0, "l", w); }
void mac_SelectWindow(uint32_t w) { pascal_trap(0xA91F, 0, "l", w); }
uint32_t mac_NewRgn(void) { return pascal_trap(0xA8D8, 4, ""); }
void mac_SetRect(uint32_t r, int16_t left, int16_t top, int16_t right, int16_t bottom) { pascal_trap(0xA8A7, 0, "lwwww", r, W(left), W(top), W(right), W(bottom)); }
void mac_RectRgn(uint32_t rgn, uint32_t r) { pascal_trap(0xA8DF, 0, "ll", rgn, r); }
void mac_UnionRgn(uint32_t a, uint32_t b, uint32_t dst) { pascal_trap(0xA8E5, 0, "lll", a, b, dst); }
void mac_DisposeRgn(uint32_t rgn) { pascal_trap(0xA8D9, 0, "l", rgn); }
void mac_CopyRgn(uint32_t src, uint32_t dst) { pascal_trap(0xA8DC, 0, "ll", src, dst); }
void mac_PaintBehind(uint32_t w, uint32_t rgn) { pascal_trap(0xA90D, 0, "ll", w, rgn); }
void mac_CalcVisBehind(uint32_t w, uint32_t rgn) { pascal_trap(0xA90A, 0, "ll", w, rgn); }
uint32_t mac_GetNewPalette(int16_t id) { return pascal_trap(0xAA92, 4, "w", W(id)); }
void mac_SetDialogItem(uint32_t d, int16_t item, int16_t type, uint32_t h, uint32_t box) { pascal_trap(0xA98E, 0, "lwwll", d, W(item), W(type), h, box); }
void mac_ShowControl(uint32_t c) { pascal_trap(0xA957, 0, "l", c); }
void mac_HideControl(uint32_t c) { pascal_trap(0xA958, 0, "l", c); }
void mac_GetPenState(uint32_t ps) { pascal_trap(0xA898, 0, "l", ps); }
void mac_SetPenState(uint32_t ps) { pascal_trap(0xA899, 0, "l", ps); }
void mac_PenNormal(void) { pascal_trap(0xA89E, 0, ""); }
uint32_t mac_GetPicture(int16_t id) { return pascal_trap(0xA9BC, 4, "w", W(id)); }
void mac_DrawPicture(uint32_t pic, uint32_t r) { pascal_trap(0xA8F6, 0, "ll", pic, r); }
void mac_OffsetRect(uint32_t r, int16_t dh, int16_t dv) { pascal_trap(0xA8A8, 0, "lww", r, W(dh), W(dv)); }
void mac_InsetRect(uint32_t r, int16_t dh, int16_t dv) { pascal_trap(0xA8A9, 0, "lww", r, W(dh), W(dv)); }
uint32_t mac_GetDeviceList(void) { return pascal_trap(0xAA29, 4, ""); }
uint32_t mac_GetNextDevice(uint32_t gd) { return pascal_trap(0xAA2B, 4, "l", gd); }
void mac_SetWRefCon(uint32_t w, uint32_t ref) { pascal_trap(0xA918, 0, "ll", w, ref); }
uint32_t mac_GetMainDevice(void) { return pascal_trap(0xAA2A, 4, ""); }
int16_t mac_SlotVInstall(uint32_t task, int16_t slot) { setreg(M68K_REG_A0, task); setreg(M68K_REG_D0, (reg(M68K_REG_D0) & 0xFFFF0000u) | W(slot)); dispatch(0xA06F); return (int16_t)reg(M68K_REG_D0); }
int16_t mac_SlotVRemove(uint32_t task, int16_t slot) { setreg(M68K_REG_A0, task); setreg(M68K_REG_D0, (reg(M68K_REG_D0) & 0xFFFF0000u) | W(slot)); dispatch(0xA070); return (int16_t)reg(M68K_REG_D0); }
uint32_t mac_GetCursor(int16_t id) { return pascal_trap(0xA9B9, 4, "w", W(id)); }
void mac_SetCursor(uint32_t crsr) { pascal_trap(0xA851, 0, "l", crsr); }
void mac_GetPort(uint32_t pport) { pascal_trap(0xA874, 0, "l", pport); }
uint32_t mac_GetNewDialog(int16_t id, uint32_t storage, uint32_t behind) { return pascal_trap(0xA97C, 4, "wll", W(id), storage, behind); }
void mac_GetDialogItem(uint32_t d, int16_t item, uint32_t ptype, uint32_t phandle, uint32_t pbox) { pascal_trap(0xA98D, 0, "lwlll", d, W(item), ptype, phandle, pbox); }
void mac_GetDialogItemText(uint32_t h, uint32_t str) { pascal_trap(0xA990, 0, "ll", h, str); }
void mac_HiliteControl(uint32_t c, int16_t state) { pascal_trap(0xA95D, 0, "lw", c, W(state)); }
void mac_ModalDialog(uint32_t filter, uint32_t pitem) { pascal_trap(0xA991, 0, "ll", filter, pitem); }
void mac_DisposeDialog(uint32_t d) { pascal_trap(0xA983, 0, "l", d); }
void mac_InvalRect(uint32_t r) { pascal_trap(0xA928, 0, "l", r); }
#undef W
int      mac_SectRect(uint32_t a, uint32_t b, uint32_t dst) { return (pascal_trap(0xA8AA, 2, "lll", a, b, dst) >> 8) & 0xFF; }
#define W(x) ((uint32_t)(uint16_t)(x))
int16_t mac_GetCurrentProcess(uint32_t psn) { return (int16_t)pascal_trap(0xA88F, 2, "lw", psn, 0x37u); }
int16_t mac_GetProcessInformation(uint32_t psn, uint32_t info) { return (int16_t)pascal_trap(0xA88F, 2, "llw", psn, info, 0x3Au); }
int16_t mac_FSpOpenResFile(uint32_t spec, int8_t perm) { setreg(M68K_REG_D0, 0xD); return (int16_t)pascal_trap(0xAA52, 2, "lw", spec, (uint32_t)(uint8_t)perm << 8); }
void mac_CloseResFile(int16_t ref) { pascal_trap(0xA99A, 0, "w", W(ref)); }
int16_t mac_AEInstallEventHandler(uint32_t cls, uint32_t id, uint32_t handler, uint32_t refcon, int sys) { setreg(M68K_REG_D0, 0x91F); return (int16_t)pascal_trap(0xA816, 2, "llllw", cls, id, handler, refcon, sys ? 0x100u : 0u); }
#undef W
/* register-based OS traps on a parameter block (A0), result in D0 */
int16_t  mac_PBCall(uint16_t trap, uint32_t pb)   { return (int16_t)os_a0(trap, pb); }
int16_t  mac_FSMakeFSSpec(int16_t vref, int32_t dir, uint32_t name, uint32_t spec) {
    setreg(M68K_REG_D0, 1); return (int16_t)pascal_trap(0xAA52, 2, "wlll", (uint32_t)(uint16_t)vref, (uint32_t)dir, name, spec);
}
int16_t  mac_FSpOpenDF(uint32_t spec, int8_t perm, uint32_t pref) {
    setreg(M68K_REG_D0, 2); return (int16_t)pascal_trap(0xAA52, 2, "lwl", spec, (uint32_t)(uint8_t)perm << 8, pref);
}
void     mac_ReleaseResource(uint32_t h)          { pascal_trap(0xA9A3, 0, "l", h); }
#define W(x) ((uint32_t)(uint16_t)(x))
void mac_GetFNum(uint32_t name, uint32_t pnum) { pascal_trap(0xA900, 0, "ll", name, pnum); }
uint32_t mac_NewPtr(int32_t size) { setreg(M68K_REG_D0, (uint32_t)size); dispatch(0xA11E); return reg(M68K_REG_A0); }
int16_t mac_Alert(int16_t id, uint32_t filter) { return (int16_t)pascal_trap(0xA985, 2, "wl", W(id), filter); }
void mac_GlobalToLocal(uint32_t pt) { pascal_trap(0xA871, 0, "l", pt); }
int16_t mac_FSpCreate(uint32_t spec, uint32_t creator, uint32_t type, int16_t script) { setreg(M68K_REG_D0, 4); return (int16_t)pascal_trap(0xAA52, 2, "lllw", spec, creator, type, W(script)); }
int16_t mac_FSpDelete(uint32_t spec) { setreg(M68K_REG_D0, 6); return (int16_t)pascal_trap(0xAA52, 2, "l", spec); }
int16_t mac_FSpExchangeFiles(uint32_t a, uint32_t b) { setreg(M68K_REG_D0, 0xF); return (int16_t)pascal_trap(0xAA52, 2, "ll", a, b); }
int16_t mac_AEProcessAppleEvent(uint32_t ev) { setreg(M68K_REG_D0, 0x21B); return (int16_t)pascal_trap(0xA816, 2, "l", ev); }
#undef W
void     mz_unsupported(const char *what)           { hx_fault("C code reached a path the port does not support: %s", what); }
uint32_t mac_GetPixBaseAddr(uint32_t pm)   { return qdx(0x4000F, 4, "l", pm); }
uint32_t mac_GetGWorldDevice(uint32_t gw)  { return qdx(0x40012, 4, "l", gw); }
uint32_t mac_GetGWorldPixMap(uint32_t gw)  { return qdx(0x40017, 4, "l", gw); }
int      mac_LockPixels(uint32_t pm)       { return (qdx(0x40001, 2, "l", pm) >> 8) & 0xFF; }
void     mac_UnlockPixels(uint32_t pm)     { qdx(0x40002, 0, "l", pm); }
void     mac_ForeColor(int32_t c)          { pascal_trap(0xA862, 0, "l", (uint32_t)c); }
void     mac_BackColor(int32_t c)          { pascal_trap(0xA863, 0, "l", (uint32_t)c); }
void     mac_CopyBits(uint32_t src, uint32_t dst, uint32_t sr, uint32_t dr, int16_t mode, uint32_t mask) {
    pascal_trap(0xA8EC, 0, "llllwl", src, dst, sr, dr, (uint32_t)(uint16_t)mode, mask);
}
/* SwapMMUMode (_A05D): the mode in D0.b, the previous one back in D0.b */
uint8_t  mac_SwapMMUMode(uint8_t mode) {
    setreg(M68K_REG_D0, (reg(M68K_REG_D0) & ~0xFFu) | mode); dispatch(0xA05D);
    return (uint8_t)reg(M68K_REG_D0);
}
void     mac_PmForeColor(int16_t entry)            { pascal_trap(0xAA97, 0, "w", (uint32_t)(uint16_t)entry); }
void     mac_PaintRect(uint32_t rect)              { pascal_trap(0xA8A2, 0, "l", rect); }
uint32_t mac_GetNewMBar(int16_t id)                { return pascal_trap(0xA9C0, 4, "w", (uint32_t)(uint16_t)id); }
void     mac_SetMenuBar(uint32_t list)             { pascal_trap(0xA93C, 0, "l", list); }
uint32_t mac_GetMenuHandle(int16_t id)             { return pascal_trap(0xA949, 4, "w", (uint32_t)(uint16_t)id); }
void     mac_EnableItem(uint32_t menu, int16_t item)  { pascal_trap(0xA939, 0, "lw", menu, (uint32_t)(uint16_t)item); }
void     mac_DisableItem(uint32_t menu, int16_t item) { pascal_trap(0xA93A, 0, "lw", menu, (uint32_t)(uint16_t)item); }
void     mac_CheckItem(uint32_t menu, int16_t item, int checked) { pascal_trap(0xA945, 0, "lww", menu, (uint32_t)(uint16_t)item, checked ? 0x100u : 0u); }
uint32_t mac_MenuKey(int16_t ch)                      { return pascal_trap(0xA93E, 4, "w", (uint32_t)(uint16_t)ch); }
int16_t  mac_FindWindow(uint32_t pt, uint32_t pwin)  { return (int16_t)pascal_trap(0xA92C, 2, "ll", pt, pwin); }
uint32_t mac_MenuSelect(uint32_t pt)                { return pascal_trap(0xA93D, 4, "l", pt); }
void     mac_HiliteMenu(int16_t menu)                 { pascal_trap(0xA938, 0, "w", (uint32_t)(uint16_t)menu); }
void     mac_AppendResMenu(uint32_t menu, uint32_t type) { pascal_trap(0xA94D, 0, "ll", menu, type); }
void     mac_DrawMenuBar(void)                     { pascal_trap(0xA937, 0, ""); }
void     mac_ParamText(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) { pascal_trap(0xA98B, 0, "llll", p0, p1, p2, p3); }
int16_t  mac_StopAlert(int16_t id, uint32_t filter) { return (int16_t)pascal_trap(0xA986, 2, "wl", (uint32_t)(uint16_t)id, filter); }
int16_t  mac_NoteAlert(int16_t id, uint32_t filter) { return (int16_t)pascal_trap(0xA987, 2, "wl", (uint32_t)(uint16_t)id, filter); }
void     mac_PmBackColor(int16_t entry)            { pascal_trap(0xAA98, 0, "w", (uint32_t)(uint16_t)entry); }
void     mac_FrameRect(uint32_t rect)              { pascal_trap(0xA8A1, 0, "l", rect); }
void     mac_EraseRect(uint32_t rect)              { pascal_trap(0xA8A3, 0, "l", rect); }
void     mac_MoveTo(int16_t h, int16_t v)          { pascal_trap(0xA893, 0, "ww", (uint32_t)(uint16_t)h, (uint32_t)(uint16_t)v); }
void     mac_Move(int16_t dh, int16_t dv)          { pascal_trap(0xA894, 0, "ww", (uint32_t)(uint16_t)dh, (uint32_t)(uint16_t)dv); }
void     mac_LineTo(int16_t h, int16_t v)          { pascal_trap(0xA891, 0, "ww", (uint32_t)(uint16_t)h, (uint32_t)(uint16_t)v); }
void     mac_Line(int16_t dh, int16_t dv)          { pascal_trap(0xA892, 0, "ww", (uint32_t)(uint16_t)dh, (uint32_t)(uint16_t)dv); }
void     mac_DrawString(uint32_t str)              { pascal_trap(0xA884, 0, "l", str); }
void     mac_TextFont(int16_t font)                { pascal_trap(0xA887, 0, "w", (uint32_t)(uint16_t)font); }
void     mac_TextFace(int16_t face)                { pascal_trap(0xA888, 0, "w", (uint32_t)(uint16_t)face); }
void     mac_TextMode(int16_t mode)                { pascal_trap(0xA889, 0, "w", (uint32_t)(uint16_t)mode); }
void     mac_TextSize(int16_t size)                { pascal_trap(0xA88A, 0, "w", (uint32_t)(uint16_t)size); }
int16_t  mac_StringWidth(uint32_t str)             { return (int16_t)pascal_trap(0xA88C, 2, "l", str); }
void     mac_ExitToShell(void)                     { pascal_trap(0xA9F4, 0, ""); if (g_stop) rt_abandon(2); }

/* register-based OS traps taking a handle in A0, result code (or size) in D0 */
static uint32_t os_a0(uint16_t w, uint32_t a0) { setreg(M68K_REG_A0, a0); dispatch(w); return reg(M68K_REG_D0); }
void mac_NumToString(int32_t n, uint32_t str) {
    push16(0); setreg(M68K_REG_D0, (uint32_t)n); setreg(M68K_REG_A0, str);   /* Pack7 selector 0 */
    dispatch(0xA9EE);
}
uint32_t mac_StripAddress(uint32_t a) { setreg(M68K_REG_D0, a); dispatch(0xA055); return reg(M68K_REG_D0); }

/* Scratch memory for C code that needs a buffer in the Mac address space where the original used
 * its stack frame (a string passed to the Toolbox, say): taken below the stack pointer at the
 * start of the substituted call, which is exactly where the original's frame lived. */
uint32_t mz_frame_begin(void) { return local_top; }
void mz_frame_end(uint32_t *mark) { local_top = *mark; }
uint32_t mz_local(uint32_t n) {
    if (!local_top) local_top = reg(M68K_REG_A7) - 0x40;
    local_top -= (n + 3) & ~3u;
    return local_top;
}

int16_t mac_HLock(uint32_t h)         { return (int16_t)os_a0(0xA029, h); }
uint32_t mac_Delay(uint32_t ticks)    { return os_a0(0xA03B, ticks); }
int16_t mac_HUnlock(uint32_t h)       { return (int16_t)os_a0(0xA02A, h); }
int16_t mac_DisposeHandle(uint32_t h) { return (int16_t)os_a0(0xA023, h); }
int32_t mac_GetHandleSize(uint32_t h) { return (int32_t)os_a0(0xA025, h); }
uint32_t mac_NewHandle(int32_t size)    { setreg(M68K_REG_D0, (uint32_t)size); dispatch(0xA122); return reg(M68K_REG_A0); }

/* any trap from register-level code: the CPU registers (except A7) are the Regs block */
/* A register-based trap from a register-level routine: the trap sees r and r gets its results; the
 * CPU's own registers are put back afterwards, so C code above the routine (which may hold a
 * zeroed Regs of its own) never disturbs the registers of the original code around it. */
void mac_trap_regs(Regs *r, uint16_t w) {
    Regs cpu; sub_regs_load(&cpu); uint32_t sr = reg(M68K_REG_SR);
    for (int i = 0; i < 8; i++) setreg(M68K_REG_D0 + i, r->d[i]);
    for (int i = 0; i < 7; i++) setreg(M68K_REG_A0 + i, r->a[i]);
    setreg(M68K_REG_SR, (reg(M68K_REG_SR) & ~0x1Fu) | r->ccr);
    dispatch(w);
    for (int i = 0; i < 8; i++) r->d[i] = reg(M68K_REG_D0 + i);
    for (int i = 0; i < 7; i++) r->a[i] = reg(M68K_REG_A0 + i);
    r->ccr = (uint8_t)(reg(M68K_REG_SR) & 0x1F);
    sub_regs_store(&cpu); setreg(M68K_REG_SR, sr);
}
