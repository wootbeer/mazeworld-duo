/* Segment Loader emulation and execution tracing. */
#include "hx.h"
void mz_seg(int n);
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif

#define MAXSEG 32
typedef struct { int loaded; uint32_t handle, base, size; uint8_t *hit; } Seg;
static Seg segs[MAXSEG];
uint32_t g_jt_count;
static int app_ref;

void seg_init(void) {
    uint32_t len;
    app_ref = 2;
    const uint8_t *c0 = rs_raw(app_ref, FOURCC('C','O','D','E'), 0, &len);
    if (!c0) hx_fault("no CODE 0");
    uint32_t above = (c0[0] << 24) | (c0[1] << 16) | (c0[2] << 8) | c0[3];
    uint32_t below = (c0[4] << 24) | (c0[5] << 16) | (c0[6] << 8) | c0[7];
    uint32_t jtlen = (c0[8] << 24) | (c0[9] << 16) | (c0[10] << 8) | c0[11];
    uint32_t jtoff = (c0[12] << 24) | (c0[13] << 16) | (c0[14] << 8) | c0[15];
    (void)above;
    memcpy(g_mem + A5_ADDR + jtoff, c0 + 16, jtlen);
    g_jt_count = jtlen / 8;
    wr32(LM_CurrentA5, A5_ADDR);
    wr32(LM_CurStackBase, A5_ADDR - below);
    hx_log("A5=%06X globals=%u bytes JT=%u entries stack base=%06X", A5_ADDR, below, g_jt_count, A5_ADDR - below);
}

int seg_load(int n) {
    if (n <= 0 || n >= MAXSEG) return -1;
    Seg *s = &segs[n];
    if (!s->loaded) {
        uint32_t h = rs_get(FOURCC('C','O','D','E'), (int16_t)n, 0);
        if (!h) return -1;
        mm_set_state(h, mm_get_state(h) | 0x80);
        s->handle = h; s->base = rd32(h) + 4; s->size = (uint32_t)mm_handle_size(h) - 4;
        if (!s->hit) s->hit = calloc(s->size, 1);
        s->loaded = 1;
        if (g_verbose) hx_log("LoadSeg %d at %06X (%u bytes)", n, s->base, s->size);
    }
    /* patch every JT entry of this segment into the loaded form: seg.w, JMP abs.l */
    for (uint32_t k = 0; k < g_jt_count; k++) {
        uint32_t e = A5_ADDR + 32 + 8 * k;
        if (rd16(e + 2) == 0x3F3C && rd16(e + 4) == (uint16_t)n && rd16(e + 6) == 0xA9F0) {
            uint16_t off = rd16(e);
            wr16(e, (uint16_t)n); wr16(e + 2, 0x4EF9); wr32(e + 4, s->base + off);
        }
    }
    return 0;
}

int seg_is_loaded(int n) { return n > 0 && n < MAXSEG && segs[n].loaded; }

/* where segment n's code is (loading it first), for data kept in a code segment */
int rt_in_check_phase(void); void rt_skip_compare(int why); void rt_abandon(int why);
uint32_t mz_seg_base(int n) {
    /* in the C phase of a check, a segment the original has yet to load cannot be read: the call is
     * not compared (as for code in it) and the C version stops here */
    if (!seg_is_loaded(n) && rt_in_check_phase()) { rt_skip_compare(n); rt_abandon(1); }
    mz_seg(n);
    return n > 0 && n < MAXSEG ? segs[n].base : 0;
}

int seg_of(uint32_t pc, uint32_t *off) {
    for (int n = 1; n < MAXSEG; n++) {
        Seg *s = &segs[n];
        if (s->loaded && pc >= s->base && pc < s->base + s->size) { if (off) *off = pc - s->base; return n; }
    }
    return 0;
}

void seg_trace_mark(uint32_t pc) {
    uint32_t off; int n = seg_of(pc, &off);
    if (n) segs[n].hit[off] = 1;
}

void seg_trace_write(const char *dir) {
    mkdir(dir, 0755);
    for (int n = 1; n < MAXSEG; n++) {
        Seg *s = &segs[n];
        if (!s->hit) continue;
        char p[512]; snprintf(p, sizeof p, "%s/seg%d.pcs", dir, n);
        /* merge with existing trace so repeated runs accumulate coverage */
        uint8_t *prev = calloc(s->size, 1);
        FILE *f = fopen(p, "r");
        if (f) { unsigned o; while (fscanf(f, "%x", &o) == 1) if (o < s->size) prev[o] = 1; fclose(f); }
        f = fopen(p, "w");
        if (!f) { hx_log("cannot write %s", p); free(prev); continue; }
        int cnt = 0;
        for (uint32_t o = 0; o < s->size; o++) if (s->hit[o] || prev[o]) { fprintf(f, "%05X\n", o); cnt++; }
        fclose(f); free(prev);
        hx_log("trace seg%d: %d instruction starts", n, cnt);
    }
}
