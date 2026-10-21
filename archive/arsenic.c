/*
 * StuffIt method 15 (Arsenic) decoder: arithmetic coding + move-to-front + BWT + RLE.
 *
 * Port to plain C of XADStuffItArsenicHandle.m and the BWT/MTF helpers in BWT.c, from The
 * Unarchiver's XADMaster library:
 *   Copyright (c) 2017-present, MacPaw Way Ltd. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version. This library is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 */
#include "arc.h"
#include <stdlib.h>
#include <string.h>

/* ---------- big-endian (high bit first) bit reader; past the end reads zeros ---------- */
typedef struct { const uint8_t *p; size_t n, pos; int bit; } BitsBE;
static int bit_be(BitsBE *b) {
    if (b->pos >= b->n) return 0;
    int v = (b->p[b->pos] >> (7 - b->bit)) & 1;
    if (++b->bit == 8) { b->bit = 0; b->pos++; }
    return v;
}
static unsigned bits_be(BitsBE *b, int n) { unsigned v = 0; for (int i = 0; i < n; i++) v = (v << 1) | (unsigned)bit_be(b); return v; }

/* ---------- adaptive arithmetic model and decoder ---------- */
typedef struct { int symbol, frequency; } Sym;
typedef struct { int totalfrequency, increment, frequencylimit, numsymbols; Sym symbols[128]; } Model;
typedef struct { BitsBE *in; int range, code; } Dec;

static void model_reset(Model *m) {
    m->totalfrequency = m->increment * m->numsymbols;
    for (int i = 0; i < m->numsymbols; i++) m->symbols[i].frequency = m->increment;
}
static void model_init(Model *m, int first, int last, int inc, int limit) {
    m->increment = inc; m->frequencylimit = limit; m->numsymbols = last - first + 1;
    for (int i = 0; i < m->numsymbols; i++) m->symbols[i].symbol = i + first;
    model_reset(m);
}
static void model_bump(Model *m, int i) {
    m->symbols[i].frequency += m->increment;
    m->totalfrequency += m->increment;
    if (m->totalfrequency > m->frequencylimit) {
        m->totalfrequency = 0;
        for (int k = 0; k < m->numsymbols; k++) {
            m->symbols[k].frequency++;
            m->symbols[k].frequency >>= 1;
            m->totalfrequency += m->symbols[k].frequency;
        }
    }
}

#define NUMBITS 26
#define ONE (1 << (NUMBITS - 1))
#define HALF (1 << (NUMBITS - 2))

static void dec_init(Dec *d, BitsBE *in) {
    d->in = in; d->range = ONE;
    unsigned hi = bits_be(in, 25);                  /* CSInputNextLongBitString(26) = 25 + 1 bits */
    d->code = (int)((hi << 1) | bits_be(in, 1));
}
static void dec_code(Dec *d, int low, int size, int tot) {
    int renorm = d->range / tot, lowincr = renorm * low;
    d->code -= lowincr;
    if (low + size == tot) d->range -= lowincr; else d->range = size * renorm;
    while (d->range <= HALF) { d->range <<= 1; d->code = (d->code << 1) | bit_be(d->in); }
}
static int dec_sym(Dec *d, Model *m) {
    int freq = d->code / (d->range / m->totalfrequency), cum = 0, n;
    for (n = 0; n < m->numsymbols - 1; n++) {
        if (cum + m->symbols[n].frequency > freq) break;
        cum += m->symbols[n].frequency;
    }
    dec_code(d, cum, m->symbols[n].frequency, m->totalfrequency);
    model_bump(m, n);
    return m->symbols[n].symbol;
}
static int dec_bits(Dec *d, Model *m, int bits) { int r = 0; for (int i = 0; i < bits; i++) if (dec_sym(d, m)) r |= 1 << i; return r; }

