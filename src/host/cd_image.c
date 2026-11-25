/*
 * cd_image.c: the player's .cue / .bin CD image for CD audio (port code; see cd_image.h).
 *
 * The cue sheet: FILE "name" BINARY (relative to the .cue's folder, matched case insensitively
 * when the exact name is missing), TRACK nn AUDIO | MODEx/2352, INDEX 00 / 01 mm:ss:ff. Other
 * lines (REM, PREGAP, POSTGAP, FLAGS, CATALOG ...) are ignored: a PREGAP has no data in the file
 * and is not counted. The files follow each other on the disc: a file's first sector is the LBA
 * after the previous file's last. A track ends where the next one's INDEX 00 (or 01) starts, the
 * last one at the end of its file.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "cd_image.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_FILES 100

typedef struct { FILE* f; uint32_t base, sectors; } CdFile;
typedef struct { int audio, file; uint32_t idx0, idx1; int has0; } CdTrack;

struct CdImage {
    CdFile files[MAX_FILES];
    int nfiles;
    CdTrack t[CD_MAX_TRACKS + 1];   /* 1-based */
    int ntracks;
    uint32_t end;
    uint8_t buf[CD_SECTOR_BYTES * 16];
};

static const char* data_root(void)
{
    const char* r = getenv("TM2_DATA");
    return r && *r ? r : "disc/files";
}

const char* cd_image_default_path(char* out, int cap)
{
    const char* e = getenv("TM2_CDIMAGE");
    if (e && *e) { snprintf(out, (size_t)cap, "%s", e); return out; }
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", data_root());
    size_t l = strlen(dir);
    while (l > 1 && dir[l - 1] == '/') dir[--l] = 0;
    char* s = strrchr(dir, '/');
    if (s && s != dir) *s = 0;
    else if (s) s[1] = 0;
    else snprintf(dir, sizeof dir, ".");
    DIR* d = opendir(dir);
    if (!d) return NULL;
    struct dirent* de;
    char best[512] = "";
    while ((de = readdir(d)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n > 4 && strcasecmp(de->d_name + n - 4, ".cue") == 0 && de->d_name[0] != '.')
            if (!best[0] || strcmp(de->d_name, best) < 0) snprintf(best, sizeof best, "%s", de->d_name);
    }
    closedir(d);
    if (!best[0]) return NULL;
    snprintf(out, (size_t)cap, "%s/%s", dir, best);
    return out;
}

/* open dir/name, or a name in dir that matches it case insensitively */
static FILE* open_in(const char* dir, const char* name)
{
    char p[1536];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE* f = fopen(p, "rb");
    if (f) return f;
    DIR* d = opendir(dir);
    if (!d) return NULL;
    struct dirent* de;
    while (!f && (de = readdir(d)) != NULL)
        if (strcasecmp(de->d_name, name) == 0) {
            snprintf(p, sizeof p, "%s/%s", dir, de->d_name);
            f = fopen(p, "rb");
        }
    closedir(d);
    return f;
}

static int parse_msf(const char* s, uint32_t* lba)
{
    unsigned m, sec, f;
    if (sscanf(s, "%u:%u:%u", &m, &sec, &f) != 3 || sec > 59 || f > 74) return 0;
    *lba = (m * 60u + sec) * 75u + f;
    return 1;
}

