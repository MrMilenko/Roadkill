/*
 * sh3_title.h: the title screen, TM2.EXE 0x004780fc (PS1 decomp: screenTitle).
 *
 * Network reset, sound 0x1b stopped, the scene (background 0x00499408 with 0x004e2839, scene
 * model resources 0x004e284c; items at frame +0x2b4, +0x260, +0x308, decor +0x35c; the attract
 * demo 0x00477b90 as the idle screen), the title music (audio object 0x00626014 method 1 with 0xb)
 * and the CD volume from the music volume: the mixer value 0x00c85098 = max(volume, minimum
 * 0x00c8507c), the CD's (volume * 65535 / 100, at most 0xffff) on both channels. After the
 * scene: scene result 3 fades the music out, else sound 0x1b plays again; music stopped
 * (method 2) and the CD volume set from 0.
 *
 * Verified against the original under emulation (tools/emu/check_sh3_title.sh).
 */
#ifndef TM2_SH3_TITLE_H
#define TM2_SH3_TITLE_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (0x5a8 bytes of locals). Returns the
 * sceneRun result (the next screen). */
uint32_t sh3_title(const ShEnv* e, uint32_t fb);

#endif /* TM2_SH3_TITLE_H */
