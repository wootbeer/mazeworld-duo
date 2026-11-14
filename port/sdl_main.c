/* The port's front end: SDL2 window, keyboard and mouse, sound, 60 Hz timing, the preferences file,
 * and (on Windows) the native menu bar. The game itself runs in port_run_game(); everything here
 * happens in port_tick(), once per tick of game time, while the game waits for that tick. */
#include "../harness/hx.h"
#include "../game/mz.h"
#include "port.h"
#include "shell.h"
#include <SDL.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include "../archive/arc.h"
#ifdef _WIN32
#include <SDL_syswm.h>
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#endif

int g_idle_limit = 0x7FFFFFFF;
extern int input_live;
void input_live_key(uint8_t code, uint8_t ch, int down, int repeat);
void input_live_mouse(int x, int y);
void input_live_button(int down);
void input_live_move(int dx, int dy);
void qd_screen_argb(uint32_t *out);
extern void (*snd_live_out)(const int16_t *s, int n);

#define TITLE "Mazeworld"

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_AudioDeviceID audio;
static uint32_t pixels[SCREEN_W * SCREEN_H];
static jmp_buf done;
static uint64_t t0, freq;        /* wall clock at tick 0 of pacing */
static uint32_t tick0;
static int quit_asked;
static char prefs_path[1024];

/* ---------- messages ---------- */

static void message(const char *title, const char *text, int error) {
    SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION, title, text, win);
}
void port_fatal(const char *msg) {
    char t[512]; snprintf(t, sizeof t, "The game stopped because of an internal error:\n\n%s", msg);
    message(TITLE, t, 1);
    longjmp(done, 2);
}
void port_quit(void) { longjmp(done, 1); }

/* ---------- sound: the game's 22254.5 Hz mono output, queued to the device ---------- */

/* the port's own settings (not the game's: those stay in its preferences file), in
 * "port settings.txt" beside it */
static struct {
    int volume;                 /* percent of the game's output */
    int x, y, w, h, full;       /* the window: position and client size when not fullscreen (w 0: none saved) */
    char archive[1024];         /* the game archive the player chose, if it is not next to the program */
    int orig_classic;           /* Mazeworld Original: its own mouse movement and gestures, not the keys */
    int game;                   /* the game played last */
    char archive2[1024];        /* Mazeworld Original's archive, if chosen */
} settings = {100, 0, 0, 0, 0, 0, "", 0, 0, ""};
static char settings_path[1024];

/* The games the port knows, by their application's creator code. An archive given on the command
 * line, or any archive next to the program, is recognised by the application inside it. */
static const struct { uint32_t creator; const char *name, *prefs; } games[] = {
    {FOURCC('M','z','w','C'), "Mazeworld Catacombs", "Mzwl Catacombs Prefs"},
    {FOURCC('M','z','w','l'), "Mazeworld (Original)", "Mazeworld Prefs"},
};
#define NGAMES (int)(sizeof games / sizeof games[0])
static char found[NGAMES][1024];      /* an archive of each game, if one was found */
static int running = -1;              /* the game being played */
static int switch_to = -1;            /* the game to start when this one has quit */
static char *archive_setting(int g) { return g ? settings.archive2 : settings.archive; }

static void settings_save(void);

static void sound_out(const int16_t *in, int n) {
    if (!audio) return;
    int16_t s[512];
    if (n > 512) n = 512;
    for (int i = 0; i < n; i++) s[i] = (int16_t)(in[i] * settings.volume / 100);
    /* keep at most ~0.2 s queued: after a pause (a menu held open) the backlog is dropped */
    if (SDL_GetQueuedAudioSize(audio) > 22255 * 2 / 5) SDL_ClearQueuedAudio(audio);
    SDL_QueueAudio(audio, s, (Uint32)n * 2);
}

#include "sdlkeys.h"

/* Modern controls. The game reads only the keypad and letter keys: Q W E turn left / forward /
 * turn right, A D sideways, X back (keypad 7 8 9, 4 6, 2), and Space, Return, Enter or Shift fire.
 * The port adds, by setting the KeyMap bit of the key the game knows (the pressed key's own events
 * are unchanged, so typing a name is not affected):
 *   S          back (X)
 *   arrow keys the classic controls: up / down forward and back, left / right turn (keypad 8 2 7 9)
 *   left mouse button  fire (Space)
 * and in play the mouse turns: its sideways movement adds to the game's turn counter G_217E, as
 * holding Q or E does once per tick (2 units = one of the 64 directions; mouse_look()).
 * Mazeworld Original gets the same keys and mouse turning (mz_alt_controls: the game reads the keys
 * in its VBL task, the turning goes to mz_alt_move), a click fires the chosen ability and 1-4
 * choose it; or, with Options > Classic Mouse Controls, its own: the mouse moves and turns, and a
 * drag with the button held chooses the ability, fired on release. */
