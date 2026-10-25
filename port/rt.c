/* The port's runtime: memory, the (register-only) CPU state the Toolbox glue uses, logging, and the
 * start of the program. The Toolbox itself is the test rig's (harness/traps.c and friends, over
 * mactb), so the game's C code runs over exactly the same Mac layer as under test; only the 68k CPU
 * is gone: every routine of the game is C. */
#include "../harness/hx.h"
#include "m68k.h"
#include "../game/mzasm.h"
#include "../archive/arc.h"
#include "port.h"
#include "../game/mz.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

uint8_t *g_mem;
int g_stop;
char g_stop_reason[256];
uint64_t g_insn_count;
int g_verbose;
uint32_t g_max_ticks;
const char *g_appfile, *g_outdir = ".";
char g_appname[64];
static FILE *logf_;

void hx_log(const char *fmt, ...) {
    if (!logf_) return;
    va_list ap; va_start(ap, fmt);
    fprintf(logf_, "[%6u] ", vbl_ticks()); vfprintf(logf_, fmt, ap); fputc('\n', logf_);
    fflush(logf_);
    va_end(ap);
}
void hx_fault(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_stop_reason, sizeof g_stop_reason, fmt, ap); va_end(ap);
    hx_log("FAULT: %s", g_stop_reason);
    g_stop = 2;
    port_fatal(g_stop_reason);
}

/* ---------- memory ---------- */
void (*mz_wtrack)(uint32_t a, int n);
void hx_track(uint32_t a, uint32_t n) { (void)a; (void)n; }
static inline int ok(uint32_t a, int n) { return a <= MEM_SIZE - (uint32_t)n; }
uint8_t rd8(uint32_t a) { return g_mem[a & 0xFFFFFF]; }
uint16_t rd16(uint32_t a) { a &= 0xFFFFFF; if (!ok(a, 2)) return 0; return (uint16_t)(g_mem[a] << 8 | g_mem[a + 1]); }
uint32_t rd32(uint32_t a) { a &= 0xFFFFFF; if (!ok(a, 4)) return 0; return (uint32_t)g_mem[a] << 24 | g_mem[a + 1] << 16 | g_mem[a + 2] << 8 | g_mem[a + 3]; }
void wr8(uint32_t a, uint8_t v) { g_mem[a & 0xFFFFFF] = v; }
void wr16(uint32_t a, uint16_t v) { a &= 0xFFFFFF; if (!ok(a, 2)) return; g_mem[a] = (uint8_t)(v >> 8); g_mem[a + 1] = (uint8_t)v; }
void wr32(uint32_t a, uint32_t v) { a &= 0xFFFFFF; if (!ok(a, 4)) return; g_mem[a] = (uint8_t)(v >> 24); g_mem[a + 1] = (uint8_t)(v >> 16); g_mem[a + 2] = (uint8_t)(v >> 8); g_mem[a + 3] = (uint8_t)v; }

/* ---------- the CPU state traps see: registers only ---------- */
static uint32_t regs[M68K_REG_SR + 1];
uint32_t reg(int r) { return (r >= 0 && r <= M68K_REG_SR) ? regs[r] : 0; }
void setreg(int r, uint32_t v) { if (r >= 0 && r <= M68K_REG_SR) regs[r] = v; }
uint16_t pop16(void) { uint32_t sp = regs[M68K_REG_A7]; uint16_t v = rd16(sp); regs[M68K_REG_A7] = sp + 2; return v; }
uint32_t pop32(void) { uint32_t sp = regs[M68K_REG_A7]; uint32_t v = rd32(sp); regs[M68K_REG_A7] = sp + 4; return v; }
void push16(uint16_t v) { uint32_t sp = regs[M68K_REG_A7] - 2; wr16(sp, v); regs[M68K_REG_A7] = sp; }
void push32(uint32_t v) { uint32_t sp = regs[M68K_REG_A7] - 4; wr32(sp, v); regs[M68K_REG_A7] = sp; }
void m68k_end_timeslice(void) {}

/* ---------- the glue's runtime hooks: nothing is checked in the port ---------- */
int  rt_in_check_phase(void) { return 0; }
void rt_skip_compare(int why) { (void)why; }
void rt_abandon(int why) { (void)why; port_quit(); }      /* only when the run is stopped */
void vbl_run_pending(void);
void rt_idle_tick(void) { vbl_advance(1); vbl_run_pending(); }
void rt_run_pending(void) { vbl_run_pending(); }

/* ---------- start ---------- */
void vfs_add_file(const char *macname, uint32_t type, uint32_t creator, const uint8_t *data, uint32_t dlen, const uint8_t *rsrc, uint32_t rlen);
int rs_open_mem_ref(const uint8_t *buf, uint32_t len, const char *macname, int as_app);
void qd_init(void);
extern uint8_t *M;

