/*
 * snd_dsound.c: DirectSound for the game on SDL2 audio with a small software mixer (port code,
 * not from the original; docs/PLATFORM.md "Sound" lists the calls: dsound_init 0x004b94f4, the
 * buffer wrappers 0x004b97b0 .. 0x004b9a48, the voice list).
 *
 *   DirectSoundCreate (DSOUND.dll import slot and the thunk 0x004d18e4) -> IDirectSound; the SDL
 *   audio device opens there (44100 Hz, 16 bit stereo). TM2_NOSOUND=1 or no audio device:
 *   DSERR_NODRIVER, and the game runs silent.
 *   A secondary buffer's data lives in game memory (dx_mem): Lock hands out its game address
 *   (two pieces when the range wraps), the mixer reads it from there. Duplicates share the data.
 *   Volume is in hundredths of a dB (gain 10^(v / 2000)), pan attenuates the other side the same
 *   way, frequency is the resampling step (linear). The primary buffer's volume is the master.
 *   8 and 16 bit PCM, mono or stereo.
 *   A second stream (the CD audio player, cd_audio.c) adds itself through snd_extra_mix; the
 *   device's open / lock are shared with it (snd_audio_open / snd_audio_lock, cd_audio.h).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "cd_audio.h"
#include "dx_com.h"
#include "dx_mem.h"
#include "platform/pf2_env.h"
#include "host/qol/qol.h"
#include "host/settings/settings.h"

#define DSERR_ALLOCATED      0x8878000au
#define DSERR_INVALIDCALL    0x88780032u
#define DSERR_NODRIVER       0x88780078u
#define DSERR_ALREADYINITIALIZED 0x887800aau
#define DSERR_OUTOFMEMORY    0x8007000eu

#define DSBCAPS_PRIMARYBUFFER 1u
#define DSBPLAY_LOOPING 1u
#define DSBLOCK_FROMWRITECURSOR 1u
#define DSBLOCK_ENTIREBUFFER 2u

#define OUT_RATE 44100

static DxIface I_DS = {"IDirectSound", 0, {0x279afa83u}};
static DxIface I_DSB = {"IDirectSoundBuffer", 0, {0x279afa85u}};

typedef struct { uint32_t addr, bytes; int refs; } Data;

typedef struct Buf {
    int primary;
    Data* data;
    uint32_t flags;
    int channels, bits, block;
    uint32_t rate, freq;          /* the format's rate; the current frequency (0 = the format's) */
    int32_t volume, pan;
    volatile int playing;
    int loop;
    double pos;                   /* play position in frames */
    uint32_t wfx[5];              /* the WAVEFORMATEX as set (18 bytes, padded) */
} Buf;

#define MAXBUF 512
static struct {
    SDL_AudioDeviceID dev;
    int open_tried;
    Buf* bufs[MAXBUF];
    int32_t master;               /* the primary buffer's volume */
} A;

static double gain(int32_t v) { return v <= -10000 ? 0.0 : v >= 0 ? 1.0 : pow(10.0, v / 2000.0); }

static int16_t sample(const Buf* b, uint32_t frame, int ch)
{
    uint32_t off = frame * (uint32_t)b->block + (uint32_t)(ch * (b->bits / 8));
    if (off + (uint32_t)(b->bits / 8) > b->data->bytes) return 0;
    const uint8_t* p = rt_ptr(b->data->addr + off);
    if (b->bits == 8) return (int16_t)((p[0] - 128) << 8);
    return (int16_t)(p[0] | p[1] << 8);
}

