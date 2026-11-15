/* Mazeworld Catacombs 68k harness: runs the original CODE resources under Musashi
 * with a high-level emulation of the parts of Mac OS the game uses.
 * Purpose: execution tracing for the disassembly and, later, tick-by-tick
 * differential testing against the C reimplementation. */
#ifndef HX_H
#define HX_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ---------- address map (24-bit clean, 16 MB) ---------- */
#define MEM_SIZE        0x01000000u
#define HEAP_BASE       0x00100000u   /* application heap zone */
#define HEAP_END        0x00C00000u
#define STACK_SIZE      0x00040000u
#define A5_ADDR         0x00D00000u   /* A5 world: globals below, JT above */
#define SCREEN_BASE     0x00E00000u   /* 8-bit main screen */
#define SCREEN_W        640
#define SCREEN_H        480
#define TRAPGATE        0x00FF0000u   /* A-line exception vector target */
#define TRAPADDR_BASE   0x00FF1000u   /* fake addresses handed out by GetTrapAddress */
#define TRAPADDR_END    (TRAPADDR_BASE + 0x2000u * 2u)
#define STOPADDR        0x00FFFF00u   /* return target that ends a nested 68k call */
#define IDLERET         0x00FFFF10u   /* return target of a VBL interrupt run while C code idles (mz_idle) */
#define CALLRET         0x00FFFF20u   /* return target of a call into the game from a trap (hx_call_pascal) */

/* ---------- low-memory globals used ---------- */
#define LM_MemTop       0x0108
#define LM_HeapEnd      0x0114
#define LM_TheZone      0x0118
#define LM_ApplLimit    0x0130
#define LM_Ticks        0x016A
#define LM_KeyMap       0x0174
#define LM_Time         0x020C
#define LM_MemErr       0x0220
#define LM_ROM85        0x028E
#define LM_SysZone      0x02A6
#define LM_ApplZone     0x02AA
#define LM_ScrnBase     0x0824
#define LM_MainDevice   0x08A4
#define LM_DeviceList   0x08A8
#define LM_CurApRefNum  0x0900
#define LM_CurrentA5    0x0904
#define LM_CurStackBase 0x0908
#define LM_CurApName    0x0910
#define LM_TopMapHndl   0x0A50
#define LM_CurMap       0x0A5A
#define LM_ResErr       0x0A60
#define LM_TheGDevice   0x0CC8

/* ---------- memory ---------- */
extern uint8_t *g_mem;
uint8_t  rd8(uint32_t a);
uint16_t rd16(uint32_t a);
uint32_t rd32(uint32_t a);
void wr8(uint32_t a, uint8_t v);
void wr16(uint32_t a, uint16_t v);
void wr32(uint32_t a, uint32_t v);
void hx_fault(const char *fmt, ...);   /* stop emulation with a message */
/* Host code that writes emulated memory directly (memcpy, mactb drawing) reports the range first,
 * so -subcheck can log and undo it like any other write. */
void hx_track(uint32_t a, uint32_t n);

/* ---------- CPU helpers ---------- */
uint32_t reg(int r);        /* Musashi register ids */
void setreg(int r, uint32_t v);
uint16_t pop16(void);
uint32_t pop32(void);
void push16(uint16_t v);
void push32(uint32_t v);

/* ---------- memory manager (heap.c) ---------- */
void     heap_init(void);
uint32_t mm_new_handle(uint32_t size, int clear);
uint32_t mm_new_ptr(uint32_t size, int clear);
void     mm_dispose_handle(uint32_t h);
void     mm_dispose_ptr(uint32_t p);
int32_t  mm_handle_size(uint32_t h);
int      mm_set_handle_size(uint32_t h, uint32_t size);
int32_t  mm_ptr_size(uint32_t p);
uint8_t  mm_get_state(uint32_t h);
void     mm_set_state(uint32_t h, uint8_t st);
uint32_t mm_free_bytes(void);

/* ---------- resources (rsrc.c) ---------- */
uint32_t rs_get(uint32_t type, int16_t id, int one_deep);
uint32_t rs_get_named(uint32_t type, const uint8_t *pname, int one_deep);
uint32_t rs_get_ind(uint32_t type, int index, int one_deep);
int      rs_count(uint32_t type, int one_deep);
int      rs_info(uint32_t h, int16_t *id, uint32_t *type, uint8_t *pname);
int      rs_home(uint32_t h);
int      rs_detach(uint32_t h);
int      rs_release(uint32_t h);
int      rs_cur(void);
void     rs_use(int ref);
int      rs_open_by_name(const uint8_t *pname);
void     rs_close(int ref);
const uint8_t *rs_raw(int ref, uint32_t type, int16_t id, uint32_t *len); /* host-side direct read */

/* ---------- segments (seg.c) ---------- */
void     seg_init(void);
int      seg_load(int n);                 /* returns 0 on success */
int      seg_of(uint32_t pc, uint32_t *off);
void     seg_trace_mark(uint32_t pc);
void     seg_trace_write(const char *dir);
extern uint32_t g_jt_count;

/* ---------- traps (traps.c) ---------- */
void traps_init(void);
void trap_dispatch(uint16_t word, uint32_t trap_pc, int via_address);
void traps_report(FILE *f);

/* ---------- run control ---------- */
extern int      g_stop;
extern char     g_stop_reason[256];
extern uint64_t g_insn_count;
extern uint32_t g_ticks;
extern int      g_verbose;
extern const char *g_appfile, *g_outdir;
void hx_log(const char *fmt, ...);
/* Calls a routine of the game from inside a trap (a dialog filter, a user item's drawing procedure):
 * Pascal convention, arguments left to right (sizes 2 or 4 bytes), a result of retbytes (0, 2 or 4)
 * returned as read from its slot (a Boolean is in the high byte of a 2-byte result). The rig runs
 * the 68k code (or its C replacement); the port calls the C function for that address. */
uint32_t hx_call_pascal(uint32_t proc, int retbytes, int nargs, const uint32_t *vals, const int *sizes);

#define FOURCC(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))
#endif
