/* Time and VBL tasks.
 *
 * Time model: an infinitely fast Mac. Ticks advance only when the game is provably waiting:
 *  - a spin loop whose registers and memory are unchanged between iterations (it can only be
 *    released by an interrupt),
 *  - WaitNextEvent with a non-zero sleep, Delay, or repeated TickCount polling.
 * Each tick decrements every installed VBL task's vblCount and runs the tasks that reach zero,
 * as a level-1 interrupt (system VBL and slot VBL both modelled at one call per tick).
 * The native port's test mode uses the same rule, so both sides see identical timing.
 * FIDELITY: real Macs run the slot VBL at the monitor's refresh rate (66.7 Hz on a 13" display). */
#include "hx.h"
#include "m68k.h"
#include <string.h>
#include <stdlib.h>

#define VBLSTUB   0x00FE0000u
#define MAXTASK   16

static uint32_t tasks[MAXTASK]; static int ntasks;
static uint32_t ticks;            /* current tick count */
static uint32_t pending;          /* ticks whose VBL processing has not run yet */
static int      in_vbl;

uint32_t vbl_ticks(void) { return ticks; }
int vbl_pending_ticks(void) { return (int)pending; }

void vbl_advance(uint32_t n) {
    if (!n) return;
    if (getenv("MZ_VBLLOG")) hx_log("vbl: advance %u at tick %u", n, ticks);
    extern uint32_t g_max_ticks;
    if (g_max_ticks && ticks + n >= g_max_ticks) n = g_max_ticks - ticks;
    ticks += n; pending += n;
    wr32(LM_Ticks, ticks);
    { void input_tick_hook(uint32_t tick); input_tick_hook(ticks); }
    if (g_max_ticks && ticks >= g_max_ticks && !g_stop) {
        /* stop exactly when tick N begins: a fixed point in game time, independent of instruction counts */
        g_stop = 1; snprintf(g_stop_reason, sizeof g_stop_reason, "tick limit %u reached", g_max_ticks);
        m68k_end_timeslice();
    }
}

static void install(uint32_t task) {
    for (int i = 0; i < ntasks; i++) if (tasks[i] == task) return;
    if (ntasks < MAXTASK) tasks[ntasks++] = task;
}
static void removet(uint32_t task) {
    for (int i = 0; i < ntasks; i++) if (tasks[i] == task) { memmove(tasks + i, tasks + i + 1, sizeof tasks[0] * (size_t)(ntasks - i - 1)); ntasks--; return; }
}

/* OS traps: VInstall A033, VRemove A034, SlotVInstall A06F, SlotVRemove A070 (A0 = task) */
void vbl_trap(uint8_t num) {
    uint32_t t = reg(M68K_REG_A0);
    if (num == 0x33 || num == 0x6F) { install(t); hx_log("VBL task %06X installed (vblAddr %06X count %d)%s", t, rd32(t + 6), (int16_t)rd16(t + 10), num == 0x6F ? " [slot]" : ""); }
    else removet(t);
    setreg(M68K_REG_D0, 0);
}

int vbl_in_irq(void) { return in_vbl; }
int vbl_is_stub(uint32_t pc) { return pc >= VBLSTUB && pc < VBLSTUB + 0x800; }

/* Called from the instruction hook before executing pc. Delivers one pending tick as an interrupt
 * when the interrupt mask allows it. Returns 1 if the PC was redirected. */
