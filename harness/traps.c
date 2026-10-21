/* Trap dispatch: implemented traps, signature-driven logging stubs for the rest. */
#include "hx.h"
#include "m68k.h"
#include "gen/trapsigs.h"
#include <string.h>
#include <stdlib.h>

typedef void (*TrapFn)(uint16_t w);
static TrapFn tb_fn[0x400];   /* Toolbox traps by 10-bit number */
static TrapFn os_fn[0x100];   /* OS traps by 8-bit number */
static const TrapSig *tb_sig[0x400];

typedef struct { char name[48]; uint32_t calls; uint32_t first_pc; int stub; } TrapStat;
static TrapStat stats[0x500];  /* 0x000-0x3FF toolbox, 0x400-0x4FF OS */
static uint32_t cur_trap_pc;

/* ---------- argument helpers (Pascal: last argument on top of stack) ---------- */
#define A7 M68K_REG_A7
#define ARGW()   ((int16_t)pop16())
#define ARGL()   (pop32())
#define RETW(v)  wr16(reg(A7), (uint16_t)(v))
#define RETL(v)  wr32(reg(A7), (uint32_t)(v))
#define RETB(v)  wr16(reg(A7), (uint16_t)((v) ? 0x0100 : 0))   /* Boolean: high byte of the word */

static const char *loc_str(uint32_t pc) {
    static char b[4][32]; static int i; i = (i + 1) & 3;
    uint32_t off; int n = seg_of(pc, &off);
    if (n) snprintf(b[i], 32, "S%d:%04X", n, off); else snprintf(b[i], 32, "%06X", pc);
    return b[i];
}

static const char *sel_name(uint16_t w, uint32_t sel) {
    for (const SelSig *s = g_selsigs; s->name; s++) if (s->trap == (w & 0xFBFF) && s->sel == sel) return s->name;
    return NULL;
}

