/* The port without a window: runs the game from a test script for a number of ticks and writes the
 * same files as the test rig (a5world.bin, heap.bin, screen.ppm, sound.wav), so a port run can be
 * compared with the original run under the rig. */
#include "../harness/hx.h"
#include "port.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

int g_idle_limit = 300;
static jmp_buf done;
static int fatal;
static uint32_t quit_at;   /* -quit N: the window's close box at tick N (the 'quit' Apple event) */
void shell_request_quit(void);
void shell_choose(int menu, int item);
static struct { uint32_t tick; int menu, item; } cmds[32]; static int ncmds;   /* -menu tick:menu:item */
void port_tick(uint32_t tick) {
    if (quit_at && tick == quit_at) shell_request_quit();
    for (int k = 0; k < ncmds; k++) if (cmds[k].tick == tick) shell_choose(cmds[k].menu, cmds[k].item);
}
void port_quit(void) { longjmp(done, 1); }
void port_fatal(const char *msg) { fprintf(stderr, "fatal: %s\n", msg); fatal = 1; longjmp(done, 1); }

int input_load(const char *path);
void qd_dump_screen(const char *path);
void snd_write_wav(const char *path);

int main(int argc, char **argv) {
    extern uint32_t g_max_ticks; extern const char *g_outdir;
    const char *script = NULL, *prefs = NULL; int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-ticks") && i + 1 < argc) g_max_ticks = (uint32_t)strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "-script") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "-idle") && i + 1 < argc) g_idle_limit = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) g_outdir = argv[++i];
        else if (!strcmp(argv[i], "-prefs") && i + 1 < argc) prefs = argv[++i];
        else if (!strcmp(argv[i], "-menu") && i + 1 < argc && ncmds < 32) {
            unsigned t; int m, it;
            if (sscanf(argv[++i], "%u:%d:%d", &t, &m, &it) == 3) { cmds[ncmds].tick = t; cmds[ncmds].menu = m; cmds[ncmds].item = it; ncmds++; }
        }
        else if (!strcmp(argv[i], "-quit") && i + 1 < argc) quit_at = (uint32_t)strtoul(argv[++i], 0, 0);
        else break;
    }
    if (i >= argc) { fprintf(stderr, "usage: mzport-headless [-ticks N] [-script file] [-o outdir] <archive>\n"); return 2; }
    char lp[512], err[256]; snprintf(lp, sizeof lp, "%s/log.txt", g_outdir);
    if (port_boot(argv[i], lp, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 1; }
    if (script && input_load(script)) { fprintf(stderr, "cannot load script %s\n", script); return 1; }
    { extern int input_live; if (quit_at || ncmds) input_live = 1; }
    if (prefs) port_prefs_load(prefs);
    if (!setjmp(done)) port_run_game();
    if (prefs && !fatal) port_prefs_save(prefs);
    char p[512];
    uint32_t base = rd32(LM_CurStackBase);
    snprintf(p, sizeof p, "%s/a5world.bin", g_outdir);
    FILE *f = fopen(p, "wb"); if (f) { fwrite(g_mem + base, 1, A5_ADDR - base, f); fclose(f); }
    snprintf(p, sizeof p, "%s/heap.bin", g_outdir);
    f = fopen(p, "wb"); if (f) { fwrite(g_mem + HEAP_BASE, 1, HEAP_END - HEAP_BASE, f); fclose(f); }
    snprintf(p, sizeof p, "%s/screen.ppm", g_outdir); qd_dump_screen(p);
    snprintf(p, sizeof p, "%s/sound.wav", g_outdir); snd_write_wav(p);
    fprintf(stderr, "stopped at tick %u%s\n", vbl_ticks(), fatal ? " (fatal)" : "");
    return fatal;
}
