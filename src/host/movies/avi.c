/*
 * avi.c: the AVI reader (avi.h). Port code, not from the original.
 *
 * RIFF layout: "RIFF" size "AVI " { LIST hdrl { avih, LIST strl { strh, strf, ... } ... },
 * LIST movi { "00dc" / "00db" video, "01wb" audio, LIST "rec " { ... }, JUNK ... }, idx1 }.
 * Chunks are padded to an even size. The stream number is the chunk id's first two characters.
 */
#include "avi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t off, size; } Chunk;

struct AviFile {
    FILE* fp;
    AviInfo info;
    int vstream, astream;     /* stream numbers, -1: none */
    Chunk* video;             /* video chunks in order */
    int nvideo, capvideo;
    Chunk* audio;
    int naudio, capaudio;
    uint8_t* buf;             /* the last frame read */
    uint32_t bufcap;
};

static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
#define TAG(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)

static int read_at(FILE* fp, uint32_t off, void* dst, uint32_t n)
{
    if (fseek(fp, (long)off, SEEK_SET) != 0) return 0;
    return fread(dst, 1, n, fp) == n;
}

static int push(Chunk** a, int* n, int* cap, uint32_t off, uint32_t size)
{
    if (*n == *cap) {
        int nc = *cap ? *cap * 2 : 1024;
        Chunk* p = realloc(*a, sizeof(Chunk) * (size_t)nc);
        if (!p) return 0;
        *a = p;
        *cap = nc;
    }
    (*a)[*n].off = off;
    (*a)[*n].size = size;
    (*n)++;
    return 1;
}

/* the stream number of a data chunk id ("00dc" -> 0), -1 when not two digits */
static int stream_of(uint32_t id)
{
    int a = (int)(id & 0xff) - '0', b = (int)((id >> 8) & 0xff) - '0';
    if (a < 0 || a > 9 || b < 0 || b > 9) return -1;
    return a * 10 + b;
}

/* one strl list: strh + strf */
static void read_strl(AviFile* f, uint32_t off, uint32_t end, int num)
{
    uint8_t h[8];
    uint32_t type = 0;
    while (off + 8 <= end && read_at(f->fp, off, h, 8)) {
        uint32_t id = rd32(h), size = rd32(h + 4);
        if (id == TAG('s', 't', 'r', 'h') && size >= 8) {
            uint8_t s[8];
            if (read_at(f->fp, off + 8, s, 8)) type = rd32(s);
        } else if (id == TAG('s', 't', 'r', 'f')) {
            uint8_t s[40] = {0};
            uint32_t n = size < sizeof s ? size : (uint32_t)sizeof s;
            if (!read_at(f->fp, off + 8, s, n)) return;
            if (type == TAG('v', 'i', 'd', 's') && f->vstream < 0 && n >= 20) {
                f->vstream = num;
                f->info.width = (int)rd32(s + 4);
                f->info.height = (int)rd32(s + 8);
                if (f->info.height < 0) f->info.height = -f->info.height;
                f->info.fourcc = rd32(s + 16);
            } else if (type == TAG('a', 'u', 'd', 's') && f->astream < 0 && n >= 16 && rd16(s) == 1) {
                f->astream = num;
                f->info.a_channels = rd16(s + 2);
                f->info.a_rate = (int)rd32(s + 4);
                f->info.a_bits = rd16(s + 14);
            }
        }
        off += 8 + size + (size & 1);
    }
}

/* the chunks of movi (and its rec lists) */
static int read_movi(AviFile* f, uint32_t off, uint32_t end, int depth)
{
    uint8_t h[12];
    while (off + 8 <= end && read_at(f->fp, off, h, 8)) {
        uint32_t id = rd32(h), size = rd32(h + 4);
        if (id == TAG('L', 'I', 'S', 'T')) {
            if (depth < 4 && size >= 4) read_movi(f, off + 12, off + 8 + size, depth + 1);
        } else {
            int s = stream_of(id);
            uint32_t kind = id >> 16;   /* the last two characters: "dc" / "db" / "wb" */
            /* the video stream's chunks: "dc" / "db", and "iv" in STLOGO.AVI (Indeo 4); not a
             * palette change "pc" */
            if (s >= 0 && s == f->vstream && kind != TAG('p', 'c', 0, 0)) {
                if (!push(&f->video, &f->nvideo, &f->capvideo, off + 8, size)) return 0;
            } else if (s >= 0 && s == f->astream && kind == TAG('w', 'b', 0, 0)) {
                if (!push(&f->audio, &f->naudio, &f->capaudio, off + 8, size)) return 0;
                f->info.a_bytes += size;
            }
        }
        off += 8 + size + (size & 1);
    }
    return 1;
}

