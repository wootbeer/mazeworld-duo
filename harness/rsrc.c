/* Resource Manager emulation over the original resource forks (AppleDouble or raw). */
#include "hx.h"
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t type; int16_t id; uint8_t attrs; uint8_t name[256]; uint32_t off, len; uint32_t handle; } Res;
typedef struct { int ref; char macname[64]; uint8_t *fork; uint32_t forklen; Res *res; int nres; int open; } RFile;

#define MAXF 8
static RFile files[MAXF];
static int nfiles = 0;      /* index order = open order; search goes newest -> oldest from current */
static int cur = -1;        /* index into files */


static uint8_t *load_fork(const char *path, uint32_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    if (n > 26 && b[0] == 0 && b[1] == 5 && b[2] == 0x16 && b[3] == 7) {  /* AppleDouble */
        int ne = (b[24] << 8) | b[25];
        for (int i = 0; i < ne; i++) {
            uint8_t *e = b + 26 + 12 * i;
            uint32_t id = (e[0] << 24) | (e[1] << 16) | (e[2] << 8) | e[3];
            uint32_t off = (e[4] << 24) | (e[5] << 16) | (e[6] << 8) | e[7];
            uint32_t ln = (e[8] << 24) | (e[9] << 16) | (e[10] << 8) | e[11];
            if (id == 2) { uint8_t *r = malloc(ln); memcpy(r, b + off, ln); free(b); *len = ln; return r; }
        }
        free(b); return NULL;
    }
    *len = (uint32_t)n; return b;
}

#define BE16(p) (uint16_t)(((p)[0] << 8) | (p)[1])
#define BE32(p) (uint32_t)(((uint32_t)(p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])

static void set_reserr(int e) { wr16(LM_ResErr, (uint16_t)e); }

static int parse_fork(RFile *rf) {
    uint8_t *f = rf->fork;
    uint32_t doff = BE32(f), moff = BE32(f + 4);
    uint8_t *m = f + moff;
    uint16_t tl = BE16(m + 24), nl = BE16(m + 26);
    uint8_t *t = m + tl;
    int nt = BE16(t) + 1, total = 0;
    for (int i = 0; i < nt; i++) total += BE16(t + 2 + 8 * i + 4) + 1;
    rf->res = calloc(total, sizeof(Res)); rf->nres = 0;
    for (int i = 0; i < nt; i++) {
        uint8_t *te = t + 2 + 8 * i;
        uint32_t type = BE32(te); int cnt = BE16(te + 4) + 1; uint16_t ref = BE16(te + 6);
        for (int j = 0; j < cnt; j++) {
            uint8_t *e = t + ref + 12 * j;
            Res *r = &rf->res[rf->nres++];
            r->type = type; r->id = (int16_t)BE16(e); uint16_t no = BE16(e + 2);
            r->attrs = e[4]; uint32_t ro = ((uint32_t)e[5] << 16) | (e[6] << 8) | e[7];
            r->len = BE32(f + doff + ro); r->off = doff + ro + 4;
            if (no != 0xFFFF) memcpy(r->name, m + nl + no, m[nl + no] + 1); else r->name[0] = 0;
            r->handle = 0;
        }
    }
    return 0;
}

int rs_open_file(const char *path, int as_app) {
    if (nfiles >= MAXF) return -1;
    RFile *rf = &files[nfiles];
    memset(rf, 0, sizeof *rf);
    rf->fork = load_fork(path, &rf->forklen);
    if (!rf->fork) return -1;
    parse_fork(rf);
    rf->ref = as_app ? 2 : 3 + nfiles;  /* refnums are arbitrary but stable */
    rf->open = 1;
    cur = nfiles++;
    return rf->ref;
}

int rs_open_mem_ref(const uint8_t *buf, uint32_t len, const char *macname, int as_app) {
    if (nfiles >= MAXF) return -1;
    RFile *rf = &files[nfiles];
    memset(rf, 0, sizeof *rf);
    rf->fork = malloc(len); memcpy(rf->fork, buf, len); rf->forklen = len;
    parse_fork(rf);
    rf->ref = as_app ? 2 : 3 + nfiles; rf->open = 1;
    snprintf(rf->macname, sizeof rf->macname, "%s", macname);
    cur = nfiles++;
    set_reserr(0);
    return rf->ref;
}
int rs_open_mem(const uint8_t *buf, uint32_t len, const char *macname) { return rs_open_mem_ref(buf, len, macname, 0); }


static int idx_of_ref(int ref) { for (int i = 0; i < nfiles; i++) if (files[i].open && files[i].ref == ref) return i; return -1; }

static uint32_t materialize(Res *r, RFile *rf) {
    if (r->handle && rd32(r->handle)) return r->handle;
    uint32_t h = r->handle ? r->handle : mm_new_handle(r->len, 0);
    if (!h) return 0;
    if (!rd32(h)) mm_set_handle_size(h, r->len);
    { hx_track(rd32(h), (uint32_t)(r->len)); memcpy(g_mem + rd32(h), rf->fork + r->off, r->len); }
    mm_set_state(h, mm_get_state(h) | 0x20);
    r->handle = h;
    return h;
}


uint32_t rs_get(uint32_t type, int16_t id, int one_deep) {
    for (int i = cur; i >= 0; i--) {
        RFile *rf = &files[i]; if (!rf->open) continue;
        for (int j = 0; j < rf->nres; j++)
            if (rf->res[j].type == type && rf->res[j].id == id) { set_reserr(0); return materialize(&rf->res[j], rf); }
        if (one_deep) break;
    }
    set_reserr(-192);
    return 0;
}

