/* Harness entry: boots the original application under Musashi. */
#include "hx.h"
#include "m68k.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif

uint8_t *g_mem;
int g_stop;
char g_stop_reason[256];
uint64_t g_insn_count;
int g_verbose;
int g_idle_limit = 300;
int g_shot_every;
uint32_t g_max_ticks;
const char *g_appfile, *g_outdir = "build";
static uint64_t max_insns = 2000000000ull;
static FILE *logf_;

#include "../archive/arc.h"
void vfs_add_file(const char *macname, uint32_t type, uint32_t creator, const uint8_t *data, uint32_t dlen, const uint8_t *rsrc, uint32_t rlen);
int rs_open_mem_ref(const uint8_t *buf, uint32_t len, const char *macname, int as_app);
char g_appname[64];
void qd_init(void);
void qd_dump_screen(const char *path);
void qd_dump_gworlds(const char *dir);
uint32_t trap_word_for_addr(uint32_t pc);

#define FAULTGATE (TRAPGATE + 0x10)

void hx_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char b[1024]; vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fprintf(logf_ ? logf_ : stderr, "[%10llu] %s\n", (unsigned long long)g_insn_count, b);
    if (g_verbose && logf_) fprintf(stderr, "%s\n", b);
}

void hx_fault(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_stop_reason, sizeof g_stop_reason, fmt, ap); va_end(ap);
    g_stop = 2;
    m68k_end_timeslice();
}

/* ---------- memory ---------- */
static inline int ok(uint32_t a, int n) { return a <= MEM_SIZE - n; }
uint8_t rd8(uint32_t a) { a &= 0xFFFFFF; return g_mem[a]; }
uint16_t rd16(uint32_t a) { a &= 0xFFFFFF; if (!ok(a, 2)) return 0; return (uint16_t)(g_mem[a] << 8 | g_mem[a + 1]); }
uint32_t rd32(uint32_t a) { a &= 0xFFFFFF; if (!ok(a, 4)) return 0; return (uint32_t)g_mem[a] << 24 | g_mem[a + 1] << 16 | g_mem[a + 2] << 8 | g_mem[a + 3]; }
static inline void stk_mark(uint32_t a, int n);
void (*mz_wtrack)(uint32_t a, int n);      /* write observer for -subcheck (sub.c) */
void hx_track(uint32_t a, uint32_t n) { if (mz_wtrack) for (uint32_t k = 0; k < n; k += 0x4000) mz_wtrack(a + k, (int)(n - k < 0x4000 ? n - k : 0x4000)); }
void wr8(uint32_t a, uint8_t v) { if (mz_wtrack) mz_wtrack(a, 1); stk_mark(a, 1); a &= 0xFFFFFF; g_mem[a] = v; }
void wr16(uint32_t a, uint16_t v) { if (mz_wtrack) mz_wtrack(a, 2); stk_mark(a, 2); a &= 0xFFFFFF; if (!ok(a, 2)) return; g_mem[a] = v >> 8; g_mem[a + 1] = (uint8_t)v; }
void wr32(uint32_t a, uint32_t v) { if (mz_wtrack) mz_wtrack(a, 4); stk_mark(a, 4); a &= 0xFFFFFF; if (!ok(a, 4)) return; g_mem[a] = v >> 24; g_mem[a + 1] = (uint8_t)(v >> 16); g_mem[a + 2] = (uint8_t)(v >> 8); g_mem[a + 3] = (uint8_t)v; }

/* Writes into low memory below 0x100 (vectors) or into the code of loaded segments are reported. */
static void watch_write(uint32_t a, int n) {
    a &= 0xFFFFFF;
    if (a < 0x100) hx_log("write to vector area %06X (%d) at pc %06X", a, n, m68k_get_reg(NULL, M68K_REG_PPC));
}

