/* In-memory Mac file system for deterministic runs.
 * Volume -1. Directory 30 = the application folder (original files, read-only host-backed).
 * Directories 19..22 = System/Preferences/Temp/other (empty at start). Everything created lives in RAM. */
#include "hx.h"
#include "m68k.h"
#include <stdlib.h>
#include <string.h>

#define APPDIR 30
#define MAXFILES 64
#define MAXFCB 16

typedef struct {
    int used; int32_t dir; uint8_t name[64];        /* Pascal string, Str63 */
    uint8_t *data; uint32_t dlen, dcap;
    uint8_t *rsrc; uint32_t rlen, rcap;
    uint8_t finfo[16];
    int readonly;                                   /* files from the game's archive */
} VFile;
typedef struct { int used; int file; int fork; uint32_t mark; } FCB;   /* fork 0 data, 1 rsrc */

static VFile vf[MAXFILES];
static FCB fcb[MAXFCB];

/* File reference numbers are offsets into the FCB buffer, as on a Mac (some code finds a file's
 * volume from its FCB: FCBSPtr $34E, FSFCBLen $3F6, fcbVPtr +$14 -> vcbVRefNum +$4E). */
#define FCB_LEN   0x5E
#define FCB_BUF   0x00FE4000u
#define VCB_ADDR  (FCB_BUF + 2 + MAXFCB * FCB_LEN + 0x10)
#define FCB_REF(i) (2 + (i) * FCB_LEN)
void vfs_init(void) {
    wr32(0x034E, FCB_BUF); wr16(0x03F6, FCB_LEN);
    wr16(FCB_BUF, (uint16_t)(2 + MAXFCB * FCB_LEN));
    for (int i = 0; i < MAXFCB; i++) wr32(FCB_BUF + FCB_REF(i) + 0x14, VCB_ADDR);
    wr16(VCB_ADDR + 0x4E, (uint16_t)-1);
}

static int name_eq(const uint8_t *a, const uint8_t *b) {  /* HFS names compare case-insensitively */
    if (a[0] != b[0]) return 0;
    for (int i = 1; i <= a[0]; i++) { int x = a[i], y = b[i]; if (x >= 'a' && x <= 'z') x -= 32; if (y >= 'a' && y <= 'z') y -= 32; if (x != y) return 0; }
    return 1;
}
static int find(int32_t dir, const uint8_t *nm) {
    for (int i = 0; i < MAXFILES; i++) if (vf[i].used && vf[i].dir == dir && name_eq(vf[i].name, nm)) return i;
    return -1;
}
static int add(int32_t dir, const uint8_t *nm) {
    for (int i = 0; i < MAXFILES; i++) if (!vf[i].used) {
        memset(&vf[i], 0, sizeof vf[i]); vf[i].used = 1; vf[i].dir = dir; memcpy(vf[i].name, nm, nm[0] + 1); return i;
    }
    return -1;
}

/* Adds a file from the game's archive to the application folder (forks are copied). */
void vfs_add_file(const char *macname, uint32_t type, uint32_t creator, const uint8_t *data, uint32_t dlen, const uint8_t *rsrc, uint32_t rlen) {
    uint8_t nm[64]; size_t L = strlen(macname); if (L > 63) L = 63; nm[0] = (uint8_t)L; memcpy(nm + 1, macname, L);
    int i = add(APPDIR, nm);
    vf[i].readonly = 1;
    if (dlen) { vf[i].data = malloc(dlen); memcpy(vf[i].data, data, dlen); vf[i].dlen = vf[i].dcap = dlen; }
    if (rlen) { vf[i].rsrc = malloc(rlen); memcpy(vf[i].rsrc, rsrc, rlen); vf[i].rlen = vf[i].rcap = rlen; }
    vf[i].finfo[0] = type >> 24; vf[i].finfo[1] = type >> 16; vf[i].finfo[2] = type >> 8; vf[i].finfo[3] = type;
    vf[i].finfo[4] = creator >> 24; vf[i].finfo[5] = creator >> 16; vf[i].finfo[6] = creator >> 8; vf[i].finfo[7] = creator;
}

/* OpenResFile(name): looks in the application folder */
int vfs_open_res_by_name(const uint8_t *pn) {
    int f = find(APPDIR, pn);
    if (f < 0 || !vf[f].rlen) return -1;
    char mn[64]; memcpy(mn, pn + 1, pn[0] > 63 ? 63 : pn[0]); mn[pn[0] > 63 ? 63 : pn[0]] = 0;
    int rs_open_mem(const uint8_t *buf, uint32_t len, const char *macname);
    return rs_open_mem(vf[f].rsrc, vf[f].rlen, mn);
}