int vbl_maybe_interrupt(uint32_t pc) {
    if (!pending || in_vbl) return 0;
    uint32_t sr = reg(M68K_REG_SR);
    if (((sr >> 8) & 7) >= 1) return 0;          /* VBL is level 1 */
    pending--;
    if (getenv("MZ_VBLLOG")) { uint32_t o = 0; int sn = seg_of(pc, &o); hx_log("vbl: tick %u delivered at S%d:%04X (pending left %u)", ticks - pending, sn, o, pending); }
    /* decrement counts, collect due tasks */
    uint32_t due[MAXTASK]; int nd = 0;
    for (int i = 0; i < ntasks; i++) {
        uint32_t t = tasks[i]; int16_t c = (int16_t)rd16(t + 10);
        if (c <= 0) continue;                    /* inactive until the task re-arms itself */
        if (--c == 0) due[nd++] = t;
        wr16(t + 10, (uint16_t)c);
    }
    /* sound channels advance on the same tick; their callbacks run in the same interrupt */
    int snd_tick(uint32_t tick, uint32_t *proc, uint32_t *chanrec, uint32_t *cmdaddr, uint32_t scratch, int max);
    uint32_t cbp[32], cbc[32], cba[32];
    int ncb = snd_tick(ticks - pending - 1, cbp, cbc, cba, VBLSTUB + 0x800, 32);
    if (!nd && !ncb) return 0;
    /* interrupt stub: save d0-d3/a0-a3; for each due task: A0 = task, call vblAddr;
     * for each sound callback: userRoutine(chan, VAR cmd) (Pascal: callee pops); restore; RTE */
    uint32_t p = VBLSTUB;
    wr16(p, 0x48E7); wr16(p + 2, 0xF0F0); p += 4;               /* movem.l d0-d3/a0-a3,-(a7) */
    for (int i = 0; i < nd; i++) {
        wr16(p, 0x41F9); wr32(p + 2, due[i]); p += 6;            /* lea task.l,a0 */
        wr16(p, 0x2268); wr16(p + 2, 0x0006); p += 4;            /* movea.l 6(a0),a1 */
        wr16(p, 0x4E91); p += 2;                                  /* jsr (a1) */
    }
    for (int i = 0; i < ncb; i++) {
        wr16(p, 0x4879); wr32(p + 2, cbc[i]); p += 6;            /* pea chan.l */
        wr16(p, 0x4879); wr32(p + 2, cba[i]); p += 6;            /* pea cmd.l */
        wr16(p, 0x4EB9); wr32(p + 2, cbp[i]); p += 6;            /* jsr userRoutine.l */
    }
    wr16(p, 0x4CDF); wr16(p + 2, 0x0F0F); p += 4;                /* movem.l (a7)+,d0-d3/a0-a3 */
    wr16(p, 0x4E71); p += 2;                                      /* nop: end-of-VBL marker for the hook */
    wr16(p, 0x4E73);                                              /* rte */
    /* push a 68020 format-0 interrupt frame for autovector level 1 */
    push16(0x0064); push32(pc); push16((uint16_t)sr);
    setreg(M68K_REG_SR, (sr & ~0x0700u) | 0x2100u);
    setreg(M68K_REG_PC, VBLSTUB);
    in_vbl = 1;
    return 1;
}

void vbl_stub_hook(uint32_t pc) {
    /* the nop before RTE marks the end of interrupt processing */
    if (rd16(pc) == 0x4E71 && rd16(pc + 2) == 0x4E73) in_vbl = 0;
}

/* ---------- spin detection ---------- */
static uint32_t spin_pc, spin_regs[16]; static int spin_n;
extern int g_mem_written;          /* set by the CPU write callbacks */

void vbl_spin_check(uint32_t pc, uint32_t prev_pc) {
    if (!(pc < prev_pc && prev_pc - pc < 64)) return;          /* only backward branches of short loops */
    uint32_t r[16];
    for (int i = 0; i < 16; i++) r[i] = reg(M68K_REG_D0 + i);
    if (pc == spin_pc && !g_mem_written && !memcmp(r, spin_regs, sizeof r)) {
        if (++spin_n >= 2) { vbl_advance(1); spin_n = 0; }
    } else { spin_pc = pc; memcpy(spin_regs, r, sizeof r); spin_n = 0; }
    g_mem_written = 0;
}

/* C game code idling where the original spins until an interrupt changes something (mz_idle):
 * one tick passes, and its VBL processing runs now, as a nested interrupt that returns to IDLERET.
 * The CPU registers are put back afterwards (the C code holds its own state). */
int g_idle_done;
void vbl_run_pending(void);
void vbl_idle_tick(void) {
    vbl_advance(1);
    if (g_stop) return;
    vbl_run_pending();
}
/* delivers the pending tick's VBL processing now, as a nested interrupt (see vbl_idle_tick) */
void vbl_run_pending(void) {
    uint32_t r[16], sr = reg(M68K_REG_SR), pc = reg(M68K_REG_PC);
    for (int i = 0; i < 16; i++) r[i] = reg(M68K_REG_D0 + i);
    setreg(M68K_REG_SR, sr & ~0x0700u);          /* the original spins with interrupts enabled */
    if (vbl_maybe_interrupt(IDLERET)) {
        g_idle_done = 0;
        while (!g_idle_done && !g_stop) m68k_execute(100000);
    }
    for (int i = 0; i < 16; i++) setreg(M68K_REG_D0 + i, r[i]);
    setreg(M68K_REG_SR, sr); setreg(M68K_REG_PC, pc);
}