/* ---------- uninitialised stack read detection (-uninit) ---------- */
static int uninit_on;
static uint8_t *stk_init;                 /* 1 = byte written since it was last allocated */
static uint32_t stk_lo, stk_hi, last_sp;
static struct { uint32_t pc, addr; uint32_t n; } ur[256]; static int nur;
static uint32_t wlog_a[64]; static uint8_t wlog_n[64]; static int wlog;    /* stack writes of the last instruction */
static void stk_track_sp(void) {
    uint32_t sp = reg(M68K_REG_A7) & 0xFFFFFF;
    if (sp < last_sp && sp >= stk_lo) {
        memset(stk_init + (sp - stk_lo), 0, last_sp - sp);   /* newly allocated: uninitialised ... */
        for (int i = 0; i < wlog; i++) memset(stk_init + (wlog_a[i] - stk_lo), 1, wlog_n[i]);   /* ... except what was just pushed */
    }
    wlog = 0;
    last_sp = sp;
}
static inline void stk_mark(uint32_t a, int n) {
    a &= 0xFFFFFF;
    if (uninit_on && a >= stk_lo && a + n <= stk_hi) {
        memset(stk_init + (a - stk_lo), 1, n);
        if (wlog < 64) { wlog_a[wlog] = a; wlog_n[wlog] = (uint8_t)n; wlog++; }
    }
}
static void stk_check(uint32_t a, int n) {
    a &= 0xFFFFFF;
    if (!uninit_on || a < stk_lo || a + n > stk_hi) return;
    for (int k = 0; k < n; k++) if (!stk_init[a + k - stk_lo]) {
        uint32_t pc = reg(M68K_REG_PPC);
        for (int i = 0; i < nur; i++) if (ur[i].pc == pc) { ur[i].n++; return; }
        if (nur < 256) { ur[nur].pc = pc; ur[nur].addr = a - reg(M68K_REG_A6); ur[nur].n = 1; nur++; }
        return;
    }
}
unsigned int m68k_read_memory_8(unsigned int a) { stk_check(a, 1); return rd8(a); }
unsigned int m68k_read_memory_16(unsigned int a) { stk_check(a, 2); return rd16(a); }
unsigned int m68k_read_memory_32(unsigned int a) { stk_check(a, 4); return rd32(a); }
int g_mem_written;
static FILE *wlogf; static uint32_t wlog_from;
static void wlog_write(uint32_t a, uint32_t v, int n) {
    if (!wlogf) return;
    uint32_t vbl_ticks(void);
    if (vbl_ticks() < wlog_from) return;
    uint32_t pc = m68k_get_reg(NULL, M68K_REG_PPC), o = 0; int s = seg_of(pc, &o);
    fprintf(wlogf, "S%d:%04X %06X %d %X\n", s, o, a & 0xFFFFFF, n, v);
}
uint32_t watch_lo, watch_hi;
#define NWATCH 1024
static struct { uint32_t pc; uint32_t n; uint32_t vals[4]; } watch_stats[NWATCH]; static int nwatch_stats;
static void watch_range(uint32_t a, uint32_t v) {
    if (!watch_hi || (a & 0xFFFFFF) < watch_lo || (a & 0xFFFFFF) >= watch_hi) return;
    uint32_t pc = m68k_get_reg(NULL, M68K_REG_PPC);
    for (int i = 0; i < nwatch_stats; i++) if (watch_stats[i].pc == pc) { if (watch_stats[i].n < 4) watch_stats[i].vals[watch_stats[i].n] = a & 0xFFFFFF; watch_stats[i].n++; return; }
    if (nwatch_stats < NWATCH) { watch_stats[nwatch_stats].pc = pc; watch_stats[nwatch_stats].n = 1; watch_stats[nwatch_stats].vals[0] = a & 0xFFFFFF; nwatch_stats++; }
    (void)v;
}
void m68k_write_memory_8(unsigned int a, unsigned int v) { watch_write(a, 1); watch_range(a, v); wlog_write(a, v, 1); g_mem_written = 1; wr8(a, (uint8_t)v); }
void m68k_write_memory_16(unsigned int a, unsigned int v) { watch_write(a, 2); watch_range(a, v); wlog_write(a, v, 2); g_mem_written = 1; wr16(a, (uint16_t)v); }
void m68k_write_memory_32(unsigned int a, unsigned int v) { watch_write(a, 4); watch_range(a, v); wlog_write(a, v, 4); g_mem_written = 1; wr32(a, v); }
unsigned int m68k_read_disassembler_8(unsigned int a) { return rd8(a); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return rd16(a); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return rd32(a); }

