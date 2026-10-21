/* The playable test build (mzplay): the test rig, running the original 68k game, in a window you
 * can play. Everything you do is recorded as a test script (input.c), with the tick it happened on,
 * so the session replays exactly under the rig and the port: a way to record games that reach
 * parts of a game random play does not.
 *   F1   capture / release the mouse (Mazeworld Original steers by mouse movement)
 *   Ctrl the Command key, Alt the Option key (as in the port)
 * Closing the window ends the run at the next tick (the rig then writes its usual outputs); the
 * recording's last line gives that tick: replay with  mzharness -script <recording> -ticks <it>. */
#include "hx.h"
#include "m68k.h"
#include "../port/sdlkeys.h"
#include <stdio.h>
#include <string.h>

extern int input_live;
extern void (*input_play_hook)(uint32_t tick);
extern void (*snd_live_out)(const int16_t *s, int n);
int  input_record(const char *path);
void input_live_key(uint8_t code, uint8_t ch, int down, int repeat);
void input_live_mouse(int x, int y);
void input_live_move(int dx, int dy);
void input_live_button(int down);
void qd_screen_argb(uint32_t *out);

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_AudioDeviceID audio;
static uint32_t pixels[SCREEN_W * SCREEN_H];
static uint64_t t0, freq; static uint32_t tick0;
static int captured;

static void sound_out(const int16_t *s, int n) {
    if (!audio) return;
    if (SDL_GetQueuedAudioSize(audio) > 22255 * 2 / 5) SDL_ClearQueuedAudio(audio);
    SDL_QueueAudio(audio, s, (Uint32)n * 2);
}
static void capture(int on) {
    captured = on;
    SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
    SDL_SetWindowTitle(win, on ? "Mazeworld test recording - mouse captured (F1 releases)" : "Mazeworld test recording - F1 captures the mouse");
}
/* closing the window: the session ends exactly when the next tick begins (the rig's tick limit),
 * a point a replay with -ticks reaches too; the recording says which */
static void stop(void) {
    extern uint32_t g_max_ticks; uint32_t vbl_ticks(void);
    void input_record_end(uint32_t ticks);
    if (g_max_ticks) return;
    g_max_ticks = vbl_ticks() + 1;
    input_record_end(g_max_ticks);
}

/* once per tick: the window's events become the game's input, then pacing and the screen */
static void play_tick(uint32_t tick) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: stop(); break;
        case SDL_KEYDOWN: case SDL_KEYUP: {
            if (e.key.keysym.scancode == SDL_SCANCODE_F1) { if (e.type == SDL_KEYDOWN && !e.key.repeat) capture(!captured); break; }
            uint8_t code, ch;
            if (sdl_mac_key(&e.key, &code, &ch)) input_live_key(code, ch, e.type == SDL_KEYDOWN, e.key.repeat);
            break; }
        case SDL_MOUSEMOTION:
            if (captured) input_live_move(e.motion.xrel, e.motion.yrel);
            else input_live_mouse(e.motion.x, e.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) {
                if (!captured) input_live_mouse(e.button.x, e.button.y);
                input_live_button(e.type == SDL_MOUSEBUTTONDOWN);
            }
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && captured) capture(0);
            break;
        }
    }
    uint64_t now = SDL_GetPerformanceCounter();
    int64_t ahead = (int64_t)((uint64_t)(tick - tick0) * freq / 60) - (int64_t)(now - t0);
    if (ahead < -(int64_t)freq / 4) { t0 = now; tick0 = tick; ahead = 0; }
    if (ahead > 0) SDL_Delay((Uint32)(ahead * 1000 / (int64_t)freq));
    qd_screen_argb(pixels);
    SDL_UpdateTexture(tex, NULL, pixels, SCREEN_W * 4);
    SDL_RenderClear(ren); SDL_RenderCopy(ren, tex, NULL, NULL); SDL_RenderPresent(ren);
}

/* opens the window and starts recording to `recording`; returns 0, or -1 with a message printed */
int play_start(const char *recording) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return -1; }
    win = SDL_CreateWindow("Mazeworld test recording", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W * 2, SCREEN_H * 2, SDL_WINDOW_RESIZABLE);
    ren = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
    if (!ren) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return -1; }
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    SDL_AudioSpec want = {0}, have;
    want.freq = 22255; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 512;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio) { snd_live_out = sound_out; SDL_PauseAudioDevice(audio, 0); }
    if (input_record(recording)) { fprintf(stderr, "cannot write %s\n", recording); return -1; }
    input_live = 1;
    input_play_hook = play_tick;
    freq = SDL_GetPerformanceFrequency(); t0 = SDL_GetPerformanceCounter(); tick0 = 0;
    capture(0);
    return 0;
}