void input_live_keybit(uint8_t code, int down);
static const struct { SDL_Scancode sc; uint8_t as; } extra[] = {
    {SDL_SCANCODE_S, 0x07},       /* X: back */
    {SDL_SCANCODE_UP, 0x5B},      /* keypad 8: forward */
    {SDL_SCANCODE_DOWN, 0x54},    /* keypad 2: back */
    {SDL_SCANCODE_LEFT, 0x59},    /* keypad 7: turn left */
    {SDL_SCANCODE_RIGHT, 0x5C},   /* keypad 9: turn right */
};
#define FIRE_KEY 0x31             /* Space */
static int mouse_fire;
static void extra_keys(void) {
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    uint8_t want[128] = {0}, touched[128] = {0};
    for (unsigned i = 0; i < sizeof extra / sizeof extra[0]; i++) { touched[extra[i].as] = 1; if (ks[extra[i].sc]) want[extra[i].as] = 1; }
    touched[FIRE_KEY] = 1; if (mouse_fire) want[FIRE_KEY] = 1;
    /* a target key may also be held itself */
    for (unsigned j = 0; j < sizeof special / sizeof special[0]; j++) if (touched[special[j].code] && ks[special[j].sc]) want[special[j].code] = 1;
    for (unsigned j = 0; j < sizeof mainkeys / sizeof mainkeys[0]; j++) if (touched[mainkeys[j].code] && ks[mainkeys[j].sc]) want[mainkeys[j].code] = 1;
    for (int c = 0; c < 128; c++) if (touched[c]) input_live_keybit((uint8_t)c, want[c]);
}

/* Mouse turning. Mouse movement arrives in bursts; handing it to the game as it comes turns in
 * uneven jumps. So the movement is collected (in turn units: MOUSE_PER_UNIT pixels each) and paid
 * out a little every tick, the way a held turn key gives one unit per tick: one unit per tick, more
 * only while the mouse is further ahead, so that a quick flick still turns at once. */
#define MOUSE_PER_UNIT 8          /* pixels per turn unit (2 units per direction, 64 directions) */
#define MOUSE_MAX_BACKLOG 64      /* units: anything beyond this is dropped rather than lag behind */
static int mouse_px, captured;    /* collected movement, in pixels */
static void mouse_look(void) {
    int play = shell_playing() && (SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS);
    if (play != captured) {
        captured = play; mouse_px = 0;
        SDL_SetRelativeMouseMode(play ? SDL_TRUE : SDL_FALSE);
        if (!play && mouse_fire) { mouse_fire = 0; extra_keys(); }
    }
    if (!play) return;
    int lim = MOUSE_MAX_BACKLOG * MOUSE_PER_UNIT;
    if (mouse_px > lim) mouse_px = lim;
    if (mouse_px < -lim) mouse_px = -lim;
    int ahead = (mouse_px < 0 ? -mouse_px : mouse_px) / MOUSE_PER_UNIT;   /* whole units waiting */
    if (!ahead) return;
    int pay = 1 + ahead / 6;                  /* 1 per tick, faster while far behind */
    if (pay > ahead) pay = ahead;
    int units = mouse_px < 0 ? -pay : pay;
    mouse_px -= units * MOUSE_PER_UNIT;
    if (mz_original) mz_alt_move[0] = (int16_t)(mz_alt_move[0] + units);
    else { uint32_t g = A5_ADDR - 0x217E; wr16(g, (uint16_t)(rd16(g) + units)); }
}

static void key(const SDL_KeyboardEvent *e, int down) {
    uint8_t code, ch;
    if (sdl_mac_key(e, &code, &ch)) input_live_key(code, ch, down, e->repeat);
}