CdImage* cd_image_open(const char* cue, const char** why)
{
    const char* dummy;
    if (!why) why = &dummy;
    FILE* cf = fopen(cue, "r");
    if (!cf) { *why = "cannot open the .cue"; return NULL; }
    CdImage* c = calloc(1, sizeof *c);
    if (!c) { fclose(cf); *why = "out of memory"; return NULL; }
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", cue);
    char* sl = strrchr(dir, '/');
    if (sl) *sl = 0; else snprintf(dir, sizeof dir, ".");

    char line[1024];
    int cur = 0;
    *why = NULL;
    while (!*why && fgets(line, sizeof line, cf)) {
        char* p = line;
        while (isspace((unsigned char)*p)) p++;
        if (strncasecmp(p, "FILE", 4) == 0 && isspace((unsigned char)p[4])) {
            p += 5;
            while (isspace((unsigned char)*p)) p++;
            char name[512];
            size_t k = 0;
            if (*p == '"') { p++; while (*p && *p != '"' && k + 1 < sizeof name) name[k++] = *p++; }
            else while (*p && !isspace((unsigned char)*p) && k + 1 < sizeof name) name[k++] = *p++;
            name[k] = 0;
            if (c->nfiles == MAX_FILES) { *why = "too many FILE lines"; break; }
            CdFile* f = &c->files[c->nfiles];
            f->f = open_in(dir, name);
            if (!f->f) { *why = "cannot open a FILE of the .cue"; break; }
            fseek(f->f, 0, SEEK_END);
            long sz = ftell(f->f);
            f->sectors = sz > 0 ? (uint32_t)(sz / CD_SECTOR_BYTES) : 0;
            f->base = c->nfiles ? c->files[c->nfiles - 1].base + c->files[c->nfiles - 1].sectors : 0;
            c->nfiles++;
        } else if (strncasecmp(p, "TRACK", 5) == 0 && isspace((unsigned char)p[5])) {
            int no = atoi(p + 6);
            if (!c->nfiles) { *why = "TRACK before FILE"; break; }
            if (no != c->ntracks + 1 || no > CD_MAX_TRACKS) { *why = "track numbers out of order"; break; }
            cur = no;
            c->ntracks = no;
            c->t[cur].file = c->nfiles - 1;
            c->t[cur].audio = strstr(p, "AUDIO") != NULL || strstr(p, "audio") != NULL;
        } else if (strncasecmp(p, "INDEX", 5) == 0 && isspace((unsigned char)p[5]) && cur) {
            int idx = 0;
            char msf[32];
            uint32_t lba;
            if (sscanf(p + 6, "%d %31s", &idx, msf) != 2 || !parse_msf(msf, &lba)) { *why = "bad INDEX"; break; }
            lba += c->files[c->t[cur].file].base;
            if (idx == 0) { c->t[cur].idx0 = lba; c->t[cur].has0 = 1; }
            else if (idx == 1) c->t[cur].idx1 = lba;
        }
    }
    fclose(cf);
    if (!*why && !c->ntracks) *why = "no tracks";
    if (*why) { cd_image_close(c); return NULL; }
    CdFile* last = &c->files[c->nfiles - 1];
    c->end = last->base + last->sectors;
    return c;
}

void cd_image_close(CdImage* c)
{
    if (!c) return;
    for (int i = 0; i < c->nfiles; i++) if (c->files[i].f) fclose(c->files[i].f);
    free(c);
}

int cd_image_tracks(const CdImage* c) { return c->ntracks; }
int cd_image_is_audio(const CdImage* c, int t) { return t >= 1 && t <= c->ntracks && c->t[t].audio; }
uint32_t cd_image_track_lba(const CdImage* c, int t)
{
    return t >= 1 && t <= c->ntracks ? c->t[t].idx1 : c->end;
}
static uint32_t track_end(const CdImage* c, int t)
{
    if (t >= c->ntracks) return c->end;
    const CdTrack* n = &c->t[t + 1];
    if (c->t[t].file != n->file) {   /* the end of this track's file */
        const CdFile* f = &c->files[c->t[t].file];
        return f->base + f->sectors;
    }
    return n->has0 ? n->idx0 : n->idx1;
}
uint32_t cd_image_track_len(const CdImage* c, int t)
{
    if (t < 1 || t > c->ntracks) return 0;
    uint32_t e = track_end(c, t), s = c->t[t].idx1;
    return e > s ? e - s : 0;
}
uint32_t cd_image_end_lba(const CdImage* c) { return c->end; }
int cd_image_track_at(const CdImage* c, uint32_t lba)
{
    int t = 1;
    for (int k = 1; k <= c->ntracks; k++) if (lba >= (c->t[k].has0 ? c->t[k].idx0 : c->t[k].idx1)) t = k;
    return t;
}

int cd_image_read(CdImage* c, uint32_t lba, int n, int16_t* out)
{
    int got = 0;
    memset(out, 0, (size_t)n * CD_SECTOR_FRAMES * 4);
    while (n > 0) {
        int fi = -1;
        for (int i = 0; i < c->nfiles; i++)
            if (lba >= c->files[i].base && lba < c->files[i].base + c->files[i].sectors) { fi = i; break; }
        int t = cd_image_track_at(c, lba);
        if (fi < 0 || !c->t[t].audio) { lba++; n--; out += CD_SECTOR_FRAMES * 2; continue; }
        CdFile* f = &c->files[fi];
        uint32_t room = f->base + f->sectors - lba;
        int k = n < 16 ? n : 16;
        if ((uint32_t)k > room) k = (int)room;
        if (t < c->ntracks && c->t[t].file == fi) {   /* stop at a data track */
            uint32_t te = track_end(c, t);
            if (te > lba && (uint32_t)k > te - lba) k = (int)(te - lba);
        }
        if (k < 1) k = 1;
        size_t rd = 0;
        if (fseek(f->f, (long)(lba - f->base) * CD_SECTOR_BYTES, SEEK_SET) == 0)
            rd = fread(c->buf, CD_SECTOR_BYTES, (size_t)k, f->f);
        for (size_t i = 0; i < rd * CD_SECTOR_FRAMES * 2; i++)
            out[i] = (int16_t)(c->buf[2 * i] | c->buf[2 * i + 1] << 8);
        got += (int)rd;
        lba += (uint32_t)k;
        n -= k;
        out += (size_t)k * CD_SECTOR_FRAMES * 2;
    }
    return got;
}
