/*
 * host/qol/volume.c: [audio] master_volume / effects_volume / music_volume (host/qol/qol.h,
 * docs/QOL.md). Port code, not from the original game. The mixer (host/snd_dsound.c) and the CD
 * player (host/cd_audio.c) scale by them; 100 / 100 is the original's level. They multiply the
 * game's own levels, the sound card mixer its audio screen sets (host/audio/mixer.h).
 */
#include "host/qol/qol.h"

#include "host/audio/mixer.h"
#include "host/settings/settings.h"

static int pct(const char* key)
{
    int v = settings_int(key, 100);
    return v < 0 ? 0 : v > 100 ? 100 : v;
}
static int gain(int* slot, const char* key)
{
    if (*slot < 0) *slot = pct("audio.master_volume") * pct(key) * 256 / 10000;
    return *slot;
}
int qol_effects_gain(void) { static int v = -1; return gain(&v, "audio.effects_volume") * mixer_effects_gain() / 256; }
int qol_music_gain(void) { static int v = -1; return gain(&v, "audio.music_volume") * mixer_music_gain() / 256; }