/* ---------- the menu bar (Windows) ---------- */
#ifdef _WIN32
static HMENU bar;
static WNDPROC sdl_proc;
#define CMD(menu, item) (((menu) - 127) * 100 + (item))
/* items are the game's in order ("": a separator, "-": an item the port leaves out) */
static HMENU popup(int menu, const char *const *items) {
    HMENU m = CreatePopupMenu();
    for (int i = 0; items[i]; i++) {
        if (items[i][0] == '-') continue;
        if (!items[i][0]) AppendMenuA(m, MF_SEPARATOR, 0, NULL);
        else AppendMenuA(m, MF_STRING, (UINT_PTR)CMD(menu, i + 1), items[i]);
    }
    return m;
}
/* Mazeworld Original's (Print Order Form left out: no printing) */
static void build_menus_original(HMENU options) {
    static const char *const help[] = {"&About Mazeworld...", "&Read Instructions...\tCtrl+/", "-", "Re&gister...", NULL};
    static const char *const file[] = {"&Play Game\tCtrl+G", "", "&New Game\tCtrl+N", "&Load Game...\tCtrl+O", "",
                                       "&Save Game As...\tCtrl+S", "", "E&xit\tCtrl+Q", NULL};
    static const char *const size[] = {"&Tiny\tCtrl+1", "&Small\tCtrl+2", "12\" &Monitor\tCtrl+3", "13\" M&onitor\tCtrl+4", NULL};
    static const char *const opts[] = {"&Fast Graphics Mode", "&Mac Friendly Mode", NULL};
    AppendMenuA(options, MF_SEPARATOR, 0, NULL);
    for (int i = 0; opts[i]; i++) AppendMenuA(options, MF_STRING, (UINT_PTR)CMD(MENU_O_OPTIONS, i + 1), opts[i]);
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)popup(MENU_FILE, file), "&File");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)popup(MENU_O_SIZE, size), "&Window Size");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)options, "&Options");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)popup(MENU_APPLE, help), "&Help");
}
static void build_menus(HWND hwnd) {
    static const char *const game[] = {"&About Mazeworld Catacombs...\tCtrl+/", NULL};
    static const char *const file[] = {"&Play Game\tCtrl+G", "", "&New Game\tCtrl+N", "", "&Clear High Scores", "", "E&xit\tCtrl+Q", NULL};
    static const char *const view[] = {"&Tiny\tCtrl+1", "&Small\tCtrl+2", "12\" &Monitor\tCtrl+3", "13\" M&onitor\tCtrl+4", "",
                                       "&Lowest Resolution", "Lo&w Resolution", "&Normal Resolution", NULL};
    static const char *const sound[] = {"&No Sound", "&Reduced Sound", "N&ormal Sound", "&Full Sound", NULL};
    /* the original's Options dialog (performance settings for 1990s Macs), as a Legacy submenu */
    static const char *const legacy[] = {"Draw &Direct to Screen", "Skip Screen &Sync", "Distribute &Mapping", NULL};
    bar = CreateMenu();
    {   /* Game: the games the port knows; choosing the other one starts it (when found) */
        HMENU game = CreatePopupMenu();
        for (int i = 0; i < NGAMES; i++) {
            char label[128];
            snprintf(label, sizeof label, "%s%s", games[i].name, !found[i][0] ? "  (archive not found)" : "");
            AppendMenuA(game, MF_STRING, (UINT_PTR)CMD(MENU_PORT_GAME, i + 1), label);
        }
        AppendMenuA(bar, MF_POPUP, (UINT_PTR)game, "&Game");
    }
    static const char *const volume[] = {"&100%", "&75%", "&50%", "&25%", "&Mute", NULL};
    HMENU options = CreatePopupMenu();
    AppendMenuA(options, MF_POPUP, (UINT_PTR)popup(MENU_PORT_VOLUME, volume), "&Volume");
    if (running == 1) {
        AppendMenuA(options, MF_STRING, (UINT_PTR)CMD(MENU_PORT_CONTROLS, 1), "&Classic Mouse Controls");
        build_menus_original(options);
        SetMenu(hwnd, bar);
        return;
    }
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)popup(MENU_FILE, file), "&File");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)popup(MENU_VIEW, view), "&View");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)popup(MENU_SOUND, sound), "&Sound");
    AppendMenuA(options, MF_SEPARATOR, 0, NULL);
    AppendMenuA(options, MF_POPUP, (UINT_PTR)popup(MENU_OPTIONS, legacy), "&Legacy");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)options, "&Options");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)popup(MENU_APPLE, game), "&Help");
    SetMenu(hwnd, bar);
}
/* the port's own menu items: they change at once, without the game */
static const int volume_levels[] = {100, 75, 50, 25, 0};
static int port_item_state(int menu, int item, int *checked) {
    if (menu == MENU_PORT_GAME) { *checked = item - 1 == running; return item - 1 != running && found[item - 1][0]; }
    if (menu == MENU_PORT_CONTROLS) { *checked = settings.orig_classic; return 1; }
    *checked = menu == MENU_PORT_VOLUME && item >= 1 && item <= 5 && settings.volume == volume_levels[item - 1];
    return 1;
}
static void port_choose(int menu, int item) {
    if (menu == MENU_PORT_VOLUME && item >= 1 && item <= 5) { settings.volume = volume_levels[item - 1]; settings_save(); }
    if (menu == MENU_PORT_CONTROLS) { settings.orig_classic = !settings.orig_classic; settings_save(); }
    if (menu == MENU_PORT_GAME && item >= 1 && item <= NGAMES && found[item - 1][0]) { switch_to = item - 1; shell_request_quit(); }
}
/* checks and greys each item as the original's menu bar would be just then */
static void update_popup(HMENU m) {
    int n = GetMenuItemCount(m);
    for (int i = 0; i < n; i++) {
        UINT id = GetMenuItemID(m, i);
        if (!id || id == (UINT)-1) continue;
        int menu = (int)id / 100 + 127, item = (int)id % 100, checked;
        int en = menu >= MENU_PORT_VOLUME ? port_item_state(menu, item, &checked) : shell_item_state(menu, item, &checked);
        EnableMenuItem(m, id, MF_BYCOMMAND | (en ? MF_ENABLED : MF_GRAYED));
        CheckMenuItem(m, id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
    }
}
static LRESULT CALLBACK menu_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_INITMENUPOPUP) { update_popup((HMENU)wp); return 0; }
    if (msg == WM_COMMAND && HIWORD(wp) == 0 && LOWORD(wp) >= 100) {
        int menu = LOWORD(wp) / 100 + 127, item = LOWORD(wp) % 100;
        if (menu >= MENU_PORT_VOLUME) port_choose(menu, item); else shell_choose(menu, item);
        return 0;
    }
    if (msg == WM_SYSCHAR || msg == WM_SYSKEYDOWN) {
        /* Alt is the Option key, so Alt+letter does not open the menus; F10 still does */
        if (wp != VK_F10 && wp != VK_MENU) return 0;
    }
    return CallWindowProc(sdl_proc, h, msg, wp, lp);
}
static void install_menus(void) {
    SDL_SysWMinfo info; SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(win, &info)) return;
    HWND hwnd = info.info.win.window;
    build_menus(hwnd);
    sdl_proc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)menu_proc);
}
#endif

