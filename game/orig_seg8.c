/* CODE segment 8 of Mazeworld Original: its own routines (no Catacombs counterpart), named by
 * Original's offsets (see build/disasm-original/seg8.s). */
#include "mzasm.h"

/* The player's state in the current level's record (G_53CC; the record is at the offset in its
 * first word): word +2 level number, longs +4 / +8 x / y, words +C heading, +E, +10, +12, +16,
 * +30 .. +3C, and per level number words at +18 and +24. fo_S8_065A loads them into the globals,
 * fo_S8_06DC stores them back. */
static const struct { uint8_t at; uint16_t g; uint8_t len; } player_state[] = {
    {0x02, 0x56DC, 2}, {0x04, 0x56D8, 4}, {0x08, 0x56D4, 4}, {0x0C, 0x56DA, 2}, {0x0E, 0x56EA, 2},
    {0x10, 0x56E6, 2}, {0x12, 0x56E2, 2}, {0x16, 0x56C6, 2}, {0x30, 0x56E0, 2}, {0x32, 0x56C2, 2},
    {0x34, 0x56C0, 2}, {0x36, 0x56BE, 2}, {0x38, 0x56BC, 2}, {0x3A, 0x56BA, 2}, {0x3C, 0x56B8, 2},
};
static uint32_t level_record(Regs *r) {
    r->a[2] = og32(0x53CC);
    r->d[0] = r16(r->a[2]);
    r->a[2] += r->d[0];
    return r->a[2];
}
void fo_S8_065A(Regs *r) {
    uint32_t p = level_record(r);
    for (unsigned i = 0; i < sizeof player_state / sizeof *player_state; i++)
        if (player_state[i].len == 4) sog32(player_state[i].g, r32(p + player_state[i].at));
        else sog16(player_state[i].g, r16(p + player_state[i].at));
    set_w(&r->d[2], og16(0x56DC)); sub_w(r, &r->d[2], 1); lsl_w(r, &r->d[2], 1);
    sog16(0x56A8, r16(p + sxw(r->d[2]) + 0x18));
    sog16(0x56A6, r16(p + sxw(r->d[2]) + 0x24));
    sog16(0x582E, 0); sog16(0x582C, 0);
    cc_nz_w(r, 0);
}
void fo_S8_06DC(Regs *r) {
    uint32_t p = level_record(r);
    for (unsigned i = 0; i < sizeof player_state / sizeof *player_state; i++)
        if (player_state[i].len == 4) w32(p + player_state[i].at, og32(player_state[i].g));
        else w16(p + player_state[i].at, og16(player_state[i].g));
    set_w(&r->d[2], og16(0x56DC)); sub_w(r, &r->d[2], 1); lsl_w(r, &r->d[2], 1);
    w16(p + sxw(r->d[2]) + 0x18, og16(0x56A8));
    w16(p + sxw(r->d[2]) + 0x24, og16(0x56A6));
    cc_nz_w(r, og16(0x56A6));
}

/* fo_S8_0510: builds the level's map G_53D4 from 'mazr' 199 + level (the word argument): 32 x 32
 * cells of $20 bytes, each from one byte of the resource: +0 the byte, +$E = 1 if it is solid
 * (bit 0), +$1C / +$1E the cell centre (8 + 16 x, 8 + 16 y), the rest 0. D0.l = 1, or 0 if the
 * resource is missing. Register routine called as C (it leaves D4 / D5 as it ran them). */
