/* Time and VBL tasks in the port. Same time model as the test rig (harness/vbl.c): a tick passes
 * only where the game waits; each tick runs the due VBL tasks and the sound channels' callbacks.
 * The tasks are the game's own routines, now C, so they are called directly instead of through an
 * interrupt stub. As in the rig, ticks that pass inside a trap are processed when the trap returns
 * (vbl_pending_ticks / rt_run_pending). */
#include "../harness/hx.h"
#include "m68k.h"
#include "../game/mzasm.h"
#include "../game/mz.h"
#include "port.h"
#include <string.h>

#define MAXTASK 16
#define SCRATCH 0x00FE0800u           /* sound callback commands (the rig's VBLSTUB + $800) */

void fn_S8_1814(Regs *r);
void fn_S8_47E0(Regs *r);
void fn_S4_42E6(uint32_t chan, uint32_t cmd);

static uint32_t tasks[MAXTASK]; static int ntasks;
static uint32_t ticks, pending;

uint32_t vbl_ticks(void) { return ticks; }
int vbl_pending_ticks(void) { return (int)pending; }
int vbl_in_irq(void) { return 0; }

void vbl_advance(uint32_t n) {
    if (!n) return;
    extern uint32_t g_max_ticks;
    if (g_max_ticks && ticks + n >= g_max_ticks) n = g_max_ticks - ticks;
    ticks += n; pending += n;
    wr32(LM_Ticks, ticks);
    { void input_tick_hook(uint32_t tick); input_tick_hook(ticks); }
    if (g_max_ticks && ticks >= g_max_ticks) port_quit();
}

static void install(uint32_t t) { for (int i = 0; i < ntasks; i++) if (tasks[i] == t) return; if (ntasks < MAXTASK) tasks[ntasks++] = t; }
static void removet(uint32_t t) {
    for (int i = 0; i < ntasks; i++) if (tasks[i] == t) { memmove(tasks + i, tasks + i + 1, sizeof tasks[0] * (size_t)(ntasks - i - 1)); ntasks--; return; }
}
void vbl_trap(uint8_t num) {
    uint32_t t = reg(M68K_REG_A0);
    if (num == 0x33 || num == 0x6F) install(t); else removet(t);
    setreg(M68K_REG_D0, 0);
}

/* the routine at a task's or callback's address: always one of the game's, reached through the jump table */
static void call_task(uint32_t addr, uint32_t task) {
    Regs r; memset(&r, 0, sizeof r);
    r.a[0] = task; r.a[5] = A5_ADDR; r.a[7] = reg(M68K_REG_A7);
    if (addr == mz_jt(0x72)) fn_S8_1814(&r);
    else if (addr == mz_jt(0x92)) fn_S8_47E0(&r);
    else hx_fault("VBL task at %06X: not a known routine", addr);
}

/* one tick's VBL processing, as the rig's interrupt does it: due tasks first, then sound callbacks */
static void one_tick(uint32_t tick) {
    uint32_t due[MAXTASK]; int nd = 0;
    for (int i = 0; i < ntasks; i++) {
        uint32_t t = tasks[i]; int16_t c = (int16_t)rd16(t + 10);
        if (c <= 0) continue;
        if (--c == 0) due[nd++] = t;
        wr16(t + 10, (uint16_t)c);
    }
    int snd_tick(uint32_t tick, uint32_t *proc, uint32_t *chanrec, uint32_t *cmdaddr, uint32_t scratch, int max);
    uint32_t cbp[32], cbc[32], cba[32];
    int ncb = snd_tick(tick, cbp, cbc, cba, SCRATCH, 32);
    for (int i = 0; i < nd; i++) call_task(rd32(due[i] + 6), due[i]);
    for (int i = 0; i < ncb; i++) {
        if (cbp[i] != mz_jt(0x1FA)) hx_fault("sound callback at %06X: not a known routine", cbp[i]);
        fn_S4_42E6(cbc[i], cba[i]);
    }
}

/* the pending ticks' processing, then the front end's turn (pacing, events, the screen) */
void vbl_run_pending(void) {
    while (pending) {
        uint32_t t = ticks - pending; pending--;
        uint32_t sp = reg(M68K_REG_A7);
        one_tick(t);
        setreg(M68K_REG_A7, sp);
        port_tick(t + 1);
    }
}
