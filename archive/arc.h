/* archive: reads classic Mac archives into memory, natively.
 * Formats: BinHex 4.0 (.hqx), MacBinary I/II/III (.bin), StuffIt 1.5-4 (.sit, "SIT!"),
 * StuffIt 5 (.sit), and any nesting of these (e.g. a .sit inside a .hqx).
 * StuffIt methods: 0 none, 1 RLE90, 13 LZ+Huffman, 15 Arsenic.
 *
 * Licensing: arc.c, hqx/MacBinary/StuffIt container code: MIT (this project).
 * sit13.c and arsenic.c are ports of The Unarchiver's XADMaster decoders and are LGPL-2.1. */
#ifndef ARC_H
#define ARC_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    char path[512];             /* "Folder/File", Mac Roman bytes, '/' separated */
    uint32_t type, creator;
    uint16_t finder_flags;
    uint8_t *data; uint32_t dlen;
    uint8_t *rsrc; uint32_t rlen;
} ArcFile;

typedef struct {
    ArcFile *files; int n, cap;
    char err[256];
} ArcSet;

/* Decodes the file (any supported format, nested archives expanded). Returns 0 on success.
 * Files whose fork cannot be decoded are reported in set->err and the call fails. */
int  arc_load_file(const char *path, ArcSet *set);
void arc_free(ArcSet *set);

/* decompressors (internal) */
int sit13_decode(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen);       /* LGPL, sit13.c */
int arsenic_decode(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen);     /* LGPL, arsenic.c */
#endif
