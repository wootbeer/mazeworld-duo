/* Scripted input for deterministic runs.
 * Script lines:  <tick> down <key>   |   <tick> up <key>   |   <tick> tap <key>   (# comments)
 *                <tick> type <text>   (taps each character, one every 4 ticks; letters, digits, space)
 *                <tick> setg <global> <value>   |   at S<seg>:<off> setg <global> <value>   (test setup)
 *                <tick> move x y   |   <tick> click x y   |   <tick> mousedown   |   <tick> mouseup
 *                <tick> menu <menu> <item>   (a menu choice: a click on the menu bar, MenuSelect answers it)
 *                <tick> key <code> <char> down|up|auto   (a key by Mac key code and character, hex: recordings)
 *                <tick> mmove dx dy   (the mouse moved by dx, dy: as the mouse driver, MTemp and RawMouse move)
 * Keys: space return enter escape tab delete shift kp0..kp9 a..z 0..9 left right up down
 * A key going down posts a keyDown event and sets its KeyMap bit; up posts keyUp and clears it. */
#include "hx.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct { uint32_t tick; int down; uint8_t code; uint8_t ch; int kind; int16_t x, y; } Step;   /* kind 0 key, 1 mouse button, 2 mouse move, 3 relative move */
static int16_t mouse_x = 320, mouse_y = 240; static int mouse_down;
static Step steps[4096];
void input_live_move(int dx, int dy);
void input_live_menu(int menu, int item);
void input_live_key(uint8_t code, uint8_t ch, int down, int repeat);
static int nsteps, next_step;

typedef struct { uint16_t what; uint32_t message; uint32_t when; uint16_t modifiers; uint32_t where; } Ev;
static Ev q[64]; static int qh, qt;

static const struct { const char *n; uint8_t code; uint8_t ch; } keys[] = {
    {"space", 0x31, ' '}, {"return", 0x24, '\r'}, {"enter", 0x4C, 0x03}, {"escape", 0x35, 0x1B}, {"shift", 0x38, 0},
    {"tab", 0x30, '\t'}, {"delete", 0x33, 0x08}, {"cmd", 0x37, 0}, {"slash", 0x2C, '/'}, {"period", 0x2F, '.'},
    {"kp0", 0x52, '0'}, {"kp1", 0x53, '1'}, {"kp2", 0x54, '2'}, {"kp3", 0x55, '3'}, {"kp4", 0x56, '4'},
    {"kp5", 0x57, '5'}, {"kp6", 0x58, '6'}, {"kp7", 0x59, '7'}, {"kp8", 0x5B, '8'}, {"kp9", 0x5C, '9'},
    {"left", 0x7B, 0x1C}, {"right", 0x7C, 0x1D}, {"down", 0x7D, 0x1F}, {"up", 0x7E, 0x1E},
    {"a", 0x00, 'a'}, {"s", 0x01, 's'}, {"d", 0x02, 'd'}, {"f", 0x03, 'f'}, {"h", 0x04, 'h'}, {"g", 0x05, 'g'},
    {"z", 0x06, 'z'}, {"x", 0x07, 'x'}, {"c", 0x08, 'c'}, {"v", 0x09, 'v'}, {"b", 0x0B, 'b'}, {"q", 0x0C, 'q'},
    {"w", 0x0D, 'w'}, {"e", 0x0E, 'e'}, {"r", 0x0F, 'r'}, {"y", 0x10, 'y'}, {"t", 0x11, 't'}, {"o", 0x1F, 'o'},
    {"u", 0x20, 'u'}, {"i", 0x22, 'i'}, {"p", 0x23, 'p'}, {"l", 0x25, 'l'}, {"j", 0x26, 'j'}, {"k", 0x28, 'k'},
    {"n", 0x2D, 'n'}, {"m", 0x2E, 'm'},
    {"1", 0x12, '1'}, {"2", 0x13, '2'}, {"3", 0x14, '3'}, {"4", 0x15, '4'}, {"5", 0x17, '5'}, {"6", 0x16, '6'},
    {"7", 0x1A, '7'}, {"8", 0x1C, '8'}, {"9", 0x19, '9'}, {"0", 0x1D, '0'},
};