static void mix(void* ud, Uint8* stream, int len)
{
    (void)ud;
    int frames = len / 4;
    int16_t* out = (int16_t*)stream;
    static int32_t acc[2 * 4096];
    if (frames > 4096) frames = 4096;
    memset(acc, 0, sizeof(int32_t) * 2 * (size_t)frames);
    double master = gain(A.master) * qol_effects_gain() / 256.0;   /* roadkill.ini [audio] */
    for (int k = 0; k < MAXBUF; k++) {
        Buf* b = A.bufs[k];
        if (!b || b->primary || !b->playing || !b->data || !b->block) continue;
        uint32_t nframes = b->data->bytes / (uint32_t)b->block;
        if (!nframes) continue;
        double g = gain(b->volume) * master;
        double gl = g * (b->pan > 0 ? gain(-b->pan) : 1.0), gr = g * (b->pan < 0 ? gain(b->pan) : 1.0);
        double step = (double)(b->freq ? b->freq : b->rate) / OUT_RATE;
        int rch = b->channels > 1 ? 1 : 0;
        for (int i = 0; i < frames; i++) {
            if (b->pos >= nframes) {
                if (b->loop) b->pos = fmod(b->pos, (double)nframes);
                else { b->playing = 0; b->pos = 0; break; }
            }
            uint32_t f = (uint32_t)b->pos;
            double t = b->pos - f;
            uint32_t f2 = f + 1 < nframes ? f + 1 : (b->loop ? 0 : f);
            double l = sample(b, f, 0) * (1 - t) + sample(b, f2, 0) * t;
            double r = sample(b, f, rch) * (1 - t) + sample(b, f2, rch) * t;
            acc[2 * i] += (int32_t)(l * gl);
            acc[2 * i + 1] += (int32_t)(r * gr);
            b->pos += step;
        }
    }
    static int log_on = -1, playing_max, fx_peak, out_peak;
    static long long logged_frames;
    if (log_on < 0) log_on = getenv("ROADKILL_SND_LOG") != NULL;
    if (log_on) {
        int np = 0;
        for (int k = 0; k < MAXBUF; k++) if (A.bufs[k] && !A.bufs[k]->primary && A.bufs[k]->playing) np++;
        if (np > playing_max) playing_max = np;
        for (int i = 0; i < 2 * frames; i++) { int v = acc[i] < 0 ? -acc[i] : acc[i]; if (v > fx_peak) fx_peak = v; }
    }
    if (snd_extra_mix) snd_extra_mix(acc, frames);
    for (int i = 0; i < 2 * frames; i++) out[i] = (int16_t)(acc[i] > 32767 ? 32767 : acc[i] < -32768 ? -32768 : acc[i]);
    if (log_on) {
        for (int i = 0; i < 2 * frames; i++) { int v = out[i] < 0 ? -out[i] : out[i]; if (v > out_peak) out_peak = v; }
        logged_frames += frames;
        if (logged_frames >= OUT_RATE) {
            fprintf(stderr, "roadkill snd: effects playing %d, effects peak %d, output peak %d, master %d\n",
                    playing_max, fx_peak, out_peak, (int)A.master);
            logged_frames = 0; playing_max = fx_peak = out_peak = 0;
        }
    }
    if (len > frames * 4) memset(stream + frames * 4, 0, (size_t)(len - frames * 4));
}

static int audio_open(void)
{
    if (A.dev) return 1;
    if (A.open_tried) return 0;
    A.open_tried = 1;
    if (!settings_bool("audio.sound", 1)) return 0;   /* roadkill.ini [audio], TM2_NOSOUND=1 */
    qol_effects_gain();   /* read the volumes here, not first on the audio thread */
    qol_music_gain();
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "tm2 host: SDL audio: %s (no sound)\n", SDL_GetError());
        return 0;
    }
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof want);
    want.freq = OUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix;
    A.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!A.dev) { fprintf(stderr, "tm2 host: SDL audio device: %s (no sound)\n", SDL_GetError()); return 0; }
    SDL_PauseAudioDevice(A.dev, 0);
    return 1;
}

static void lock_audio(void) { if (A.dev) SDL_LockAudioDevice(A.dev); }
static void unlock_audio(void) { if (A.dev) SDL_UnlockAudioDevice(A.dev); }

/* the hook for the CD audio stream (cd_audio.h) */
void (*snd_extra_mix)(int32_t* acc, int frames);
int snd_audio_open(void) { return audio_open(); }
void snd_audio_lock(void) { lock_audio(); }
void snd_audio_unlock(void) { unlock_audio(); }

static Buf* buf_of(uint32_t a) { DxObj* o = dx_obj_get(a, &I_DSB); return o ? o->host : NULL; }