AviFile* avi_open(const char* path, const char** why)
{
    const char* dummy;
    if (!why) why = &dummy;
    FILE* fp = fopen(path, "rb");
    if (!fp) { *why = "cannot open the file"; return NULL; }
    uint8_t h[12];
    if (!read_at(fp, 0, h, 12) || rd32(h) != TAG('R', 'I', 'F', 'F') || rd32(h + 8) != TAG('A', 'V', 'I', ' ')) {
        fclose(fp);
        *why = "not an AVI file";
        return NULL;
    }
    AviFile* f = calloc(1, sizeof *f);
    if (!f) { fclose(fp); *why = "out of memory"; return NULL; }
    f->fp = fp;
    f->vstream = f->astream = -1;
    fseek(fp, 0, SEEK_END);
    long flen = ftell(fp);
    uint32_t end = 8 + rd32(h + 4);
    if (flen > 0 && end > (uint32_t)flen) end = (uint32_t)flen;
    uint32_t off = 12, movi = 0, movi_end = 0;
    while (off + 8 <= end && read_at(fp, off, h, 12)) {
        uint32_t id = rd32(h), size = rd32(h + 4);
        if (id == TAG('L', 'I', 'S', 'T') && rd32(h + 8) == TAG('h', 'd', 'r', 'l')) {
            uint32_t o = off + 12, e = off + 8 + size;
            int num = 0;
            while (o + 8 <= e && read_at(fp, o, h, 12)) {
                uint32_t id2 = rd32(h), sz2 = rd32(h + 4);
                if (id2 == TAG('a', 'v', 'i', 'h') && sz2 >= 4) {
                    uint8_t a[4];
                    if (read_at(fp, o + 8, a, 4)) f->info.us_per_frame = rd32(a);
                } else if (id2 == TAG('L', 'I', 'S', 'T') && rd32(h + 8) == TAG('s', 't', 'r', 'l')) {
                    read_strl(f, o + 12, o + 8 + sz2, num++);
                }
                o += 8 + sz2 + (sz2 & 1);
            }
        } else if (id == TAG('L', 'I', 'S', 'T') && rd32(h + 8) == TAG('m', 'o', 'v', 'i')) {
            movi = off + 12;
            movi_end = off + 8 + size;
            if (movi_end > end) movi_end = end;
        }
        off += 8 + size + (size & 1);
    }
    if (f->vstream < 0 || !movi) { *why = f->vstream < 0 ? "no video stream" : "no movi list"; avi_close(f); return NULL; }
    if (!read_movi(f, movi, movi_end, 0)) { *why = "out of memory"; avi_close(f); return NULL; }
    f->info.frames = f->nvideo;
    f->info.has_audio = f->astream >= 0 && f->naudio > 0 && (f->info.a_bits == 8 || f->info.a_bits == 16) &&
                        f->info.a_channels >= 1 && f->info.a_channels <= 2 && f->info.a_rate > 0;
    if (!f->info.us_per_frame) f->info.us_per_frame = 66667;
    return f;
}

void avi_close(AviFile* f)
{
    if (!f) return;
    if (f->fp) fclose(f->fp);
    free(f->video);
    free(f->audio);
    free(f->buf);
    free(f);
}

const AviInfo* avi_info(const AviFile* f) { return &f->info; }

int avi_read_frame(AviFile* f, int i, const uint8_t** data)
{
    if (i < 0 || i >= f->nvideo) return -1;
    uint32_t n = f->video[i].size;
    if (n + 16 > f->bufcap) {
        uint8_t* p = realloc(f->buf, n + 16);
        if (!p) return -1;
        f->buf = p;
        f->bufcap = n + 16;
    }
    if (n && !read_at(f->fp, f->video[i].off, f->buf, n)) return -1;
    memset(f->buf + n, 0, 16);
    *data = f->buf;
    return (int)n;
}

uint32_t avi_read_audio(AviFile* f, uint8_t* dst, uint32_t cap)
{
    uint32_t got = 0;
    for (int i = 0; i < f->naudio && got < cap; i++) {
        uint32_t n = f->audio[i].size;
        if (n > cap - got) n = cap - got;
        if (!read_at(f->fp, f->audio[i].off, dst + got, n)) break;
        got += n;
    }
    return got;
}
