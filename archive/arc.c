/* Archive formats: detection, BinHex 4.0, MacBinary, StuffIt 1.5-4 and StuffIt 5 containers.
 * Container layouts follow the descriptions in Deark (MIT, Jason Summers) and the formats' own
 * documentation; no code is copied. MIT license (this project). */
#include "arc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static int fail(ArcSet *s, const char *msg, const char *what) {
    snprintf(s->err, sizeof s->err, "%.120s%s%.120s", msg, what ? ": " : "", what ? what : "");
    return -1;
}

/* CRC-16/ARC (reflected 0x8005), used by StuffIt for forks */
static uint16_t crc16_arc(const uint8_t *p, size_t n) {
    uint16_t c = 0;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c & 1) ? (uint16_t)((c >> 1) ^ 0xA001) : (uint16_t)(c >> 1); }
    return c;
}
/* CRC-16/XMODEM, used by BinHex */
static uint16_t crc16_xmodem(const uint8_t *p, size_t n) {
    uint16_t c = 0;
    for (size_t i = 0; i < n; i++) { c ^= (uint16_t)(p[i] << 8); for (int k = 0; k < 8; k++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1); }
    return c;
}

static int add_file(ArcSet *s, const char *path, uint32_t type, uint32_t creator, uint16_t ff,
                    uint8_t *data, uint32_t dlen, uint8_t *rsrc, uint32_t rlen);
static int load_any(const uint8_t *buf, size_t len, ArcSet *set, const char *prefix, int depth);

/* ---------- StuffIt RLE90 (method 1) ---------- */
static int rle90(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen) {
    size_t o = 0; int last = 0;
    for (size_t i = 0; i < slen && o < dlen; i++) {
        uint8_t b = src[i];
        if (b == 0x90 && i + 1 < slen) {
            uint8_t n = src[++i];
            if (n == 0) { dst[o++] = 0x90; last = 0x90; }
            else for (int k = 1; k < n && o < dlen; k++) dst[o++] = (uint8_t)last;
        } else { dst[o++] = b; last = b; }
    }
    return o == dlen ? 0 : -1;
}

static int decode_fork(ArcSet *s, int method, const uint8_t *src, uint32_t clen, uint32_t ulen, uint8_t **out, const char *name) {
    *out = NULL;
    if (ulen == 0) return 0;
    if (method & 0x10) return fail(s, "encrypted StuffIt archives are not supported", name);
    uint8_t *dst = malloc(ulen);
    int r;
    switch (method & 0x0F) {
    case 0: r = clen >= ulen ? (memcpy(dst, src, ulen), 0) : -1; break;
    case 1: r = rle90(src, clen, dst, ulen); break;
    case 13: r = sit13_decode(src, clen, dst, ulen); break;
    case 15: r = arsenic_decode(src, clen, dst, ulen); break;
    default: free(dst); { char m[64]; snprintf(m, sizeof m, "StuffIt compression method %d is not supported", method & 0x0F); return fail(s, m, name); }
    }
    if (r) { free(dst); return fail(s, "corrupt compressed data", name); }
    *out = dst;
    return 0;
}

/* ---------- StuffIt 1.5 - 4 ("SIT!" and friends) ---------- */
static int is_sit_classic(const uint8_t *b, size_t n) {
    static const char *sigs[] = {"SIT!", "ST46", "ST50", "ST60", "ST65", "STin", "STi2", "STi3", "STi4", "ST4", 0};
    if (n < 22 || memcmp(b + 10, "rLau", 4)) return 0;
    for (int i = 0; sigs[i]; i++) if (!memcmp(b, sigs[i], strlen(sigs[i]))) return 1;
    return 0;
}
static int load_sit_classic(const uint8_t *b, size_t n, ArcSet *s, const char *prefix, int depth) {
    size_t pos = 22;
    char path[512]; snprintf(path, sizeof path, "%s", prefix);
    while (pos + 112 <= n) {
        const uint8_t *h = b + pos;
        int rm = h[0], dm = h[1], nl = h[2] > 63 ? 63 : h[2];
        char name[64]; memcpy(name, h + 3, (size_t)nl); name[nl] = 0;
        uint32_t rlen = be32(h + 84), dlen = be32(h + 88), rcl = be32(h + 92), dcl = be32(h + 96);
        pos += 112;
        if (rm == 32 || dm == 32) {                     /* folder start */
            size_t L = strlen(path); snprintf(path + L, sizeof path - L, "%s/", name); continue;
        }
        if (rm == 33 || dm == 33) {                     /* folder end */
            size_t L = strlen(path); if (L) path[L - 1] = 0;
            char *sl = strrchr(path, '/'); if (sl) sl[1] = 0; else path[0] = 0;
            continue;
        }
        if (pos + rcl + dcl > n) return fail(s, "truncated StuffIt archive", name);
        uint8_t *rs = NULL, *ds = NULL;
        if (decode_fork(s, rm, b + pos, rcl, rlen, &rs, name)) return -1;
        if (decode_fork(s, dm, b + pos + rcl, dcl, dlen, &ds, name)) { free(rs); return -1; }
        pos += rcl + dcl;
        if ((rlen && crc16_arc(rs, rlen) != be16(h + 100) && (rm & 15) != 15) ||
            (dlen && crc16_arc(ds, dlen) != be16(h + 102) && (dm & 15) != 15)) {
            free(rs); free(ds); return fail(s, "checksum mismatch", name);
        }
        char full[512]; snprintf(full, sizeof full, "%.440s%.63s", path, name);
        if (add_file(s, full, be32(h + 66), be32(h + 70), be16(h + 74), ds, dlen, rs, rlen)) return -1;
    }
    return 0;
}