static void set_format(Buf* b, uint32_t wfx)
{
    memcpy(b->wfx, rt_ptr(wfx), 18);
    b->channels = rt_r16(wfx + 2);
    b->rate = rt_r32(wfx + 4);
    b->bits = rt_r16(wfx + 0xe);
    if (b->channels < 1) b->channels = 1;
    if (b->channels > 2) b->channels = 2;
    if (b->bits != 8) b->bits = 16;
    b->block = b->channels * b->bits / 8;
}

static void buf_destroy(DxObj* o)
{
    Buf* b = o->host;
    lock_audio();
    for (int k = 0; k < MAXBUF; k++) if (A.bufs[k] == b) A.bufs[k] = NULL;
    unlock_audio();
    if (b->data && --b->data->refs == 0) { dx_mem_free(b->data->addr); free(b->data); }
    free(b);
}

static DxObj* buf_new(Buf* b)
{
    int slot = -1;
    for (int k = 0; k < MAXBUF; k++) if (!A.bufs[k]) { slot = k; break; }
    if (slot < 0) return NULL;
    DxObj* o = dx_obj_new(&I_DSB, b, buf_destroy);
    if (!o) return NULL;
    lock_audio();
    A.bufs[slot] = b;
    unlock_audio();
    return o;
}

/* ---- IDirectSound ---- */

/* 0x0c CreateSoundBuffer(DSBUFFERDESC*, out, outer) */
static uint32_t ds_create_buffer(const uint32_t* a, int n)
{
    uint32_t desc = dx_arg(a, n, 1), out = dx_arg(a, n, 2);
    if (!desc || !out) return DX_E_INVALIDARG;
    rt_w32(out, 0);
    uint32_t flags = rt_r32(desc + 4), bytes = rt_r32(desc + 8), wfx = rt_r32(desc + 0x10);
    Buf* b = calloc(1, sizeof *b);
    if (!b) return DSERR_OUTOFMEMORY;
    b->flags = flags;
    if (flags & DSBCAPS_PRIMARYBUFFER) {
        b->primary = 1;
        b->channels = 2; b->bits = 16; b->block = 4; b->rate = 22050;
    } else {
        if (!wfx || bytes < 4 || bytes > 0x01000000u) { free(b); return DX_E_INVALIDARG; }
        set_format(b, wfx);
        b->data = calloc(1, sizeof *b->data);
        if (!b->data) { free(b); return DSERR_OUTOFMEMORY; }
        b->data->bytes = bytes;
        b->data->refs = 1;
        b->data->addr = dx_mem_alloc(bytes);
        if (!b->data->addr) { free(b->data); free(b); return DSERR_OUTOFMEMORY; }
    }
    DxObj* o = buf_new(b);
    if (!o) {
        if (b->data) { dx_mem_free(b->data->addr); free(b->data); }
        free(b);
        return DSERR_OUTOFMEMORY;
    }
    rt_w32(out, o->addr);
    return DX_OK;
}

/* 0x14 DuplicateSoundBuffer(src, out): a new buffer on the same data, stopped, at 0 */
static uint32_t ds_duplicate(const uint32_t* a, int n)
{
    Buf* s = buf_of(dx_arg(a, n, 1));
    uint32_t out = dx_arg(a, n, 2);
    if (!s || !out || s->primary) return DX_E_INVALIDARG;
    Buf* b = malloc(sizeof *b);
    if (!b) return DSERR_OUTOFMEMORY;
    lock_audio();
    *b = *s;
    unlock_audio();
    b->playing = 0;
    b->pos = 0;
    b->data->refs++;
    DxObj* o = buf_new(b);
    if (!o) { b->data->refs--; free(b); rt_w32(out, 0); return DSERR_OUTOFMEMORY; }
    rt_w32(out, o->addr);
    return DX_OK;
}

