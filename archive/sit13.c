/*
 * StuffIt method 13 (LZ+Huffman) decoder.
 *
 * Port to plain C of XADStuffIt13Handle.m and the parts of XADPrefixCode.m / CSInputBuffer.m it uses,
 * from The Unarchiver's XADMaster library:
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

/* ---------- little-endian (low bit first) bit reader; reads past the end as zeros ---------- */
typedef struct { const uint8_t *p; size_t n, pos; int bit; } BitsLE;
static int bit_le(BitsLE *b) {
    if (b->pos >= b->n) return 0;
    int v = (b->p[b->pos] >> b->bit) & 1;
    if (++b->bit == 8) { b->bit = 0; b->pos++; }
    return v;
}
static unsigned bits_le(BitsLE *b, int n) { unsigned v = 0; for (int i = 0; i < n; i++) v |= (unsigned)bit_le(b) << i; return v; }

/* ---------- prefix code as a binary tree; bits are consumed first bit = high bit of the code ---------- */
typedef struct { int (*node)[2]; int n, cap; } Code;      /* node[i][b]: >0 child index, <0 -(value+1), 0 empty */
static void code_init(Code *c) { c->cap = 64; c->n = 1; c->node = calloc((size_t)c->cap, sizeof *c->node); }
static void code_free(Code *c) { free(c->node); c->node = NULL; }
static int code_add(Code *c, int value, uint32_t code, int len) {   /* high bit first */
    int cur = 0;
    for (int i = len - 1; i >= 0; i--) {
        int b = (code >> i) & 1;
        if (i == 0) { if (c->node[cur][b]) return -1; c->node[cur][b] = -(value + 1); return 0; }
        if (c->node[cur][b] < 0) return -1;
        if (!c->node[cur][b]) {
            if (c->n == c->cap) { c->cap *= 2; c->node = realloc(c->node, (size_t)c->cap * sizeof *c->node); }
            memset(c->node[c->n], 0, sizeof c->node[0]);
            c->node[cur][b] = c->n++;
        }
        cur = c->node[cur][b];
    }
    return 0;
}
static uint32_t reverse_n(uint32_t v, int n) { uint32_t r = 0; for (int i = 0; i < n; i++) { r = (r << 1) | (v & 1); v >>= 1; } return r; }
/* canonical code from lengths, shortest code all zeros (XADPrefixCode initWithLengths ... shortestCodeIsZeros:YES) */
static int code_from_lengths(Code *c, const int *lengths, int nsym, int maxlen) {
    code_init(c);
    uint32_t code = 0; int left = nsym;
    for (int len = 1; len <= maxlen; len++) {
        for (int i = 0; i < nsym; i++) {
            if (lengths[i] != len) continue;
            if (code_add(c, i, code, len)) return -1;
            code++;
            if (--left == 0) return 0;
        }
        code <<= 1;
    }
    return 0;
}
static int code_next(Code *c, BitsLE *b) {
    int cur = 0;
    for (int guard = 0; guard < 64; guard++) {
        int nx = c->node[cur][bit_le(b)];
        if (nx < 0) return -nx - 1;
        if (nx == 0) return -1;
        cur = nx;
    }
    return -1;
}

/* ---------- predefined code sets ---------- */
#include "sit13_tables.h"

static int parse_code(Code *out, int numcodes, Code *meta, BitsLE *b) {
    int length = 0;
    int *lengths = calloc((size_t)numcodes + 80, sizeof(int));
    for (int i = 0; i < numcodes; i++) {
        int val = code_next(meta, b), k;
        if (val < 0) { free(lengths); return -1; }
        switch (val) {
        case 31: length = -1; break;
        case 32: length++; break;
        case 33: length--; break;
        case 34: if (bit_le(b)) lengths[i++] = length; break;
        case 35: k = (int)bits_le(b, 3) + 2; while (k--) lengths[i++] = length; break;
        case 36: k = (int)bits_le(b, 6) + 10; while (k--) lengths[i++] = length; break;
        default: length = val + 1; break;
        }
        if (i < numcodes + 80) lengths[i] = length;
    }
    int r = code_from_lengths(out, lengths, numcodes, 32);
    free(lengths);
    return r;
}

int sit13_decode(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen) {
    if (!slen) return dlen ? -1 : 0;
    BitsLE b = {src, slen, 1, 0};
    int val = src[0], set = val >> 4;
    Code first = {0}, second = {0}, offs = {0}, meta = {0};
    int shared = 0, rc = -1;
    if (set == 0) {
        code_init(&meta);
        for (int i = 0; i < 37; i++) if (code_add(&meta, i, reverse_n((uint32_t)MetaCodes[i], MetaCodeLengths[i]), MetaCodeLengths[i])) goto out;
        if (parse_code(&first, 321, &meta, &b)) goto out;
        if (val & 0x08) { second = first; shared = 1; }
        else if (parse_code(&second, 321, &meta, &b)) goto out;
        if (parse_code(&offs, (val & 0x07) + 10, &meta, &b)) goto out;
    } else if (set < 6) {
        if (code_from_lengths(&first, FirstCodeLengths[set - 1], 321, 32)) goto out;
        if (code_from_lengths(&second, SecondCodeLengths[set - 1], 321, 32)) goto out;
        if (code_from_lengths(&offs, OffsetCodeLengths[set - 1], OffsetCodeSize[set - 1], 32)) goto out;
    } else goto out;

    Code *cur = &first;
    size_t pos = 0;
    while (pos < dlen) {
        int v = code_next(cur, &b);
        if (v < 0) goto out;
        if (v < 0x100) { dst[pos++] = (uint8_t)v; cur = &first; continue; }
        cur = &second;
        int length;
        if (v < 0x13E) length = v - 0x100 + 3;
        else if (v == 0x13E) length = (int)bits_le(&b, 10) + 65;
        else if (v == 0x13F) length = (int)bits_le(&b, 15) + 65;
        else break;                                     /* end of stream */
        int bl = code_next(&offs, &b), offset;
        if (bl < 0) goto out;
        if (bl == 0) offset = 1;
        else if (bl == 1) offset = 2;
        else offset = (1 << (bl - 1)) + (int)bits_le(&b, bl - 1) + 1;
        /* 64 KB window, zero-filled at the start */
        for (int k = 0; k < length && pos < dlen; k++, pos++) dst[pos] = pos >= (size_t)offset ? dst[pos - (size_t)offset] : 0;
    }
    rc = pos == dlen ? 0 : -1;
out:
    code_free(&first); if (!shared) code_free(&second); code_free(&offs); code_free(&meta);
    return rc;
}