/* resolve (vRefNum, dirID, name) -> directory used by the spec */
static int32_t norm_dir(int16_t vref, int32_t dir) { (void)vref; return dir == 0 || dir == 2 ? APPDIR : dir; }

static void read_spec(uint32_t sp, int32_t *dir, uint8_t *nm) {
    *dir = norm_dir((int16_t)rd16(sp), (int32_t)rd32(sp + 2));
    memcpy(nm, g_mem + sp + 6, rd8(sp + 6) + 1);
}

int vfs_open_fork(int file, int fork) {
    for (int i = 0; i < MAXFCB; i++) if (!fcb[i].used) { fcb[i].used = 1; fcb[i].file = file; fcb[i].fork = fork; fcb[i].mark = 0; return FCB_REF(i); }
    return 0;
}
static FCB *fcb_of(int16_t ref) {
    if (ref < 2 || (ref - 2) % FCB_LEN) return NULL;
    int i = (ref - 2) / FCB_LEN; return (i < MAXFCB && fcb[i].used) ? &fcb[i] : NULL;
}

int rs_open_mem(const uint8_t *buf, uint32_t len, const char *macname);

/* FSSpec dispatcher (HighLevelFSDispatch, selector in D0 low nibble) */
void vfs_hlfs(uint16_t w, uint32_t trap_pc, const char *where) {
    (void)w; (void)trap_pc;
    int sel = reg(M68K_REG_D0) & 0xF;
    uint8_t nm[256]; int32_t dir; int f;
    switch (sel) {
    case 1: { /* FSMakeFSSpec(vRefNum, dirID, fileName, VAR spec) */
        uint32_t spec = pop32(), pn = pop32(); int32_t d = (int32_t)pop32(); int16_t v = (int16_t)pop16();
        memcpy(nm, g_mem + pn, rd8(pn) + 1);
        if (nm[0] && nm[1] == ':') { memmove(nm + 1, nm + 2, nm[0] - 1); nm[0]--; }   /* ":name" relative path */
        dir = norm_dir(v, d);
        wr16(spec, (uint16_t)-1); wr32(spec + 2, (uint32_t)dir); { hx_track(spec + 6, (uint32_t)(nm[0] + 1)); memcpy(g_mem + spec + 6, nm, nm[0] + 1); }
        wr16(reg(M68K_REG_A7), find(dir, nm) >= 0 ? 0 : (uint16_t)-43);
        hx_log("FSMakeFSSpec(%d,%d,\"%.*s\") at %s -> %s", v, d, nm[0], nm + 1, where, find(dir, nm) >= 0 ? "noErr" : "fnfErr");
        break; }
    case 2: case 3: { /* FSpOpenDF / FSpOpenRF(spec, perm, VAR refNum) */
        uint32_t pref = pop32(); pop16(); uint32_t spec = pop32();
        read_spec(spec, &dir, nm); f = find(dir, nm);
        if (f < 0) { wr16(reg(M68K_REG_A7), (uint16_t)-43); break; }
        wr16(pref, (uint16_t)vfs_open_fork(f, sel == 3)); wr16(reg(M68K_REG_A7), 0); break; }
    case 4: { /* FSpCreate(spec, creator, fileType, scriptTag) */
        pop16(); uint32_t type = pop32(), cr = pop32(), spec = pop32();
        read_spec(spec, &dir, nm);
        if (find(dir, nm) >= 0) { wr16(reg(M68K_REG_A7), (uint16_t)-48); break; }
        f = add(dir, nm);
        uint8_t *fi = vf[f].finfo; fi[0] = type >> 24; fi[1] = type >> 16; fi[2] = type >> 8; fi[3] = type; fi[4] = cr >> 24; fi[5] = cr >> 16; fi[6] = cr >> 8; fi[7] = cr;
        hx_log("FSpCreate(dir %d \"%.*s\") at %s", dir, nm[0], nm + 1, where);
        wr16(reg(M68K_REG_A7), 0); break; }
    case 6: { /* FSpDelete(spec) */
        uint32_t spec = pop32(); read_spec(spec, &dir, nm); f = find(dir, nm);
        if (f >= 0 && !vf[f].readonly) { free(vf[f].data); free(vf[f].rsrc); vf[f].used = 0; }
        wr16(reg(M68K_REG_A7), f >= 0 ? 0 : (uint16_t)-43); break; }
    case 7: case 8: { /* FSpGetFInfo / FSpSetFInfo(spec, finfo) */
        uint32_t fi = pop32(), spec = pop32(); read_spec(spec, &dir, nm); f = find(dir, nm);
        if (f < 0) { wr16(reg(M68K_REG_A7), (uint16_t)-43); break; }
        if (sel == 7) { hx_track(fi, (uint32_t)(16)); memcpy(g_mem + fi, vf[f].finfo, 16); } else memcpy(vf[f].finfo, g_mem + fi, 16);
        wr16(reg(M68K_REG_A7), 0); break; }
    case 0xD: { /* FSpOpenResFile(spec, permission): refNum */
        pop16(); uint32_t spec = pop32(); read_spec(spec, &dir, nm); f = find(dir, nm);
        int ref = -1; char mn[64]; memcpy(mn, nm + 1, nm[0]); mn[nm[0]] = 0;
        if (f >= 0 && vf[f].rlen) ref = rs_open_mem(vf[f].rsrc, vf[f].rlen, mn);
        if (ref < 0) { wr16(LM_ResErr, f < 0 ? (uint16_t)-43 : (uint16_t)-39); ref = -1; }
        hx_log("FSpOpenResFile(\"%s\") at %s -> %d", mn, where, ref);
        wr16(reg(M68K_REG_A7), (uint16_t)ref); break; }
    case 0xE: { /* FSpCreateResFile(spec, creator, fileType, scriptTag) */
        pop16(); uint32_t type = pop32(), cr = pop32(), spec = pop32();
        read_spec(spec, &dir, nm); f = find(dir, nm); if (f < 0) f = add(dir, nm);
        uint8_t *fi = vf[f].finfo; fi[0] = type >> 24; fi[1] = type >> 16; fi[2] = type >> 8; fi[3] = type; fi[4] = cr >> 24; fi[5] = cr >> 16; fi[6] = cr >> 8; fi[7] = cr;
        /* empty resource fork: 256-byte header area + 30-byte map with no types */
        static const uint8_t empty[286] = {0,0,1,0, 0,0,1,0, 0,0,0,0, 0,0,0,0x1E};
        uint8_t e[286]; memcpy(e, empty, sizeof e);
        e[256 + 24] = 0; e[256 + 25] = 28; e[256 + 26] = 0; e[256 + 27] = 30; e[256 + 28] = 0xFF; e[256 + 29] = 0xFF;
        free(vf[f].rsrc); vf[f].rsrc = malloc(sizeof e); memcpy(vf[f].rsrc, e, sizeof e); vf[f].rlen = vf[f].rcap = sizeof e;
        hx_log("FSpCreateResFile(dir %d \"%.*s\") at %s", dir, nm[0], nm + 1, where);
        wr16(LM_ResErr, 0); break; }
    case 0xF: { /* FSpExchangeFiles(source, dest): the two files swap contents (both forks), keeping their names */
        uint32_t dspec = pop32(), sspec = pop32(); int32_t d2; uint8_t nm2[256];
        read_spec(sspec, &dir, nm); read_spec(dspec, &d2, nm2);
        int a = find(dir, nm), b = find(d2, nm2);
        if (a < 0 || b < 0) { wr16(reg(M68K_REG_A7), (uint16_t)-43); break; }
        if (vf[a].readonly || vf[b].readonly) { wr16(reg(M68K_REG_A7), (uint16_t)-61); break; }   /* wrPermErr */
        VFile t = vf[a];
        vf[a].data = vf[b].data; vf[a].dlen = vf[b].dlen; vf[a].dcap = vf[b].dcap;
        vf[a].rsrc = vf[b].rsrc; vf[a].rlen = vf[b].rlen; vf[a].rcap = vf[b].rcap;
        vf[b].data = t.data; vf[b].dlen = t.dlen; vf[b].dcap = t.dcap;
        vf[b].rsrc = t.rsrc; vf[b].rlen = t.rlen; vf[b].rcap = t.rcap;
        hx_log("FSpExchangeFiles(\"%.*s\", \"%.*s\") at %s", nm[0], nm + 1, nm2[0], nm2 + 1, where);
        wr16(reg(M68K_REG_A7), 0); break; }
    default:
        hx_fault("HighLevelFSDispatch selector %d not implemented at %s", sel, where);
    }
}