uint32_t reg(int r) { return m68k_get_reg(NULL, (m68k_register_t)r); }
void setreg(int r, uint32_t v) { m68k_set_reg((m68k_register_t)r, v); }
uint16_t pop16(void) { uint32_t sp = reg(M68K_REG_A7); uint16_t v = rd16(sp); setreg(M68K_REG_A7, sp + 2); return v; }
uint32_t pop32(void) { uint32_t sp = reg(M68K_REG_A7); uint32_t v = rd32(sp); setreg(M68K_REG_A7, sp + 4); return v; }
void push16(uint16_t v) { uint32_t sp = reg(M68K_REG_A7) - 2; wr16(sp, v); setreg(M68K_REG_A7, sp); }
void push32(uint32_t v) { uint32_t sp = reg(M68K_REG_A7) - 4; wr32(sp, v); setreg(M68K_REG_A7, sp); }

/* ---------- calls into the game from a trap ---------- */
int g_call_done;
uint32_t hx_call_pascal(uint32_t proc, int retbytes, int nargs, const uint32_t *vals, const int *sizes) {
    uint32_t r[16], sr = reg(M68K_REG_SR), pc = reg(M68K_REG_PC);
    for (int i = 0; i < 16; i++) r[i] = reg(M68K_REG_D0 + i);
    if (retbytes == 2) push16(0); else if (retbytes == 4) push32(0);
    for (int i = 0; i < nargs; i++) { if (sizes[i] == 2) push16((uint16_t)vals[i]); else push32(vals[i]); }
    push32(CALLRET);
    setreg(M68K_REG_PC, proc);
    int outer = g_call_done; g_call_done = 0;
    while (!g_call_done && !g_stop) m68k_execute(100000);
    uint32_t sp = reg(M68K_REG_A7), res = retbytes == 2 ? rd16(sp) : retbytes == 4 ? rd32(sp) : 0;
    g_call_done = outer;
    for (int i = 0; i < 16; i++) setreg(M68K_REG_D0 + i, r[i]);
    setreg(M68K_REG_SR, sr); setreg(M68K_REG_PC, pc);
    return res;
}

