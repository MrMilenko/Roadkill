/*
 * sh3_car_select.h: the car select screen, TM2.EXE 0x0047baa0 (PS1 decomp: screenCarSelect).
 *
 * SHELLDB1\CARSEL.TIM background, the SHELLDB1 CARSEL scene model. The screen it goes back to
 * (sceneInit's third argument) and the one it goes on to (stored in the "select" item's command,
 * frame +0x364) depend on the game type (0x00624ea8): back 1 -> single player menu 0x00478edc,
 * 3 / 4 -> level select 0x0047a388, 2 / 5 / 6 / 7 -> 0x0047a18c, else 0; next 1 / 2 -> 0x00480c08,
 * 3 -> AI select 0x0047e624, 4 -> 0x00480c8c, 5..7 -> 0x0047e010, else the title 0x004780fc.
 * Before the scene runs it waits for the network / players (0x004632f4(0x13, 0x14)), leaving
 * for the previous screen on button 0x14 or 0x15. Picking a car: in game types 1 and 3 it is the
 * player's car at once (0x00624ead); else 0x004637c4 decides (2 = taken, 1 = "car is already
 * selected" message, else stay). The background picture index is 0x004e8021 (0..13).
 *
 * Verified against the original under emulation (tools/emu/check_sh3_car_select.sh).
 */
#ifndef TM2_SH3_CAR_SELECT_H
#define TM2_SH3_CAR_SELECT_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (0xa70 bytes of locals). Returns the next
 * screen (the last sceneRun result) or the previous screen when left before the scene ran. */
uint32_t sh3_car_select(const ShEnv* e, uint32_t fb);

#endif /* TM2_SH3_CAR_SELECT_H */
