/* The autopilot (mzharness -auto <goals>): plays Mazeworld Original by reading its state each tick
 * and moving the mouse as a player would; what it does is recorded as an ordinary test script
 * (<outdir>/auto.txt, input.c), so it replays like a recorded game. A way to reach creatures and
 * conversations random play does not. Run it with -script <prefix> (the clicks that start a game)
 * and -ticks; the test is the prefix followed by auto.txt.
 * Goals, one a line, done in order (# comments):
 *   at T            wait for tick T
 *   wait N          N ticks doing nothing
 *   goto X Y        walk to map point X, Y (pixels; a cell is $10), by the map's free cells
 *   near R D        walk to record R (of G_53C8) until within D
 *   face R          turn to record R
 *   fire N          use ability N (1-4: the mouse-button gesture to that corner)
 *   until R S T     wait (at most T ticks) for record R to be in state S
 *   shoot R N T     face record R and use ability N again and again until it is gone (type
 *                   negative) or T ticks have passed
 *   setg G V        set the word global G_<G> (hex) to V, as a test's setup (written to the test)
 *   setr R O V      set the word at offset O (hex) of record R to V, likewise
 *   dump            log the map, the player and the records */
#include "hx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern int input_live;
extern void (*input_play_hook)(uint32_t tick);
int  input_record(const char *path);
void input_live_move(int dx, int dy);
void input_live_button(int down);

#define G16(o) rd16(A5_ADDR - (o))
#define G32(o) rd32(A5_ADDR - (o))
static uint32_t rec_at(int n) { return G32(0x53C8) + (uint32_t)n * 0x40; }
static int dir_from(int n) {                               /* fn_S7_12BE's G_3830: the G_53C0 table */
    uint32_t o = rec_at(n);
    int dx = (int16_t)G16(0x56D8) - (int16_t)rd16(o + 4), dy = (int16_t)G16(0x56D4) - (int16_t)rd16(o + 8);
    return rd8(G32(0x53C0) + (uint32_t)(((dy & 0x7F) << 7) | (dx & 0x7F)));
}
/* the game's measure of the player's distance from record n (fn_S7_12BE: the G_53BC table by the
 * offset; -1 beyond 63 either way) */
static int dist_to(int n) {
    uint32_t o = rec_at(n);
    int dx = (int16_t)G16(0x56D8) - (int16_t)rd16(o + 4), dy = (int16_t)G16(0x56D4) - (int16_t)rd16(o + 8);
    if (dx + 0x3F < 0 || dx + 0x3F >= 0x7E || dy + 0x3F < 0 || dy + 0x3F >= 0x7E) return -1;
    return rd8(G32(0x53BC) + (uint32_t)(((dy & 0x7F) << 7) | (dx & 0x7F)));
}

typedef struct { char op[8]; int a, b, c; } Goal;
static Goal goals[512]; static int ngoals, cur;
static uint32_t started;   /* when the current goal began */

static uint8_t bad[1024];          /* cells found blocked by something not in the map (cleared each goal) */
static int blocked(int col, int row) {
    if (col < 0 || row < 0 || col > 31 || row > 31) return 1;
    if (bad[row * 32 + col]) return 1;
    return rd8(G32(0x53D4) + (uint32_t)((row << 10) | (col << 5))) & 1;
}
/* the next cell's centre on the shortest way from the player's cell to (tx, ty); 0 if none */
static int next_point(int tx, int ty, int *nx, int *ny) {
    int pc = G16(0x56D8) >> 4, pr = G16(0x56D4) >> 4, tc = tx >> 4, tr = ty >> 4;
    if (pc == tc && pr == tr) { *nx = tx; *ny = ty; return 1; }
    static int16_t from[1024]; int q[1024], h = 0, t = 0;
    memset(from, -1, sizeof from);
    from[tr * 32 + tc] = (int16_t)(tr * 32 + tc); q[t++] = tr * 32 + tc;   /* from the target back */
    while (h < t) {
        int c = q[h++], col = c & 31, row = c >> 5;
        static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++) {
            int nc = col + d[k][0], nr = row + d[k][1], n = nr * 32 + nc;
            if (nc < 0 || nr < 0 || nc > 31 || nr > 31 || from[n] >= 0) continue;
            if (blocked(nc, nr) && !(nc == pc && nr == pr)) continue;
            from[n] = (int16_t)c; q[t++] = n;
            if (nc == pc && nr == pr) { int s = from[n]; *nx = ((s & 31) << 4) + 8; *ny = ((s >> 5) << 4) + 8; return 1; }
        }
    }
    return 0;
}
/* the heading G_56DA facing (x, y) from the player (0-63, clockwise on the map: 0 west, $10 north,
 * $20 east, $30 south). The game reads the mouse once a frame (4 ticks); a tick's movement of 3 is
 * one turn (leftward movement turns clockwise) or one step forward (upward movement). */