void fo_S8_0510(Regs *r) {
    int16_t level = (int16_t)r16(r->a[7] + 4);
    uint32_t h = mac_GetResource(0x6D617A72, (int16_t)(level + 0xC7));   /* 'mazr' */
    if (mac_ResError() || !h) { r->d[0] = 0; return; }
    r->a[0] = h; mac_trap_regs(r, 0xA029);                    /* HLock */
    uint32_t p = r32(h), cell = og32(0x53D4);
    for (uint16_t y = 8; y < 0x200; y += 0x10)
        for (uint16_t x = 8; x < 0x200; x += 0x10) {
            uint8_t b = r8(p++);
            w8(cell, b); w8(cell + 1, 0); w16(cell + 2, 0);
            for (uint32_t k = 4; k < 0x1C; k += 4) w32(cell + k, 0);
            if (b & 1) w16(cell + 0xE, 1);
            w16(cell + 0x1C, x); w16(cell + 0x1E, y);
            cell += 0x20;
        }
    r->d[4] = (r->d[4] & 0xFFFF0000u) | 8; r->d[5] = (r->d[5] & 0xFFFF0000u) | 0x208;
    r->a[0] = h; mac_trap_regs(r, 0xA02A);                    /* HUnlock */
    mac_ReleaseResource(h);
    r->d[0] = 1;
}

/* fo_S8_07D4: copies the objects G_53C8 (records of 64 bytes, words from +2) into the level
 * records G_53CC: first into the records of the levels before (each record's first word is its
 * length in words, the table of record offsets ends at the first record), then into the level's
 * own (the word argument; its offset is in that table), until 120 objects. Register routine called
 * as C (D3 / D4, A2 / A3 as it ran them). */
void fo_S8_07D4(Regs *r) {
    int16_t level = (int16_t)r16(r->a[7] + 4);
    r->d[4] = 0;
    uint32_t a2 = og32(0x53CC);
    r->d[3] = a2;
    a2 += r16(a2); r->d[3] += r16(og32(0x53CC) + 2);
    a2 += (uint32_t)r16(a2) << 1;
    uint32_t a3 = 0;
    while (a2 != r->d[3]) {
        a3 = og32(0x53C8) + (r->d[4] << 6);
        uint32_t a0 = a3 + 2;
        uint16_t n = r16(a2); a2 += 2;
        if (!--n) goto own;
        do { w16(a2, r16(a0)); a2 += 2; a0 += 2; } while (--n);
        r->d[4]++;
    }
own:
    a2 = og32(0x53CC);
    a2 += r16(a2 + sxw((uint16_t)(level << 1)));
    do {
        a3 = og32(0x53C8) + (r->d[4] << 6);
        uint32_t a0 = a3 + 2;
        uint16_t n = r16(a2); a2 += 2;
        if (!--n) break;
        do { w16(a2, r16(a0)); a2 += 2; a0 += 2; } while (--n);
        r->d[4]++;
    } while ((uint16_t)r->d[4] != 0x78);
    r->a[2] = a2; r->a[3] = a3;
}

/* fo_S8_0756: the reverse of fo_S8_07D4: fills the objects G_53C8 from the level records (the
 * records of the levels before, then the level's own until 120 objects; each record, its length
 * word first, is copied to the object's start) and enters each into the map (fn_S8_0468, Original
 * fn_S8_0852). Register routine called as C. */
void fn_S8_0468(Regs *r);
static void copy_in(Regs *r, uint32_t *a2) {
    r->a[3] = og32(0x53C8) + (r->d[4] << 6);
    uint32_t a0 = r->a[3];
    uint16_t n = r16(*a2);
    do { w16(a0, r16(*a2)); a0 += 2; *a2 += 2; } while (--n);
    r->a[0] = a0;
    r->d[0] = r->d[4];
    fn_S8_0468(r);
    r->d[4]++;
}
void fo_S8_0756(Regs *r) {
    int16_t level = (int16_t)r16(r->a[7] + 4);
    r->d[4] = 0;
    uint32_t a2 = og32(0x53CC);
    r->d[3] = a2;
    r->d[3] += r16(a2 + 2); a2 += r16(a2);
    a2 += (uint32_t)r16(a2) << 1;
    while (a2 != r->d[3]) copy_in(r, &a2);
    a2 = og32(0x53CC);
    a2 += r16(a2 + sxw((uint16_t)(level << 1)));
    do copy_in(r, &a2); while ((uint16_t)r->d[4] != 0x78);
    r->a[2] = a2;
}