/* ---------- the instruction hook: trap gate, call-by-address, faults, tracing ---------- */
static uint32_t pc_ring[32]; static unsigned pc_ring_i;
int vbl_maybe_interrupt(uint32_t pc);
int vbl_is_stub(uint32_t pc);
void vbl_stub_hook(uint32_t pc);
void vbl_spin_check(uint32_t pc, uint32_t prev_pc);
extern int g_mem_written;
void hx_instr_hook(unsigned int pc) {
    g_insn_count++;
    if (uninit_on) stk_track_sp();
    uint32_t prev = pc_ring[(pc_ring_i - 1) & 31];
    pc_ring[pc_ring_i++ & 31] = pc;
    if (vbl_is_stub(pc)) {
        extern int g_sub_pending; void sub_check_return(uint32_t pc);
        if (g_sub_pending) sub_check_return(pc);     /* a checked VBL task or sound callback returning */
        vbl_stub_hook(pc); return;
    }
    if (pc >= TRAPGATE) {
        if (pc == TRAPGATE) {
            /* 68020 format-0 frame: SR.w, PC.l, format/vector.w; PC points at the A-line word */
            uint32_t sp = reg(M68K_REG_A7);
            uint16_t sr = rd16(sp); uint32_t tpc = rd32(sp + 2);
            setreg(M68K_REG_A7, sp + 8);
            setreg(M68K_REG_SR, sr);
            setreg(M68K_REG_PC, tpc + 2);
            trap_dispatch(rd16(tpc), tpc, 0);
            if (rd32(0x28) != TRAPGATE) { uint32_t o = 0; int s4 = seg_of(tpc, &o); hx_fault("A-line vector overwritten (now %08X) by trap %04X at S%d:%04X", rd32(0x28), rd16(tpc), s4, o); return; }
        } else if (pc == FAULTGATE) {
            uint32_t sp = reg(M68K_REG_A7);
            uint16_t fv = rd16(sp + 6);
            hx_fault("CPU exception vector %u at pc %06X", (fv & 0xFFF) / 4, rd32(sp + 2));
            return;
        } else if (pc >= TRAPADDR_BASE && pc < TRAPADDR_END) {
            uint16_t w = (uint16_t)trap_word_for_addr(pc);
            uint32_t ret = pop32();
            setreg(M68K_REG_PC, ret);
            trap_dispatch(w, ret, 1);
        } else if (pc == IDLERET) {
            extern int g_idle_done; g_idle_done = 1;
            m68k_end_timeslice(); return;
        } else if (pc == CALLRET || pc == CALLRET + 2) {   /* +2: reached without the hook (after a C function returned) */
            extern int g_call_done; g_call_done = 1;
            { extern int g_sub_pending; void sub_check_return(uint32_t pc); if (g_sub_pending) sub_check_return(CALLRET); }
            m68k_end_timeslice(); return;
        } else if (pc == STOPADDR) {
            g_stop = 1; snprintf(g_stop_reason, sizeof g_stop_reason, "returned from entry point");
            m68k_end_timeslice(); return;
        } else { hx_fault("jump into harness area %06X", pc); return; }
        if (g_stop) { m68k_end_timeslice(); return; }
        seg_trace_mark(reg(M68K_REG_PC));
        return;
    }
    static uint32_t last_seg_pc;
    uint32_t off;
    if (vbl_maybe_interrupt(pc)) return;
    vbl_spin_check(pc, prev);
    int sn0 = seg_of(pc, &off);
    if (sn0) {
        int sub_check(int seg, uint32_t off, uint32_t pc); extern int g_sub_pending; void sub_check_return(uint32_t pc);
        { void input_pc_hook(int seg, uint32_t off); static int has = -1; int input_has_pc_hooks(void);
          if (has < 0) has = input_has_pc_hooks();
          if (has) input_pc_hook(sn0, off); }
        if (g_sub_pending) sub_check_return(pc);
        if (sub_check(sn0, off, pc)) { if (g_stop) m68k_end_timeslice(); return; }   /* a C function ran */
        seg_trace_mark(pc); last_seg_pc = pc; return;
    }
    if (pc >= A5_ADDR + 32 && pc < A5_ADDR + 32 + 8 * g_jt_count) return;    /* jump-table entry */
    if (pc < HEAP_BASE || pc >= HEAP_END) {
        uint32_t o2 = 0; int s2 = seg_of(last_seg_pc, &o2);
        hx_fault("execution outside code at %06X; last code pc S%d:%04X", pc, s2, o2);
        return;
    }
    /* code running from the heap (generated or copied by the game): allowed, logged once per block */
    static uint32_t seen_heap_pc;
    if ((pc & ~0xFFFu) != seen_heap_pc) { seen_heap_pc = pc & ~0xFFFu; uint32_t o2 = 0; int s2 = seg_of(last_seg_pc, &o2); hx_log("executing heap code at %06X (entered from S%d:%04X)", pc, s2, o2); }
}

static void usage(void) {
    fprintf(stderr, "usage: mzharness [-v] [-n max_insns] [-ticks N] [-idle N] [-script file] [-sub list|all] [-subcheck list|all] [-fuzz name:count:d4,d5,...] [-shots N] [-o outdir] <archive .hqx/.sit/.bin>\n");
    exit(2);
}