/* ---------- presentation ---------- */

static uint32_t shown[SCREEN_W * SCREEN_H];
static int redraw = 1;           /* the window needs drawing even if the screen has not changed */
static uint32_t view[SCREEN_W * SCREEN_H];
static int view_x, view_y, view_w = SCREEN_W, view_h = SCREEN_H;   /* the part of the screen shown */
static void present(void) {
    int vx, vy, vw, vh;
    qd_screen_argb(pixels);
    shell_view(&vx, &vy, &vw, &vh);
    for (int y = 0; y < vh; y++)                            /* the view; black where it leaves the screen */
        for (int x = 0; x < vw; x++) {
            int sx = vx + x, sy = vy + y;
            view[y * vw + x] = sx >= 0 && sy >= 0 && sx < SCREEN_W && sy < SCREEN_H ? pixels[sy * SCREEN_W + sx] : 0xFF000000u;
        }
    view_x = vx; view_y = vy;
    if (vw != view_w || vh != view_h) { view_w = vw; view_h = vh; SDL_RenderSetLogicalSize(ren, vw, vh); redraw = 1; }
    if (!redraw && !memcmp(view, shown, (size_t)vw * vh * 4)) return;
    memcpy(shown, view, (size_t)vw * vh * 4); redraw = 0;
    SDL_Rect src = {0, 0, vw, vh};
    SDL_UpdateTexture(tex, &src, view, vw * 4);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, &src, NULL);
    SDL_RenderPresent(ren);
}

/* ---------- once per tick ---------- */