/* ---------- OS: Memory Manager ---------- */
static void os_NewHandle(uint16_t w) { uint32_t h = mm_new_handle(reg(M68K_REG_D0), w & 0x0200); setreg(M68K_REG_A0, h); setreg(M68K_REG_D0, h ? 0 : (uint32_t)-108); }
static void os_NewPtr(uint16_t w) { uint32_t p = mm_new_ptr(reg(M68K_REG_D0), w & 0x0200); setreg(M68K_REG_A0, p); setreg(M68K_REG_D0, p ? 0 : (uint32_t)-108); }
static void os_DisposePtr(uint16_t w) { (void)w; mm_dispose_ptr(reg(M68K_REG_A0)); setreg(M68K_REG_D0, 0); }
static void os_DisposeHandle(uint16_t w) { (void)w; mm_dispose_handle(reg(M68K_REG_A0)); setreg(M68K_REG_D0, 0); }
static void os_GetHandleSize(uint16_t w) { (void)w; setreg(M68K_REG_D0, (uint32_t)mm_handle_size(reg(M68K_REG_A0))); wr16(LM_MemErr, 0); }
static void os_SetHandleSize(uint16_t w) { (void)w; int e = mm_set_handle_size(reg(M68K_REG_A0), reg(M68K_REG_D0)); setreg(M68K_REG_D0, (uint32_t)e); wr16(LM_MemErr, (uint16_t)e); }
static void os_GetPtrSize(uint16_t w) { (void)w; setreg(M68K_REG_D0, (uint32_t)mm_ptr_size(reg(M68K_REG_A0))); }
static void os_SetPtrSize(uint16_t w) {
    (void)w; uint32_t p = reg(M68K_REG_A0), n = reg(M68K_REG_D0);
    int32_t cur = mm_ptr_size(p);
    /* non-relocatable blocks can only shrink in place */
    setreg(M68K_REG_D0, (int32_t)n <= cur ? 0 : (uint32_t)-108);
}
static void os_HLock(uint16_t w) { (void)w; uint32_t h = reg(M68K_REG_A0); mm_set_state(h, mm_get_state(h) | 0x80); setreg(M68K_REG_D0, 0); }
static void os_HUnlock(uint16_t w) { (void)w; uint32_t h = reg(M68K_REG_A0); mm_set_state(h, mm_get_state(h) & ~0x80); setreg(M68K_REG_D0, 0); }
static void os_HPurge(uint16_t w) { (void)w; uint32_t h = reg(M68K_REG_A0); mm_set_state(h, mm_get_state(h) | 0x40); setreg(M68K_REG_D0, 0); }
static void os_HNoPurge(uint16_t w) { (void)w; uint32_t h = reg(M68K_REG_A0); mm_set_state(h, mm_get_state(h) & ~0x40); setreg(M68K_REG_D0, 0); }
static void os_HGetState(uint16_t w) { (void)w; setreg(M68K_REG_D0, mm_get_state(reg(M68K_REG_A0))); }
static void os_HSetState(uint16_t w) { (void)w; mm_set_state(reg(M68K_REG_A0), (uint8_t)reg(M68K_REG_D0)); setreg(M68K_REG_D0, 0); }
static void os_HSetRBit(uint16_t w) { (void)w; uint32_t h = reg(M68K_REG_A0); mm_set_state(h, mm_get_state(h) | 0x20); setreg(M68K_REG_D0, 0); }
static void os_HClrRBit(uint16_t w) { (void)w; uint32_t h = reg(M68K_REG_A0); mm_set_state(h, mm_get_state(h) & ~0x20); setreg(M68K_REG_D0, 0); }
static void os_EmptyHandle(uint16_t w) { (void)w; uint32_t h = reg(M68K_REG_A0); uint32_t p = rd32(h); if (p) { uint32_t mp = h; mm_set_handle_size(h, 0); (void)mp; } setreg(M68K_REG_D0, 0); }
static void os_BlockMove(uint16_t w) {
    (void)w; uint32_t s = reg(M68K_REG_A0), d = reg(M68K_REG_A1); int32_t n = (int32_t)reg(M68K_REG_D0);
    extern uint32_t watch_lo, watch_hi;
    if (watch_hi && n > 0 && (d & 0xFFFFFF) < watch_hi && (d & 0xFFFFFF) + (uint32_t)n > watch_lo)
        hx_log("watch: BlockMove %06X -> %06X (%d bytes) at %s", s, d, n, loc_str(cur_trap_pc));
    if (n > 0) { if (s + n > MEM_SIZE || d + n > MEM_SIZE) hx_fault("BlockMove out of range %06X->%06X %d", s, d, n); { hx_track(d, (uint32_t)(n)); memmove(g_mem + d, g_mem + s, n); } }
    setreg(M68K_REG_D0, 0);
}
static void os_FreeMem(uint16_t w) { (void)w; setreg(M68K_REG_D0, mm_free_bytes()); }
static void os_MaxMem(uint16_t w) { (void)w; setreg(M68K_REG_D0, mm_free_bytes()); setreg(M68K_REG_A0, 0); }
static void os_CompactMem(uint16_t w) { (void)w; setreg(M68K_REG_D0, mm_free_bytes()); }
static void os_PurgeSpace(uint16_t w) { (void)w; setreg(M68K_REG_D0, mm_free_bytes()); setreg(M68K_REG_A0, mm_free_bytes()); }
static void os_StackSpace(uint16_t w) { (void)w; setreg(M68K_REG_D0, reg(A7) - (rd32(LM_CurStackBase) - STACK_SIZE)); }
static void os_GetZone(uint16_t w) { (void)w; setreg(M68K_REG_A0, HEAP_BASE); setreg(M68K_REG_D0, 0); }
static void os_HandleZone(uint16_t w) { (void)w; setreg(M68K_REG_A0, HEAP_BASE); setreg(M68K_REG_D0, 0); }
static void os_RecoverHandle(uint16_t w) { (void)w; uint32_t p = reg(M68K_REG_A0); setreg(M68K_REG_A0, rd32(p - 16 + 8)); setreg(M68K_REG_D0, 0); }
static void os_NewEmptyHandle(uint16_t w) { (void)w; uint32_t h = mm_new_handle(0, 0); setreg(M68K_REG_A0, h); setreg(M68K_REG_D0, 0); }
static void os_noerr(uint16_t w) { (void)w; setreg(M68K_REG_D0, 0); }
static void os_StripAddress(uint16_t w) { (void)w; setreg(M68K_REG_D0, reg(M68K_REG_D0) & 0x00FFFFFF); }  /* our memory is 24-bit clean */
static void os_SwapMMUMode(uint16_t w) {   /* D0.b = new mode in, previous mode out; always 32-bit here */
    (void)w; setreg(M68K_REG_D0, (reg(M68K_REG_D0) & 0xFFFFFF00u) | 1);
}

/* ---------- OS: environment ---------- */
static void os_Gestalt(uint16_t w) {
    if ((w & 0x0600) != 0) { setreg(M68K_REG_D0, (uint32_t)-5551); return; }  /* New/Replace/GetGestaltValue */
    uint32_t sel = reg(M68K_REG_D0), v; int ok = 1;
    switch (sel) {
    case FOURCC('s','y','s','v'): v = 0x0710; break;
    case FOURCC('q','d',' ',' '): v = 0x0230; break;          /* 32-bit Color QuickDraw */
    case FOURCC('p','r','o','c'): v = 4; break;               /* 68030 */
    case FOURCC('f','p','u',' '): v = 0; break;
    case FOURCC('m','a','c','h'): v = 49; break;              /* Macintosh IIci-class id; arbitrary but fixed */
    case FOURCC('s','n','d',' '): v = 0x0FFF; break;
    case FOURCC('o','s',' ',' '): v = 0x0F; break;
    case FOURCC('t','m','g','r'): v = 3; break;
    case FOURCC('a','d','d','r'): v = 1; break;               /* 32-bit addressing capable flag */
    case FOURCC('f','o','l','d'): v = 1; break;               /* FindFolder present */
    case FOURCC('f','s',' ',' '): v = 0x02; break;            /* gestaltHasFSSpecCalls */
    case FOURCC('s','t','d','f'): v = 0x07; break;            /* Standard File 7.0 calls */
    case FOURCC('e','v','n','t'): v = 0x01; break;            /* Apple Events present */
    default: ok = 0; v = 0;
    }
    setreg(M68K_REG_A0, v);
    setreg(M68K_REG_D0, ok ? 0 : (uint32_t)-5550);
    if (!ok) hx_log("Gestalt('%c%c%c%c') -> gestaltUndefSelectorErr at %s", sel >> 24, (sel >> 16) & 255, (sel >> 8) & 255, sel & 255, loc_str(cur_trap_pc));
}
static void os_SysEnvirons(uint16_t w) {
    (void)w; uint32_t r = reg(M68K_REG_A0);
    wr16(r + 0, 2); wr16(r + 2, 11 /*envMacII*/); wr16(r + 4, 0x0710); wr16(r + 6, 4 /*env68030*/);
    wr8(r + 8, 0); wr8(r + 9, 1); wr16(r + 10, 2); wr16(r + 12, 0); wr16(r + 14, -1);
    setreg(M68K_REG_D0, 0);
}
uint32_t vbl_ticks(void);
void vbl_advance(uint32_t n);
void vbl_trap(uint8_t num);
static uint32_t ticks_now(void) { return vbl_ticks(); }
static void os_Delay(uint16_t w) { (void)w; vbl_advance(reg(M68K_REG_A0)); setreg(M68K_REG_D0, ticks_now()); }
static void os_VBL(uint16_t w) { vbl_trap(w & 0xFF); }
static void os_ReadDateTime(uint16_t w) { (void)w; wr32(reg(M68K_REG_A0), rd32(LM_Time)); setreg(M68K_REG_D0, 0); }