/* ---------- parameter-block file I/O (OS traps) ---------- */
static uint8_t **fork_buf(FCB *c, uint32_t **len, uint32_t **cap) {
    VFile *v = &vf[c->file];
    if (c->fork) { *len = &v->rlen; *cap = &v->rcap; return &v->rsrc; }
    *len = &v->dlen; *cap = &v->dcap; return &v->data;
}
void vfs_pb(uint8_t trapnum, const char *where) {
    uint32_t pb = reg(M68K_REG_A0);
    int16_t ref = (int16_t)rd16(pb + 24);
    FCB *c = fcb_of(ref);
    int16_t err = 0;
    if (!c) { err = -51; goto done; }
    uint32_t *len, *cap; uint8_t **buf = fork_buf(c, &len, &cap);
    switch (trapnum) {
    case 0x01: c->used = 0; break;                                    /* PBClose */
    case 0x02: case 0x03: {                                            /* PBRead / PBWrite */
        uint16_t mode = rd16(pb + 44); int32_t off = (int32_t)rd32(pb + 46);
        uint32_t pos = (mode & 3) == 1 ? (uint32_t)off : (mode & 3) == 2 ? *len + off : (mode & 3) == 3 ? c->mark + off : c->mark;
        uint32_t n = rd32(pb + 36), b = rd32(pb + 32);
        if (trapnum == 0x02) {
            if (pos > *len) { err = -39; n = 0; } else if (pos + n > *len) { n = *len - pos; err = -39; }
            { hx_track(b, (uint32_t)(n)); memcpy(g_mem + b, *buf + pos, n); }
        } else {
            if (pos + n > *cap) { *cap = (pos + n) * 2 + 64; *buf = realloc(*buf, *cap); }
            if (pos > *len) memset(*buf + *len, 0, pos - *len);
            memcpy(*buf + pos, g_mem + b, n);
            if (pos + n > *len) *len = pos + n;
        }
        c->mark = pos + n; wr32(pb + 40, n); wr32(pb + 46, c->mark); break; }
    case 0x11: wr32(pb + 28, *len); break;                             /* PBGetEOF */
    case 0x12: { uint32_t e = rd32(pb + 28); if (e > *cap) { *cap = e; *buf = realloc(*buf, e); } if (e > *len) memset(*buf + *len, 0, e - *len); *len = e; if (c->mark > e) c->mark = e; break; }
    case 0x18: wr32(pb + 46, c->mark); break;                          /* PBGetFPos */
    case 0x44: {                                                       /* PBSetFPos */
        uint16_t mode = rd16(pb + 44); int32_t off = (int32_t)rd32(pb + 46);
        uint32_t pos = (mode & 3) == 1 ? (uint32_t)off : (mode & 3) == 2 ? *len + off : (mode & 3) == 3 ? c->mark + off : c->mark;
        if (pos > *len) { err = -39; pos = *len; }
        c->mark = pos; wr32(pb + 46, pos); break; }
    default: hx_fault("file trap %02X not implemented at %s", trapnum, where); return;
    }
done:
    wr16(pb + 16, (uint16_t)err);
    setreg(M68K_REG_D0, (uint32_t)(int32_t)err);
}