/* "at S<seg>:<off> setg <global> <value>": the first time execution reaches that code address, the
 * word global G_<global> (hex, below A5) is set to <value> (a test setup, e.g. the starting level) */
typedef struct { int seg; uint32_t off, g; uint16_t v; int done; } PcPoke;
static PcPoke pokes[16]; static int npokes;
void input_pc_hook(int seg, uint32_t off) {
    for (int i = 0; i < npokes; i++)
        if (!pokes[i].done && pokes[i].seg == seg && pokes[i].off == off) {
            pokes[i].done = 1;
            wr16(A5_ADDR - pokes[i].g, pokes[i].v);
            hx_log("input script: G_%04X = %u at S%d:%04X", pokes[i].g, pokes[i].v, seg, off);
        }
}
int input_has_pc_hooks(void) { return npokes > 0; }

/* "<tick> setg <global> <value>": the same when tick <tick> begins (independent of which code,
 * original or C, is running then) */
typedef struct { uint32_t tick, g; uint16_t v; int done; } TickPoke;
static TickPoke tpokes[16]; static int ntpokes;
/* called as each tick begins: the script's pokes and steps due by then are applied (so input
 * reaches the game at the same point whether replayed or played live), then the live front end's
 * turn (input_play_hook: pacing, the window, the real keyboard and mouse) */
void (*input_play_hook)(uint32_t tick);
static FILE *rec;   /* recording live input (input_record) */
void input_poll(uint32_t tick);
void input_tick_hook(uint32_t tick) {
    for (int i = 0; i < ntpokes; i++)
        if (!tpokes[i].done && tick >= tpokes[i].tick) {
            tpokes[i].done = 1;
            wr16(A5_ADDR - tpokes[i].g, tpokes[i].v);
            hx_log("input script: G_%04X = %u at tick %u", tpokes[i].g, tpokes[i].v, tick);
        }
    input_poll(tick);
    if (input_play_hook) input_play_hook(tick);
    if (rec) fflush(rec);
}

/* recording live input as a script (the playable test build): each live input is written with
 * the tick it arrived on, so the script replays the session exactly */
int input_record(const char *path) { rec = fopen(path, "w"); if (rec) fprintf(rec, "# recorded session\n"); return rec ? 0 : -1; }
static uint32_t now_tick(void) { return rd32(LM_Ticks); }
void input_record_end(uint32_t ticks) { if (rec) { fprintf(rec, "# ends: replay with -ticks %u\n", ticks); fflush(rec); } }

