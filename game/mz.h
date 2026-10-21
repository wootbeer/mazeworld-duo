/* Common header for the C reimplementation of Mazeworld Catacombs.
 *
 * Memory model: the game's data lives in a big-endian Mac address space (`M`), with the same
 * layout as the original: A5 world (globals below A5), application heap, Toolbox records.
 * The harness backs M with the emulated machine's RAM; the native port backs it with its own
 * buffer and the same native Toolbox layer. Either way this code runs natively.
 *
 * Types follow the original compiler: `int` was 16 bits, so 16-bit values are int16_t/uint16_t.
 * Names: functions are fn_S<seg>_<offset>, globals G_<offset below A5>, until a name is certain.
 */
#ifndef MZ_H
#define MZ_H
#include <stdint.h>

extern uint8_t *M;                 /* base of the Mac address space */
#define MZ_A5 0x00D00000u          /* same A5 as the harness */

static inline uint8_t  r8(uint32_t a)  { return M[a & 0xFFFFFF]; }
static inline uint16_t r16(uint32_t a) { a &= 0xFFFFFF; return (uint16_t)(M[a] << 8 | M[a + 1]); }
static inline uint32_t r32(uint32_t a) { a &= 0xFFFFFF; return (uint32_t)M[a] << 24 | M[a + 1] << 16 | M[a + 2] << 8 | M[a + 3]; }
/* In the harness, writes can be logged so a C function's effects can be checked against the
 * original's and then undone (-subcheck). The port compiles this out. */
#ifdef MZ_HARNESS
extern void (*mz_wtrack)(uint32_t a, int n);
#define MZ_WHOOK(a, n) do { if (mz_wtrack) mz_wtrack(a, n); } while (0)
#else
#define MZ_WHOOK(a, n) ((void)0)
#endif
static inline void w8(uint32_t a, uint8_t v)   { MZ_WHOOK(a, 1); M[a & 0xFFFFFF] = v; }
static inline void w16(uint32_t a, uint16_t v) { MZ_WHOOK(a, 2); a &= 0xFFFFFF; M[a] = (uint8_t)(v >> 8); M[a + 1] = (uint8_t)v; }
static inline void w32(uint32_t a, uint32_t v) { MZ_WHOOK(a, 4); a &= 0xFFFFFF; M[a] = (uint8_t)(v >> 24); M[a + 1] = (uint8_t)(v >> 16); M[a + 2] = (uint8_t)(v >> 8); M[a + 3] = (uint8_t)v; }

/* globals: G_xxxx is at A5 - 0xxxxx (Catacombs offsets; for Mazeworld Original, mz_gmap moves
 * them to that build's: game/games.c) */
extern const uint16_t *mz_gmap;
extern int mz_original;
/* Original with Catacombs-style controls (a port option; the rig never sets it): the keys move and
 * turn (mz_alt_move: turn, forward, sideways units, 2 a step), the mouse only turns, a click fires
 * the chosen ability and 1-4 choose it, instead of the mouse moving and its gestures choosing */
extern int mz_alt_controls;
extern int16_t mz_alt_move[3];
void     mz_select_original(void);
uint32_t mz_jt(uint16_t off);          /* jump-table entry `off` above A5 */
uint32_t mz_orig_func(int seg, uint16_t off);
uint32_t mz_gnone(uint32_t off);        /* a Catacombs global Original does not have: a fault */
static inline uint32_t GADDR(uint32_t off) {
    if (!mz_gmap) return MZ_A5 - off;
    uint16_t o = mz_gmap[(uint16_t)off];
    return o ? MZ_A5 - o : mz_gnone(off);
}
/* Mazeworld Original's own globals and functions (no Catacombs counterpart) are named by Original's
 * offsets: globals through OADDR / og* (never translated), functions fo_S<seg>_<off>. */
#define OADDR(off) (MZ_A5 - (uint32_t)(off))
static inline uint8_t  g8(uint32_t off)  { return r8(GADDR(off)); }
static inline uint16_t g16(uint32_t off) { return r16(GADDR(off)); }
static inline uint32_t g32(uint32_t off) { return r32(GADDR(off)); }
static inline void sg8(uint32_t off, uint8_t v)   { w8(GADDR(off), v); }
static inline void sg16(uint32_t off, uint16_t v) { w16(GADDR(off), v); }
static inline void sg32(uint32_t off, uint32_t v) { w32(GADDR(off), v); }
static inline uint8_t  og8(uint32_t off)  { return r8(OADDR(off)); }
static inline uint16_t og16(uint32_t off) { return r16(OADDR(off)); }
static inline uint32_t og32(uint32_t off) { return r32(OADDR(off)); }
static inline void sog8(uint32_t off, uint8_t v)   { w8(OADDR(off), v); }
static inline void sog16(uint32_t off, uint16_t v) { w16(OADDR(off), v); }
static inline void sog32(uint32_t off, uint32_t v) { w32(OADDR(off), v); }