static int heading_to(int x, int y) {
    double a = atan2((double)(y - (int16_t)G16(0x56D4)), (double)(x - (int16_t)G16(0x56D8)));
    return ((int)lround(a / (2 * M_PI) * 64) + 0x20) & 63;
}
static int turn_to(int h) {                                /* a turn step if not facing h; 1 when facing */
    int d = (h - (G16(0x56DA) & 63)) & 63;
    if (d <= 1 || d >= 63) return 1;
    input_live_move(d < 32 ? -3 : 3, 0);
    return 0;
}
static int walk_to(int x, int y, int within) {             /* 1 there */
    int px = (int16_t)G16(0x56D8), py = (int16_t)G16(0x56D4);
    if (abs(px - x) <= within && abs(py - y) <= within) return 1;
    int nx, ny;
    static int lastx, lasty, still;
    if (!next_point(x, y, &nx, &ny)) return 1;
    if (!turn_to(heading_to(nx, ny))) { still = 0; return 0; }
    input_live_move(0, -3);
    if (px == lastx && py == lasty) {
        if (++still > 24 && !((nx >> 4) == (x >> 4) && (ny >> 4) == (y >> 4))) {   /* stuck: avoid that cell */
            bad[(ny >> 4) * 32 + (nx >> 4)] = 1; still = 0;
            hx_log("auto: cell %d,%d blocked", nx >> 4, ny >> 4);
        }
    } else still = 0;
    lastx = px; lasty = py;
    return 0;
}
static void dump(void) {
    char line[40];
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) line[col] = blocked(col, row) ? '#' : '.';
        line[32] = 0;
        if (row == (G16(0x56D4) >> 4)) line[G16(0x56D8) >> 4] = '@';
        hx_log("auto map %2d %s", row, line);
    }
    for (int col = 6; col < 12; col++) {
        uint32_t c = G32(0x53D4) + (uint32_t)((2 << 10) | (col << 5));
        char b[100]; int n = 0;
        for (int k = 0; k < 32; k += 2) n += sprintf(b + n, "%04X ", rd16(c + k));
        hx_log("auto cell %d,2: %s", col, b);
    }
    hx_log("auto player x %d y %d heading %d level %d life %d", (int16_t)G16(0x56D8), (int16_t)G16(0x56D4), G16(0x56DA), G16(0x56DC), G16(0x56EA));
    for (int n = 0; n < 0x78; n++) {
        uint32_t o = rec_at(n);
        if ((rd16(o + 2) & 0x8000 || !rd16(o + 2)) && !(n == 0x36 || n == 0x37)) continue;
        hx_log("auto rec %d at %X type %X x %d y %d state %d", n, o, rd16(o + 2), (int16_t)rd16(o + 4), (int16_t)rd16(o + 8), rd16(o + 0x16));
    }
}
/* an ability by the mouse-button gesture: down, 10 still ticks, 6 moves to that corner of the mood
 * square (1 up-left, 2 up-right, 3 down-left, 4 down-right), up; 1 when it is over */
