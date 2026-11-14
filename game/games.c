/* One C game for both builds: Mazeworld Catacombs (as translated) and Mazeworld Original, whose
 * shared code differs mostly in where things are: its globals sit at other A5 offsets, and its
 * jump-table slots and functions at other places. game/orig_map.h (tools/map_builds.py) pairs
 * them up; for Original the global offsets in the C code are translated through mz_gmap. */
#include "mz.h"
#include "orig_map.h"
#include <string.h>

int mz_original;
int mz_alt_controls;
int mz_full_labels;
int16_t mz_alt_move[3];
const uint16_t *mz_gmap;
static uint16_t gmap[0x10000];

/* Every offset belongs to the nearest Catacombs global at or above it (a struct or array is
 * addressed from its base, the field at base - k); it moves with that global. */
void mz_select_original(void) {
    int n = (int)(sizeof orig_globals / sizeof orig_globals[0]), k = 0;
    for (uint32_t off = 0; off < 0x10000; off++) {
        while (k < n && orig_globals[k][0] < off) k++;
        if (k < n && orig_globals[k][1]) gmap[off] = (uint16_t)(orig_globals[k][1] - (orig_globals[k][0] - off));
        else gmap[off] = 0;
    }
    mz_original = 1;
    mz_gmap = gmap;
}

/* the C code reached a Catacombs global with no Original counterpart: a translation error */
uint32_t mz_gnone(uint32_t off) {
    char msg[64];
    static const char hex[] = "0123456789ABCDEF";
    memcpy(msg, "Original has no global G_", 25);
    for (int i = 0; i < 4; i++) msg[25 + i] = hex[(off >> (12 - 4 * i)) & 15];
    msg[29] = 0;
    mz_unsupported(msg);
    return MZ_A5;
}

/* the address of jump-table entry `off` (above A5) in the running game */
uint32_t mz_jt(uint16_t off) {
    if (mz_original)
        for (unsigned i = 0; i < sizeof orig_jt / sizeof orig_jt[0]; i++) if (orig_jt[i][0] == off) return MZ_A5 + orig_jt[i][1];
    return MZ_A5 + off;
}

static int pair_of(int seg, uint16_t off) {
    for (unsigned i = 0; i < sizeof orig_funcs / sizeof orig_funcs[0]; i++)
        if (orig_funcs[i][0] == seg && orig_funcs[i][1] == off) return (int)i;
    return -1;
}
/* where translated function seg:off is in Original (segment << 16 | offset), or 0 if it cannot
 * run there as it is */
uint32_t mz_orig_func(int seg, uint16_t off) {
    int i = pair_of(seg, off);
    return i >= 0 && orig_funcs[i][4] ? (uint32_t)orig_funcs[i][2] << 16 | orig_funcs[i][3] : 0;
}
/* before a call into function seg:off of another segment: loads the segment it is in */
void mz_seg_of(int seg, uint16_t off) {
    int i = mz_original ? pair_of(seg, off) : -1;
    mz_seg(i >= 0 ? orig_funcs[i][2] : seg);
}
