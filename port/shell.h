/* The port's shell (shell.c) and what it needs from the front end. */
#ifndef SHELL_H
#define SHELL_H
enum { MENU_APPLE = 128, MENU_FILE, MENU_EDIT, MENU_VIEW, MENU_SOUND, MENU_OPTIONS };
/* Mazeworld Original's: Apple, File and Edit as above, then Window Size and Options */
enum { MENU_O_SIZE = 131, MENU_O_OPTIONS };
/* the port's own menus (the front end handles them; the game never sees them) */
enum { MENU_PORT_VOLUME = 200, MENU_PORT_GAME, MENU_PORT_CONTROLS, MENU_PORT_LABELS };
/* Options > Legacy: the original Options dialog's check boxes */
enum { OPT_DIRECT = 1, OPT_NOSYNC, OPT_DISTRIBUTE };
int  shell_playing(void);                                   /* is the game window in front (in play)? */
int  shell_view(int *x, int *y, int *w, int *h);            /* the part of the Mac screen the window shows */
int  shell_item_state(int menu, int item, int *checked);   /* returns 1 if enabled */
void shell_choose(int menu, int item);                      /* a command chosen in the native menu */
void shell_request_quit(void);
void shell_focus(int active);

#endif