/* Alt+Enter: fullscreen (the menu bar is taken away meanwhile) */
static void toggle_fullscreen(void) {
    int full = !(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (full) { SDL_GetWindowPosition(win, &settings.x, &settings.y); SDL_GetWindowSize(win, &settings.w, &settings.h); }   /* to come back to */
#ifdef _WIN32
    { SDL_SysWMinfo info; SDL_VERSION(&info.version);
      if (SDL_GetWindowWMInfo(win, &info)) SetMenu(info.info.win.window, full ? NULL : bar); }
#endif
    SDL_SetWindowFullscreen(win, full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(full ? SDL_DISABLE : SDL_ENABLE);
    redraw = 1;
}

static void events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            if (quit_asked) longjmp(done, 1);    /* asked twice: the game is not listening, leave */
            quit_asked = 1; shell_request_quit();
            break;
        case SDL_KEYDOWN: case SDL_KEYUP:
            if (e.key.keysym.scancode == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
                if (e.type == SDL_KEYDOWN && !e.key.repeat) toggle_fullscreen();
                break;
            }
            key(&e.key, e.type == SDL_KEYDOWN);
            extra_keys();
            break;
        case SDL_MOUSEMOTION:
            if (captured && mz_original && settings.orig_classic) input_live_move(e.motion.xrel, e.motion.yrel);
            else if (captured) mouse_px += e.motion.xrel;
            else input_live_mouse(e.motion.x + view_x, e.motion.y + view_y);
            break;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
            if (captured) {
                if (e.button.button != SDL_BUTTON_LEFT) break;
                if (mz_original) input_live_button(e.type == SDL_MOUSEBUTTONDOWN);   /* Original reads the button itself */
                else { mouse_fire = e.type == SDL_MOUSEBUTTONDOWN; extra_keys(); }
                break;
            }
            if (e.button.button == SDL_BUTTON_LEFT) {
                input_live_mouse(e.button.x + view_x, e.button.y + view_y);
                input_live_button(e.type == SDL_MOUSEBUTTONDOWN);
            }
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) shell_focus(1);
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) shell_focus(0);
            redraw = 1;
            break;
        }
    }
}

void port_tick(uint32_t tick) {
    events();
    mz_alt_controls = mz_original && !settings.orig_classic;
    mouse_look();
    /* the Mac's ticks are 1/60 s: wait for this one's time; after a stall (a menu held open,
     * the window dragged) start counting again rather than racing to catch up */
    uint64_t now = SDL_GetPerformanceCounter();
    int64_t due = (int64_t)((uint64_t)(tick - tick0) * freq / 60);
    int64_t ahead = due - (int64_t)(now - t0);
    if (ahead < -(int64_t)freq / 4) { t0 = now; tick0 = tick; ahead = 0; }
    if (ahead > 0) SDL_Delay((Uint32)(ahead * 1000 / (int64_t)freq));
    present();
}

/* ---------- the preferences file, kept on this computer ---------- */

static void prefs_location(void) {
    char *dir = SDL_GetPrefPath("Mazeworld", "Mazeworld Catacombs");
    if (dir) {
        snprintf(settings_path, sizeof settings_path, "%s%s", dir, "port settings.txt");
        SDL_free(dir);
    }
}
static void settings_load(void) {
    FILE *f = fopen(settings_path, "r"); if (!f) return;
    char line[256]; int v;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "volume=%d", &v) == 1 && v >= 0 && v <= 100) settings.volume = v;
        else if (sscanf(line, "window=%d,%d,%d,%d", &settings.x, &settings.y, &settings.w, &settings.h) == 4) {}
        else if (sscanf(line, "fullscreen=%d", &v) == 1) settings.full = v != 0;
        else if (sscanf(line, "original_classic_mouse=%d", &v) == 1) settings.orig_classic = v != 0;
        else if (sscanf(line, "game=%d", &v) == 1) settings.game = v == 1;
        else if (!strncmp(line, "archive=", 8) || !strncmp(line, "archive2=", 9)) {
            char *a = line[7] == '=' ? settings.archive : settings.archive2;
            snprintf(a, sizeof settings.archive, "%s", strchr(line, '=') + 1);
            a[strcspn(a, "\r\n")] = 0;
        }
    fclose(f);
}
static void settings_save(void) {
    if (!settings_path[0]) return;
    FILE *f = fopen(settings_path, "w"); if (!f) return;
    fprintf(f, "volume=%d\n", settings.volume);
    if (settings.w) fprintf(f, "window=%d,%d,%d,%d\nfullscreen=%d\n", settings.x, settings.y, settings.w, settings.h, settings.full);
    if (settings.archive[0]) fprintf(f, "archive=%s\n", settings.archive);
    if (settings.archive2[0]) fprintf(f, "archive2=%s\n", settings.archive2);
    fprintf(f, "game=%d\n", settings.game);
    fprintf(f, "original_classic_mouse=%d\n", settings.orig_classic);
    fclose(f);
}
static void prefs_load(void) {
    if (settings_path[0]) snprintf(prefs_path, sizeof prefs_path, "%.*s%s", (int)(strlen(settings_path) - strlen("port settings.txt")), settings_path, games[running].prefs);
    if (prefs_path[0]) port_prefs_load(prefs_path);
}
static void prefs_save(void) { if (prefs_path[0]) port_prefs_save(prefs_path); }

