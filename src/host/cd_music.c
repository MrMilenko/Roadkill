/*
 * cd_music.c: cd_music.h over dr_mp3 / dr_flac (third_party/dr_libs). Port code.
 *
 * The CD mixer reads forward a few sectors at a time, so the decoder normally just goes on; a
 * read somewhere else (a new track, the game's MCI play from a position) seeks. dr_flac seeks with
 * the file's seek table; for MP3 a seek table is built the first time a seek isn't to the start.
 */
#include "cd_music.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#if defined(ROADKILL_XBOX)
#define DR_MP3_NO_SIMD
#endif
#include "dr_mp3.h"
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_OGG
#if defined(ROADKILL_XBOX)
#define DR_FLAC_NO_SIMD
#endif
#include "dr_flac.h"

enum { K_MP3 = 1, K_FLAC = 2 };

struct CdMusic {
    FILE* f;
    int kind;
    drmp3 mp3;
    drflac* flac;
    drmp3_seek_point* seek;
    uint64_t pos;   /* the next frame the decoder gives */
    int ended;
};

static size_t on_read(void* u, void* out, size_t n) { return fread(out, 1, n, ((CdMusic*)u)->f); }
static int on_seek_any(void* u, int off, int origin)
{
    int w = origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END;
    return fseek(((CdMusic*)u)->f, off, w) == 0;
}
static int on_tell_any(void* u, int64_t* at)
{
    long p = ftell(((CdMusic*)u)->f);
    if (p < 0) return 0;
    *at = p;
    return 1;
}
static drmp3_bool32 mp3_seek(void* u, int off, drmp3_seek_origin o) { return on_seek_any(u, off, (int)o); }
static drmp3_bool32 mp3_tell(void* u, drmp3_int64* at) { int64_t v; if (!on_tell_any(u, &v)) return 0; *at = v; return 1; }
static drflac_bool32 flac_seek(void* u, int off, drflac_seek_origin o) { return on_seek_any(u, off, (int)o); }
static drflac_bool32 flac_tell(void* u, drflac_int64* at) { int64_t v; if (!on_tell_any(u, &v)) return 0; *at = v; return 1; }

static const char* ext(const char* name)
{
    const char* d = strrchr(name, '.');
    return d ? d + 1 : "";
}

int cd_music_kind(const char* name, const char* kind)
{
    if (kind && (!strcasecmp(kind, "MP3") || !strcasecmp(kind, "FLAC"))) return 1;
    return !strcasecmp(ext(name), "mp3") || !strcasecmp(ext(name), "flac");
}

CdMusic* cd_music_open(FILE* f, const char* name, const char* kind)
{
    CdMusic* m = calloc(1, sizeof *m);
    if (!m) { fclose(f); return NULL; }
    m->f = f;
    int flac = kind && *kind ? !strcasecmp(kind, "FLAC") : !strcasecmp(ext(name), "flac");
    if (flac) {
        m->kind = K_FLAC;
        m->flac = drflac_open(on_read, flac_seek, flac_tell, m, NULL);
        if (!m->flac || m->flac->sampleRate != 44100 || m->flac->channels != 2) { cd_music_close(m); return NULL; }
    } else {
        m->kind = K_MP3;
        if (!drmp3_init(&m->mp3, on_read, mp3_seek, mp3_tell, NULL, m, NULL)) { fclose(f); free(m); return NULL; }
        if (m->mp3.sampleRate != 44100 || m->mp3.channels != 2) { cd_music_close(m); return NULL; }
    }
    return m;
}

void cd_music_close(CdMusic* m)
{
    if (!m) return;
    if (m->kind == K_MP3) drmp3_uninit(&m->mp3);
    if (m->flac) drflac_close(m->flac);
    free(m->seek);
    if (m->f) fclose(m->f);
    free(m);
}

static int seek(CdMusic* m, uint64_t at)
{
    if (m->kind == K_FLAC) return drflac_seek_to_pcm_frame(m->flac, at);
    if (at && !m->seek) {
        drmp3_uint32 n = 512;   /* one point per ~1 s of a 4 minute track is plenty */
        m->seek = malloc(n * sizeof *m->seek);
        if (m->seek && drmp3_calculate_seek_points(&m->mp3, &n, m->seek)) drmp3_bind_seek_table(&m->mp3, n, m->seek);
    }
    return drmp3_seek_to_pcm_frame(&m->mp3, at);
}

uint32_t cd_music_read(CdMusic* m, uint64_t at, uint32_t n, int16_t* out)
{
    memset(out, 0, (size_t)n * 4);
    if (at != m->pos) {
        m->ended = !seek(m, at);
        m->pos = at;
    }
    if (m->ended) return 0;
    uint64_t got = m->kind == K_FLAC ? drflac_read_pcm_frames_s16(m->flac, n, out)
                                     : drmp3_read_pcm_frames_s16(&m->mp3, n, out);
    m->pos += got;
    if (got < n) m->ended = 1;
    return (uint32_t)got;
}

uint32_t cd_music_sectors(CdMusic* m)
{
    uint64_t n = m->kind == K_FLAC ? m->flac->totalPCMFrameCount : drmp3_get_pcm_frame_count(&m->mp3);
    m->pos = (uint64_t)-1;   /* measuring moved the decoder: the next read seeks */
    return (uint32_t)((n + 587) / 588);
}
