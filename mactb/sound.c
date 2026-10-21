/* Sound Manager model: sampled-sound channels with command queues.
 * Game-visible behaviour (when a callBackCmd is reached) is advanced in whole ticks, so it is
 * deterministic and identical in the port and the harness. Audio is mixed alongside it. */
#include "mactb.h"
#include <string.h>

#define OUT_RATE_FIXED 0x56EE8BA3u     /* 22254.5454 Hz, the classic Mac output rate */

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

void mtb_snd_init(MtbSndChan *c) { memset(c, 0, sizeof *c); c->amp = 255; c->used = 1; }

int mtb_snd_command(MtbSndChan *c, uint16_t cmd, int16_t p1, uint32_t p2, const uint8_t *hdr) {
    int n = (c->qt + 1) % MTB_SND_QLEN;
    if (n == c->qh) return -203;                       /* queueFull */
    c->q[c->qt] = (MtbSndCmd){(uint16_t)(cmd & 0x7FFF), p1, p2, hdr};
    c->qt = n;
    return 0;
}

static void start_buffer(MtbSndChan *c, const uint8_t *hdr) {
    if (!hdr) { c->playing = 0; return; }
    /* standard sampled sound header: samplePtr, length, sampleRate (Fixed), loopStart, loopEnd, encode, baseFreq, data */
    uint32_t sp = be32(hdr);
    c->frames = be32(hdr + 4);
    c->rate = be32(hdr + 8);
    c->data = sp ? NULL : hdr + 22;                    /* external sample pointers are not used by the game */
    c->pos = 0;
    c->playing = c->data != NULL && c->frames > 0 && hdr[20] == 0;
}

/* Immediate commands. Returns 1 if the command was a callBackCmd that must be delivered now. */
int mtb_snd_immediate(MtbSndChan *c, uint16_t cmd, int16_t p1, uint32_t p2, const uint8_t *hdr) {
    (void)p2;
    switch (cmd & 0x7FFF) {
    case 3: c->playing = 0; break;                     /* quietCmd */
    case 4: c->qh = c->qt = 0; break;                  /* flushCmd */
    case 43: c->amp = (uint8_t)p1; break;              /* ampCmd */
    case 81: case 80: start_buffer(c, hdr); break;     /* bufferCmd / soundCmd */
    case 13: return 1;                                 /* callBackCmd */
    }
    return 0;
}

/* Advance one tick (1/60 s). Mixes this tick's output into mix[0..*nout) (adds, signed 16-bit centred),
 * and reports callbacks reached: up to maxcb (param1, param2) pairs. Returns the number of callbacks. */
int mtb_snd_tick(MtbSndChan *c, int32_t *mix, int nmix, int16_t *cb_p1, uint32_t *cb_p2, int maxcb) {
    int ncb = 0, out = 0;
    while (out < nmix || !c->playing) {
        if (!c->playing) {
            if (c->qh == c->qt) break;
            MtbSndCmd k = c->q[c->qh]; c->qh = (c->qh + 1) % MTB_SND_QLEN;
            switch (k.cmd) {
            case 81: case 80: start_buffer(c, k.hdr); break;
            case 13: if (ncb < maxcb) { cb_p1[ncb] = k.p1; cb_p2[ncb] = k.p2; ncb++; } break;
            case 43: c->amp = (uint8_t)k.p1; break;
            case 3: c->playing = 0; break;
            case 4: c->qh = c->qt; break;
            }
            continue;
        }
        if (out >= nmix) break;
        /* step through the source at rate/out_rate per output sample (16.16 fixed) */
        uint64_t step = ((uint64_t)c->rate << 16) / OUT_RATE_FIXED;
        for (; out < nmix; out++) {
            uint32_t i = (uint32_t)(c->pos >> 16);
            if (i >= c->frames) { c->playing = 0; break; }
            int s = (int)c->data[i] - 128;
            mix[out] += s * 256 * c->amp / 255;
            c->pos += step;
        }
    }
    return ncb;
}

/* Output samples to produce for tick number `tick` (so that samples-per-second is exact over time). */
int mtb_snd_samples_for_tick(uint32_t tick) {
    uint64_t a = (uint64_t)tick * OUT_RATE_FIXED / 60, b = (uint64_t)(tick + 1) * OUT_RATE_FIXED / 60;
    return (int)((b >> 16) - (a >> 16));
}
