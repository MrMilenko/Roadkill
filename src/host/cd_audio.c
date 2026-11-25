/*
 * cd_audio.c: the CD audio player (port code, not from the original; cd_audio.h).
 *
 * The music the game plays as CD audio tracks comes from the player's own CD image, read at run
 * time (cd_image.c: TM2_CDIMAGE, default the .cue in TM2_DATA's parent folder, e.g.
 * disc/TWISTEDMETAL2.cue). Nothing of it is copied anywhere. The SDL device of snd_dsound.c runs
 * at the CD's own rate (44.1 kHz 16 bit stereo), so the sectors are added to the DirectSound mix
 * as they are, times the CD volume (auxSetVolume: low word left, high word right, linear
 * 0..0xffff; the DirectSound master volume does not apply, as on a real CD drive's analog line).
 *
 * The mixer callback reads the image (16 sectors ahead at a time) on the audio thread, under the
 * device lock that the game's calls also take. A play that reaches its end stops (the MCI mode
 * goes to "stopped", the game's monitor thread then starts the track again) and counts in
 * cd_audio_finished for MCI_NOTIFY.
 *
 * TM2_CD_LOG=1: the MCI calls (snd_mci.c) and, once a second while playing, the position and the
 * peak level of what was mixed.
 */
#include "cd_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AHEAD 16

static struct {
    CdImage* img;
    int tried;
    const char* why;
    volatile int state;
    uint32_t pos, end;        /* LBAs; pos's frame offset in off */
    uint32_t off;
    uint32_t finished;
    uint32_t volume;
    /* the decoded sectors [buf_lba, buf_lba + buf_n) */
    int16_t buf[AHEAD * CD_SECTOR_FRAMES * 2];
    uint32_t buf_lba;
    int buf_n;
    /* log */
    int log;
    uint32_t log_frames;
    int log_peak;
    uint64_t total_frames;
} C = {.volume = 0xffffffffu, .log = -1};

int cd_audio_log(void)
{
    if (C.log < 0) {
        const char* l = getenv("TM2_CD_LOG");
        C.log = l && *l == '1';
    }
    return C.log;
}

static void mix(int32_t* acc, int frames)
{
    if (C.state != CD_PLAYING || !C.img) return;
    int32_t gl = (int32_t)(C.volume & 0xffff), gr = (int32_t)(C.volume >> 16);
    int peak = 0, i = 0;
    while (i < frames) {
        if (C.pos >= C.end) {
            C.state = CD_STOPPED;
            C.finished++;
            if (cd_audio_log()) fprintf(stderr, "tm2 cd: play reached its end at LBA %u (track %d)\n", C.pos, cd_image_track_at(C.img, C.pos ? C.pos - 1 : 0));
            break;
        }
        if (!C.buf_n || C.pos < C.buf_lba || C.pos >= C.buf_lba + (uint32_t)C.buf_n) {
            uint32_t n = C.end - C.pos;
            if (n > AHEAD) n = AHEAD;
            cd_image_read(C.img, C.pos, (int)n, C.buf);
            C.buf_lba = C.pos;
            C.buf_n = (int)n;
        }
        const int16_t* s = C.buf + ((size_t)(C.pos - C.buf_lba) * CD_SECTOR_FRAMES + C.off) * 2;
        int k = (int)(CD_SECTOR_FRAMES - C.off);
        if (k > frames - i) k = frames - i;
        for (int j = 0; j < k; j++) {
            int32_t l = s[2 * j] * gl / 65535, r = s[2 * j + 1] * gr / 65535;
            acc[2 * (i + j)] += l;
            acc[2 * (i + j) + 1] += r;
            int al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
            if (al > peak) peak = al;
            if (ar > peak) peak = ar;
        }
        i += k;
        C.off += (uint32_t)k;
        if (C.off == CD_SECTOR_FRAMES) { C.off = 0; C.pos++; }
    }
    C.total_frames += (uint64_t)i;
    if (cd_audio_log()) {
        if (peak > C.log_peak) C.log_peak = peak;
        C.log_frames += (uint32_t)i;
        if (C.log_frames >= 44100) {
            int t = cd_image_track_at(C.img, C.pos);
            uint32_t rel = C.pos - cd_image_track_lba(C.img, t);
            fprintf(stderr, "tm2 cd: mixed %llu frames, track %d at %u:%02u, peak %d over the last second, volume %08x\n",
                    (unsigned long long)C.total_frames, t, rel / 4500, rel / 75 % 60, C.log_peak, C.volume);
            C.log_frames = 0;
            C.log_peak = 0;
        }
    }
}

CdImage* cd_audio_init(const char** why)
{
    if (!C.tried) {
        C.tried = 1;
        char path[1024];
        if (!snd_audio_open()) C.why = "no audio device (TM2_NOSOUND=1 or SDL audio failed)";
        else if (!cd_image_default_path(path, sizeof path)) C.why = "no CD image (set TM2_CDIMAGE to the .cue)";
        else {
            C.img = cd_image_open(path, &C.why);
            if (C.img) {
                snd_audio_lock();
                snd_extra_mix = mix;
                snd_audio_unlock();
            }
            if (cd_audio_log() || !C.img)
                fprintf(stderr, "tm2 cd: %s: %s\n", path, C.img ? "opened" : C.why);
            if (C.img && cd_audio_log())
                fprintf(stderr, "tm2 cd: %d tracks, %u sectors\n", cd_image_tracks(C.img), cd_image_end_lba(C.img));
        }
    }
    if (why) *why = C.why;
    return C.img;
}

void cd_audio_play(uint32_t from, uint32_t to)
{
    snd_audio_lock();
    C.pos = from;
    C.off = 0;
    C.end = to;
    C.state = from < to ? CD_PLAYING : CD_STOPPED;
    snd_audio_unlock();
}
void cd_audio_stop(void) { snd_audio_lock(); C.state = CD_STOPPED; snd_audio_unlock(); }
void cd_audio_pause(void) { snd_audio_lock(); if (C.state == CD_PLAYING) C.state = CD_PAUSED; snd_audio_unlock(); }
void cd_audio_resume(void)
{
    snd_audio_lock();
    if (C.state == CD_PAUSED && C.pos < C.end) C.state = CD_PLAYING;
    snd_audio_unlock();
}
void cd_audio_seek(uint32_t lba) { snd_audio_lock(); C.state = CD_STOPPED; C.pos = lba; C.off = 0; snd_audio_unlock(); }
int cd_audio_state(void) { return C.state; }
uint32_t cd_audio_pos(void) { return C.pos; }
uint32_t cd_audio_end(void) { return C.end; }
uint32_t cd_audio_finished(void) { return C.finished; }

void cd_audio_set_volume(uint32_t v)
{
    if (cd_audio_log() && v != C.volume) fprintf(stderr, "tm2 cd: volume %08x\n", v);
    C.volume = v;
}
uint32_t cd_audio_get_volume(void) { return C.volume; }