/* GetTrapAddress family: unimplemented traps share the Unimplemented address. */
static int implemented(uint16_t w);
static uint32_t trap_addr(uint16_t tw) {
    if (!implemented(tw)) tw = 0xA89F;
    return TRAPADDR_BASE + 2u * ((tw & 0x0800) ? 0x100u + (tw & 0x3FF) : (tw & 0xFF));
}
static void os_GetTrapAddress(uint16_t w) {
    uint16_t n = (uint16_t)reg(M68K_REG_D0);
    uint16_t tw;
    if ((w & 0x0200) == 0) tw = (n & 0x3FF) < 0x50 && !(n & 0x800) ? (0xA000 | (n & 0xFF)) : (0xA800 | (n & 0x3FF));  /* old style */
    else tw = (w & 0x0400) ? (0xA800 | (n & 0x3FF)) : (0xA000 | (n & 0xFF));
    setreg(M68K_REG_A0, trap_addr(tw));
    setreg(M68K_REG_D0, 0);
}
static void os_SetTrapAddress(uint16_t w) { (void)w; hx_log("SetTrapAddress(%04X -> %06X) at %s ignored", reg(M68K_REG_D0) & 0xFFFF, reg(M68K_REG_A0), loc_str(cur_trap_pc)); setreg(M68K_REG_D0, 0); }

/* ---------- Toolbox: Resource Manager ---------- */
static void tb_GetResource(uint16_t w) { int16_t id = ARGW(); uint32_t t = ARGL(); RETL(rs_get(t, id, (w & 0x3FF) == 0x01F)); }
static void tb_GetNamedResource(uint16_t w) { uint32_t pn = ARGL(); uint32_t t = ARGL(); RETL(rs_get_named(t, g_mem + pn, (w & 0x3FF) == 0x020)); }
static void tb_GetIndResource(uint16_t w) { int16_t i = ARGW(); uint32_t t = ARGL(); RETL(rs_get_ind(t, i, (w & 0x3FF) == 0x00E)); }
static void tb_CountResources(uint16_t w) { uint32_t t = ARGL(); RETW(rs_count(t, (w & 0x3FF) == 0x00D)); }
static void tb_ReleaseResource(uint16_t w) { (void)w; rs_release(ARGL()); }
static void tb_DetachResource(uint16_t w) { (void)w; rs_detach(ARGL()); }
static void tb_LoadResource(uint16_t w) { (void)w; ARGL(); wr16(LM_ResErr, 0); }
static void tb_HomeResFile(uint16_t w) { (void)w; RETW(rs_home(ARGL())); }
static void tb_CurResFile(uint16_t w) { (void)w; RETW(rs_cur()); }
static void tb_UseResFile(uint16_t w) { (void)w; rs_use(ARGW()); }
static void tb_ResError(uint16_t w) { (void)w; RETW((int16_t)rd16(LM_ResErr)); }
static void tb_OpenResFile(uint16_t w) { (void)w; uint32_t pn = ARGL(); RETW(rs_open_by_name(g_mem + pn)); }
static void tb_CloseResFile(uint16_t w) { (void)w; rs_close(ARGW()); }
static void tb_SizeResource(uint16_t w) { (void)w; uint32_t h = ARGL(); RETL(h && rd32(h) ? mm_handle_size(h) : -1); }
static void tb_GetResInfo(uint16_t w) {
    (void)w; uint32_t pn = ARGL(), pt = ARGL(), pid = ARGL(), h = ARGL();
    int16_t id; uint32_t t; uint8_t nm[256];
    if (rs_info(h, &id, &t, nm) == 0) { if (pid) wr16(pid, id); if (pt) wr32(pt, t); if (pn) { hx_track(pn, (uint32_t)(nm[0] + 1)); memcpy(g_mem + pn, nm, nm[0] + 1); } }
}
static void tb_SetResLoad(uint16_t w) { (void)w; ARGW(); }
static void tb_GetResAttrs(uint16_t w) { (void)w; ARGL(); RETW(0); }