/* ---------- start ---------- */

static int has_suffix(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    if (a < b) return 0;
    for (size_t i = 0; i < b; i++) if (tolower((unsigned char)s[a - b + i]) != suf[i]) return 0;
    return 1;
}
static int which_game(const char *path) {
    static ArcSet arc;
    if (arc_load_file(path, &arc)) return -1;
    int g = -1;
    for (int k = 0; k < arc.n && g < 0; k++)
        if (arc.files[k].type == FOURCC('A','P','P','L'))
            for (int i = 0; i < NGAMES; i++) if (arc.files[k].creator == games[i].creator) g = i;
    arc_free(&arc);
    return g;
}
static void find_games(void) {
    char *base = SDL_GetBasePath();
    DIR *d = opendir(base ? base : ".");
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (!has_suffix(e->d_name, ".hqx") && !has_suffix(e->d_name, ".sit") && !has_suffix(e->d_name, ".bin")) continue;
        char path[1024]; snprintf(path, sizeof path, "%s%s", base ? base : "", e->d_name);
        int g = which_game(path);
        if (g >= 0 && !found[g][0]) snprintf(found[g], sizeof found[g], "%s", path);
    }
    if (d) closedir(d);
    SDL_free(base);
}

/* the standard Open / Save dialogs (Windows); 0 if the player chose a file */
static int file_dialog(int save, const char *filter, const char *title, const char *defname, char *out, size_t n) {
#ifdef _WIN32
    char path[1024]; snprintf(path, sizeof path, "%s", defname ? defname : "");
    OPENFILENAMEA o; memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o; o.hwndOwner = NULL;
    o.lpstrFilter = filter; o.lpstrFile = path; o.nMaxFile = sizeof path; o.lpstrTitle = title;
    o.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameA(&o) : GetOpenFileNameA(&o))) return -1;
    snprintf(out, n, "%s", path);
    return 0;
#else
    (void)save; (void)filter; (void)title; (void)defname; (void)out; (void)n; return -1;
#endif
}
/* No archive next to the program: the player picks one. */
static int choose_archive(char *out, size_t n) {
    return file_dialog(0, "Mac game archives (*.hqx, *.sit, *.bin)\0*.hqx;*.sit;*.bin\0All files (*.*)\0*.*\0",
                       "Select an original Mazeworld archive", NULL, out, n);
}
/* the game's Load Game / Save Game As (Standard File): a saved game on this computer (vfs.c) */
extern int (*vfs_host_choose)(int save, const char *defname, char *path, size_t n);
static int choose_saved_game(int save, const char *defname, char *path, size_t n) {
    return file_dialog(save, "Saved games (*.*)\0*.*\0", save ? "Save Game As" : "Load Game", defname, path, n);
}
/* starts the program again with another game's archive (Game menu) */
static void relaunch(const char *archive) {
#ifdef _WIN32
    char exe[1024], cmd[2200];
    GetModuleFileNameA(NULL, exe, sizeof exe);
    snprintf(cmd, sizeof cmd, "\"%s\" \"%s\"", exe, archive);
    STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si, 0, sizeof si); si.cb = sizeof si;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
#else
    (void)archive;
#endif
}

/* is a saved window still (mostly) on one of the displays? (displays change between runs) */
static int on_a_display(int x, int y, int w, int h) {
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        SDL_Rect r;
        if (SDL_GetDisplayUsableBounds(i, &r)) continue;
        if (x >= r.x - 16 && y >= r.y - 16 && x + w / 2 <= r.x + r.w && y + h / 2 <= r.y + r.h) return 1;
    }
    return 0;
}

