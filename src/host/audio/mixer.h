/*
 * host/audio/mixer.h: the sound card mixer the game's audio screen drives through the WINMM
 * mixer API (win32_mm.c), port code. Three lines like a Windows 95 card's: the speakers (the
 * destination, MIXERLINE_COMPONENTTYPE_DST_SPEAKERS), the wave out and the CD audio sources,
 * each with a volume (0..0xffff per channel) and a mute. The levels scale what is heard: the
 * sound effects by speakers x wave, the CD music by speakers x CD (host/qol/volume.c folds them
 * into the effects / music gains, on top of roadkill.ini's volumes). They start at full and are
 * not kept between runs: the game sets them from its registry (GameVolume) at start up.
 * Any thread may read them.
 */
#ifndef TM2_HOST_AUDIO_MIXER_H
#define TM2_HOST_AUDIO_MIXER_H

#include <stdint.h>

enum { MIXER_SPEAKERS, MIXER_WAVE, MIXER_CD, MIXER_LINES };

/* a line's volume (channel 0 left, 1 right; 0..0xffff) and mute */
uint32_t mixer_volume(int line, int channel);
void mixer_set_volume(int line, uint32_t left, uint32_t right);
int mixer_muted(int line);
void mixer_set_mute(int line, int on);

/* the gains the levels give, 0..256 (256: full): speakers x wave, speakers x CD */
int mixer_effects_gain(void);
int mixer_music_gain(void);

#endif /* TM2_HOST_AUDIO_MIXER_H */
