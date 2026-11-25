/*
 * sh3_single_menu.h: the single player menu, TM2.EXE 0x00478edc (PS1 decomp:
 * screenSinglePlayerMenu).
 *
 * First the game disc check (0x00499884 with 0x004e285b; below 0 a message box, 0x004bc660(0x16):
 * 4 = retry, else back to the title 0x004780fc). Then the scene (background 0x00499408 with
 * 0x004e2863, scene model resources 0x004e2877) with three items: frame +0x308 (tournament:
 * game type 1, level 1, the opponents' total from 0x00474e0c minus one human, 0x00624ebf = 3),
 * +0x260 (challenge: game type 3, 0x00624ebf = 1) and +0x2b4. When the scene ends normally the
 * player setup is reset first: one human (0x00624ea9 = 1), opponent counts 0, car slots
 * 0x00624ead..0x00624ec1 = 0xff.
 *
 * Verified against the original under emulation (tools/emu/check_sh3_single_menu.sh).
 */
#ifndef TM2_SH3_SINGLE_MENU_H
#define TM2_SH3_SINGLE_MENU_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (0x644 bytes of locals). Returns the next
 * screen (the sceneRun result) or the title when the disc check is given up. */
uint32_t sh3_single_menu(const ShEnv* e, uint32_t fb);

#endif /* TM2_SH3_SINGLE_MENU_H */