/* ---------- StuffIt 5 ---------- */
static int is_sit5(const uint8_t *b, size_t n) { return n > 100 && !memcmp(b, "StuffIt (c)1997", 15); }
static int sit5_members(const uint8_t *b, size_t n, ArcSet *s, uint32_t pos, int count, const char *prefix, int depth) {
    for (int k = 0; k < count && pos; k++) {
        if ((size_t)pos + 48 > n || be32(b + pos) != 0xA5A5A5A5u) return fail(s, "bad StuffIt 5 entry", NULL);
        const uint8_t *h = b + pos;
        uint16_t hsize = be16(h + 6);
        uint8_t flags = h[9];
        uint32_t next = be32(h + 22);
        uint16_t fnlen = be16(h + 30);
        size_t q = (size_t)pos + 34;
        if (flags & 0x20) return fail(s, "encrypted StuffIt archives are not supported", NULL);
        if (flags & 0x40) {                              /* folder */
            uint32_t first = be32(b + q); uint16_t nfiles = be16(b + q + 12);
            q += 14;
            char name[256]; size_t L = fnlen > 255 ? 255 : fnlen; memcpy(name, b + q, L); name[L] = 0;
            char sub[512]; snprintf(sub, sizeof sub, "%.250s%.250s/", prefix, name);
            if (depth > 16) return fail(s, "folders nested too deeply", NULL);
            if (nfiles && sit5_members(b, n, s, first, nfiles, sub, depth + 1)) return -1;
            pos = next; continue;
        }
        uint32_t dlen = be32(b + q), dcl = be32(b + q + 4); uint16_t dcrc = be16(b + q + 8); uint8_t dm = b[q + 12];
        q += 13; q += b[q] + 1u;                         /* password data */
        char name[256]; size_t L = fnlen > 255 ? 255 : fnlen; memcpy(name, b + q, L); name[L] = 0;
        q = (size_t)pos + hsize;
        uint16_t flags2 = be16(b + q); q += 4;
        uint32_t type = be32(b + q), creator = be32(b + q + 4); uint16_t ff = be16(b + q + 8);
        q += 10 + 22;
        uint32_t rlen = 0, rcl = 0; uint16_t rcrc = 0; uint8_t rm = 0;
        if (flags2 & 1) { rlen = be32(b + q); rcl = be32(b + q + 4); rcrc = be16(b + q + 8); rm = b[q + 12]; q += 13; q += b[q] + 1u; }
        if (q + rcl + dcl > n) return fail(s, "truncated StuffIt 5 archive", name);
        uint8_t *rs = NULL, *ds = NULL;
        if (decode_fork(s, rm, b + q, rcl, rlen, &rs, name)) return -1;
        if (decode_fork(s, dm, b + q + rcl, dcl, dlen, &ds, name)) { free(rs); return -1; }
        if ((rlen && (rm & 15) != 15 && crc16_arc(rs, rlen) != rcrc) || (dlen && (dm & 15) != 15 && crc16_arc(ds, dlen) != dcrc)) {
            free(rs); free(ds); return fail(s, "checksum mismatch", name);
        }
        char full[512]; snprintf(full, sizeof full, "%.250s%.250s", prefix, name);
        if (add_file(s, full, type, creator, ff, ds, dlen, rs, rlen)) return -1;
        pos = next;
    }
    return 0;
}
static int load_sit5(const uint8_t *b, size_t n, ArcSet *s, const char *prefix) {
    uint16_t nm = be16(b + 92); uint32_t first = be32(b + 94);
    return sit5_members(b, n, s, first, nm, prefix, 0);
}