/* ---------- MTF and inverse BWT ---------- */
typedef struct { int table[256]; } MTF;
static void mtf_reset(MTF *t) { for (int i = 0; i < 256; i++) t->table[i] = i; }
static int mtf_decode(MTF *t, int s) { int r = t->table[s]; for (int i = s; i > 0; i--) t->table[i] = t->table[i - 1]; t->table[0] = r; return r; }
static void inverse_bwt(uint32_t *tr, const uint8_t *blk, int n) {
    int counts[256] = {0}, cum[256];
    for (int i = 0; i < n; i++) counts[blk[i]]++;
    int total = 0;
    for (int i = 0; i < 256; i++) { cum[i] = total; total += counts[i]; counts[i] = 0; }
    for (int i = 0; i < n; i++) { tr[cum[blk[i]] + counts[blk[i]]] = (uint32_t)i; counts[blk[i]]++; }
}

static const uint16_t RandomizationTable[256] = {
    0xee,0x56,0xf8,0xc3,0x9d,0x9f,0xae,0x2c,0xad,0xcd,0x24,0x9d,0xa6,0x101,0x18,0xb9,
    0xa1,0x82,0x75,0xe9,0x9f,0x55,0x66,0x6a,0x86,0x71,0xdc,0x84,0x56,0x96,0x56,0xa1,
    0x84,0x78,0xb7,0x32,0x6a,0x3,0xe3,0x2,0x11,0x101,0x8,0x44,0x83,0x100,0x43,0xe3,
    0x1c,0xf0,0x86,0x6a,0x6b,0xf,0x3,0x2d,0x86,0x17,0x7b,0x10,0xf6,0x80,0x78,0x7a,
    0xa1,0xe1,0xef,0x8c,0xf6,0x87,0x4b,0xa7,0xe2,0x77,0xfa,0xb8,0x81,0xee,0x77,0xc0,
    0x9d,0x29,0x20,0x27,0x71,0x12,0xe0,0x6b,0xd1,0x7c,0xa,0x89,0x7d,0x87,0xc4,0x101,
    0xc1,0x31,0xaf,0x38,0x3,0x68,0x1b,0x76,0x79,0x3f,0xdb,0xc7,0x1b,0x36,0x7b,0xe2,
    0x63,0x81,0xee,0xc,0x63,0x8b,0x78,0x38,0x97,0x9b,0xd7,0x8f,0xdd,0xf2,0xa3,0x77,
    0x8c,0xc3,0x39,0x20,0xb3,0x12,0x11,0xe,0x17,0x42,0x80,0x2c,0xc4,0x92,0x59,0xc8,
    0xdb,0x40,0x76,0x64,0xb4,0x55,0x1a,0x9e,0xfe,0x5f,0x6,0x3c,0x41,0xef,0xd4,0xaa,
    0x98,0x29,0xcd,0x1f,0x2,0xa8,0x87,0xd2,0xa0,0x93,0x98,0xef,0xc,0x43,0xed,0x9d,
    0xc2,0xeb,0x81,0xe9,0x64,0x23,0x68,0x1e,0x25,0x57,0xde,0x9a,0xcf,0x7f,0xe5,0xba,
    0x41,0xea,0xea,0x36,0x1a,0x28,0x79,0x20,0x5e,0x18,0x4e,0x7c,0x8e,0x58,0x7a,0xef,
    0x91,0x2,0x93,0xbb,0x56,0xa1,0x49,0x1b,0x79,0x92,0xf3,0x58,0x4f,0x52,0x9c,0x2,
    0x77,0xaf,0x2a,0x8f,0x49,0xd0,0x99,0x4d,0x98,0x101,0x60,0x93,0x100,0x75,0x31,0xce,
    0x49,0x20,0x56,0x57,0xe2,0xf5,0x26,0x2b,0x8a,0xbf,0xde,0xd0,0x83,0x34,0xf4,0x17,
};

static uint32_t crc32_byte(uint32_t crc, uint8_t b) {
    crc ^= b;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    return crc;
}

