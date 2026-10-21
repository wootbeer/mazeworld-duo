/* Sound Manager traps over mactb's channel model. Callbacks run as interrupt-time calls
 * (see vbl.c), once per tick, in channel order. The mixed output is written to sound.wav. */
#include "hx.h"
#include "m68k.h"
#include "../mactb/mactb.h"
#include <string.h>
#include <stdlib.h>

#define A7 M68K_REG_A7
#define MAXCH 8
typedef struct { uint32_t rec; uint32_t proc; MtbSndChan m; } Chan;
static Chan ch[MAXCH];
static int16_t *wav; static size_t wav_n, wav_cap;
/* the port plays the output live instead of collecting it (22254.5 Hz, mono, 16 bit) */
void (*snd_live_out)(const int16_t *s, int n);
const char *hx_where(void);

static Chan *find(uint32_t rec) { for (int i = 0; i < MAXCH; i++) if (ch[i].m.used && ch[i].rec == rec) return &ch[i]; return NULL; }

static void t_SndNewChannel(uint16_t w) {
    (void)w; uint32_t proc = pop32(); pop32(); int16_t synth = (int16_t)pop16(); uint32_t pchan = pop32();
    if (synth != 5) hx_log("SndNewChannel synth %d (only sampled sound is modelled) at %s", synth, hx_where());
    uint32_t rec = rd32(pchan);
    if (!rec) { rec = mm_new_ptr(36 + 8 * MTB_SND_QLEN, 1); wr32(pchan, rec); }
    int i = 0; while (i < MAXCH && ch[i].m.used) i++;
    if (i == MAXCH) { wr16(rd32(A7), (uint16_t)-108); return; }
    mtb_snd_init(&ch[i].m); ch[i].rec = rec; ch[i].proc = proc;
    wr32(rec + 8, proc); wr16(rec + 30, MTB_SND_QLEN);
    hx_log("SndNewChannel #%d rec %06X callback %06X at %s", i, rec, proc, hx_where());
    wr16(rd32(A7), 0);
}
static void t_SndDisposeChannel(uint16_t w) {
    (void)w; pop16(); uint32_t rec = pop32(); Chan *c = find(rec);
    if (c) c->m.used = 0;
    wr16(rd32(A7), 0);
}
static const uint8_t *hdr_of(uint16_t cmd, uint32_t p2) {
    cmd &= 0x7FFF;
    return (cmd == 80 || cmd == 81) && p2 ? g_mem + (p2 & 0xFFFFFF) : NULL;
}
static void t_SndDoCommand(uint16_t w) {
    (void)w; pop16(); uint32_t cp = pop32(), rec = pop32(); Chan *c = find(rec);
    if (!c) { wr16(rd32(A7), (uint16_t)-205); return; }   /* badChannel */
    uint16_t k = rd16(cp); int16_t p1 = (int16_t)rd16(cp + 2); uint32_t p2 = rd32(cp + 4);
    wr16(rd32(A7), (uint16_t)mtb_snd_command(&c->m, k, p1, p2, hdr_of(k, p2)));
}
static void t_SndDoImmediate(uint16_t w) {
    (void)w; uint32_t cp = pop32(), rec = pop32(); Chan *c = find(rec);
    if (!c) { wr16(rd32(A7), (uint16_t)-205); return; }
    uint16_t k = rd16(cp); int16_t p1 = (int16_t)rd16(cp + 2); uint32_t p2 = rd32(cp + 4);
    if (mtb_snd_immediate(&c->m, k, p1, p2, hdr_of(k, p2))) hx_fault("immediate callBackCmd is not modelled (at %s)", hx_where());
    wr16(rd32(A7), 0);
}

/* Called once per tick from the interrupt builder. Appends callback calls (proc, chan record, command
 * in emulated memory) to the arrays; returns how many. */
int snd_tick(uint32_t tick, uint32_t *proc, uint32_t *chanrec, uint32_t *cmdaddr, uint32_t scratch, int max) {
    int n = mtb_snd_samples_for_tick(tick);
    int32_t mix[512]; memset(mix, 0, sizeof mix); if (n > 512) n = 512;
    int ncb = 0;
    for (int i = 0; i < MAXCH; i++) {
        if (!ch[i].m.used) continue;
        int16_t p1[8]; uint32_t p2[8];
        int k = mtb_snd_tick(&ch[i].m, mix, n, p1, p2, 8);
        for (int j = 0; j < k && ncb < max; j++) {
            uint32_t a = scratch + 8u * (uint32_t)ncb;
            wr16(a, 13); wr16(a + 2, (uint16_t)p1[j]); wr32(a + 4, p2[j]);
            proc[ncb] = ch[i].proc; chanrec[ncb] = ch[i].rec; cmdaddr[ncb] = a; ncb++;
        }
    }
    if (snd_live_out) {
        int16_t out[512];
        for (int i = 0; i < n; i++) { int v = mix[i]; out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
        snd_live_out(out, n);
        return ncb;
    }
    if (wav_n + (size_t)n > wav_cap) { wav_cap = (wav_cap + (size_t)n) * 2; wav = realloc(wav, wav_cap * sizeof *wav); }
    for (int i = 0; i < n; i++) { int v = mix[i]; wav[wav_n++] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
    return ncb;
}

void snd_write_wav(const char *path) {
    if (!wav_n) return;
    FILE *f = fopen(path, "wb"); if (!f) return;
    uint32_t rate = 22255, bytes = (uint32_t)(wav_n * 2);
    uint8_t h[44] = "RIFF....WAVEfmt \x10\0\0\0\x01\0\x01\0........\x02\0\x10\0data....";
    memcpy(h + 4, &(uint32_t){36 + bytes}, 4); memcpy(h + 24, &rate, 4); memcpy(h + 28, &(uint32_t){rate * 2}, 4); memcpy(h + 40, &bytes, 4);
    fwrite(h, 1, 44, f); fwrite(wav, 2, wav_n, f); fclose(f);
}

typedef void (*TrapFn)(uint16_t w);
void traps_register_tb(uint16_t n, TrapFn f);
void snd_register(void) {
    traps_register_tb(0x007, t_SndNewChannel); traps_register_tb(0x001, t_SndDisposeChannel);
    traps_register_tb(0x003, t_SndDoCommand); traps_register_tb(0x004, t_SndDoImmediate);
}