/* ---------- Mazeworld Original's player, each frame (fn_S8_1152): steering by mouse ---------- */
void fn_S8_1376(Regs *r);
void fn_S8_138A(Regs *r);
void fn_S8_139E(Regs *r);
void fn_S8_13B6(Regs *r);

static void clamp_max(uint16_t g, int16_t max) { if ((int16_t)og16(g) > max) sog16(g, (uint16_t)max); }

/* fo_S8_121A: the mood G_56E2 sets the two mood axes G_71F4 / G_71F6 (0, 100 or 200 each) */
void fo_S8_121A(void) {
    static const uint16_t axes[5][2] = {{0x64, 0x64}, {0, 0}, {0xC8, 0}, {0, 0xC8}, {0xC8, 0xC8}};
    uint16_t m = og16(0x56E2);
    if (m > 9) return;
    sog16(0x71F4, axes[m % 5][0]); sog16(0x71F6, axes[m % 5][1]);
}
/* fo_S8_1282: the mouse moves the mood axes (each kept in 0..199); the mood follows: 6 both under
 * 70, 7 the first over 130 and the second under 70, 8 the reverse, 9 both over 130, else 5 */
void fo_S8_1282(void) {
    sog16(0x71F4, (uint16_t)(og16(0x71F4) + og16(0x7200)));
    sog16(0x71F6, (uint16_t)(og16(0x71F6) + og16(0x7202)));
    static const uint16_t axis[2] = {0x71F4, 0x71F6};
    for (int k = 0; k < 2; k++) {
        if ((int16_t)og16(axis[k]) < 0) sog16(axis[k], 0);
        if ((int16_t)og16(axis[k]) >= 0xC8) sog16(axis[k], 0xC7);
    }
    int16_t a = (int16_t)og16(0x71F4), b = (int16_t)og16(0x71F6);
    sog16(0x56E2, a < 0x46 && b < 0x46 ? 6 : a > 0x82 && b < 0x46 ? 7 : a < 0x46 && b > 0x82 ? 8 : a > 0x82 && b > 0x82 ? 9 : 5);
}
/* fo_S8_132A: the mouse turns and moves the player: the horizontal movement G_7200 accumulates in
 * G_71F0 (-25..25), every 10 a turn step (fn_S8_1376 right / 138A left, at most two at a time);
 * the vertical G_7202 in G_71F2 (-18..13), every 5 a step (fn_S8_13B6 forward / 139E back, at most
 * two forward, three back). No movement clears the accumulator. */
