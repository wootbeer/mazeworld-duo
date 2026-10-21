/* arcdump: list an archive and optionally write each file's forks (<dir>/<path>.data / .rsrc). */
#include "../arc.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static void mkdirs(char *p) { for (char *q = p + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(p, 0755); *q = '/'; } }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: arcdump archive [outdir]\n"); return 2; }
    ArcSet s;
    if (arc_load_file(argv[1], &s)) { fprintf(stderr, "error: %s\n", s.err); return 1; }
    for (int i = 0; i < s.n; i++) {
        ArcFile *f = &s.files[i];
        printf("%-60s %c%c%c%c/%c%c%c%c data %7u rsrc %7u\n", f->path, f->type >> 24, f->type >> 16, f->type >> 8, f->type,
               f->creator >> 24, f->creator >> 16, f->creator >> 8, f->creator, f->dlen, f->rlen);
        if (argc > 2) {
            char p[1024];
            snprintf(p, sizeof p, "%s/%s.data", argv[2], f->path); mkdirs(p);
            FILE *o = fopen(p, "wb"); if (o) { fwrite(f->data, 1, f->dlen, o); fclose(o); }
            snprintf(p, sizeof p, "%s/%s.rsrc", argv[2], f->path);
            o = fopen(p, "wb"); if (o) { fwrite(f->rsrc, 1, f->rlen, o); fclose(o); }
        }
    }
    arc_free(&s);
    return 0;
}
