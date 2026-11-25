/*
 * movies.h: the MCI "avivideo" device the game plays its movies through (mci_avi.c), on the AVI
 * reader (avi.h) and the Indeo decoders (third_party/indeo). Port code, not from the original:
 * stands in for Windows 95's MCIAVI driver.
 */
#ifndef TM2_HOST_MOVIES_H
#define TM2_HOST_MOVIES_H

#include <stdint.h>

/* mciSendStringA for the avivideo device (snd_mci.c hands every string here first): 1 when the
 * string is the movie device's, with the MCI error (0: success) in *err; 0 when it is not
 * (another device: snd_mci.c answers it). ret / retlen: the game's return buffer, cb the window
 * for "notify". */
int mci_avi_string(const char* cmd, uint32_t ret, uint32_t retlen, uint32_t cb, uint32_t* err);

/* called by the Win32 host while the game waits (WaitMessage) on the game's thread: shows the
 * playing movie's due frames and sends MM_MCINOTIFY when a play ends */
void mci_avi_idle(void);

/* ---- provided by snd_dsound.c (the SDL audio device's mixer) ---- */
/* another stream: called with the device locked, adds frames of 44.1 kHz stereo to acc */
int snd_mix_add(void (*fn)(int32_t* acc, int frames));
void snd_mix_remove(void (*fn)(int32_t* acc, int frames));

#endif /* TM2_HOST_MOVIES_H */