/* Handles: a Handle is the address of a master pointer. */
static inline uint32_t deref(uint32_t h) { return r32(h); }

/* A call through the jump table into another segment: the first one loads that segment (in the
 * harness this allocates its code, exactly as the original's LoadSeg does; the port has nothing
 * to load). Placed before each cross-segment call, mirroring the original's jsr x(A5). */
void mz_seg(int n);
void mz_seg_of(int seg, uint16_t off);
uint32_t mz_seg_base(int n);            /* where segment n's code is (loaded first): data kept in code */   /* the same, named by the function called (games.c) */
/* Where the original spins until an interrupt (a VBL task) changes something: one tick passes and
 * its VBL processing runs. Call once per pass of the original's loop. */
void mz_idle(void);

/* ---------- Toolbox (implemented by the native Mac layer; same code in harness and port) ---------- */
int16_t mac_Random(void);
/* SANE FP68K (_Pack4): opword, destination and (for binary operations) source addresses; returns
 * the condition codes (meaningful after a comparison) */
uint8_t  mac_FP68K(uint16_t op, uint32_t dst, uint32_t src);
/* FP68K opwords: an operation plus the source format (extended unless given) */
enum { FP_SINGLE = 0x1000, FP_INT = 0x2000, FP_LONG = 0x2800, FP_COMP = 0x3000,
       FP_ADD = 0, FP_SUB = 2, FP_MUL = 4, FP_DIV = 6, FP_CMP = 8, FP_LOAD = 0x0E, FP_STORE = 0x10, FP_TRUNC = 0x16 };
#define FP_LESS(ccr) ((((ccr) >> 3) ^ ((ccr) >> 1)) & 1)   /* blt after FP_CMP: less, or unordered */
void     mac_NumToString(int32_t n, uint32_t str);
uint32_t mac_StripAddress(uint32_t a);
/* n bytes of scratch in the Mac address space, valid until the outermost C call returns (where
 * the original used a buffer in its stack frame) */
uint32_t mz_local(uint32_t n);
/* Every function that takes scratch with mz_local starts with MZ_FRAME: its scratch (and that of
 * everything it calls) is given back when it returns, like the original's stack frame. */
