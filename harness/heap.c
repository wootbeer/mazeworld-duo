/* Memory Manager emulation: one application heap zone inside emulated RAM.
 * Blocks: 16-byte header + data. Relocatable blocks are reached through
 * master pointers allocated in non-relocatable blocks of 64 pointers. */
#include "hx.h"
#include <string.h>

#define HDR 16
/* header layout (big-endian, inside emulated memory):
 * +0 u32 block size (incl. header, multiple of 8)
 * +4 u8  in-use (1) / free (0)
 * +5 u8  kind: 0 = non-relocatable, 1 = relocatable
 * +6 u8  handle state bits (0x80 locked, 0x40 purgeable, 0x20 resource)
 * +7 u8  size correction (bytes of padding at end)
 * +8 u32 master pointer address (relocatable) */
#define B_SIZE(b)  rd32(b)
#define B_USED(b)  rd8((b) + 4)
#define B_KIND(b)  rd8((b) + 5)
#define B_STATE(b) rd8((b) + 6)
#define B_PAD(b)   rd8((b) + 7)
#define B_MP(b)    rd32((b) + 8)

/* Head of the free master pointer list (linked through the pointer slots). It lives in emulated
 * memory, harness-private, so the whole heap state is in RAM: -subcheck can undo any heap change. */
#define FREE_MP_ADDR 0x00FFE100u
#define free_mp_get() rd32(FREE_MP_ADDR)
#define free_mp_set(v) wr32(FREE_MP_ADDR, (v))

static void set_hdr(uint32_t b, uint32_t sz, int used, int kind, uint8_t st, uint8_t pad, uint32_t mp) {
    wr32(b, sz); wr8(b + 4, used); wr8(b + 5, kind); wr8(b + 6, st); wr8(b + 7, pad); wr32(b + 8, mp); wr32(b + 12, 0);
}

void heap_init(void) {
    set_hdr(HEAP_BASE, HEAP_END - HEAP_BASE, 0, 0, 0, 0, 0);
    free_mp_set(0);
}

static uint32_t alloc_block(uint32_t datasize, int kind) {
    uint32_t need = (datasize + HDR + 7) & ~7u;
    if (need < HDR + 8) need = HDR + 8;
    for (uint32_t b = HEAP_BASE; b < HEAP_END; b += B_SIZE(b)) {
        if (B_SIZE(b) == 0) hx_fault("heap corrupt at %06X", b);
        if (B_USED(b)) continue;
        /* coalesce following free blocks */
        uint32_t sz = B_SIZE(b);
        while (b + sz < HEAP_END && !B_USED(b + sz)) sz += B_SIZE(b + sz);
        wr32(b, sz);
        if (sz < need) continue;
        if (sz - need >= HDR + 16) {
            set_hdr(b + need, sz - need, 0, 0, 0, 0, 0);
            sz = need;
        }
        set_hdr(b, sz, 1, kind, 0, (uint8_t)(sz - HDR - datasize), 0);
        return b;
    }
    return 0;
}

static void free_block(uint32_t b) { wr8(b + 4, 0); }

static uint32_t block_of_ptr(uint32_t p) { return p - HDR; }

static void more_masters(void) {
    uint32_t b = alloc_block(64 * 4, 0);
    if (!b) hx_fault("out of memory for master pointers");
    uint32_t p = b + HDR;
    for (int i = 0; i < 64; i++) { wr32(p + 4 * i, free_mp_get()); free_mp_set(p + 4 * i); }
}

static uint32_t new_mp(void) {
    if (!free_mp_get()) more_masters();
    uint32_t mp = free_mp_get(); free_mp_set(rd32(mp));
    wr32(mp, 0);
    return mp;
}

uint32_t mm_new_ptr(uint32_t size, int clear) {
    uint32_t b = alloc_block(size, 0);
    if (!b) { wr16(LM_MemErr, (uint16_t)-108); return 0; }
    if (clear) { hx_track(b + HDR, (uint32_t)(size)); memset(g_mem + b + HDR, 0, size); }
    wr16(LM_MemErr, 0);
    return b + HDR;
}

uint32_t mm_new_handle(uint32_t size, int clear) {
    uint32_t mp = new_mp();
    uint32_t b = alloc_block(size, 1);
    if (!b) { wr32(mp, free_mp_get()); free_mp_set(mp); wr16(LM_MemErr, (uint16_t)-108); return 0; }
    wr32(b + 8, mp);
    if (clear) { hx_track(b + HDR, (uint32_t)(size)); memset(g_mem + b + HDR, 0, size); }
    wr32(mp, b + HDR);
    wr16(LM_MemErr, 0);
    return mp;
}


void mm_dispose_handle(uint32_t h) {
    if (!h) return;
    uint32_t p = rd32(h);
    if (p) free_block(block_of_ptr(p));
    wr32(h, free_mp_get()); free_mp_set(h);
    wr16(LM_MemErr, 0);
}

void mm_dispose_ptr(uint32_t p) { if (p) free_block(block_of_ptr(p)); wr16(LM_MemErr, 0); }

static int32_t logical_size(uint32_t b) { return (int32_t)(B_SIZE(b) - HDR - B_PAD(b)); }

int32_t mm_handle_size(uint32_t h) {
    uint32_t p = rd32(h);
    if (!p) return 0;  /* purged/empty handle */
    return logical_size(block_of_ptr(p));
}

int32_t mm_ptr_size(uint32_t p) { return logical_size(block_of_ptr(p)); }

int mm_set_handle_size(uint32_t h, uint32_t size) {
    uint32_t p = rd32(h);
    if (!p) {  /* empty handle: allocate */
        uint32_t b = alloc_block(size, 1);
        if (!b) return -108;
        wr32(b + 8, h); wr32(h, b + HDR); return 0;
    }
    uint32_t b = block_of_ptr(p);
    uint32_t cap = B_SIZE(b) - HDR;
    if (size <= cap) { wr8(b + 7, (uint8_t)(cap - size)); return 0; }
    /* try growing in place */
    uint32_t sz = B_SIZE(b);
    while (b + sz < HEAP_END && !B_USED(b + sz)) sz += B_SIZE(b + sz);
    uint32_t need = (size + HDR + 7) & ~7u;
    if (sz >= need) {
        if (sz - need >= HDR + 16) { set_hdr(b + need, sz - need, 0, 0, 0, 0, 0); sz = need; }
        wr32(b, sz); wr8(b + 7, (uint8_t)(sz - HDR - size)); return 0;
    }
    wr32(b, B_SIZE(b));
    if (B_STATE(b) & 0x80) return -108;  /* locked: cannot move */
    int32_t old = logical_size(b);
    uint8_t st = B_STATE(b);
    uint32_t nb = alloc_block(size, 1);
    if (!nb) return -108;
    { hx_track(nb + HDR, (uint32_t)(old)); memmove(g_mem + nb + HDR, g_mem + p, old); }
    wr32(nb + 8, h); wr8(nb + 6, st);
    free_block(b);
    wr32(h, nb + HDR);
    return 0;
}

uint8_t mm_get_state(uint32_t h) { uint32_t p = rd32(h); return p ? B_STATE(block_of_ptr(p)) : 0; }
void mm_set_state(uint32_t h, uint8_t st) { uint32_t p = rd32(h); if (p) wr8(block_of_ptr(p) + 6, st); }

uint32_t mm_free_bytes(void) {
    uint32_t total = 0;
    for (uint32_t b = HEAP_BASE; b < HEAP_END; b += B_SIZE(b)) if (!B_USED(b)) total += B_SIZE(b) - HDR;
    return total;
}
