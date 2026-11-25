/*
 * sh3_net_wait.h: the network game's car wait screen, TM2.EXE 0x0047e010 (Ghidra's old name
 * match_network_start). Network game types 5..7 go here from car select.
 *
 * SHELLDB1\CARSEL.TIM background (0x004e2983), the scene model resources 0x004e2998, two decor
 * objects (frame +0x1e8: the car shown, its choice set to the player's car 0x00624ead; +0x194).
 * The session's current player is set to 0x10, the scene runs, the players' cars are read back
 * (0x00463bdc) and, when the network flag (0x004632d4) is set, it waits a second. Scene result 0
 * goes on to 0x00485100, else the sceneRun result.
 *
 * Verified against the original under emulation (tools/emu/check_sh3_net_wait.sh).
 */
#ifndef TM2_SH3_NET_WAIT_H
#define TM2_SH3_NET_WAIT_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (0x328 bytes of locals). */
uint32_t sh3_net_wait(const ShEnv* e, uint32_t fb);

#endif /* TM2_SH3_NET_WAIT_H */