int input_load(const char *path);
uint32_t hx_ticks(void);
int main(int argc, char **argv) {
    const char *script = NULL, *play = NULL, *sublist = NULL, *fuzzspec = NULL; uint32_t max_ticks = 0;
    (void)play;
    extern uint32_t g_max_ticks;
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-v")) g_verbose = 1;
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) max_insns = strtoull(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "-idle") && i + 1 < argc) g_idle_limit = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) g_outdir = argv[++i];
        else if (!strcmp(argv[i], "-script") && i + 1 < argc) script = argv[++i];
#ifdef HX_PLAY
        else if (!strcmp(argv[i], "-play") && i + 1 < argc) play = argv[++i];
#endif
        else if (!strcmp(argv[i], "-shots") && i + 1 < argc) g_shot_every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-sub") && i + 1 < argc) sublist = argv[++i];
        else if (!strcmp(argv[i], "-subcheck") && i + 1 < argc) { extern int g_sub_checkmode; g_sub_checkmode = 1; sublist = argv[++i]; }
        else if (!strcmp(argv[i], "-fuzz") && i + 1 < argc) { fuzzspec = argv[++i]; }
        else if (!strcmp(argv[i], "-uninit")) uninit_on = 1;
        else if (!strcmp(argv[i], "-watch") && i + 1 < argc) { sscanf(argv[++i], "%x-%x", &watch_lo, &watch_hi); }
        else if (!strcmp(argv[i], "-ticks") && i + 1 < argc) g_max_ticks = max_ticks = (uint32_t)strtoul(argv[++i], 0, 0);
        else usage();
    }
    if (argc - i != 1) usage();
    g_appfile = argv[i];
    { char cmd[600]; snprintf(cmd, sizeof cmd, "%s", g_outdir);   /* mkdir -p */
      for (char *q = cmd + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(cmd, 0755); *q = '/'; }
      mkdir(cmd, 0755); }
    char lp[512]; snprintf(lp, sizeof lp, "%s/harness.log", g_outdir);
    logf_ = fopen(lp, "w");
    if (g_shot_every) { char sd[512]; snprintf(sd, sizeof sd, "%s/shots", g_outdir); mkdir(sd, 0755); }

    g_mem = calloc(MEM_SIZE, 1);
    /* low memory */
    wr32(LM_MemTop, MEM_SIZE); wr16(LM_ROM85, 0x3FFF);
    wr32(LM_ApplZone, HEAP_BASE); wr32(LM_TheZone, HEAP_BASE); wr32(LM_SysZone, HEAP_BASE);
    wr32(LM_HeapEnd, HEAP_END); wr32(LM_ApplLimit, HEAP_END); wr32(LM_ScrnBase, SCREEN_BASE);
    wr32(LM_Time, 0xAF8B7E00u);  /* fixed clock: 1997-05-31 00:00, deterministic */
    for (uint32_t v = 2; v < 64; v++) wr32(4 * v, FAULTGATE);
    wr32(0x28, TRAPGATE);        /* A-line */
    wr16(TRAPGATE, 0x4E71); wr16(FAULTGATE, 0x4E71);  /* placeholders; hook intercepts before execution */
    for (uint32_t a = TRAPADDR_BASE; a < TRAPADDR_END; a += 2) wr16(a, 0x4E71);
    wr16(STOPADDR, 0x4E71); wr16(IDLERET, 0x4E71); wr16(CALLRET, 0x4E71); wr16(CALLRET + 2, 0x4E71);

    heap_init();
    qd_init();
    /* the game comes straight from its original archive: the first application is the game,
     * and every file in the same folder appears in the application folder of the virtual disk */
    ArcSet arc;
    if (arc_load_file(g_appfile, &arc)) { fprintf(stderr, "cannot read %s: %s\n", g_appfile, arc.err); return 1; }
    const ArcFile *app = NULL;
    for (int k = 0; k < arc.n; k++) if (arc.files[k].type == FOURCC('A','P','P','L') && arc.files[k].rlen) { app = &arc.files[k]; break; }
    if (!app) { fprintf(stderr, "%s contains no application\n", g_appfile); return 1; }
    const char *slash = strrchr(app->path, '/');
    snprintf(g_appname, sizeof g_appname, "%.63s", slash ? slash + 1 : app->path);
    size_t dirlen = slash ? (size_t)(slash - app->path + 1) : 0;
    for (int k = 0; k < arc.n; k++) {
        const ArcFile *f = &arc.files[k];
        if (strncmp(f->path, app->path, dirlen) || strchr(f->path + dirlen, '/')) continue;   /* same folder only */
        vfs_add_file(f->path + dirlen, f->type, f->creator, f->data, f->dlen, f->rsrc, f->rlen);
    }
    if (rs_open_mem_ref(app->rsrc, app->rlen, g_appname, 1) < 0) { fprintf(stderr, "bad resource fork in %s\n", g_appname); return 1; }
    hx_log("game: \"%s\" (creator %c%c%c%c) from %s", g_appname, app->creator >> 24, app->creator >> 16, app->creator >> 8, app->creator, g_appfile);
    wr16(LM_CurApRefNum, 2);
    wr8(LM_CurApName, (uint8_t)strlen(g_appname)); memcpy(g_mem + LM_CurApName + 1, g_appname, strlen(g_appname));
    traps_init();
    seg_init();
    if (sublist) { int sub_enable(const char *list); sub_enable(sublist); }
    if (fuzzspec) { int sub_fuzz(const char *spec); if (sub_fuzz(fuzzspec)) { fprintf(stderr, "bad -fuzz spec %s (name:count:d4,d5,...)\n", fuzzspec); return 2; } }
    if (getenv("MZ_WLOG")) { char wp[512]; snprintf(wp, sizeof wp, "%s/writes.txt", g_outdir); wlogf = fopen(wp, "w"); wlog_from = (uint32_t)atoi(getenv("MZ_WLOG")); }
    if (script && input_load(script)) { fprintf(stderr, "cannot load input script %s\n", script); return 2; }