static uint32_t ds_getcaps(const uint32_t* a, int n)   /* 0x10 GetCaps(DSCAPS*) */
{
    uint32_t c = dx_arg(a, n, 1);
    if (!c) return DX_E_INVALIDARG;
    uint32_t size = rt_r32(c);
    if (size >= 8) {
        memset(rt_ptr(c + 4), 0, size - 4);
        /* PRIMARYMONO | PRIMARYSTEREO | PRIMARY8BIT | PRIMARY16BIT | CONTINUOUSRATE | SECONDARY* */
        rt_w32(c + 4, 0x1u | 0x2u | 0x4u | 0x8u | 0x10u | 0x100u | 0x200u | 0x400u | 0x800u);
        if (size >= 0x10) { rt_w32(c + 8, 100); rt_w32(c + 0xc, 100000); }   /* min / max secondary rate */
    }
    return DX_OK;
}

static uint32_t ds_ok(const uint32_t* a, int n) { (void)a; (void)n; return DX_OK; }
static uint32_t ds_speaker(const uint32_t* a, int n)   /* 0x20 GetSpeakerConfig(&cfg): stereo */
{
    if (dx_arg(a, n, 1)) rt_w32(dx_arg(a, n, 1), 4);
    return DX_OK;
}
static uint32_t ds_initialize(const uint32_t* a, int n) { (void)a; (void)n; return DSERR_ALREADYINITIALIZED; }

/* ---- IDirectSoundBuffer ---- */

static uint32_t b_getcaps(const uint32_t* a, int n)   /* 0x0c GetCaps(DSBCAPS*) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    uint32_t c = dx_arg(a, n, 1);
    if (!b || !c) return DX_E_INVALIDARG;
    rt_w32(c + 4, b->flags | 0x8u);   /* | DSBCAPS_LOCSOFTWARE */
    rt_w32(c + 8, b->data ? b->data->bytes : 0x4000u);
    rt_w32(c + 0xc, 0);
    rt_w32(c + 0x10, 0);
    return DX_OK;
}

/* 0x10 GetCurrentPosition(&play, &write) in bytes */
static uint32_t b_getpos(const uint32_t* a, int n)
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b) return DX_E_INVALIDARG;
    uint32_t size = b->data ? b->data->bytes : 0x4000u;
    uint32_t play = (uint32_t)b->pos * (uint32_t)(b->block ? b->block : 4);
    if (size) play %= size;
    uint32_t wr = size ? (play + (uint32_t)b->block * 256u) % size : 0;
    if (dx_arg(a, n, 1)) rt_w32(dx_arg(a, n, 1), play);
    if (dx_arg(a, n, 2)) rt_w32(dx_arg(a, n, 2), b->playing ? wr : play);
    return DX_OK;
}

static uint32_t b_getformat(const uint32_t* a, int n)   /* 0x14 GetFormat(wfx, size, &written) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    uint32_t wfx = dx_arg(a, n, 1), size = dx_arg(a, n, 2), wr = dx_arg(a, n, 3);
    if (!b) return DX_E_INVALIDARG;
    if (wfx) {
        uint8_t f[18];
        memcpy(f, b->wfx, 18);
        if (b->primary || !f[0]) {
            memset(f, 0, sizeof f);
            uint32_t r = b->rate, bps = b->rate * (uint32_t)b->block;
            f[0] = 1; f[2] = (uint8_t)b->channels;
            memcpy(f + 4, &r, 4); memcpy(f + 8, &bps, 4);
            f[0xc] = (uint8_t)b->block; f[0xe] = (uint8_t)b->bits;
        }
        memcpy(rt_ptr(wfx), f, size < 18 ? size : 18);
    }
    if (wr) rt_w32(wr, 18);
    return DX_OK;
}

static uint32_t b_getvolume(const uint32_t* a, int n)   /* 0x18 GetVolume(&v) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b || !dx_arg(a, n, 1)) return DX_E_INVALIDARG;
    rt_w32(dx_arg(a, n, 1), (uint32_t)(b->primary ? A.master : b->volume));
    return DX_OK;
}

static uint32_t b_getpan(const uint32_t* a, int n)   /* 0x1c GetPan(&p) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b || !dx_arg(a, n, 1)) return DX_E_INVALIDARG;
    rt_w32(dx_arg(a, n, 1), (uint32_t)b->pan);
    return DX_OK;
}

static uint32_t b_getfreq(const uint32_t* a, int n)   /* 0x20 GetFrequency(&hz) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b || !dx_arg(a, n, 1)) return DX_E_INVALIDARG;
    rt_w32(dx_arg(a, n, 1), b->freq ? b->freq : b->rate);
    return DX_OK;
}

static uint32_t b_getstatus(const uint32_t* a, int n)   /* 0x24 GetStatus(&st): PLAYING 1, LOOPING 4 */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b || !dx_arg(a, n, 1)) return DX_E_INVALIDARG;
    int p = b->playing;
    rt_w32(dx_arg(a, n, 1), (p ? 1u : 0u) | (p && b->loop ? 4u : 0u));
    return DX_OK;
}