/* ---------- Toolbox: utilities ---------- */
static void tb_TickCount(uint16_t w) {
    /* polling TickCount without the clock moving is waiting: let a tick pass every 32 polls */
    static uint32_t last, polls;
    uint32_t t = ticks_now();
    if (t == last) { if (++polls >= 32) { vbl_advance(1); polls = 0; t = ticks_now(); } } else polls = 0;
    last = t; RETL(t);
}
int16_t mtb_qd_random(uint32_t *seed);
static void tb_Random(uint16_t w) {
    (void)w;
    uint32_t seedp = rd32(rd32(LM_CurrentA5)) - 126;   /* qd.randSeed */
    uint32_t seed = rd32(seedp);
    int16_t r = mtb_qd_random(&seed);
    wr32(seedp, seed);
    RETW(r);
}
static void tb_InitGraf(uint16_t w) {
    (void)w; uint32_t gp = ARGL();
    wr32(rd32(LM_CurrentA5), gp);           /* (A5) -> &thePort */
    uint32_t qd = gp;
    wr32(qd - 126, 1);                       /* randSeed */
    uint32_t sb = qd - 122;                  /* screenBits */
    wr32(sb, SCREEN_BASE); wr16(sb + 4, SCREEN_W); wr16(sb + 6, 0); wr16(sb + 8, 0); wr16(sb + 10, SCREEN_H); wr16(sb + 12, SCREEN_W);
    for (int i = 0; i < 8; i++) { wr8(qd - 8 + i, 0x00); wr8(qd - 16 + i, 0xFF); wr8(qd - 24 + i, (i & 1) ? 0x55 : 0xAA); wr8(qd - 32 + i, (i & 1) ? 0x22 : 0x88); wr8(qd - 40 + i, (i & 1) ? 0xDD : 0x77); }
    wr32(qd, 0);                              /* thePort */
}
void input_poll(uint32_t tick);
static void tb_GetKeys(uint16_t w) { (void)w; uint32_t p = ARGL(); input_poll(ticks_now()); { hx_track(p, (uint32_t)(16)); memcpy(g_mem + p, g_mem + LM_KeyMap, 16); } }
int input_button(void); void input_mouse(int16_t *x, int16_t *y);
static void tb_Button(uint16_t w) { (void)w; input_poll(ticks_now()); RETB(input_button()); }
static void tb_GetMouse(uint16_t w) {   /* local coordinates of the current port */
    (void)w; uint32_t pt = ARGL(); input_poll(ticks_now());
    int16_t x, y; input_mouse(&x, &y);
    uint32_t port = rd32(rd32(rd32(LM_CurrentA5)));
    int16_t top = 0, left = 0;
    if (port && (rd16(port + 6) & 0xC000) == 0xC000) { uint32_t pm = rd32(rd32(port + 2)); top = (int16_t)rd16(pm + 6); left = (int16_t)rd16(pm + 8); }
    wr16(pt, (uint16_t)(y + top)); wr16(pt + 2, (uint16_t)(x + left));
}

/* ---------- Toolbox: events ---------- */
static uint32_t idle_calls;
extern int g_idle_limit;
void input_mouse(int16_t *x, int16_t *y); int input_button(void);
static void null_event(uint32_t ev) {
    for (int i = 0; i < 16; i++) wr8(ev + i, 0);
    int16_t mx, my; input_mouse(&mx, &my);
    wr32(ev + 2, 0); wr32(ev + 6, ticks_now()); wr16(ev + 10, (uint16_t)my); wr16(ev + 12, (uint16_t)mx);
    wr16(ev + 14, input_button() ? 0 : 0x0080);
}
uint32_t qd_take_update(void);
void input_poll(uint32_t tick);
int input_pending(void);
int input_get_event(uint32_t ev, uint16_t mask, int remove);
uint32_t hx_ticks(void) { return ticks_now(); }
static int update_event_(uint32_t ev, uint16_t mask);
static int get_event(uint32_t ev, uint16_t mask, int remove) {
    input_poll(ticks_now());
    if (remove && update_event_(ev, mask)) return 1;
    if (input_get_event(ev, mask, remove)) return 1;
    null_event(ev);
    if (!input_pending() && ++idle_calls >= (uint32_t)g_idle_limit) {
        g_stop = 1; snprintf(g_stop_reason, sizeof g_stop_reason, "idle in event loop (%u null events, input script done) at %s", idle_calls, loc_str(cur_trap_pc));
    }
    return 0;
}
static int update_event_(uint32_t ev, uint16_t mask) {
    if (!(mask & (1 << 6))) return 0;
    uint32_t win = qd_take_update();
    if (!win) return 0;
    null_event(ev); wr16(ev, 6); wr32(ev + 2, win);
    hx_log("updateEvt for window %06X", win);
    return 1;
}
static void tb_WaitNextEvent(uint16_t w) {
    (void)w; ARGL(); uint32_t sleep = ARGL(); uint32_t ev = ARGL(); uint16_t mask = (uint16_t)ARGW();
    int got = get_event(ev, mask, 1);
    if (!got && sleep) vbl_advance(1);    /* sleeping lets time pass */
    RETB(got);
}
static void tb_GetNextEvent(uint16_t w) { (void)w; uint32_t ev = ARGL(); uint16_t mask = (uint16_t)ARGW(); RETB(get_event(ev, mask, 1)); }
static void tb_EventAvail(uint16_t w) { (void)w; uint32_t ev = ARGL(); uint16_t mask = (uint16_t)ARGW(); RETB(get_event(ev, mask, 0)); }
static void os_GetOSEvent(uint16_t w) {   /* GetOSEvent A031 / OSEventAvail A030: D0 mask, A0 event -> D0 0 or -1 */
    int got = get_event(reg(M68K_REG_A0), (uint16_t)reg(M68K_REG_D0), (w & 0xFF) == 0x31);
    setreg(M68K_REG_D0, got ? 0 : (uint32_t)-1);
}