#ifdef HX_PLAY
    { int play_start(const char *recording);   /* play.c: the window; the session is recorded */
      if (play) { if (play_start(play)) return 2; max_insns = ~0ull; } }
#endif

    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68020);
    wr32(0, rd32(LM_CurStackBase)); wr32(4, A5_ADDR + 32 + 2);
    m68k_pulse_reset();
    for (uint32_t v = 2; v < 64; v++) wr32(4 * v, FAULTGATE);
    wr32(0x28, TRAPGATE);
    setreg(M68K_REG_SR, 0x2000);
    setreg(M68K_REG_A7, rd32(LM_CurStackBase));
    push32(STOPADDR);
    setreg(M68K_REG_A5, A5_ADDR);
    setreg(M68K_REG_PC, A5_ADDR + 32 + 2);  /* JT[0]: move.w #1,-(sp); _LoadSeg */
    if (uninit_on) {
        stk_hi = rd32(LM_CurStackBase); stk_lo = stk_hi - STACK_SIZE;
        stk_init = calloc(STACK_SIZE, 1);
        memset(stk_init + (reg(M68K_REG_A7) - stk_lo), 1, stk_hi - reg(M68K_REG_A7));
        last_sp = reg(M68K_REG_A7);
    }

    while (!g_stop && g_insn_count < max_insns) {
        m68k_execute(100000);
        (void)max_ticks;   /* the tick limit stops inside vbl_advance, at an exact point */
    }
    if (!g_stop) snprintf(g_stop_reason, sizeof g_stop_reason, "instruction budget reached");

    uint32_t pc = reg(M68K_REG_PC), off; int sn = seg_of(pc, &off);
    hx_log("STOP: %s", g_stop_reason);
    { void sub_report(void); sub_report(); }
    for (int k = 0; k < nur; k++) { uint32_t o = 0; int s6 = seg_of(ur[k].pc, &o);
        hx_log("uninit: S%d:%04X reads uninitialised stack at A6%+d (%u times)", s6, o, (int32_t)ur[k].addr, ur[k].n); }
    /* backtrace through LINK A6 frames */
    {
        char bt[1024] = ""; size_t L = 0;
        uint32_t fp = reg(M68K_REG_A6);
        for (int d = 0; d < 24 && fp > 0x2000 && fp < MEM_SIZE - 8; d++) {
            uint32_t ret = rd32(fp + 4), o; int s2 = seg_of(ret, &o);
            if (!s2) break;
            L += snprintf(bt + L, sizeof bt - L, " <- S%d:%04X", s2, o);
            uint32_t nfp = rd32(fp); if (nfp <= fp) break; fp = nfp;
        }
        hx_log("backtrace (return addresses):%s", bt);
        if (g_stop == 2) {
            char rb[1024] = ""; size_t L2 = 0;
            for (unsigned k = pc_ring_i - 32; k != pc_ring_i; k++) {
                uint32_t p = pc_ring[k & 31], o; int s3 = seg_of(p, &o);
                if (s3) L2 += snprintf(rb + L2, sizeof rb - L2, " S%d:%04X", s3, o);
                else L2 += snprintf(rb + L2, sizeof rb - L2, " %06X", p);
            }
            hx_log("last pcs:%s", rb);
        }
    }
    hx_log("pc=%06X (%s%d:%04X) instructions=%llu", pc, sn ? "S" : "?", sn, sn ? off : 0, (unsigned long long)g_insn_count);
    for (int k = 0; k < nwatch_stats; k++) {
        uint32_t o = 0; int s5 = seg_of(watch_stats[k].pc, &o);
        hx_log("watch: writer S%d:%04X (%06X) %u writes, first addresses %06X %06X %06X %06X", s5, o, watch_stats[k].pc, watch_stats[k].n, watch_stats[k].vals[0], watch_stats[k].vals[1], watch_stats[k].vals[2], watch_stats[k].vals[3]);
    }
    {   /* A5 world snapshot: globals (below A5) + jump table area */
        char gp[512]; snprintf(gp, sizeof gp, "%s/a5world.bin", g_outdir);
        uint32_t base = rd32(LM_CurStackBase);
        FILE *gf = fopen(gp, "wb"); if (gf) { fwrite(g_mem + base, 1, A5_ADDR - base, gf); fclose(gf); }
        snprintf(gp, sizeof gp, "%s/heap.bin", g_outdir);
        gf = fopen(gp, "wb"); if (gf) { fwrite(g_mem + HEAP_BASE, 1, HEAP_END - HEAP_BASE, gf); fclose(gf); }
    }
    char td[512]; snprintf(td, sizeof td, "%s/trace", g_outdir);
    seg_trace_write(td);
    char sp[512]; snprintf(sp, sizeof sp, "%s/screen.ppm", g_outdir); qd_dump_screen(sp); qd_dump_gworlds(g_outdir);
    { void snd_write_wav(const char *path); char wp[512]; snprintf(wp, sizeof wp, "%s/sound.wav", g_outdir); snd_write_wav(wp); }
    char rp[512]; snprintf(rp, sizeof rp, "%s/traps.txt", g_outdir);
    FILE *rf = fopen(rp, "w"); traps_report(rf); fclose(rf);
    fprintf(stderr, "STOP: %s\n  pc=%06X S%d:%04X  instructions=%llu  (log: %s)\n", g_stop_reason, pc, sn, sn ? off : 0, (unsigned long long)g_insn_count, lp);
    if (logf_) fclose(logf_);
    if (wlogf) fclose(wlogf);
    return g_stop == 2 ? 1 : 0;
}