/* 0x2c Lock(offset, bytes, &p1, &n1, &p2, &n2, flags) */
static uint32_t b_lock(const uint32_t* a, int n)
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b) return DX_E_INVALIDARG;
    if (b->primary || !b->data) return DSERR_INVALIDCALL;
    uint32_t off = dx_arg(a, n, 1), bytes = dx_arg(a, n, 2), flags = dx_arg(a, n, 7);
    uint32_t p1 = dx_arg(a, n, 3), n1 = dx_arg(a, n, 4), p2 = dx_arg(a, n, 5), n2 = dx_arg(a, n, 6);
    uint32_t size = b->data->bytes;
    if (flags & DSBLOCK_FROMWRITECURSOR) off = ((uint32_t)b->pos * (uint32_t)b->block) % size;
    if (flags & DSBLOCK_ENTIREBUFFER) bytes = size;
    if (off >= size || bytes > size || !p1 || !n1) return DX_E_INVALIDARG;
    uint32_t first = bytes <= size - off ? bytes : size - off;
    rt_w32(p1, b->data->addr + off);
    rt_w32(n1, first);
    if (p2) rt_w32(p2, first < bytes ? b->data->addr : 0);
    if (n2) rt_w32(n2, bytes - first);
    return DX_OK;
}

/* 0x30 Play(reserved, priority, flags) */
static uint32_t b_play(const uint32_t* a, int n)
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b) return DX_E_INVALIDARG;
    lock_audio();
    b->loop = (dx_arg(a, n, 3) & DSBPLAY_LOOPING) != 0;
    b->playing = 1;
    unlock_audio();
    return DX_OK;
}

static uint32_t b_setpos(const uint32_t* a, int n)   /* 0x34 SetCurrentPosition(bytes) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b) return DX_E_INVALIDARG;
    if (!b->primary && b->block) {
        lock_audio();
        b->pos = (double)(dx_arg(a, n, 1) / (uint32_t)b->block);
        unlock_audio();
    }
    return DX_OK;
}

static uint32_t b_setformat(const uint32_t* a, int n)   /* 0x38 SetFormat(wfx): the primary only */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    uint32_t wfx = dx_arg(a, n, 1);
    if (!b || !wfx) return DX_E_INVALIDARG;
    if (!b->primary) return DSERR_INVALIDCALL;
    set_format(b, wfx);   /* the mixer's output format stays SDL's */
    return DX_OK;
}

static uint32_t b_setvolume(const uint32_t* a, int n)   /* 0x3c SetVolume(hundredths of a dB) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    int32_t v = (int32_t)dx_arg(a, n, 1);
    if (!b) return DX_E_INVALIDARG;
    if (v > 0 || v < -10000) return DX_E_INVALIDARG;
    if (b->primary) A.master = v;
    else b->volume = v;
    return DX_OK;
}

static uint32_t b_setpan(const uint32_t* a, int n)   /* 0x40 SetPan(-10000 left .. 10000 right) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    int32_t p = (int32_t)dx_arg(a, n, 1);
    if (!b) return DX_E_INVALIDARG;
    if (p > 10000 || p < -10000) return DX_E_INVALIDARG;
    b->pan = p;
    return DX_OK;
}

static uint32_t b_setfreq(const uint32_t* a, int n)   /* 0x44 SetFrequency(hz, 0 = the format's) */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    uint32_t f = dx_arg(a, n, 1);
    if (!b) return DX_E_INVALIDARG;
    if (b->primary) return DSERR_INVALIDCALL;
    if (f && (f < 100 || f > 100000)) return DX_E_INVALIDARG;
    b->freq = f;
    return DX_OK;
}