uint32_t rs_get_named(uint32_t type, const uint8_t *pn, int one_deep) {
    for (int i = cur; i >= 0; i--) {
        RFile *rf = &files[i]; if (!rf->open) continue;
        for (int j = 0; j < rf->nres; j++)
            if (rf->res[j].type == type && rf->res[j].name[0] == pn[0] && !memcmp(rf->res[j].name + 1, pn + 1, pn[0])) {
                set_reserr(0); return materialize(&rf->res[j], rf);
            }
        if (one_deep) break;
    }
    set_reserr(-192); return 0;
}

int rs_count(uint32_t type, int one_deep) {
    int n = 0;
    for (int i = cur; i >= 0; i--) {
        RFile *rf = &files[i]; if (!rf->open) continue;
        for (int j = 0; j < rf->nres; j++) if (rf->res[j].type == type) n++;
        if (one_deep) break;
    }
    return n;
}

uint32_t rs_get_ind(uint32_t type, int index, int one_deep) {
    int n = 0;
    for (int i = cur; i >= 0; i--) {
        RFile *rf = &files[i]; if (!rf->open) continue;
        for (int j = 0; j < rf->nres; j++) if (rf->res[j].type == type && ++n == index) { set_reserr(0); return materialize(&rf->res[j], rf); }
        if (one_deep) break;
    }
    set_reserr(-192); return 0;
}

static Res *find_h(uint32_t h, int *fi) {
    if (!h) return NULL;
    for (int i = 0; i < nfiles; i++) for (int j = 0; j < files[i].nres; j++)
        if (files[i].res[j].handle == h) { if (fi) *fi = i; return &files[i].res[j]; }
    return NULL;
}

int rs_info(uint32_t h, int16_t *id, uint32_t *type, uint8_t *pname) {
    Res *r = find_h(h, NULL);
    if (!r) { set_reserr(-192); return -1; }
    if (id) *id = r->id;
    if (type) *type = r->type;
    if (pname) memcpy(pname, r->name, r->name[0] + 1);
    set_reserr(0); return 0;
}

int rs_home(uint32_t h) { int fi; if (!find_h(h, &fi)) { set_reserr(-192); return -1; } set_reserr(0); return files[fi].ref; }

int rs_detach(uint32_t h) {
    Res *r = find_h(h, NULL);
    if (!r) { set_reserr(-192); return -1; }
    mm_set_state(h, mm_get_state(h) & ~0x20);
    r->handle = 0; set_reserr(0); return 0;
}

int rs_release(uint32_t h) {
    Res *r = find_h(h, NULL);
    if (!r) { set_reserr(-192); return -1; }
    mm_dispose_handle(h); r->handle = 0; set_reserr(0); return 0;
}

int rs_cur(void) { return cur >= 0 ? files[cur].ref : 0; }
void rs_use(int ref) { int i = idx_of_ref(ref); if (i >= 0) { cur = i; set_reserr(0); } else set_reserr(-193); }

int rs_open_by_name(const uint8_t *pn) {
    char name[256]; memcpy(name, pn + 1, pn[0]); name[pn[0]] = 0;
    for (int i = 0; i < nfiles; i++) if (files[i].open && !strcmp(files[i].macname, name)) { cur = i; set_reserr(0); return files[i].ref; }
    int vfs_open_res_by_name(const uint8_t *pn);
    int ref = vfs_open_res_by_name(pn);
    if (ref >= 0) return ref;
    hx_log("OpenResFile(\"%s\") -> fnfErr", name);
    set_reserr(-43); return -1;
}

void rs_close(int ref) {
    int i = idx_of_ref(ref); if (i < 0) { set_reserr(-193); return; }
    for (int j = 0; j < files[i].nres; j++) if (files[i].res[j].handle) { mm_dispose_handle(files[i].res[j].handle); files[i].res[j].handle = 0; }
    files[i].open = 0;
    if (cur == i) { cur = i; while (cur >= 0 && !files[cur].open) cur--; }
    set_reserr(0);
}

const uint8_t *rs_raw(int ref, uint32_t type, int16_t id, uint32_t *len) {
    int i = idx_of_ref(ref); if (i < 0) return NULL;
    for (int j = 0; j < files[i].nres; j++) if (files[i].res[j].type == type && files[i].res[j].id == id) { *len = files[i].res[j].len; return files[i].fork + files[i].res[j].off; }
    return NULL;
}

/* The loaded-resource handles are kept here, outside emulated memory. The per-call checker undoes
 * the C version's memory writes before the original runs, so it saves and restores these with them
 * (otherwise a resource the C version loaded would look loaded to the original, in freed memory). */
static uint32_t *saved_handles; static size_t saved_n;
void rs_state_save(void) {
    size_t n = 0;
    for (int i = 0; i < nfiles; i++) n += (size_t)files[i].nres;
    if (n > saved_n) { saved_handles = realloc(saved_handles, n * sizeof *saved_handles); }
    saved_n = n; n = 0;
    for (int i = 0; i < nfiles; i++) for (int j = 0; j < files[i].nres; j++) saved_handles[n++] = files[i].res[j].handle;
}
void rs_state_restore(void) {
    size_t n = 0;
    for (int i = 0; i < nfiles; i++) for (int j = 0; j < files[i].nres; j++) if (n < saved_n) files[i].res[j].handle = saved_handles[n++];
}