void fo_S8_132A(void) {
    Regs r = {{0}, {0}, 0};
    if (!og16(0x7200)) sog16(0x71F0, 0);
    else {
        sog16(0x71F0, (uint16_t)(og16(0x71F0) + og16(0x7200)));
        if ((int16_t)og16(0x71F0) > 0) {
            clamp_max(0x71F0, 0x19);
            if ((int16_t)og16(0x71F0) > 0x14) { fn_S8_1376(&r); fn_S8_1376(&r); sog16(0x71F0, (uint16_t)(og16(0x71F0) - 0x14)); }
            else if ((int16_t)og16(0x71F0) > 0xA) { fn_S8_1376(&r); sog16(0x71F0, (uint16_t)(og16(0x71F0) - 0xA)); }
        } else {
            if ((int16_t)og16(0x71F0) < -0x19) sog16(0x71F0, (uint16_t)-0x19);
            if ((int16_t)og16(0x71F0) < -0x14) { fn_S8_138A(&r); fn_S8_138A(&r); sog16(0x71F0, (uint16_t)(og16(0x71F0) + 0x14)); }
            else if ((int16_t)og16(0x71F0) < -0xA) { fn_S8_138A(&r); sog16(0x71F0, (uint16_t)(og16(0x71F0) + 0xA)); }
        }
    }
    if (!og16(0x7202)) { sog16(0x71F2, 0); return; }
    sog16(0x71F2, (uint16_t)(og16(0x71F2) + og16(0x7202)));
    if ((int16_t)og16(0x71F2) > 0) {
        clamp_max(0x71F2, 0xD);
        if ((int16_t)og16(0x71F2) > 0xA) { fn_S8_13B6(&r); fn_S8_13B6(&r); sog16(0x71F2, (uint16_t)(og16(0x71F2) - 0xA)); }
        if ((int16_t)og16(0x71F2) > 5) { fn_S8_13B6(&r); sog16(0x71F2, (uint16_t)(og16(0x71F2) - 5)); }
    } else {
        if ((int16_t)og16(0x71F2) < -0x12) sog16(0x71F2, (uint16_t)-0x12);
        if ((int16_t)og16(0x71F2) < -0xF) { fn_S8_139E(&r); fn_S8_139E(&r); fn_S8_139E(&r); sog16(0x71F2, (uint16_t)(og16(0x71F2) + 0xF)); }
        else if ((int16_t)og16(0x71F2) < -0xA) { fn_S8_139E(&r); fn_S8_139E(&r); sog16(0x71F2, (uint16_t)(og16(0x71F2) + 0xA)); }
        else if ((int16_t)og16(0x71F2) < -5) { fn_S8_139E(&r); sog16(0x71F2, (uint16_t)(og16(0x71F2) + 5)); }
    }
}
/* port option (mz_alt_controls): the keys' and mouse's movement, a step each 2 units (the remainder
 * kept), as Catacombs' fn_S8_0E80: turn right / left, forward / back, sideways */
void fn_S8_13D2(Regs *r);
void fn_S8_13EE(Regs *r);
static void alt_moves(void) {
    Regs r = {{0}, {0}, 0};
    static void (*const fn[3][2])(Regs *) = {{fn_S8_1376, fn_S8_138A}, {fn_S8_139E, fn_S8_13B6}, {fn_S8_13EE, fn_S8_13D2}};
    for (int k = 0; k < 3; k++) {
        while (mz_alt_move[k] >= 2)  { fn[k][0](&r); mz_alt_move[k] -= 2; }
        while (mz_alt_move[k] <= -2) { fn[k][1](&r); mz_alt_move[k] += 2; }
    }
}
/* fo_S8_1152: each frame: the frame count G_56C4; with health G_56EA the second gauge G_56E6 grows
 * every 4th frame and health every 16th (both to $8E); without, G_56E6 = 0 and G_56E0 = 7. The
 * mood (G_71FE on, not yet set G_71F8): the counters G_71FA (frames) and G_71FC (frames with the
 * mouse nearly still) run to 32000; set from the mood (fo_S8_121A) once either says so. Then the
 * mouse steers the mood (fo_S8_1282) once set, or the player (fo_S8_132A). */
void fo_S8_1152(void) {
    sog16(0x56C4, (uint16_t)(og16(0x56C4) + 1));
    if (!og16(0x56EA)) { sog16(0x56E6, 0); sog16(0x56E0, 7); }
    else {
        if (!(og16(0x56C4) & 3)) { sog16(0x56E6, (uint16_t)(og16(0x56E6) + 1)); if (og16(0x56E6) > 0x8E) sog16(0x56E6, 0x8E); }
        if ((og16(0x56C4) & 0xF) == 7) { sog16(0x56EA, (uint16_t)(og16(0x56EA) + 1)); if (og16(0x56EA) > 0x8E) sog16(0x56EA, 0x8E); }
    }
    if (og8(0x71FE) && !og8(0x71F8)) {
        sog16(0x71FA, (uint16_t)(og16(0x71FA) + 1)); clamp_max(0x71FA, 0x7D00);
        int16_t dx = (int16_t)og16(0x7200), dy = (int16_t)og16(0x7202);
        if (dx < 5 && dx > -5 && dy < 5 && dy > -5) { sog16(0x71FC, (uint16_t)(og16(0x71FC) + 1)); clamp_max(0x71FC, 0x7D00); }
        if ((int16_t)og16(0x71FC) >= 1 || (int16_t)og16(0x71FA) >= 2) { fo_S8_121A(); sog8(0x71F8, 1); }
    }
    if (mz_alt_controls) alt_moves();
    else if (og8(0x71F8)) fo_S8_1282(); else fo_S8_132A();
}