int input_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *h = strchr(line, '#'); if (h) *h = 0;
        unsigned t; char act[16], key[16]; int mx, my;
        { int sg; unsigned off, g, v;
          if (sscanf(line, " at S%d:%x setg %x %u", &sg, &off, &g, &v) == 4) {
              if (npokes < 16) pokes[npokes++] = (PcPoke){sg, off, g, (uint16_t)v, 0};
              continue;
          }
          unsigned tk;
          if (sscanf(line, " %u setg %x %u", &tk, &g, &v) == 3) {
              if (ntpokes < 16) tpokes[ntpokes++] = (TickPoke){tk, g, (uint16_t)v, 0};
              continue;
          } }
        { unsigned kc, kch; char how[8];
          if (sscanf(line, "%u key %x %x %7s", &t, &kc, &kch, how) == 4) {
              if (nsteps < 4096) steps[nsteps++] = (Step){t, strcmp(how, "up") != 0, (uint8_t)kc, (uint8_t)kch, !strcmp(how, "auto") ? 6 : 5, 0, 0};
              continue;
          } }
        if (sscanf(line, "%u %15s %d %d", &t, act, &mx, &my) == 4 && !strcmp(act, "menu")) {
            if (nsteps < 4096) steps[nsteps++] = (Step){t, 0, 0, 0, 4, (int16_t)mx, (int16_t)my};
            continue;
        }
        if (sscanf(line, "%u %15s %d %d", &t, act, &mx, &my) == 4 && !strcmp(act, "mmove")) {
            if (nsteps < 4096) steps[nsteps++] = (Step){t, 0, 0, 0, 3, (int16_t)mx, (int16_t)my};
            continue;
        }
        if (sscanf(line, "%u %15s %d %d", &t, act, &mx, &my) == 4 && (!strcmp(act, "move") || !strcmp(act, "click"))) {
            if (nsteps + 3 > 4096) break;
            steps[nsteps++] = (Step){t, 0, 0, 0, 2, (int16_t)mx, (int16_t)my};
            if (!strcmp(act, "click")) { steps[nsteps++] = (Step){t, 1, 0, 0, 1, 0, 0}; steps[nsteps++] = (Step){t + 2, 0, 0, 0, 1, 0, 0}; }
            continue;
        }
        if (sscanf(line, "%u %15s", &t, act) == 2 && !strcmp(act, "type")) {
            char *p = strstr(line, "type") + 4; while (*p == ' ') p++;
            for (; *p && *p != '\n' && *p != '\r' && nsteps + 2 <= 4096; p++, t += 4) {
                char lc[2] = {(char)tolower((unsigned char)*p), 0};
                int k = -1;
                for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) if (!strcmp(keys[i].n, *p == ' ' ? "space" : lc)) k = (int)i;
                if (k < 0) { fprintf(stderr, "input script: cannot type '%c'\n", *p); fclose(f); return -1; }
                steps[nsteps++] = (Step){t, 1, keys[k].code, (uint8_t)*p, 0, 0, 0};
                steps[nsteps++] = (Step){t + 2, 0, keys[k].code, (uint8_t)*p, 0, 0, 0};
            }
            continue;
        }
        if (sscanf(line, "%u %15s", &t, act) == 2 && (!strcmp(act, "mousedown") || !strcmp(act, "mouseup"))) {
            if (nsteps < 4096) steps[nsteps++] = (Step){t, !strcmp(act, "mousedown"), 0, 0, 1, 0, 0};
            continue;
        }
        if (sscanf(line, "%u %15s %15s", &t, act, key) != 3) continue;
        int k = -1;
        for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) if (!strcmp(keys[i].n, key)) k = (int)i;
        if (k < 0) { fprintf(stderr, "input script: unknown key '%s'\n", key); fclose(f); return -1; }
        int n = !strcmp(act, "tap") ? 2 : 1;
        for (int j = 0; j < n && nsteps < 4096; j++) {
            Step *s = &steps[nsteps++];
            s->tick = t + (uint32_t)j * 2; s->code = keys[k].code; s->ch = keys[k].ch; s->kind = 0;
            s->down = !strcmp(act, "down") || (n == 2 && j == 0);
        }
    }
    fclose(f);
    /* keep script order stable by tick */
    for (int i = 1; i < nsteps; i++) for (int j = i; j > 0 && steps[j - 1].tick > steps[j].tick; j--) { Step x = steps[j]; steps[j] = steps[j - 1]; steps[j - 1] = x; }
    hx_log("input script %s: %d key steps", path, nsteps);
    return 0;
}