/* Sets up the Mac the game runs on, from the original archive. Returns 0, or -1 with a message. */
int port_boot(const char *archive, const char *logpath, char *err, size_t errlen) {
    if (logpath) logf_ = fopen(logpath, "w");
    g_appfile = archive;
    g_mem = calloc(MEM_SIZE, 1);
    M = g_mem;
    wr32(LM_MemTop, MEM_SIZE); wr16(LM_ROM85, 0x3FFF);
    wr32(LM_ApplZone, HEAP_BASE); wr32(LM_TheZone, HEAP_BASE); wr32(LM_SysZone, HEAP_BASE);
    wr32(LM_HeapEnd, HEAP_END); wr32(LM_ApplLimit, HEAP_END); wr32(LM_ScrnBase, SCREEN_BASE);
    wr32(LM_Time, 0xAF8B7E00u);  /* the harness's fixed clock (the port sets the real time in port_main) */
    /* the exception vectors as the rig has them: Mazeworld Original reads them through nil pointers
     * (its projection the word at $30, its game loop the long at $20), so they must hold the same */
    for (uint32_t v = 2; v < 64; v++) wr32(4 * v, 0x00FF0010u);
    wr32(0x28, 0x00FF0000u);
    heap_init();
    qd_init();
    static ArcSet arc;
    if (arc_load_file(archive, &arc)) { snprintf(err, errlen, "cannot read %s: %s", archive, arc.err); return -1; }
    const ArcFile *app = NULL;
    for (int k = 0; k < arc.n; k++) if (arc.files[k].type == FOURCC('A','P','P','L') && arc.files[k].rlen) { app = &arc.files[k]; break; }
    if (!app) { snprintf(err, errlen, "%s contains no application", archive); return -1; }
    { void mz_select_original(void); if (app->creator == FOURCC('M','z','w','l')) mz_select_original(); }   /* Mazeworld Original */
    const char *slash = strrchr(app->path, '/');
    snprintf(g_appname, sizeof g_appname, "%.63s", slash ? slash + 1 : app->path);
    size_t dirlen = slash ? (size_t)(slash - app->path + 1) : 0;
    for (int k = 0; k < arc.n; k++) {
        const ArcFile *f = &arc.files[k];
        if (strncmp(f->path, app->path, dirlen) || strchr(f->path + dirlen, '/')) continue;
        vfs_add_file(f->path + dirlen, f->type, f->creator, f->data, f->dlen, f->rsrc, f->rlen);
    }
    if (rs_open_mem_ref(app->rsrc, app->rlen, g_appname, 1) < 0) { snprintf(err, errlen, "bad resource fork in %s", g_appname); return -1; }
    wr16(LM_CurApRefNum, 2);
    wr8(LM_CurApName, (uint8_t)strlen(g_appname)); memcpy(g_mem + LM_CurApName + 1, g_appname, strlen(g_appname));
    traps_init();
    seg_init();
    return 0;
}

void fn_S1_0044(Regs *r);
void fn_S8_21AA(void);

/* What the original's start-up code (CODE 1, fn_S1_0008) does, then main (fn_S8_21AA). */
void port_run_game(void) {
    /* the stack as the original's start-up leaves it when it calls main */
    setreg(M68K_REG_A7, rd32(LM_CurStackBase) - 4 - 18 - 4);
    setreg(M68K_REG_A5, A5_ADDR);
    seg_load(1);
    wr16(0x0A4A, 0);
    { Regs r = {{0}, {0}, 0}; fn_S1_0044(&r); }
    mz_seg(8);
    fn_S8_21AA();
}

/* the rig's debugging switches, off in the port */
uint32_t watch_lo, watch_hi;
int g_shot_every;

/* ---------- the preferences file, kept on the host ---------- */
/* The game finds it in its Preferences folder (dir 20) under the name in 'STR#' 128; the file's
 * contents are the same bytes as on a Mac, so a Mac's preferences file can be used as it is. */
#define PREFS_NAME (mz_original ? "Mazeworld Prefs" : "Mzwl Catacombs Prefs")
void vfs_put_file(int32_t dir, const char *macname, uint32_t type, uint32_t creator, const uint8_t *data, uint32_t dlen);
const uint8_t *vfs_get_file(int32_t dir, const char *macname, uint32_t *dlen);
void port_prefs_load(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return;
    static uint8_t buf[0x10000]; size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
    if (n) vfs_put_file(20, PREFS_NAME, FOURCC('p','r','e','f'), mz_original ? FOURCC('M','z','w','l') : FOURCC('M','z','w','C'), buf, (uint32_t)n);
}
void port_prefs_save(const char *path) {
    uint32_t n = 0; const uint8_t *d = vfs_get_file(20, PREFS_NAME, &n);
    if (!d || !n) return;
    char tmp[1100]; snprintf(tmp, sizeof tmp, "%s.new", path);
    FILE *f = fopen(tmp, "wb"); if (!f) return;
    int ok = fwrite(d, 1, n, f) == n; ok &= fclose(f) == 0;
    if (ok) { remove(path); rename(tmp, path); }
}

/* ---------- calls into the game from a trap ---------- */
/* The rig runs the 68k code at proc; here every routine is C, so the address (a jump-table entry
 * the game handed to the Toolbox) names the C function to call. */
void fn_S8_1B0E(uint32_t dlg, int16_t item);
uint8_t fn_S8_1D5C(uint32_t dlg, uint32_t ev, uint32_t pitem);
uint32_t hx_call_pascal(uint32_t proc, int retbytes, int nargs, const uint32_t *v, const int *sizes) {
    (void)retbytes; (void)sizes;
    uint32_t sp = reg(M68K_REG_A7);
    uint32_t res = 0;
    if (proc == mz_jt(0xA2) && nargs == 2) fn_S8_1B0E(v[0], (int16_t)v[1]);              /* JT 16 */
    else if (proc == mz_jt(0xAA) && nargs == 3) res = fn_S8_1D5C(v[0], v[1], v[2]) ? 0x0100 : 0;   /* JT 17: a Boolean */
    else hx_fault("call from the Toolbox to %06X: not a known routine", proc);
    setreg(M68K_REG_A7, sp);
    return res;
}