/* ---------- Dialog Manager: alerts are logged and answered with item 1 ---------- */
static uint8_t param_text[4][256];
static void pstr_copy(uint8_t *dst, uint32_t src) { if (!src) { dst[0] = 0; return; } memcpy(dst, g_mem + src, rd8(src) + 1); }
static void tb_ParamText(uint16_t w) {
    (void)w; uint32_t p3 = ARGL(), p2 = ARGL(), p1 = ARGL(), p0 = ARGL();
    pstr_copy(param_text[0], p0); pstr_copy(param_text[1], p1); pstr_copy(param_text[2], p2); pstr_copy(param_text[3], p3);
}
static void tb_Alert(uint16_t w) {
    (void)w; ARGL(); int16_t id = ARGW();
    static const char *kind[] = {"Alert", "StopAlert", "NoteAlert", "CautionAlert"};
    int k = (w & 0x3FF) - 0x185; if (k < 0 || k > 3) k = 0;
    char t[4][257];
    for (int i = 0; i < 4; i++) { memcpy(t[i], param_text[i] + 1, param_text[i][0]); t[i][param_text[i][0]] = 0; }
    hx_log("%s(%d) at %s: ^0=\"%s\" ^1=\"%s\" ^2=\"%s\" ^3=\"%s\"", kind[k], id, loc_str(cur_trap_pc), t[0], t[1], t[2], t[3]);
    RETW(1);
}

/* ---------- Folder Manager ---------- */
static void tb_AliasDispatch(uint16_t w) {
    uint16_t sel = (uint16_t)reg(M68K_REG_D0);
    if (sel != 0) { hx_fault("AliasDispatch selector %u at %s", sel, loc_str(cur_trap_pc)); return; }
    /* FindFolder(vRefNum, folderType, createFolder, VAR foundVRefNum, VAR foundDirID): OSErr */
    uint32_t pdir = ARGL(), pvol = ARGL(); ARGW(); uint32_t type = ARGL(); ARGW();
    int32_t dir = type == FOURCC('p','r','e','f') ? 20 : type == FOURCC('t','e','m','p') ? 21 : type == FOURCC('m','a','c','s') ? 19 : 22;
    wr16(pvol, (uint16_t)-1); wr32(pdir, (uint32_t)dir);
    RETW(0);
}

/* ---------- File Manager (vfs.c) ---------- */
void vfs_hlfs(uint16_t w, uint32_t trap_pc, const char *where);
void vfs_pb(uint8_t trapnum, const char *where);
static void tb_HLFS(uint16_t w) { vfs_hlfs(w, cur_trap_pc, loc_str(cur_trap_pc)); }
static void os_FilePB(uint16_t w) { vfs_pb(w & 0xFF, loc_str(cur_trap_pc)); }

/* ---------- Process Manager (OSDispatch A88F, selector word on the stack) ---------- */
static void tb_OSDispatch(uint16_t w) {
    (void)w; uint16_t sel = pop16();
    switch (sel) {
    case 0x37: { uint32_t psn = ARGL(); wr32(psn, 0); wr32(psn + 4, 2); RETW(0); break; }   /* GetCurrentProcess: kCurrentProcess-style PSN {0,2} */
    case 0x3A: { /* GetProcessInformation(psn, VAR info) */
        uint32_t info = ARGL(); ARGL();
        extern char g_appname[64]; const char *nm = g_appname;
        uint32_t name = rd32(info + 4), spec = rd32(info + 56);
        if (name) { wr8(name, (uint8_t)strlen(nm)); { hx_track(name + 1, (uint32_t)(strlen(nm))); memcpy(g_mem + name + 1, nm, strlen(nm)); } }
        wr32(info + 8, 0); wr32(info + 12, 2);
        wr32(info + 16, FOURCC('A','P','P','L')); wr32(info + 20, FOURCC('M','z','w','C'));
        wr32(info + 24, 0x5800); wr32(info + 28, 0); wr32(info + 32, HEAP_END - HEAP_BASE); wr32(info + 36, mm_free_bytes());
        if (spec) { wr16(spec, (uint16_t)-1); wr32(spec + 2, 30); wr8(spec + 6, (uint8_t)strlen(nm)); { hx_track(spec + 7, (uint32_t)(strlen(nm))); memcpy(g_mem + spec + 7, nm, strlen(nm)); } }
        RETW(0); break; }
    case 0x16: RETL(HEAP_END); break;                                     /* TempTopMem */
    default: hx_fault("OSDispatch selector %X at %s", sel, loc_str(cur_trap_pc));
    }
}