/* fo_S8_0F5A: the full-view pictures while G_56E0 counts (each over the whole view): 1-6 the dying
 * fade (frame G_56E0 - 1 of G_5380 in colour 0; counts down), 7-18 the level's end (frame
 * (G_56E0 - 7) / 2 + ... of G_5380 in colour $2B; counts up, and at 19 ends the level: G_5787,
 * G_57F2, G_57F1 set, G_5786 cleared), and the still pictures $80 (G_537C's first, colour G_56DE),
 * $81 (G_53A4's 16th, colour G_56DE), $82 / $83 (G_5338 entry $58, colours $25 / $26). */
static void whole_view(uint32_t stream, uint16_t dist) {
    Regs r = {{0}, {0}, 0};
    sog32(0x56AE, stream);
    sog16(0x56B4, (uint16_t)(og16(0x5736) + 4)); sog16(0x56B2, 4); sog16(0x56B0, (uint16_t)(og16(0x5738) + 4));
    sog16(0x56B6, dist);
    r.a[0] = stream;
    mz_draw_stream(&r, stream);
}
static uint32_t model(uint32_t table, uint32_t off) { return table + r32(table + off); }
void fo_S8_0F5A(void) {
    int16_t t = (int16_t)og16(0x56E0);
    if (!t) return;
    if (t < 7) {
        sog16(0x56AA, 0);
        whole_view(model(og32(0x5380), sxw((uint16_t)((t - 1) << 2))), 7);
        sog16(0x56E0, (uint16_t)(og16(0x56E0) - 1));
    } else if (t < 0x13) {
        sog16(0x56AA, 0x2B);
        whole_view(model(og32(0x5380), (uint32_t)(uint16_t)(((uint16_t)(t - 7) >> 1) << 2)), 7);
        uint16_t n = (uint16_t)(og16(0x56E0) + 1);
        if ((int16_t)n < 0x13) sog16(0x56E0, n);
        else { sog8(0x5787, 1); sog16(0x5786, 0); sog8(0x57F2, 1); sog8(0x57F1, 1); }
    } else if (t == 0x80) { sog16(0x56AA, og16(0x56DE)); whole_view(model(og32(0x537C), 0), 2); }
    else if (t == 0x81) { sog16(0x56AA, og16(0x56DE)); whole_view(model(og32(0x53A4), 0x40), 2); }
    else if (t == 0x82) { sog16(0x56AA, 0x25); whole_view(model(og32(0x5338), 0x58), 1); }
    else if (t == 0x83) { sog16(0x56AA, 0x26); whole_view(model(og32(0x5338), 0x58), 1); }
}

/* fo_S8_145E: the mouse button's shot, by the mood G_56E2 (5-9 count as 0-4, and are left so):
 * 0 none (G_56B8 = -1), 1-4 a shot of type $17-$1A (fn_S8_0F1A, Original fn_S8_14B4) */
void fn_S8_0F1A(Regs *r);
void fo_S8_145E(void) {
    if ((int16_t)og16(0x56E2) >= 5) sog16(0x56E2, (uint16_t)(og16(0x56E2) - 5));
    int16_t m = (int16_t)og16(0x56E2);
    if (m == 0) { sog16(0x56B8, 0xFFFF); return; }
    if (m < 0 || m > 4) return;
    Regs r = {{0}, {0}, 0};
    set_w(&r.d[0], (uint16_t)(0x16 + m));
    fn_S8_0F1A(&r);
}