static int fire(int n, uint32_t s) {
    int dx = (n == 2 || n == 4) ? 30 : -30, dy = n >= 3 ? 30 : -30;
    if (s == 0) input_live_button(1);
    else if (s >= 10 && s < 16) input_live_move(dx, dy);  /* once the game has taken the press (frames of 4 ticks) */
    else if (s == 18) input_live_button(0);
    return s >= 22;
}
static void auto_tick(uint32_t tick) {
    if (getenv("AUTO_TRACE") && cur < ngoals && tick % (uint32_t)atoi(getenv("AUTO_TRACE")) == 0) hx_log("auto trace %u x %d y %d h %d life %d full %d george %d snails %d %d %d dist %d 56C2 %X say %X dir %d h %d", tick, (int16_t)G16(0x56D8), (int16_t)G16(0x56D4), G16(0x56DA), G16(0x56EA), G16(0x56E0), rd16(rec_at(1) + 0x16), rd16(rec_at(2) + 0x16), rd16(rec_at(3) + 0x16), rd16(rec_at(4) + 0x16), dist_to(2), G16(0x56C2), G16(0x56BE), dir_from(2), G16(0x56DA));
    while (cur < ngoals) {
        Goal *g = &goals[cur];
        if (!started) started = tick;
        int done = 1;
        if (!strcmp(g->op, "at")) done = tick >= (uint32_t)g->a;
        else if (!strcmp(g->op, "wait")) done = tick - started >= (uint32_t)g->a;
        else if (!strcmp(g->op, "dump")) dump();
        else if (!strcmp(g->op, "setr")) {
            FILE *input_rec_file(void);
            wr16(rec_at(g->a) + (uint32_t)g->b, (uint16_t)g->c);
            fprintf(input_rec_file(), "%u setr %d %X %u\n", tick, g->a, g->b, (unsigned)(uint16_t)g->c);
        }
        else if (!strcmp(g->op, "setg")) {
            FILE *input_rec_file(void);
            wr16(A5_ADDR - (uint32_t)g->a, (uint16_t)g->b);
            fprintf(input_rec_file(), "%u setg %X %u\n", tick, g->a, (unsigned)(uint16_t)g->b);
        }
        else if (!strcmp(g->op, "goto")) done = walk_to(g->a, g->b, 4);
        else if (!strcmp(g->op, "near")) { uint32_t o = rec_at(g->a); done = walk_to((int16_t)rd16(o + 4), (int16_t)rd16(o + 8), g->b); }
        else if (!strcmp(g->op, "face")) { uint32_t o = rec_at(g->a); done = turn_to(heading_to((int16_t)rd16(o + 4), (int16_t)rd16(o + 8))); }
        else if (!strcmp(g->op, "until")) done = rd16(rec_at(g->a) + 0x16) == g->b || tick - started >= (uint32_t)g->c;
        else if (!strcmp(g->op, "fire")) done = fire(g->a, tick - started);
        else if (!strcmp(g->op, "shoot")) {
            static uint32_t from;
            uint32_t o = rec_at(g->a);
            done = (rd16(o + 2) & 0x8000) || tick - started >= (uint32_t)g->c;
            if (!done) {
                if (!from) { uint32_t s0 = tick; if (turn_to(heading_to((int16_t)rd16(o + 4), (int16_t)rd16(o + 8)))) from = s0; }
                else if (fire(g->b, tick - from)) from = 0;
            } else from = 0;
        }
        if (!done) return;
        hx_log("auto: goal %d (%s %d %d) done at tick %u", cur + 1, g->op, g->a, g->b, tick);
        cur++; started = 0; memset(bad, 0, sizeof bad);
        if (strcmp(goals[cur - 1].op, "dump") && strcmp(goals[cur - 1].op, "at") && strcmp(goals[cur - 1].op, "setg") && strcmp(goals[cur - 1].op, "setr")) return;   /* one action a tick */
    }
}
int auto_start(const char *goalfile, const char *outdir) {
    FILE *f = fopen(goalfile, "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", goalfile); return -1; }
    char line[256];
    while (fgets(line, sizeof line, f) && ngoals < 512) {
        Goal g = {{0}, 0, 0, 0};
        if (line[0] == '#' || sscanf(line, "%7s", g.op) < 1) continue;
        sscanf(line, !strcmp(g.op, "setg") ? "%*s %x %d" : !strcmp(g.op, "setr") ? "%*s %d %x %d" : "%*s %d %d %d", &g.a, &g.b, &g.c);
        goals[ngoals++] = g;
    }
    fclose(f);
    char p[512]; snprintf(p, sizeof p, "%s/auto.txt", outdir);
    if (input_record(p)) { fprintf(stderr, "cannot write %s\n", p); return -1; }
    input_live = 1;
    input_play_hook = auto_tick;
    return 0;
}
