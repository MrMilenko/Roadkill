/*
 * cd_audio.h: the CD audio player behind the MCI "cdaudio" device (snd_mci.c): plays the audio
 * tracks of the player's CD image (cd_image.h) into the DirectSound mixer's SDL device
 * (snd_dsound.c) as a second stream. Port code, not from the original.
 */
#ifndef TM2_CD_AUDIO_H
#define TM2_CD_AUDIO_H

#include <stdint.h>

#include "cd_image.h"

/* ---- provided by snd_dsound.c (the SDL audio device and its mixer) ---- */
int snd_audio_open(void);   /* open the device if not yet (0: no sound, TM2_NOSOUND=1) */
void snd_audio_lock(void);
void snd_audio_unlock(void);
/* called by the mixer callback with the device locked: add frames of 44.1 kHz stereo to acc */
extern void (*snd_extra_mix)(int32_t* acc, int frames);

/* ---- cd_audio.c ---- */
enum { CD_STOPPED, CD_PLAYING, CD_PAUSED };

/* the image (opened once: TM2_CDIMAGE or the .cue next to TM2_DATA) and the audio device; NULL
 * when either is missing (why says which) */
CdImage* cd_audio_init(const char** why);
int cd_audio_log(void);   /* TM2_CD_LOG=1 */

/* play disc LBAs [from, to); stop / pause keep the position; resume plays on to the same end */
void cd_audio_play(uint32_t from, uint32_t to);
void cd_audio_stop(void);
void cd_audio_pause(void);
void cd_audio_resume(void);
void cd_audio_seek(uint32_t lba);   /* stopped at lba */
int cd_audio_state(void);
uint32_t cd_audio_pos(void);        /* the current LBA */
uint32_t cd_audio_end(void);        /* the end of the current play */
/* counts the plays that reached their end (for MCI_NOTIFY) */
uint32_t cd_audio_finished(void);

/* the CD's volume as auxSetVolume gives it: low word left, high word right, 0..0xffff */
void cd_audio_set_volume(uint32_t v);
uint32_t cd_audio_get_volume(void);

#endif /* TM2_CD_AUDIO_H */