/* ---------- Packages ---------- */
#include "../mactb/mactb.h"
static void tb_Pack4(uint16_t w) {
    /* FP68K: (SP) opword, 4(SP) dst address, 8(SP) src address (binary operations) */
    (void)w; uint32_t sp = reg(A7);
    uint16_t op = rd16(sp); uint32_t dst = rd32(sp + 2), src = rd32(sp + 6);
    int pop = mtb_sane_pop_bytes(op);
    uint8_t ccr = 0xFF;
    if (mtb_fp68k(op, g_mem + dst, pop == 10 ? g_mem + src : NULL, &ccr)) { hx_fault("SANE opword %04X not implemented at %s", op, loc_str(cur_trap_pc)); return; }
    setreg(A7, sp + pop);
    if (ccr != 0xFF) setreg(M68K_REG_SR, (reg(M68K_REG_SR) & 0xFFE0) | ccr);
}
static void tb_Pack7(uint16_t w) {
    /* Binary-Decimal package: selector word on the stack, register arguments */
    (void)w; uint16_t sel = pop16();
    if (sel == 0) {          /* NumToString(D0 num, A0 Str255) */
        char b[16]; int n = snprintf(b, sizeof b, "%ld", (long)(int32_t)reg(M68K_REG_D0));
        uint32_t a = reg(M68K_REG_A0); wr8(a, (uint8_t)n); { hx_track(a + 1, (uint32_t)(n)); memcpy(g_mem + a + 1, b, n); }
    } else if (sel == 1) {   /* StringToNum(A0 Str255) -> D0 */
        uint32_t a = reg(M68K_REG_A0); int n = rd8(a), i = 1, neg = 0; int32_t v = 0;
        if (n && (rd8(a + 1) == '-' || rd8(a + 1) == '+')) { neg = rd8(a + 1) == '-'; i = 2; }
        for (; i <= n; i++) v = v * 10 + (rd8(a + i) - '0');   /* ROM ignores non-digits the same way */
        setreg(M68K_REG_D0, (uint32_t)(neg ? -v : v));
    } else hx_fault("Pack7 selector %u at %s", sel, loc_str(cur_trap_pc));
}

/* ---------- Segment Loader ---------- */
static void tb_LoadSeg(uint16_t w) {
    (void)w;
    int n = (int16_t)pop16();
    /* called from a JT entry: trap word at entry+6; resume at entry+2 (now JMP abs) */
    uint32_t e = cur_trap_pc - 6;
    if (seg_load(n)) hx_fault("LoadSeg(%d) failed", n);
    if (rd16(e + 2) != 0x4EF9) hx_fault("LoadSeg not called from a jump-table entry (%06X)", cur_trap_pc);
    setreg(M68K_REG_PC, e + 2);
}
static void tb_UnloadSeg(uint16_t w) { (void)w; ARGL(); /* segments stay resident */ }
static void tb_ExitToShell(uint16_t w) { (void)w; g_stop = 1; snprintf(g_stop_reason, sizeof g_stop_reason, "ExitToShell at %s", loc_str(cur_trap_pc)); }

/* ---------- registration ---------- */
void qd_register(void);
static void reg_os(uint8_t n, TrapFn f) { os_fn[n] = f; }
static void reg_tb(uint16_t n, TrapFn f) { tb_fn[n & 0x3FF] = f; }

static int implemented(uint16_t w) {
    if (w & 0x0800) return tb_fn[w & 0x3FF] || tb_sig[w & 0x3FF];
    return os_fn[w & 0xFF] != NULL;
}

