/* Function substitution: when execution reaches the entry of a function that has a C version,
 * run the C version instead and return to the caller. Enabled per function (-sub list / all).
 *
 * Check mode (-subcheck list / all) verifies every single call instead: at the entry the C version
 * runs with all its memory writes logged, its resulting registers and written bytes are recorded,
 * and everything is undone. Then the original 68k code runs, and when it returns the two are
 * compared: registers (all of them and the CCR for assembly routines; D0 as far as callers see it and
 * the callee-saved D3-D7/A2-A6 for compiled C) and every byte either side wrote, except the dead
 * stack below the returned SP, a compiled function's own arguments (C code may modify its parameters
 * in place; the caller pops them unread) and writes made by interrupt handlers. Any difference stops the run.
 * Execution always follows the original, so this checks each call against its true inputs.
 * Functions marked nocheck (they call Toolbox routines whose state is not in emulated memory, such
 * as the window list) run as C in check mode too; the end-to-end comparison covers them.
 *
 * Also provides the Toolbox glue (mac_*) that the C game code calls in the harness. */
#include "hx.h"
#include "m68k.h"
#include "../game/mzasm.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
int mtb_sane_pop_bytes(uint16_t op);   /* mactb/sane.c */
const char *hx_where(void);
extern uint32_t g_sub_pc;
void mz_local_reset(void);
void sub_regs_load(Regs *r);
void sub_regs_store(const Regs *r);

typedef struct { int seg; uint32_t off; const char *name; void (*fn)(void); int regs; uint32_t d0mask; int argbytes; int nocheck; int pops; uint32_t ignore; int orig; } SubEntry;
extern const SubEntry g_subs[];
extern void (*mz_wtrack)(uint32_t a, int n);
int vbl_in_irq(void);
uint32_t vbl_ticks(void);

static uint8_t enabled[1024];
static uint32_t at_off[1024];   /* where each runs: its offset, or for Mazeworld Original the paired one */
static int at_seg[1024];
static uint32_t calls[1024], checked[1024];
static int any_enabled;
int g_sub_checkmode, g_sub_pending;

int sub_enable(const char *list) {
    M = g_mem;
    int n = 0;
    for (int i = 0; g_subs[i].name; i++) {
        if (!strcmp(list, "all")) enabled[i] = 1;
        else {
            const char *p = strstr(list, g_subs[i].name);
            size_t L = strlen(g_subs[i].name);
            if (p && (p[L] == 0 || p[L] == ',')) enabled[i] = 1;
        }
        /* Mazeworld Original: a shared function runs at its paired address, if it can run there; one
         * of Original's own (subs.txt "orig") only there */
        at_off[i] = g_subs[i].off; at_seg[i] = g_subs[i].seg;
        if (g_subs[i].orig && !mz_original) enabled[i] = 0;
        if (enabled[i] && mz_original && !g_subs[i].orig) {
            uint32_t at = mz_orig_func(g_subs[i].seg, (uint16_t)g_subs[i].off);
            if (!at) enabled[i] = 0;
            at_seg[i] = (int)(at >> 16); at_off[i] = at & 0xFFFF;
        }
        if (enabled[i]) { n++; any_enabled = 1; }
    }
    hx_log("substitution: %d C functions enabled%s", n, g_sub_checkmode ? " (check mode: every call compared with the original)" : "");
    return n;
}


/* ---------- write logs for check mode: first write of each byte, with its old value ---------- */
typedef struct { uint32_t a; uint8_t old, now; } WByte;
typedef struct { WByte *b; int n, cap; uint16_t gen; } WLog;
static uint16_t *seen;             /* per byte of memory: generation of the log that saw it */
static uint16_t gen_counter;
static WLog wl_c, wl_68k;

