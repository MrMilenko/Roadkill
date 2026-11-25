/*
 * sh3_level_select.h: the level (arena) select screen, TM2.EXE 0x0047a388 (PS1 decomp:
 * screenLevelSelect).
 *
 * The background file (0x00499408 with 0x004e2907) and the SHELLDB1 LEVELSEL scene model. Called
 * with the previous screen in eax: from 0x0047a274 it goes back there and on to it again, else
 * back to the single player menu 0x00478edc and on to car select 0x0047baa0. The arena wheel is
 * the choice object at frame +0x3b4; its value + 1 is the level (0x00624ec7). In game type 3
 * (challenge) 0x00474e0c(game state, level) gives the opponents' total, minus the humans, into
 * 0x00624eaa. Scene result 4 is the secret level: level = the byte at frame +0x51b + 10 and the
 * "secret level selected" message (0x004e2930), then on as above.
 *
 * Verified against the original under emulation (tools/emu/check_sh3_level_select.sh).
 */
#ifndef TM2_SH3_LEVEL_SELECT_H
#define TM2_SH3_LEVEL_SELECT_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (0x748 bytes of locals), prev = eax. */
uint32_t sh3_level_select(const ShEnv* e, uint32_t fb, uint32_t prev);

#endif /* TM2_SH3_LEVEL_SELECT_H */