void traps_init(void) {
    { void vfs_init(void); vfs_init(); }
    for (const TrapSig *s = g_trapsigs; s->name; s++) tb_sig[s->trap & 0x3FF] = s;
    /* Memory Manager */
    reg_os(0x22, os_NewHandle); reg_os(0x1E, os_NewPtr); reg_os(0x1F, os_DisposePtr); reg_os(0x23, os_DisposeHandle);
    reg_os(0x25, os_GetHandleSize); reg_os(0x24, os_SetHandleSize); reg_os(0x21, os_GetPtrSize); reg_os(0x20, os_SetPtrSize);
    reg_os(0x29, os_HLock); reg_os(0x2A, os_HUnlock); reg_os(0x49, os_HPurge); reg_os(0x4A, os_HNoPurge);
    reg_os(0x69, os_HGetState); reg_os(0x6A, os_HSetState); reg_os(0x67, os_HSetRBit); reg_os(0x68, os_HClrRBit);
    reg_os(0x2B, os_EmptyHandle); reg_os(0x2E, os_BlockMove); reg_os(0x1C, os_FreeMem); reg_os(0x1D, os_MaxMem);
    reg_os(0x4C, os_CompactMem); reg_os(0x62, os_PurgeSpace); reg_os(0x65, os_StackSpace); reg_os(0x1A, os_GetZone);
    reg_os(0x26, os_HandleZone); reg_os(0x28, os_RecoverHandle); reg_os(0x66, os_NewEmptyHandle);
    reg_os(0x63, os_noerr); /* MaxApplZone */ reg_os(0x36, os_noerr); /* MoreMasters */ reg_os(0x2D, os_noerr); /* SetApplLimit */
    reg_os(0x4D, os_noerr); /* PurgeMem */ reg_os(0x40, os_noerr); /* ReserveMem */ reg_os(0x64, os_noerr); /* MoveHHi */
    reg_os(0x1B, os_noerr); /* SetZone */ reg_os(0x4B, os_noerr); /* SetGrowZone */ reg_os(0x32, os_noerr); /* FlushEvents */ reg_os(0x31, os_GetOSEvent); reg_os(0x30, os_GetOSEvent);
    reg_os(0x55, os_StripAddress); reg_os(0x5D, os_SwapMMUMode);
    reg_os(0x33, os_VBL); reg_os(0x34, os_VBL); reg_os(0x6F, os_VBL); reg_os(0x70, os_VBL);
    reg_os(0x98, os_noerr); /* HWPriv */
    /* environment */
    reg_os(0xAD, os_Gestalt); reg_os(0x90, os_SysEnvirons); reg_os(0x3B, os_Delay); reg_os(0x39, os_ReadDateTime);
    reg_os(0x46, os_GetTrapAddress); reg_os(0x47, os_SetTrapAddress);
    /* Resource Manager */
    reg_tb(0x1A0, tb_GetResource); reg_tb(0x01F, tb_GetResource); reg_tb(0x1A1, tb_GetNamedResource); reg_tb(0x020, tb_GetNamedResource);
    reg_tb(0x19D, tb_GetIndResource); reg_tb(0x00E, tb_GetIndResource); reg_tb(0x19C, tb_CountResources); reg_tb(0x00D, tb_CountResources);
    reg_tb(0x1A3, tb_ReleaseResource); reg_tb(0x192, tb_DetachResource); reg_tb(0x1A2, tb_LoadResource); reg_tb(0x1A4, tb_HomeResFile);
    reg_tb(0x194, tb_CurResFile); reg_tb(0x198, tb_UseResFile); reg_tb(0x1AF, tb_ResError); reg_tb(0x197, tb_OpenResFile);
    reg_tb(0x19A, tb_CloseResFile); reg_tb(0x1A5, tb_SizeResource); reg_tb(0x1A8, tb_GetResInfo); reg_tb(0x19B, tb_SetResLoad);
    reg_tb(0x1A6, tb_GetResAttrs);
    /* utilities and events */
    reg_tb(0x175, tb_TickCount); reg_tb(0x061, tb_Random); reg_tb(0x06E, tb_InitGraf); reg_tb(0x176, tb_GetKeys); reg_tb(0x174, tb_Button); reg_tb(0x173, tb_Button); /* StillDown */ reg_tb(0x172, tb_GetMouse);
    reg_tb(0x060, tb_WaitNextEvent); reg_tb(0x170, tb_GetNextEvent); reg_tb(0x171, tb_EventAvail);
    reg_tb(0x1EE, tb_Pack7); reg_tb(0x1EB, tb_Pack4); reg_tb(0x08F, tb_OSDispatch);
    reg_tb(0x18B, tb_ParamText); reg_tb(0x185, tb_Alert); reg_tb(0x186, tb_Alert); reg_tb(0x187, tb_Alert); reg_tb(0x188, tb_Alert);
    reg_tb(0x023, tb_AliasDispatch); reg_tb(0x252, tb_HLFS);
    reg_os(0x01, os_FilePB); reg_os(0x02, os_FilePB); reg_os(0x03, os_FilePB); reg_os(0x11, os_FilePB);
    reg_os(0x12, os_FilePB); reg_os(0x18, os_FilePB); reg_os(0x44, os_FilePB); reg_os(0x13, os_noerr); /* FlushVol */
    /* Segment Loader */
    reg_tb(0x1F0, tb_LoadSeg); reg_tb(0x1F1, tb_UnloadSeg); reg_tb(0x1F4, tb_ExitToShell);
    qd_register();
    void snd_register(void); snd_register();
    void menu_register(void); menu_register();
}

void traps_register_tb(uint16_t n, TrapFn f) { reg_tb(n, f); }
const char *hx_where(void) { return loc_str(cur_trap_pc); }
static int cur_via_address;
/* A trap that has to wait (ModalDialog) leaves its arguments on the stack and calls this:
 * the A-line instruction runs again, after any due VBL interrupt. */
int g_trap_wait;                   /* a trap called from C glue asked to run again after a tick */
int hx_trap_retry(void) {
    if (cur_via_address) { g_trap_wait = 1; return 1; }
    setreg(M68K_REG_PC, cur_trap_pc);
    return 1;
}
const uint8_t *hx_param_text(int i) { return param_text[i & 3]; }

