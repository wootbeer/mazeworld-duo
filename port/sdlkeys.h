/* SDL keys to Mac key codes and characters: shared by the port (sdl_main.c) and the playable test
 * build (harness/play.c). Ctrl is the Command key, Alt the Option key; the main keys by position
 * (US layout). */
#ifndef SDLKEYS_H
#define SDLKEYS_H
#include <SDL.h>
#include <ctype.h>
#include <stdint.h>

static const struct { SDL_Scancode sc; uint8_t code; uint8_t ch; } special[] = {
    {SDL_SCANCODE_RETURN, 0x24, '\r'}, {SDL_SCANCODE_TAB, 0x30, '\t'}, {SDL_SCANCODE_SPACE, 0x31, ' '},
    {SDL_SCANCODE_BACKSPACE, 0x33, 0x08}, {SDL_SCANCODE_ESCAPE, 0x35, 0x1B},
    {SDL_SCANCODE_LCTRL, 0x37, 0}, {SDL_SCANCODE_RCTRL, 0x37, 0},            /* Ctrl is the Command key */
    {SDL_SCANCODE_LSHIFT, 0x38, 0}, {SDL_SCANCODE_RSHIFT, 0x38, 0}, {SDL_SCANCODE_CAPSLOCK, 0x39, 0},
    {SDL_SCANCODE_LALT, 0x3A, 0}, {SDL_SCANCODE_RALT, 0x3A, 0},              /* Alt is the Option key */
    {SDL_SCANCODE_KP_PERIOD, 0x41, '.'}, {SDL_SCANCODE_KP_MULTIPLY, 0x43, '*'}, {SDL_SCANCODE_KP_PLUS, 0x45, '+'},
    {SDL_SCANCODE_KP_DIVIDE, 0x4B, '/'}, {SDL_SCANCODE_KP_ENTER, 0x4C, 0x03},
    {SDL_SCANCODE_KP_MINUS, 0x4E, '-'}, {SDL_SCANCODE_KP_EQUALS, 0x51, '='},
    {SDL_SCANCODE_KP_0, 0x52, '0'}, {SDL_SCANCODE_KP_1, 0x53, '1'}, {SDL_SCANCODE_KP_2, 0x54, '2'}, {SDL_SCANCODE_KP_3, 0x55, '3'},
    {SDL_SCANCODE_KP_4, 0x56, '4'}, {SDL_SCANCODE_KP_5, 0x57, '5'}, {SDL_SCANCODE_KP_6, 0x58, '6'}, {SDL_SCANCODE_KP_7, 0x59, '7'},
    {SDL_SCANCODE_KP_8, 0x5B, '8'}, {SDL_SCANCODE_KP_9, 0x5C, '9'},
    {SDL_SCANCODE_F1, 0x7A, 0x10}, {SDL_SCANCODE_F2, 0x78, 0x10}, {SDL_SCANCODE_F3, 0x63, 0x10}, {SDL_SCANCODE_F4, 0x76, 0x10},
    {SDL_SCANCODE_F5, 0x60, 0x10}, {SDL_SCANCODE_F6, 0x61, 0x10}, {SDL_SCANCODE_F7, 0x62, 0x10}, {SDL_SCANCODE_F8, 0x64, 0x10},
    {SDL_SCANCODE_F9, 0x65, 0x10}, {SDL_SCANCODE_F10, 0x6D, 0x10}, {SDL_SCANCODE_F11, 0x67, 0x10}, {SDL_SCANCODE_F12, 0x6F, 0x10},
    {SDL_SCANCODE_INSERT, 0x72, 0x05}, {SDL_SCANCODE_HOME, 0x73, 0x01}, {SDL_SCANCODE_PAGEUP, 0x74, 0x0B},
    {SDL_SCANCODE_DELETE, 0x75, 0x7F}, {SDL_SCANCODE_END, 0x77, 0x04}, {SDL_SCANCODE_PAGEDOWN, 0x79, 0x0C},
    {SDL_SCANCODE_LEFT, 0x7B, 0x1C}, {SDL_SCANCODE_RIGHT, 0x7C, 0x1D}, {SDL_SCANCODE_DOWN, 0x7D, 0x1F}, {SDL_SCANCODE_UP, 0x7E, 0x1E},
};
/* the main keys by position (US layout), unshifted and shifted characters */
static const struct { SDL_Scancode sc; uint8_t code; char ch, shifted; } mainkeys[] = {
    {SDL_SCANCODE_A, 0x00, 'a', 'A'}, {SDL_SCANCODE_S, 0x01, 's', 'S'}, {SDL_SCANCODE_D, 0x02, 'd', 'D'}, {SDL_SCANCODE_F, 0x03, 'f', 'F'},
    {SDL_SCANCODE_H, 0x04, 'h', 'H'}, {SDL_SCANCODE_G, 0x05, 'g', 'G'}, {SDL_SCANCODE_Z, 0x06, 'z', 'Z'}, {SDL_SCANCODE_X, 0x07, 'x', 'X'},
    {SDL_SCANCODE_C, 0x08, 'c', 'C'}, {SDL_SCANCODE_V, 0x09, 'v', 'V'}, {SDL_SCANCODE_B, 0x0B, 'b', 'B'}, {SDL_SCANCODE_Q, 0x0C, 'q', 'Q'},
    {SDL_SCANCODE_W, 0x0D, 'w', 'W'}, {SDL_SCANCODE_E, 0x0E, 'e', 'E'}, {SDL_SCANCODE_R, 0x0F, 'r', 'R'}, {SDL_SCANCODE_Y, 0x10, 'y', 'Y'},
    {SDL_SCANCODE_T, 0x11, 't', 'T'}, {SDL_SCANCODE_1, 0x12, '1', '!'}, {SDL_SCANCODE_2, 0x13, '2', '@'}, {SDL_SCANCODE_3, 0x14, '3', '#'},
    {SDL_SCANCODE_4, 0x15, '4', '$'}, {SDL_SCANCODE_6, 0x16, '6', '^'}, {SDL_SCANCODE_5, 0x17, '5', '%'}, {SDL_SCANCODE_EQUALS, 0x18, '=', '+'},
    {SDL_SCANCODE_9, 0x19, '9', '('}, {SDL_SCANCODE_7, 0x1A, '7', '&'}, {SDL_SCANCODE_MINUS, 0x1B, '-', '_'}, {SDL_SCANCODE_8, 0x1C, '8', '*'},
    {SDL_SCANCODE_0, 0x1D, '0', ')'}, {SDL_SCANCODE_RIGHTBRACKET, 0x1E, ']', '}'}, {SDL_SCANCODE_O, 0x1F, 'o', 'O'}, {SDL_SCANCODE_U, 0x20, 'u', 'U'},
    {SDL_SCANCODE_LEFTBRACKET, 0x21, '[', '{'}, {SDL_SCANCODE_I, 0x22, 'i', 'I'}, {SDL_SCANCODE_P, 0x23, 'p', 'P'}, {SDL_SCANCODE_L, 0x25, 'l', 'L'},
    {SDL_SCANCODE_J, 0x26, 'j', 'J'}, {SDL_SCANCODE_APOSTROPHE, 0x27, '\'', '"'}, {SDL_SCANCODE_K, 0x28, 'k', 'K'}, {SDL_SCANCODE_SEMICOLON, 0x29, ';', ':'},
    {SDL_SCANCODE_BACKSLASH, 0x2A, '\\', '|'}, {SDL_SCANCODE_COMMA, 0x2B, ',', '<'}, {SDL_SCANCODE_SLASH, 0x2C, '/', '?'}, {SDL_SCANCODE_N, 0x2D, 'n', 'N'},
    {SDL_SCANCODE_M, 0x2E, 'm', 'M'}, {SDL_SCANCODE_PERIOD, 0x2F, '.', '>'}, {SDL_SCANCODE_GRAVE, 0x32, '`', '~'},
};
/* the Mac key code and character for an SDL key event; 0 if the key has no Mac counterpart */
static int sdl_mac_key(const SDL_KeyboardEvent *e, uint8_t *code, uint8_t *ch) {
    SDL_Scancode sc = e->keysym.scancode;
    int shift = (e->keysym.mod & KMOD_SHIFT) != 0, caps = (e->keysym.mod & KMOD_CAPS) != 0;
    for (unsigned i = 0; i < sizeof special / sizeof special[0]; i++)
        if (special[i].sc == sc) { *code = special[i].code; *ch = special[i].ch; return 1; }
    for (unsigned i = 0; i < sizeof mainkeys / sizeof mainkeys[0]; i++)
        if (mainkeys[i].sc == sc) {
            int up = shift ^ (caps && isalpha((unsigned char)mainkeys[i].ch));
            *code = mainkeys[i].code; *ch = (uint8_t)(up ? mainkeys[i].shifted : mainkeys[i].ch);
            return 1;
        }
    return 0;
}
#endif