static void wlog_start(WLog *l) {
    if (!seen) seen = calloc(MEM_SIZE, sizeof *seen);
    if (++gen_counter == 0) { memset(seen, 0, MEM_SIZE * sizeof *seen); gen_counter = 1; }
    l->gen = gen_counter; l->n = 0;
}
static void wlog_add(WLog *l, uint32_t a, int n) {
    for (int k = 0; k < n; k++) {
        uint32_t x = (a + (uint32_t)k) & 0xFFFFFF;
        if (seen[x] == l->gen) continue;
        seen[x] = l->gen;
        if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 1024; l->b = realloc(l->b, (size_t)l->cap * sizeof *l->b); }
        l->b[l->n++] = (WByte){x, g_mem[x], 0};
    }
}
static void track_c(uint32_t a, int n) { wlog_add(&wl_c, a, n); }
static int cmp_wbyte(const void *x, const void *y) { uint32_t a = ((const WByte *)x)->a, b = ((const WByte *)y)->a; return a < b ? -1 : a > b; }

/* ---------- the pending comparison ---------- */
static struct {
    int i;                         /* g_subs index */
    uint32_t ret, sp_after;
    Regs c;                        /* registers the C version left */
    uint32_t tick;
    int irq;                       /* entered from a VBL task or sound callback (interrupt time) */
} pend;
static void track_68k(uint32_t a, int n) { if (vbl_in_irq() == pend.irq) wlog_add(&wl_68k, a, n); }

static uint32_t skipped[1024];

/* ---------- the runtime hooks the glue (glue.c) calls ---------- */
static int cphase, cphase_needs_seg;   /* in the C phase of a check; why its comparison is skipped */
static jmp_buf c_abort;
int  rt_in_check_phase(void) { return cphase; }
void rt_skip_compare(int why) { cphase_needs_seg = why; }          /* a segment number, or -1: waits */
void rt_abandon(int why) { longjmp(c_abort, why); }               /* 1: in a check, 2: run stopped */
void vbl_idle_tick(void);
void vbl_run_pending(void);
void rt_idle_tick(void) { vbl_idle_tick(); }
void rt_run_pending(void) { vbl_run_pending(); }


/* ---------- fuzzing (-fuzz name:count:regs) ----------
 * The first checked call of the function is followed by `count` more runs of it from the same
 * entry state with the low words of the listed data registers (e.g. d4,d5,d6,d7) randomised,
 * each compared like a normal call: this reaches paths the recorded games never take. Afterwards
 * the real call's results are put back and the run continues. */
static int fuzz_i = -1, fuzz_left, fuzz_total, fuzz_active;
static uint8_t fuzz_regs;          /* bit k: randomise D<k>.w */
static uint16_t fuzz_frame[16]; static int fuzz_nframe;   /* words at A6 - offset (fNN, hex) or A5 - offset (gNN) to randomise */
static uint8_t fuzz_glob[16]; static uint16_t fuzz_mod[16];  /* gNN: a global; %N (decimal): the value taken modulo N */
static uint32_t fuzz_addr(int k, uint32_t a6) { return (fuzz_glob[k] ? reg(M68K_REG_A5) : a6) - fuzz_frame[k]; }
static uint16_t fuzz_frame_old[16]; static int fuzz_frame_saved;
static Regs fuzz_entry, fuzz_post;
static uint32_t fuzz_sr, fuzz_pc, fuzz_post_sr, fuzz_post_pc, fuzz_rng = 0x2545F491u;
static WByte *fuzz_postw; static int fuzz_npostw;
int sub_fuzz(const char *spec) {   /* name:count:d4,d5,... */
    char name[64]; int count = 0; const char *c1 = strchr(spec, ':');
    if (!c1 || (size_t)(c1 - spec) >= sizeof name) return -1;
    memcpy(name, spec, (size_t)(c1 - spec)); name[c1 - spec] = 0;
    count = atoi(c1 + 1);
    const char *c2 = strchr(c1 + 1, ':');
    for (const char *p = c2 ? c2 + 1 : ""; *p; p++) {
        if ((*p == 'd' || *p == 'D') && p[1] >= '0' && p[1] <= '7') fuzz_regs |= (uint8_t)(1u << (p[1] - '0'));
        else if ((*p == 'f' || *p == 'g') && fuzz_nframe < 16) {
            char *e; fuzz_glob[fuzz_nframe] = *p == 'g';
            fuzz_frame[fuzz_nframe] = (uint16_t)strtoul(p + 1, &e, 16);
            fuzz_mod[fuzz_nframe] = *e == '%' ? (uint16_t)strtoul(e + 1, NULL, 10) : 0;
            fuzz_nframe++;
            while (p[1] && p[1] != ',') p++;
        }
    }
    for (int i = 0; g_subs[i].name; i++) if (!strcmp(g_subs[i].name, name)) { fuzz_i = i; fuzz_left = fuzz_total = count; return 0; }
    return -1;
}
static uint32_t fuzz_rand(void) { fuzz_rng ^= fuzz_rng << 13; fuzz_rng ^= fuzz_rng >> 17; fuzz_rng ^= fuzz_rng << 5; return fuzz_rng; }
static uint16_t fuzz_word(uint16_t was) {
    static const uint16_t edge[] = {0, 1, 2, 0xFFFF, 0xFFFE, 0x7FFE, 0x7FFF, 0x8000, 0x8001, 0x40, 0xA0, 0x13F, 0x140};
    switch (fuzz_rand() % 5) {
    case 0:  return (uint16_t)fuzz_rand();
    case 1:  return edge[fuzz_rand() % (sizeof edge / sizeof *edge)];
    case 2:  return (uint16_t)(was + (int16_t)(fuzz_rand() % 33) - 16);
    case 3:  return (uint16_t)((int16_t)(fuzz_rand() % 1200) - 400);
    default: return (uint16_t)(0x7FF0 + fuzz_rand() % 32);
    }
}
static Regs fuzz_in;
static int run_checked(int i, uint32_t pc);
/* after a compared call of the fuzz target: undo the original's writes, then start the next random
 * run, or put the real call's results back when done */