static void set_mouse_lowmem(void) {
    uint32_t pt = (uint32_t)(uint16_t)mouse_y << 16 | (uint16_t)mouse_x;
    wr32(0x0828, pt); wr32(0x082C, pt); wr32(0x0830, pt);   /* MTemp, RawMouse, Mouse */
    wr8(0x0172, mouse_down ? 0x00 : 0x80);                   /* MBState: bit 7 set = up */
}
int input_button(void) { return mouse_down; }
void input_mouse(int16_t *x, int16_t *y) { *x = mouse_x; *y = mouse_y; }
static uint16_t modifiers(void) {
    uint16_t m = mouse_down ? 0x0000 : 0x0080;   /* btnState: set when the button is up */
    static const struct { uint8_t code; uint16_t bit; } mk[] = {
        {0x37, 0x0100}, {0x38, 0x0200}, {0x39, 0x0400}, {0x3A, 0x0800}, {0x3B, 0x1000},   /* cmd shift caps option control */
    };
    for (unsigned i = 0; i < sizeof mk / sizeof mk[0]; i++)
        if (rd8(LM_KeyMap + (mk[i].code >> 3)) & (1 << (mk[i].code & 7))) m |= mk[i].bit;
    return m;
}

/* Called from the trap layer with the current tick: applies due script steps. */
void input_poll(uint32_t tick) {
    while (next_step < nsteps && steps[next_step].tick <= tick) {
        Step *s = &steps[next_step++];
        if (s->kind == 2) { mouse_x = s->x; mouse_y = s->y; set_mouse_lowmem(); hx_log("input: tick %u mouse at (%d,%d)", tick, s->x, s->y); continue; }
        if (s->kind == 5 || s->kind == 6) { int r = rec != NULL; FILE *k = rec; rec = NULL; input_live_key(s->code, s->ch, s->down, s->kind == 6); rec = r ? k : NULL; continue; }
        if (s->kind == 4) { input_live_menu(s->x, s->y); hx_log("input: tick %u menu %d item %d", tick, s->x, s->y); continue; }
        if (s->kind == 3) { input_live_move(s->x, s->y); hx_log("input: tick %u mouse moved by (%d,%d)", tick, s->x, s->y); continue; }
        if (s->kind == 1) {
            mouse_down = s->down; set_mouse_lowmem();
            int n = (qt + 1) & 63;
            if (n != qh) { q[qt] = (Ev){(uint16_t)(s->down ? 1 : 2), 0, tick, modifiers()}; q[qt].where = (uint32_t)(uint16_t)mouse_y << 16 | (uint16_t)mouse_x; qt = n; }
            hx_log("input: tick %u mouse %s at (%d,%d)", tick, s->down ? "down" : "up", mouse_x, mouse_y);
            continue;
        }
        uint32_t kb = LM_KeyMap + (s->code >> 3); uint8_t bit = (uint8_t)(1 << (s->code & 7));
        if (s->down) wr8(kb, rd8(kb) | bit); else wr8(kb, rd8(kb) & ~bit);
        if (s->ch || (s->code != 0x38 && s->code != 0x37)) {
            int n = (qt + 1) & 63;
            if (n != qh) { q[qt] = (Ev){(uint16_t)(s->down ? 3 : 4), (uint32_t)s->code << 8 | s->ch, tick, modifiers(), (uint32_t)(uint16_t)mouse_y << 16 | (uint16_t)mouse_x}; qt = n; }
        }
        hx_log("input: tick %u %s key %02X", tick, s->down ? "down" : "up", s->code);
    }
}

int input_live;   /* set by the port: input never runs out, so idling never ends the run */
int input_pending(void) { return input_live || next_step < nsteps || qh != qt; }

