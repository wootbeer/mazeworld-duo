/* The native port: what the runtime (rt.c), the time module (vbl.c) and the front end share. */
#ifndef PORT_H
#define PORT_H
#include <stdint.h>
#include <stddef.h>

int  port_boot(const char *archive, const char *logpath, char *err, size_t errlen);
void port_run_game(void);
void port_prefs_load(const char *path);   /* before port_run_game: the game's preferences file */
void port_prefs_save(const char *path);   /* after it: written back if the game saved it */            /* returns when the game quits (or port_quit was called) */

/* time */
uint32_t vbl_ticks(void);
void vbl_advance(uint32_t n);

/* provided by the front end */
void port_tick(uint32_t tick);       /* once per tick of game time: pacing, events, presentation */
void port_quit(void);                /* the run is over: does not return */
void port_fatal(const char *msg);    /* does not return */
#endif