int arsenic_decode(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen) {
    BitsBE in = {src, slen, 0, 0};
    Dec d; Model initial, selector, mtfm[7]; MTF mtf;
    dec_init(&d, &in);
    model_init(&initial, 0, 1, 1, 256);
    model_init(&selector, 0, 10, 8, 1024);
    model_init(&mtfm[0], 2, 3, 8, 1024); model_init(&mtfm[1], 4, 7, 4, 1024);
    model_init(&mtfm[2], 8, 15, 4, 1024); model_init(&mtfm[3], 16, 31, 4, 1024);
    model_init(&mtfm[4], 32, 63, 2, 1024); model_init(&mtfm[5], 64, 127, 2, 1024);
    model_init(&mtfm[6], 128, 255, 1, 1024);
    if (dec_bits(&d, &initial, 8) != 'A' || dec_bits(&d, &initial, 8) != 's') return -1;
    int blockbits = dec_bits(&d, &initial, 4) + 9, blocksize = 1 << blockbits;
    uint8_t *block = malloc((size_t)blocksize);
    uint32_t *tr = malloc(sizeof(uint32_t) * (size_t)blocksize);
    int endofblocks = dec_sym(&d, &initial);
    uint32_t crc = 0xFFFFFFFFu, compcrc = 0;
    size_t out = 0;
    int rc = -1;

    int numbytes = 0, bytecount = 0, tindex = 0, randomized = 0, randindex = 0, randcount = 0;
    int repeat = 0, count = 0, last = 0;
    while (out < dlen) {
        int outbyte;
        if (repeat) { repeat--; outbyte = last; }
        else {
        retry:
            if (bytecount >= numbytes) {
                if (endofblocks) break;
                /* read a block */
                mtf_reset(&mtf);
                randomized = dec_sym(&d, &initial);
                tindex = dec_bits(&d, &initial, blockbits);
                numbytes = 0;
                for (;;) {
                    int sel = dec_sym(&d, &selector);
                    if (sel == 0 || sel == 1) {
                        int zstate = 1, zcount = 0;
                        while (sel < 2) {
                            if (sel == 0) zcount += zstate; else zcount += 2 * zstate;
                            zstate *= 2;
                            sel = dec_sym(&d, &selector);
                        }
                        if (numbytes + zcount > blocksize) goto done;
                        memset(block + numbytes, mtf_decode(&mtf, 0), (size_t)zcount);
                        numbytes += zcount;
                    }
                    int symbol;
                    if (sel == 10) break;
                    else if (sel == 2) symbol = 1;
                    else symbol = dec_sym(&d, &mtfm[sel - 3]);
                    if (numbytes >= blocksize) goto done;
                    block[numbytes++] = (uint8_t)mtf_decode(&mtf, symbol);
                }
                if (tindex >= numbytes) goto done;
                model_reset(&selector);
                for (int i = 0; i < 7; i++) model_reset(&mtfm[i]);
                if (dec_sym(&d, &initial)) { compcrc = (uint32_t)dec_bits(&d, &initial, 32); endofblocks = 1; }
                inverse_bwt(tr, block, numbytes);
                bytecount = 0; count = 0; last = 0; randindex = 0; randcount = RandomizationTable[0];
            }
            tindex = (int)tr[tindex];
            int byte = block[tindex];
            if (randomized && randcount == bytecount) {
                byte ^= 1;
                randindex = (randindex + 1) & 255;
                randcount += RandomizationTable[randindex];
            }
            bytecount++;
            if (count == 4) {
                count = 0;
                if (byte == 0) goto retry;
                repeat = byte - 1;
                outbyte = last;
            } else {
                if (byte == last) count++; else { count = 1; last = byte; }
                outbyte = byte;
            }
        }
        dst[out++] = (uint8_t)outbyte;
        crc = crc32_byte(crc, (uint8_t)outbyte);
    }
    rc = (out == dlen && endofblocks && compcrc == ~crc) ? 0 : -1;
done:
    free(block); free(tr);
    return rc;
}
