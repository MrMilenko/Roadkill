/*
 * sh3_ai_select.h: the AI opponent select screen, TM2.EXE 0x0047e624 (PS1 decomp:
 * screenAiSelect; Ghidra's old name match_challenge_start is wrong).
 *
 * One player game: the player picks the opponents one by one on the car select scene
 * (SHELLDB1\CARSEL.TIM background, the SHELLDB1 AICARSEL scene model). Game state 0x00624ea8:
 * +2 u8 opponent count wanted (0x00624eaa), +4 u8 opponents chosen so far (0x00624eac),
 * +0x0d.. opponent car ids (0x00624eb5 + k, the unchosen ones set to 0xff first).
 * Items: frame +0x1e8 (0x2329, "select this car"), +0x23c (0x232a, "change background"),
 * +0x194 (0x232b, "done"); the car wheel's choice object at +0x330.
 *
 * Verified against the original under emulation (tools/emu/check_sh3_ai_select.sh).
 */
#ifndef TM2_SH3_AI_SELECT_H
#define TM2_SH3_AI_SELECT_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (0x960 bytes of locals), esi_in = the
 * caller's esi (returned when no opponent is left to pick). Returns the last sceneRun result. */
uint32_t sh3_ai_select(const ShEnv* e, uint32_t fb, uint32_t esi_in);

#endif /* TM2_SH3_AI_SELECT_H */