/* the largest whole multiple of the Mac's screen that fits on this display */
static int best_scale(void) {
    SDL_Rect r;
    if (SDL_GetDisplayUsableBounds(0, &r)) return 2;
    int s = 1;
    while (SCREEN_W * (s + 1) <= r.w - 16 && SCREEN_H * (s + 1) <= r.h - 64) s++;
    return s;
}

int main(int argc, char **argv) {
    char archive[1024], err[512];
    prefs_location();
    settings_load();
    find_games();
    for (int g = 0; g < NGAMES; g++)                         /* chosen before */
        if (!found[g][0] && archive_setting(g)[0] && which_game(archive_setting(g)) == g)
            snprintf(found[g], sizeof found[g], "%s", archive_setting(g));
    if (argc > 1 && (has_suffix(argv[argc - 1], ".hqx") || has_suffix(argv[argc - 1], ".sit") || has_suffix(argv[argc - 1], ".bin"))) {
        int g = which_game(argv[argc - 1]);
        if (g < 0) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, TITLE, "That archive does not contain Mazeworld Catacombs or Mazeworld.", NULL);
            return 1;
        }
        snprintf(found[g], sizeof found[g], "%s", argv[argc - 1]);
        running = g;
    } else if (found[settings.game][0]) running = settings.game;
    else if (found[0][0]) running = 0;
    else if (found[1][0]) running = 1;
    while (running < 0) {   /* not found: ask for it, and remember it */
        char path[1024];
#ifdef _WIN32
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, TITLE,
            "This program needs an original game archive: Mazeworld Catacombs 1.0.5 or Mazeworld 1.0.3, as a .hqx, .sit "
            "or .bin file, under any name.\n\nPlease select one. (Putting them in the same folder as this program also works.)", NULL);
#endif
        if (choose_archive(path, sizeof path)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, TITLE,
                "The original game is needed: put the Mazeworld Catacombs 1.0.5 or Mazeworld 1.0.3 archive (.hqx, .sit or "
                ".bin, any name) in the same folder as this program, or drop it onto the program.", NULL);
            return 1;
        }
        int g = which_game(path);
        if (g >= 0) { snprintf(found[g], sizeof found[g], "%s", path); snprintf(archive_setting(g), sizeof settings.archive, "%s", path); running = g; }
        else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, TITLE, "That file does not contain Mazeworld Catacombs or Mazeworld. Please select another.", NULL);
    }
    settings.game = running;
    settings_save();
    snprintf(archive, sizeof archive, "%s", found[running]);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1;
    }
    int scale = best_scale(), ww = SCREEN_W * scale, wh = SCREEN_H * scale;
    int wx = SDL_WINDOWPOS_CENTERED, wy = SDL_WINDOWPOS_CENTERED;
    if (settings.w >= SCREEN_W / 2 && settings.h >= SCREEN_H / 2 && on_a_display(settings.x, settings.y, settings.w, settings.h)) {
        ww = settings.w; wh = settings.h; wx = settings.x; wy = settings.y;
    }
    win = SDL_CreateWindow(games[running].name, wx, wy, ww, wh,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
#ifdef _WIN32
    install_menus();
    SDL_SetWindowSize(win, ww, wh);                          /* the client area, now that it has a menu bar */
    SDL_SetWindowPosition(win, wx, wy);
#endif
    SDL_AudioSpec want = {0}, have;
    want.freq = 22255; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 512;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio) { snd_live_out = sound_out; SDL_PauseAudioDevice(audio, 0); }

    const char *log = getenv("MZ_LOG");
    if (port_boot(archive, log, err, sizeof err)) { message(TITLE, err, 1); return 1; }
    vfs_host_choose = choose_saved_game;
    input_live = 1;
    prefs_load();
    if (settings.full) toggle_fullscreen();
    freq = SDL_GetPerformanceFrequency(); t0 = SDL_GetPerformanceCounter(); tick0 = 0;
    int how = setjmp(done);
    if (!how) { port_run_game(); how = 1; }
    if (how == 1) prefs_save();
    /* the window as it was left (its windowed geometry, if fullscreen) */
    settings.full = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    if (!settings.full) { SDL_GetWindowPosition(win, &settings.x, &settings.y); SDL_GetWindowSize(win, &settings.w, &settings.h); }
    settings_save();
    if (audio) SDL_CloseAudioDevice(audio);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    SDL_Quit();
    if (how == 1 && switch_to >= 0) { settings.game = switch_to; settings_save(); relaunch(found[switch_to]); }
    return how == 1 ? 0 : 1;
}