static TrapStat *stat_for(uint16_t w) {
    int i = (w & 0x0800) ? (w & 0x3FF) : 0x400 + (w & 0xFF);
    TrapStat *s = &stats[i];
    if (!s->name[0]) {
        const char *n = NULL;
        if (w & 0x0800) { if (tb_sig[w & 0x3FF]) n = tb_sig[w & 0x3FF]->name; }
        snprintf(s->name, sizeof s->name, "%s", n ? n : "");
        if (!n) snprintf(s->name, sizeof s->name, "%s_%03X", (w & 0x0800) ? "Toolbox" : "OS", (w & 0x0800) ? (w & 0x3FF) : (w & 0xFF));
        s->first_pc = cur_trap_pc;
    }
    return s;
}

static void generic_toolbox(uint16_t w) {
    const TrapSig *sg = tb_sig[w & 0x3FF];
    TrapStat *st = stat_for(w);
    int argbytes = sg ? sg->argbytes : -1, retbytes = sg ? sg->retbytes : 0;
    const char *nm = st->name;
    /* dispatchers: selector decides the signature */
    for (const SelSig *s = g_selsigs; s->name; s++) {
        if (s->trap != (w & 0xFBFF)) continue;
        uint32_t sel = 0; int found = 0;
        if (!strcmp(s->loc, "D0L")) { sel = reg(M68K_REG_D0); found = 1; }
        else if (!strcmp(s->loc, "D0W")) { sel = reg(M68K_REG_D0) & 0xFFFF; found = 1; }
        else if (!strncmp(s->loc, "D0<", 3)) { sel = reg(M68K_REG_D0) & 0xFFFFFF; found = 1; }
        else if (!strcmp(s->loc, "StackW")) { sel = rd16(reg(A7)); found = 2; }
        else if (!strcmp(s->loc, "StackL")) { sel = rd32(reg(A7)); found = 3; }
        const char *sn = found ? sel_name(w, sel) : NULL;
        if (!sn) { hx_fault("unimplemented dispatcher %s selector %lX at %s", nm, (unsigned long)sel, loc_str(cur_trap_pc)); return; }
        for (const SelSig *t = g_selsigs; t->name; t++) if (t->trap == (w & 0xFBFF) && t->sel == sel) { argbytes = t->argbytes; retbytes = t->retbytes; }
        if (argbytes < 0) { hx_fault("dispatcher %s/%s is register-based and not implemented at %s", nm, sn, loc_str(cur_trap_pc)); return; }
        if (found == 2) argbytes += 2; else if (found == 3) argbytes += 4;
        char key[96]; snprintf(key, sizeof key, "%s/%s", nm, sn);
        if (st->calls == 0 || g_verbose) hx_log("STUB %s at %s", key, loc_str(cur_trap_pc));
        break;
    }
    if (argbytes < 0) { hx_fault("trap %s (%04X) has no known signature at %s", nm, w, loc_str(cur_trap_pc)); return; }
    if (st->calls == 0) hx_log("STUB %s (%d arg bytes, %d result) first at %s", nm, argbytes, retbytes, loc_str(cur_trap_pc));
    st->stub = 1;
    setreg(A7, reg(A7) + argbytes);
    if (retbytes == 2) RETW(0); else if (retbytes == 4) RETL(0);
    if (w & 0x0400) { /* autopop: discard the return address of the glue */ setreg(M68K_REG_PC, pop32()); }
}

void trap_dispatch(uint16_t w, uint32_t trap_pc, int via_address) {
    /* a trap can run game code (a dialog filter) that calls traps: keep the outer trap's place */
    uint32_t outer_pc = cur_trap_pc; int outer_via = cur_via_address;
    cur_trap_pc = trap_pc;
    cur_via_address = via_address;
    TrapStat *st = stat_for(w);
    if (w & 0x0800) {
        TrapFn f = tb_fn[w & 0x3FF];
        if (f) { f(w); if (w & 0x0400) setreg(M68K_REG_PC, pop32()); }
        else generic_toolbox(w);
    } else {
        TrapFn f = os_fn[w & 0xFF];
        if (f) f(w);
        else {
            if (st->calls == 0) hx_log("STUB OS %s (D0=%08X A0=%08X) first at %s -> noErr", st->name, reg(M68K_REG_D0), reg(M68K_REG_A0), loc_str(trap_pc));
            st->stub = 1; setreg(M68K_REG_D0, 0);
        }
        /* the OS trap dispatcher returns with TST.W D0: N and Z from the result code, V and C clear */
        uint16_t r = (uint16_t)reg(M68K_REG_D0); uint32_t sr = reg(M68K_REG_SR) & ~0x0Fu;
        setreg(M68K_REG_SR, sr | (r ? 0 : 4) | (r & 0x8000 ? 8 : 0));
    }
    st->calls++;
    cur_trap_pc = outer_pc; cur_via_address = outer_via;
}

void traps_report(FILE *f) {
    fprintf(f, "%-28s %8s  %-10s %s\n", "trap", "calls", "first at", "status");
    for (int i = 0; i < 0x500; i++) if (stats[i].calls)
        fprintf(f, "%-28s %8u  %-10s %s\n", stats[i].name, stats[i].calls, loc_str(stats[i].first_pc), stats[i].stub ? "STUB" : "impl");
}

uint32_t trap_word_for_addr(uint32_t pc) {
    uint32_t i = (pc - TRAPADDR_BASE) / 2;
    return i >= 0x100 ? (0xA800 | (i - 0x100)) : (0xA000 | i);
}