/* ---------- live input (the port): the front end reports the real keyboard and mouse ---------- */
static void post(uint16_t what, uint32_t msg) {
    int n = (qt + 1) & 63;
    if (n == qh) return;                                  /* queue full: the event is lost, as on a Mac */
    q[qt] = (Ev){what, msg, rd32(LM_Ticks), modifiers(), (uint32_t)(uint16_t)mouse_y << 16 | (uint16_t)mouse_x};
    qt = n;
}
/* a key going down (repeat: an auto-key event) or up: Mac key code and character */
void input_live_key(uint8_t code, uint8_t ch, int down, int repeat) {
    if (rec) fprintf(rec, "%u key %02X %02X %s\n", now_tick(), code, ch, repeat ? "auto" : down ? "down" : "up");
    uint32_t kb = LM_KeyMap + (code >> 3); uint8_t bit = (uint8_t)(1 << (code & 7));
    if (down) wr8(kb, rd8(kb) | bit); else wr8(kb, rd8(kb) & ~bit);
    if (!repeat) hx_log("input: key %02X %s", code, down ? "down" : "up");
    if (code >= 0x37 && code <= 0x3B) return;            /* modifier keys post no events */
    post(down ? (repeat ? 5 : 3) : 4, (uint32_t)code << 8 | ch);
}
/* a KeyMap bit alone, with no event (the port's extra key bindings) */
void input_live_keybit(uint8_t code, int down) {
    uint32_t kb = LM_KeyMap + (code >> 3); uint8_t bit = (uint8_t)(1 << (code & 7));
    if (down) wr8(kb, rd8(kb) | bit); else wr8(kb, rd8(kb) & ~bit);
}
/* the mouse moved by (dx, dy), as the mouse driver reports it: MTemp and RawMouse move (a game that
 * re-centres the mouse reads the movement from MTemp) and the cursor follows */
void input_live_move(int dx, int dy) {
    if (rec) fprintf(rec, "%u mmove %d %d\n", now_tick(), dx, dy);
    for (uint32_t a = 0x0828; a <= 0x082C; a += 4) { wr16(a, (uint16_t)(rd16(a) + dy)); wr16(a + 2, (uint16_t)(rd16(a + 2) + dx)); }
    mouse_y = (int16_t)rd16(0x082C); mouse_x = (int16_t)rd16(0x082E);
    wr32(0x0830, rd32(0x082C));
    wr8(0x08CE, rd8(0x08CF));                                /* CrsrNew = CrsrCouple */
}
/* a menu choice: a mouse-down on the menu bar (and its release), which the game passes to
 * MenuSelect; MenuSelect returns this item (harness/menu.c) */
void input_live_menu(int menu, int item) {
    void menu_choose(int menu, int item); menu_choose(menu, item);
    int16_t x = mouse_x, y = mouse_y;
    mouse_x = 30; mouse_y = 5; post(1, 0); post(2, 0);
    mouse_x = x; mouse_y = y;
}
void input_live_mouse(int x, int y) {
    if (x == mouse_x && y == mouse_y) return;
    if (rec) fprintf(rec, "%u move %d %d\n", now_tick(), x, y);
    mouse_x = (int16_t)x; mouse_y = (int16_t)y; set_mouse_lowmem();
}
void input_live_button(int down) {
    if (rec) fprintf(rec, "%u %s\n", now_tick(), down ? "mousedown" : "mouseup");
    mouse_down = down; set_mouse_lowmem(); post(down ? 1 : 2, 0);
}
/* any other event (the native shell's menu commands, suspend / resume, high-level events) */
void input_live_event(uint16_t what, uint32_t msg, uint32_t where) {
    int n = (qt + 1) & 63;
    if (n == qh) return;
    q[qt] = (Ev){what, msg, rd32(LM_Ticks), modifiers(), where};
    qt = n;
}

/* Takes (or peeks at) the first queued event matching mask. Returns 1 and fills the 16-byte EventRecord. */
int input_get_event(uint32_t ev, uint16_t mask, int remove) {
    for (int i = qh; i != qt; i = (i + 1) & 63) {
        if (!(mask & (q[i].what == 23 ? 0x0400 : 1 << q[i].what))) continue;   /* high-level events: highLevelEventMask */
        wr16(ev, q[i].what); wr32(ev + 2, q[i].message); wr32(ev + 6, q[i].when);
        wr32(ev + 10, q[i].where); wr16(ev + 14, q[i].modifiers);
        if (remove) {
            for (int j = i; j != qh; j = (j - 1) & 63) q[j] = q[(j - 1) & 63];
            qh = (qh + 1) & 63;
        }
        return 1;
    }
    return 0;
}