uint32_t mz_frame_begin(void);
void     mz_frame_end(uint32_t *mark);
#define MZ_FRAME uint32_t mz_frame_ __attribute__((cleanup(mz_frame_end))) = mz_frame_begin()
int16_t  mac_StopAlert(int16_t id, uint32_t filter);
int16_t mac_HLock(uint32_t h);
int16_t mac_HUnlock(uint32_t h);
int16_t mac_DisposeHandle(uint32_t h);
int32_t mac_GetHandleSize(uint32_t h);
uint32_t mac_NewHandle(int32_t size);
void     mac_InitGraf(uint32_t globals);
void     mac_InitFonts(void);
void     mac_InitWindows(void);
void     mac_InitMenus(void);
void     mac_TEInit(void);
void     mac_InitDialogs(uint32_t resume);
void     mac_InitCursor(void);
void     mac_ShowCursor(void);
void     mac_HideCursor(void);
void     mac_CloseWindow(uint32_t w);
void     mac_DisposeWindow(uint32_t w);
uint32_t mac_GetWRefCon(uint32_t w);
void     mac_CopyRgn(uint32_t src, uint32_t dst);
void     mac_PaintBehind(uint32_t w, uint32_t rgn);
void     mac_CalcVisBehind(uint32_t w, uint32_t rgn);
uint32_t mac_GetNewPalette(int16_t id);
void     mac_SetDialogItem(uint32_t d, int16_t item, int16_t type, uint32_t h, uint32_t box);
void     mac_ShowControl(uint32_t c);
void     mac_HideControl(uint32_t c);
void     mac_GetPenState(uint32_t ps);
void     mac_SetPenState(uint32_t ps);
void     mac_PenNormal(void);
uint32_t mac_GetPicture(int16_t id);
void     mac_DrawPicture(uint32_t pic, uint32_t r);
void     mac_OffsetRect(uint32_t r, int16_t dh, int16_t dv);
void     mac_InsetRect(uint32_t r, int16_t dh, int16_t dv);
uint32_t mac_Delay(uint32_t ticks);
uint32_t mac_FrontWindow(void);
void     mac_BeginUpdate(uint32_t w);
void     mac_EndUpdate(uint32_t w);
void     mac_SetPort(uint32_t port);
void     mac_GetGWorld(uint32_t pport, uint32_t pgdh);
void     mac_SetGWorld(uint32_t port, uint32_t gdh);
uint32_t mac_GetGDevice(void);
int32_t  mac_Entry2Index(int16_t entry);
int16_t  mac_CurResFile(void);
void     mac_UseResFile(int16_t file);
uint32_t mac_GetResource(uint32_t type, int16_t id);
int16_t  mac_ResError(void);
void     mac_DetachResource(uint32_t h);
int16_t  mac_SndNewChannel(uint32_t pchan, int16_t synth, int32_t init, uint32_t userRoutine);
int16_t  mac_SndDisposeChannel(uint32_t chan, int quietNow);
int16_t  mac_SndDoCommand(uint32_t chan, uint32_t cmd, int noWait);
int16_t  mac_SndDoImmediate(uint32_t chan, uint32_t cmd);
void     mac_TETextBox(uint32_t text, int32_t len, uint32_t box, int16_t just);
uint32_t mac_GetCTable(int16_t id);
void     mac_DisposeCTable(uint32_t ct);
uint32_t mac_NewPalette(int16_t entries, uint32_t ctab, int16_t usage, int16_t tolerance);
void     mac_DisposeGWorld(uint32_t gw);
int16_t  mac_Gestalt(uint32_t selector, uint32_t presponse);
uint32_t mac_NGetTrapAddress(uint16_t trap, int toolbox);
void     mac_BlockMove(uint32_t src, uint32_t dst, int32_t n);
int16_t  mac_FindFolder(int16_t vRefNum, uint32_t type, int create, uint32_t pfoundVRefNum, uint32_t pfoundDirID);
int16_t  mac_NewGWorld(uint32_t pgw, int16_t depth, uint32_t rect, uint32_t ctab, uint32_t gdh, uint32_t flags);
uint32_t mac_UpdateGWorld(uint32_t pgw, int16_t depth, uint32_t rect, uint32_t ctab, uint32_t gdh, uint32_t flags);
void     mac_NSetPalette(uint32_t win, uint32_t pal, int16_t updates);
void     mac_ClipRect(uint32_t r);
int      mac_WaitNextEvent(int16_t mask, uint32_t eventRecord, uint32_t sleep, uint32_t mouseRgn);
int      mac_TestDeviceAttribute(uint32_t gdh, int16_t attribute);
void     mac_DisposePalette(uint32_t pal);
void     mac_ValidRect(uint32_t r);
int16_t  mac_VInstall(uint32_t vblTask);
int16_t  mac_VRemove(uint32_t vblTask);
void     mac_GetKeys(uint32_t keymap);
uint32_t mac_GetNewCWindow(int16_t id, uint32_t storage, uint32_t behind);
void mac_SizeWindow(uint32_t w, int16_t width, int16_t height, int update);
void mac_MoveWindow(uint32_t w, int16_t h, int16_t v, int front);
void mac_ShowWindow(uint32_t w);
void mac_HideWindow(uint32_t w);
void mac_SelectWindow(uint32_t w);
uint32_t mac_NewRgn(void);
void mac_SetRect(uint32_t r, int16_t left, int16_t top, int16_t right, int16_t bottom);
void mac_RectRgn(uint32_t rgn, uint32_t r);
void mac_UnionRgn(uint32_t a, uint32_t b, uint32_t dst);
void mac_DisposeRgn(uint32_t rgn);
uint32_t mac_GetDeviceList(void);
uint32_t mac_GetNextDevice(uint32_t gd);
void mac_SetWRefCon(uint32_t w, uint32_t ref);
uint32_t mac_GetMainDevice(void);
int16_t mac_SlotVInstall(uint32_t task, int16_t slot);
int16_t mac_SlotVRemove(uint32_t task, int16_t slot);
uint32_t mac_GetCursor(int16_t id);
void mac_SetCursor(uint32_t crsr);
void mac_GetPort(uint32_t pport);
uint32_t mac_GetNewDialog(int16_t id, uint32_t storage, uint32_t behind);
void mac_GetDialogItem(uint32_t d, int16_t item, uint32_t ptype, uint32_t phandle, uint32_t pbox);
void mac_GetDialogItemText(uint32_t h, uint32_t str);
void mac_HiliteControl(uint32_t c, int16_t state);
void mac_ModalDialog(uint32_t filter, uint32_t pitem);
void mac_DisposeDialog(uint32_t d);
void mac_InvalRect(uint32_t r);
int      mac_SectRect(uint32_t a, uint32_t b, uint32_t dst);
int16_t mac_GetCurrentProcess(uint32_t psn);
int16_t mac_GetProcessInformation(uint32_t psn, uint32_t info);
int16_t mac_FSpOpenResFile(uint32_t spec, int8_t perm);
void mac_CloseResFile(int16_t ref);
int16_t mac_AEInstallEventHandler(uint32_t cls, uint32_t id, uint32_t handler, uint32_t refcon, int sys);
int16_t  mac_PBCall(uint16_t trap, uint32_t paramBlock);
int16_t  mac_FSMakeFSSpec(int16_t vRefNum, int32_t dirID, uint32_t name, uint32_t spec);
int16_t  mac_FSpOpenDF(uint32_t spec, int8_t permission, uint32_t prefNum);
void     mac_ReleaseResource(uint32_t h);
void mac_GetFNum(uint32_t name, uint32_t pnum);
uint32_t mac_NewPtr(int32_t size);
int16_t mac_Alert(int16_t id, uint32_t filter);
void mac_GlobalToLocal(uint32_t pt);
int16_t mac_FSpCreate(uint32_t spec, uint32_t creator, uint32_t type, int16_t script);
int16_t mac_FSpDelete(uint32_t spec);
int16_t mac_FSpExchangeFiles(uint32_t a, uint32_t b);
int16_t mac_AEProcessAppleEvent(uint32_t ev);
/* The native shell's side (menus and the mouse are native in the port; see docs/SHELL.md) */
void     shell_menu_key(uint32_t ev);     /* a command-key press in the shell */
void     shell_mouse_down(uint32_t ev);   /* a mouse-down event in the shell */
void     shell_high_level_event(uint32_t ev); /* a high-level (Apple) event in the shell */
/* a path the port deliberately leaves out (stops with a message) */
void     mz_unsupported(const char *what);
uint32_t mac_GetPixBaseAddr(uint32_t pm);
uint32_t mac_GetGWorldDevice(uint32_t gw);
uint32_t mac_GetGWorldPixMap(uint32_t gw);
int      mac_LockPixels(uint32_t pm);
void     mac_UnlockPixels(uint32_t pm);
void     mac_ForeColor(int32_t c);
void     mac_BackColor(int32_t c);
void     mac_CopyBits(uint32_t src, uint32_t dst, uint32_t srcRect, uint32_t dstRect, int16_t mode, uint32_t maskRgn);
uint8_t  mac_SwapMMUMode(uint8_t mode);
void     mac_PmForeColor(int16_t entry);
void     mac_PaintRect(uint32_t rect);
uint32_t mac_GetNewMBar(int16_t id);
void     mac_SetMenuBar(uint32_t list);
uint32_t mac_GetMenuHandle(int16_t id);
void     mac_EnableItem(uint32_t menu, int16_t item);
void     mac_DisableItem(uint32_t menu, int16_t item);
void     mac_CheckItem(uint32_t menu, int16_t item, int checked);
uint32_t mac_MenuKey(int16_t ch);
void     mac_HiliteMenu(int16_t menu);
int16_t  mac_FindWindow(uint32_t pt, uint32_t pwin);
uint32_t mac_MenuSelect(uint32_t pt);
void     mac_AppendResMenu(uint32_t menu, uint32_t type);
void     mac_DrawMenuBar(void);
void     mac_ParamText(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);
int16_t  mac_NoteAlert(int16_t id, uint32_t filter);
void     mac_ExitToShell(void);
void     mac_PmBackColor(int16_t entry);
void     mac_FrameRect(uint32_t rect);
void     mac_EraseRect(uint32_t rect);
void     mac_MoveTo(int16_t h, int16_t v);
void     mac_DrawChar(int16_t ch);
void     mac_Move(int16_t dh, int16_t dv);
void     mac_LineTo(int16_t h, int16_t v);
void     mac_Line(int16_t dh, int16_t dv);
void     mac_DrawString(uint32_t str);
void     mac_TextFont(int16_t font);
void     mac_TextFace(int16_t face);
void     mac_TextMode(int16_t mode);
void     mac_TextSize(int16_t size);
int16_t  mac_StringWidth(uint32_t str);
#endif