/* The port keeps the preferences on the host: a file put into a folder before the game starts,
 * and read back when it quits. dir: 20 = the Preferences folder. */
void vfs_put_file(int32_t dir, const char *macname, uint32_t type, uint32_t creator, const uint8_t *data, uint32_t dlen) {
    uint8_t nm[64]; size_t L = strlen(macname); if (L > 63) L = 63; nm[0] = (uint8_t)L; memcpy(nm + 1, macname, L);
    int i = find(dir, nm); if (i < 0) i = add(dir, nm); if (i < 0) return;
    free(vf[i].data); vf[i].data = NULL; vf[i].dlen = vf[i].dcap = 0;
    if (dlen) { vf[i].data = malloc(dlen); memcpy(vf[i].data, data, dlen); vf[i].dlen = vf[i].dcap = dlen; }
    vf[i].finfo[0] = type >> 24; vf[i].finfo[1] = type >> 16; vf[i].finfo[2] = type >> 8; vf[i].finfo[3] = type;
    vf[i].finfo[4] = creator >> 24; vf[i].finfo[5] = creator >> 16; vf[i].finfo[6] = creator >> 8; vf[i].finfo[7] = creator;
}
const uint8_t *vfs_get_file(int32_t dir, const char *macname, uint32_t *dlen) {
    uint8_t nm[64]; size_t L = strlen(macname); if (L > 63) L = 63; nm[0] = (uint8_t)L; memcpy(nm + 1, macname, L);
    int i = find(dir, nm);
    if (i < 0) return NULL;
    *dlen = vf[i].dlen; return vf[i].data;
}