/* ---------- BinHex 4.0 ---------- */
static int is_hqx(const uint8_t *b, size_t n) {
    const char *k = "(This file must be converted with BinHex";
    size_t L = strlen(k);
    for (size_t i = 0; i + L <= n && i < 4096; i++) if (!memcmp(b + i, k, L)) return 1;
    return 0;
}
static int load_hqx(const uint8_t *b, size_t n, ArcSet *s, const char *prefix, int depth) {
    static const char *alpha = "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr";
    int8_t lut[256]; memset(lut, -1, sizeof lut);
    for (int i = 0; alpha[i]; i++) lut[(uint8_t)alpha[i]] = (int8_t)i;
    const char *k = "(This file must be converted with BinHex";
    size_t i = 0, L = strlen(k);
    while (i + L <= n && memcmp(b + i, k, L)) i++;
    while (i < n && b[i] != ':') i++;
    i++;
    uint8_t *raw = malloc(n), *dec = malloc(n * 2 + 16);
    size_t nr = 0, nd = 0; uint32_t acc = 0; int bits = 0;
    for (; i < n && b[i] != ':'; i++) {
        if (b[i] == '\r' || b[i] == '\n' || b[i] == ' ' || b[i] == '\t') continue;
        if (lut[b[i]] < 0) { free(raw); free(dec); return fail(s, "bad character in BinHex data", NULL); }
        acc = acc << 6 | (uint32_t)lut[b[i]]; bits += 6;
        if (bits >= 8) { bits -= 8; raw[nr++] = (uint8_t)(acc >> bits); }
    }
    uint8_t last = 0;
    for (size_t j = 0; j < nr; j++) {               /* RLE: 0x90 n */
        if (raw[j] == 0x90 && j + 1 < nr) {
            uint8_t c = raw[++j];
            if (c == 0) { dec[nd++] = 0x90; last = 0x90; }
            else for (int r = 1; r < c; r++) dec[nd++] = last;
        } else { dec[nd++] = raw[j]; last = raw[j]; }
        if (nd > n * 2) break;
    }
    free(raw);
    size_t p = 0; int nl = dec[0];
    char name[64]; memcpy(name, dec + 1, (size_t)(nl > 63 ? 63 : nl)); name[nl > 63 ? 63 : nl] = 0;
    p = 1 + (size_t)nl + 1;
    uint32_t type = be32(dec + p), creator = be32(dec + p + 4); uint16_t ff = be16(dec + p + 8);
    uint32_t dl = be32(dec + p + 10), rl = be32(dec + p + 14);
    p += 18;
    if (crc16_xmodem(dec, p) != be16(dec + p) || p + 2 + dl + 2 + rl + 2 > nd) { free(dec); return fail(s, "BinHex header damaged", name); }
    p += 2;
    if (crc16_xmodem(dec + p, dl) != be16(dec + p + dl)) { free(dec); return fail(s, "BinHex data fork checksum mismatch", name); }
    uint8_t *data = dl ? malloc(dl) : NULL; if (dl) memcpy(data, dec + p, dl);
    p += dl + 2;
    if (crc16_xmodem(dec + p, rl) != be16(dec + p + rl)) { free(dec); free(data); return fail(s, "BinHex resource fork checksum mismatch", name); }
    uint8_t *rsrc = rl ? malloc(rl) : NULL; if (rl) memcpy(rsrc, dec + p, rl);
    free(dec);
    /* a BinHexed archive is expanded; anything else is a file */
    if (dl && depth < 4) {
        ArcSet inner = {0};
        if (load_any(data, dl, &inner, prefix, depth + 1) == 0 && inner.n) {
            for (int f = 0; f < inner.n; f++) { add_file(s, inner.files[f].path, inner.files[f].type, inner.files[f].creator, inner.files[f].finder_flags, inner.files[f].data, inner.files[f].dlen, inner.files[f].rsrc, inner.files[f].rlen); inner.files[f].data = inner.files[f].rsrc = NULL; }
            arc_free(&inner); free(data); free(rsrc); return 0;
        }
        if (inner.err[0]) { snprintf(s->err, sizeof s->err, "%s", inner.err); arc_free(&inner); free(data); free(rsrc); return -1; }
        arc_free(&inner);
    }
    char full[512]; snprintf(full, sizeof full, "%.250s%.250s", prefix, name);
    return add_file(s, full, type, creator, ff, data, dl, rsrc, rl);
}

