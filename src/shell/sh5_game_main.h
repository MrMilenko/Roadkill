/*
 * sh5_game_main.h: the game's start and end around the front end, TM2.EXE 0x00477124 (Ghidra's
 * old name game_flow_init).
 *
 * The audio configuration (0x00624edc), the display and audio objects built in the frame (their
 * addresses into 0x00626010 / 0x00626014), the DirectDraw display (0x004b3d80: 1 = failed, then
 * 640 x 480 x 16 bit; failing returns that result), the registry, every system's init, the sound
 * and music start, the font file, the game state defaults (level 1, counts 0, the volume bytes
 * from 0x00c85098 / 0x00c8508c, the score tables cleared), three intro pictures (each shown at
 * least 2 s; a button or timeout 0x00493694 skips the rest) and, when none was skipped, the intro
 * movie (0x004ba15c on root 6's file) and a CD track; then the volume, the shell loop 0x00485338,
 * and the shutdown (once, flag 0x00624db4), the audio settings saved, the display object
 * destroyed. Returns 0 (or the failed display result).
 *
 * Verified against the original under emulation (tools/emu/check_sh5_game_main.sh).
 */
#ifndef TM2_SH5_GAME_MAIN_H
#define TM2_SH5_GAME_MAIN_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (esp after the prologue, 0x3b4 bytes:
 * +0 / +0x12c / +0x258 path buffers, +0x384 the display object, +0x3a4 the audio object, +0x3b0
 * the registry key); ebx_in = the caller's ebx (the display init gets it as its third argument). */
uint32_t sh5_game_main(const ShEnv* e, uint32_t fb, uint32_t ebx_in);

#endif /* TM2_SH5_GAME_MAIN_H */