static void fuzz_next(const WLog *orig) {
    if (!fuzz_active) {            /* the real call: keep its results */
        sub_regs_load(&fuzz_post); fuzz_post_sr = reg(M68K_REG_SR); fuzz_post_pc = reg(M68K_REG_PC);
        fuzz_postw = malloc((size_t)(orig->n ? orig->n : 1) * sizeof *fuzz_postw); fuzz_npostw = orig->n;
        for (int k = 0; k < orig->n; k++) { fuzz_postw[k] = orig->b[k]; fuzz_postw[k].now = g_mem[orig->b[k].a]; }
        fuzz_active = 1;
    }
    for (int k = orig->n - 1; k >= 0; k--) g_mem[orig->b[k].a] = orig->b[k].old;
    uint32_t a6 = fuzz_entry.a[6];
    if (fuzz_frame_saved) for (int k = fuzz_nframe - 1; k >= 0; k--) wr16(fuzz_addr(k, a6), fuzz_frame_old[k]);
    fuzz_frame_saved = 0;
    if (fuzz_left > 0) {
        fuzz_left--;
        fuzz_in = fuzz_entry;
        for (int k = 0; k < 8; k++) if (fuzz_regs >> k & 1) set_w(&fuzz_in.d[k], fuzz_word((uint16_t)fuzz_in.d[k]));
        for (int k = 0; k < fuzz_nframe; k++) {
            uint32_t a = fuzz_addr(k, a6); uint16_t v = fuzz_word(rd16(a));
            fuzz_frame_old[k] = rd16(a);
            wr16(a, fuzz_mod[k] ? (uint16_t)(v % fuzz_mod[k]) : v);
        }
        fuzz_frame_saved = 1;
        sub_regs_store(&fuzz_in); setreg(M68K_REG_SR, fuzz_sr); setreg(M68K_REG_PC, fuzz_pc);
        g_sub_pc = fuzz_pc; mz_local_reset(); calls[fuzz_i]++;
        run_checked(fuzz_i, fuzz_pc);
        return;
    }
    for (int k = 0; k < fuzz_npostw; k++) g_mem[fuzz_postw[k].a] = fuzz_postw[k].now;
    free(fuzz_postw); fuzz_postw = NULL;
    sub_regs_store(&fuzz_post); setreg(M68K_REG_SR, fuzz_post_sr); setreg(M68K_REG_PC, fuzz_post_pc);
    hx_log("fuzz: %s matched the original on %d random inputs", g_subs[fuzz_i].name, fuzz_total);
    fuzz_total = 0; fuzz_active = 0;
}
static int run_checked(int i, uint32_t pc) {
    Regs entry; sub_regs_load(&entry);
    uint32_t sr = reg(M68K_REG_SR);
    if (i == fuzz_i && fuzz_total && !fuzz_active) { fuzz_entry = entry; fuzz_sr = sr; fuzz_pc = pc; }
    wlog_start(&wl_c);
    mz_wtrack = track_c;
    { void rs_state_save(void); rs_state_save(); }
    cphase = 1; cphase_needs_seg = 0;
    jmp_buf outer; memcpy(outer, c_abort, sizeof outer);
    if (!setjmp(c_abort)) g_subs[i].fn();
    memcpy(c_abort, outer, sizeof outer);
    cphase = 0;
    mz_wtrack = NULL;
    sub_regs_load(&pend.c);
    for (int k = 0; k < wl_c.n; k++) wl_c.b[k].now = g_mem[wl_c.b[k].a];
    for (int k = wl_c.n - 1; k >= 0; k--) g_mem[wl_c.b[k].a] = wl_c.b[k].old;    /* undo */
    { void rs_state_restore(void); rs_state_restore(); }
    qsort(wl_c.b, (size_t)wl_c.n, sizeof *wl_c.b, cmp_wbyte);
    sub_regs_store(&entry); setreg(M68K_REG_SR, sr); setreg(M68K_REG_PC, pc);
    if (cphase_needs_seg) {   /* the original will load a segment on the way: run it unchecked */
        skipped[i]++;
        if (cphase_needs_seg < 0) { if (skipped[i] <= 3) hx_log("subcheck: %s call %u not compared: it waits for a tick%s", g_subs[i].name, calls[i], skipped[i] == 3 ? " (further ones not logged)" : ""); }
        else hx_log("subcheck: %s call %u not compared: the original loads segment %d during it", g_subs[i].name, calls[i], cphase_needs_seg);
        return 0;
    }
    pend.i = i; pend.ret = rd32(entry.a[7]); pend.sp_after = entry.a[7] + 4 + (uint32_t)g_subs[i].pops; pend.tick = vbl_ticks(); pend.irq = vbl_in_irq();
    wlog_start(&wl_68k);
    mz_wtrack = track_68k;
    g_sub_pending = 1;
    return 0;                      /* the original runs now */
}