/* ---------- MacBinary I/II/III ---------- */
static int is_macbinary(const uint8_t *b, size_t n) {
    if (n < 128 || b[0] != 0 || b[74] != 0 || b[82] != 0 || b[1] < 1 || b[1] > 63) return 0;
    uint32_t dl = be32(b + 83), rl = be32(b + 87);
    size_t need = 128 + ((dl + 127) & ~127u) + rl;
    return dl < 0x800000 && rl < 0x800000 && need <= n + 127;
}
static int load_macbinary(const uint8_t *b, size_t n, ArcSet *s, const char *prefix, int depth) {
    char name[64]; memcpy(name, b + 2, b[1]); name[b[1]] = 0;
    uint32_t dl = be32(b + 83), rl = be32(b + 87);
    size_t dp = 128, rp = 128 + ((dl + 127) & ~127u);
    if (rp + rl > n) return fail(s, "truncated MacBinary file", name);
    uint8_t *data = dl ? malloc(dl) : NULL, *rsrc = rl ? malloc(rl) : NULL;
    if (dl) memcpy(data, b + dp, dl);
    if (rl) memcpy(rsrc, b + rp, rl);
    if (dl && depth < 4) {
        ArcSet inner = {0};
        if (load_any(data, dl, &inner, prefix, depth + 1) == 0 && inner.n) {
            for (int f = 0; f < inner.n; f++) { add_file(s, inner.files[f].path, inner.files[f].type, inner.files[f].creator, inner.files[f].finder_flags, inner.files[f].data, inner.files[f].dlen, inner.files[f].rsrc, inner.files[f].rlen); inner.files[f].data = inner.files[f].rsrc = NULL; }
            arc_free(&inner); free(data); free(rsrc); return 0;
        }
        arc_free(&inner);
    }
    char full[512]; snprintf(full, sizeof full, "%.250s%.250s", prefix, name);
    return add_file(s, full, be32(b + 65), be32(b + 69), (uint16_t)(b[73] << 8 | b[101]), data, dl, rsrc, rl);
}

/* ---------- common ---------- */
static int add_file(ArcSet *s, const char *path, uint32_t type, uint32_t creator, uint16_t ff,
                    uint8_t *data, uint32_t dlen, uint8_t *rsrc, uint32_t rlen) {
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 16; s->files = realloc(s->files, (size_t)s->cap * sizeof *s->files); }
    ArcFile *f = &s->files[s->n++];
    memset(f, 0, sizeof *f);
    snprintf(f->path, sizeof f->path, "%s", path);
    f->type = type; f->creator = creator; f->finder_flags = ff;
    f->data = data; f->dlen = dlen; f->rsrc = rsrc; f->rlen = rlen;
    return 0;
}

static int load_any(const uint8_t *buf, size_t len, ArcSet *set, const char *prefix, int depth) {
    if (is_sit5(buf, len)) return load_sit5(buf, len, set, prefix);
    if (is_sit_classic(buf, len)) return load_sit_classic(buf, len, set, prefix, depth);
    if (is_hqx(buf, len)) return load_hqx(buf, len, set, prefix, depth);
    if (is_macbinary(buf, len)) return load_macbinary(buf, len, set, prefix, depth);
    return fail(set, "not a recognised archive (expected .hqx, .sit or MacBinary)", NULL);
}

int arc_load(const uint8_t *buf, size_t len, ArcSet *set) { memset(set, 0, sizeof *set); return load_any(buf, len, set, "", 0); }

int arc_load_file(const char *path, ArcSet *set) {
    memset(set, 0, sizeof *set);
    FILE *f = fopen(path, "rb");
    if (!f) return fail(set, "cannot open", path);
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return fail(set, "cannot read", path); }
    fclose(f);
    int r = load_any(b, (size_t)n, set, "", 0);
    free(b);
    return r;
}

void arc_free(ArcSet *set) {
    for (int i = 0; i < set->n; i++) { free(set->files[i].data); free(set->files[i].rsrc); }
    free(set->files); set->files = NULL; set->n = set->cap = 0;
}

const ArcFile *arc_find(const ArcSet *set, const char *name) {
    for (int i = 0; i < set->n; i++) {
        const char *b = strrchr(set->files[i].path, '/');
        b = b ? b + 1 : set->files[i].path;
        if (!strcmp(b, name)) return &set->files[i];
    }
    return NULL;
}