static uint32_t b_stop(const uint32_t* a, int n)   /* 0x48 Stop: the position stays */
{
    Buf* b = buf_of(dx_arg(a, n, 0));
    if (!b) return DX_E_INVALIDARG;
    lock_audio();
    b->playing = 0;
    unlock_audio();
    return DX_OK;
}

static uint32_t b_ok(const uint32_t* a, int n) { return buf_of(dx_arg(a, n, 0)) ? DX_OK : DX_E_INVALIDARG; }

/* DirectSoundCreate(GUID*, IDirectSound** out, outer), DSOUND.dll */
static uint32_t imp_directsound_create(const uint32_t* a, int n)
{
    uint32_t out = dx_arg(a, n, 1);
    if (!out) return DX_E_INVALIDARG;
    rt_w32(out, 0);
    if (!audio_open()) return DSERR_NODRIVER;
    DxObj* o = dx_obj_new(&I_DS, NULL, NULL);
    if (!o) return DSERR_OUTOFMEMORY;
    rt_w32(out, o->addr);
    return DX_OK;
}

static const DxMethod M_DS[] = {
    {0x00, dx_unk_qi, "IDirectSound::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectSound::AddRef"},
    {0x08, dx_unk_release, "IDirectSound::Release"},
    {0x0c, ds_create_buffer, "IDirectSound::CreateSoundBuffer"},
    {0x10, ds_getcaps, "IDirectSound::GetCaps"},
    {0x14, ds_duplicate, "IDirectSound::DuplicateSoundBuffer"},
    {0x18, ds_ok, "IDirectSound::SetCooperativeLevel"},
    {0x1c, ds_ok, "IDirectSound::Compact"},
    {0x20, ds_speaker, "IDirectSound::GetSpeakerConfig"},
    {0x24, ds_ok, "IDirectSound::SetSpeakerConfig"},
    {0x28, ds_initialize, "IDirectSound::Initialize"},
};

static const DxMethod M_DSB[] = {
    {0x00, dx_unk_qi, "IDirectSoundBuffer::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectSoundBuffer::AddRef"},
    {0x08, dx_unk_release, "IDirectSoundBuffer::Release"},
    {0x0c, b_getcaps, "IDirectSoundBuffer::GetCaps"},
    {0x10, b_getpos, "IDirectSoundBuffer::GetCurrentPosition"},
    {0x14, b_getformat, "IDirectSoundBuffer::GetFormat"},
    {0x18, b_getvolume, "IDirectSoundBuffer::GetVolume"},
    {0x1c, b_getpan, "IDirectSoundBuffer::GetPan"},
    {0x20, b_getfreq, "IDirectSoundBuffer::GetFrequency"},
    {0x24, b_getstatus, "IDirectSoundBuffer::GetStatus"},
    {0x28, ds_initialize, "IDirectSoundBuffer::Initialize"},
    {0x2c, b_lock, "IDirectSoundBuffer::Lock"},
    {0x30, b_play, "IDirectSoundBuffer::Play"},
    {0x34, b_setpos, "IDirectSoundBuffer::SetCurrentPosition"},
    {0x38, b_setformat, "IDirectSoundBuffer::SetFormat"},
    {0x3c, b_setvolume, "IDirectSoundBuffer::SetVolume"},
    {0x40, b_setpan, "IDirectSoundBuffer::SetPan"},
    {0x44, b_setfreq, "IDirectSoundBuffer::SetFrequency"},
    {0x48, b_stop, "IDirectSoundBuffer::Stop"},
    {0x4c, b_ok, "IDirectSoundBuffer::Unlock"},
    {0x50, b_ok, "IDirectSoundBuffer::Restore"},
};

#define NM(m) ((int)(sizeof m / sizeof m[0]))

static void rt_bind_host_snd_dsound(void)
{
    dx_iface_build(&I_DS, M_DS, NM(M_DS), 0x2c / 4, PF2_IDS);
    dx_iface_build(&I_DSB, M_DSB, NM(M_DSB), 0x54 / 4, PF2_IDSBUF);
    rt_bind_import("DirectSoundCreate", imp_directsound_create);
}

RT_BIND_AREA(host_snd_dsound)