static int mismatches;
static void report(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void report(const char *fmt, ...) {
    char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    if (mismatches++ < 16) hx_log("subcheck %s call %u (tick %u): %s", g_subs[pend.i].name, checked[pend.i] + 1, pend.tick, b);
}

void sub_check_return(uint32_t pc) {
    if (pc != pend.ret || reg(M68K_REG_A7) != pend.sp_after || vbl_in_irq() != pend.irq) return;
    mz_wtrack = NULL; g_sub_pending = 0;
    const SubEntry *e = &g_subs[pend.i];
    Regs o; sub_regs_load(&o);
    mismatches = 0;
    /* registers */
    if (e->regs) {
        for (int k = 0; k < 8; k++) if (!(e->ignore >> k & 1) && o.d[k] != pend.c.d[k]) report("D%d: C %08X, original %08X", k, pend.c.d[k], o.d[k]);
        for (int k = 0; k < 7; k++) if (!(e->ignore >> (8 + k) & 1) && o.a[k] != pend.c.a[k]) report("A%d: C %08X, original %08X", k, pend.c.a[k], o.a[k]);
        if (!(e->ignore >> 16 & 1) && o.ccr != pend.c.ccr) report("CCR: C %02X, original %02X (XNZVC)", pend.c.ccr, o.ccr);
    } else {
        if ((o.d[0] ^ pend.c.d[0]) & e->d0mask) report("D0: C %08X, original %08X (mask %08X)", pend.c.d[0], o.d[0], e->d0mask);
        for (int k = 3; k < 8; k++) if (!(e->ignore >> k & 1) && o.d[k] != pend.c.d[k]) report("D%d (callee-saved): C %08X, original %08X", k, pend.c.d[k], o.d[k]);
        for (int k = 2; k < 7; k++) if (!(e->ignore >> (8 + k) & 1) && o.a[k] != pend.c.a[k]) report("A%d (callee-saved): C %08X, original %08X", k, pend.c.a[k], o.a[k]);
    }
    /* memory: everything above the returned SP's dead stack */
    uint32_t stack_lo = rd32(LM_CurStackBase) - STACK_SIZE;
    #define DEAD(x) ((x) >= stack_lo && (x) < pend.sp_after + (e->pops ? 0u : (uint32_t)e->argbytes))
    for (int k = 0; k < wl_c.n; k++) {
        WByte *w = &wl_c.b[k];
        if (DEAD(w->a)) continue;
        if (g_mem[w->a] != w->now) report("byte %06X: C wrote %02X, original left %02X", w->a, w->now, g_mem[w->a]);
    }
    for (int k = 0; k < wl_68k.n; k++) {
        WByte *w = &wl_68k.b[k];
        if (DEAD(w->a)) continue;
        WByte key = {w->a, 0, 0};
        if (bsearch(&key, wl_c.b, (size_t)wl_c.n, sizeof *wl_c.b, cmp_wbyte)) continue;   /* compared above */
        if (g_mem[w->a] != w->old) report("byte %06X: original wrote %02X (was %02X), C did not write it", w->a, g_mem[w->a], w->old);
    }
    #undef DEAD
    checked[pend.i]++;
    if (mismatches && fuzz_active && pend.i == fuzz_i) {
        char in[160]; int L = 0;
        for (int k = 0; k < 8; k++) if (fuzz_regs >> k & 1) L += snprintf(in + L, sizeof in - (size_t)L, " D%d=%08X", k, fuzz_in.d[k]);
        for (int k = 0; k < fuzz_nframe && L < 140; k++) L += snprintf(in + L, sizeof in - (size_t)L, " -%X(A%d)=%04X", fuzz_frame[k], fuzz_glob[k] ? 5 : 6, rd16(fuzz_addr(k, fuzz_entry.a[6])));
        hx_log("fuzz input:%s", in);
    }
    if (!mismatches && pend.i == fuzz_i && fuzz_total) { fuzz_next(&wl_68k); return; }
    if (mismatches) hx_fault("subcheck: %s differs from the original (%d differences, see log)", e->name, mismatches);
}

/* Called from the instruction hook for every instruction in a loaded segment. Returns 1 if a C
 * function ran (the PC is then at the caller's return address). */
int sub_check(int seg, uint32_t off, uint32_t pc) {
    if (!any_enabled || g_sub_pending) return 0;     /* while a check runs, the original runs untouched */
    for (int i = 0; g_subs[i].name; i++) {
        if (!enabled[i] || at_seg[i] != seg || at_off[i] != off) continue;
        g_sub_pc = pc;
        mz_local_reset();
        calls[i]++;
        if (g_sub_checkmode && g_subs[i].nocheck) return 0;   /* the original runs, so its callees are checked; the end-to-end run covers the C version */
        if (g_sub_checkmode) return run_checked(i, pc);
        /* C code may be nested (a VBL task run while C code idles): keep the outer abort point */
        jmp_buf outer; memcpy(outer, c_abort, sizeof outer);
        if (setjmp(c_abort)) { memcpy(c_abort, outer, sizeof outer); return 1; }   /* stopped at the tick limit while idling */
        g_subs[i].fn();
        memcpy(c_abort, outer, sizeof outer);
        uint32_t ret = pop32();          /* rts (a Pascal routine also pops its arguments) */
        setreg(M68K_REG_A7, reg(M68K_REG_A7) + (uint32_t)g_subs[i].pops);
        setreg(M68K_REG_PC, ret);
        return 1;
    }
    return 0;
}

void sub_report(void) {
    for (int i = 0; g_subs[i].name; i++) if (enabled[i]) {
        if (g_sub_checkmode && g_subs[i].nocheck) hx_log("subcheck: %s not checked per call (Toolbox state outside memory; the original ran %u times, the end-to-end run covers it)", g_subs[i].name, calls[i]);
        else if (g_sub_checkmode) hx_log("subcheck: %s matched the original on %u of %u calls%s%s", g_subs[i].name, checked[i], calls[i] - skipped[i],
                                         skipped[i] ? " (plus calls that load a segment or wait for a tick, not compared)" : "",
                                         g_subs[i].ignore ? " (some registers left by compiled callees not compared)" : "");
        else hx_log("substitution: %s ran %u times", g_subs[i].name, calls[i]);
    }
}

