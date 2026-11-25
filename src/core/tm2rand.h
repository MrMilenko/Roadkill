/*
 * tm2rand.h: the game's random numbers, which are the Watcom C runtime rand()/srand() in TM2.EXE:
 *   rand  0x004899c2: state = state * 0x41c64e6d + 0x3039; return (state >> 16) & 0x7fff
 *   srand 0x004899e6: state = seed (state lives in the runtime's per thread data, +0xc)
 * The game seeds it from timer_get_elapsed() when a match is set up (0x00457464, 0x004576a8,
 * 0x00457c80), so sequences differ per play. Verified under emulation (tools/emu/check_rand.sh).
 */
#ifndef TM2_RAND_H
#define TM2_RAND_H

#include <stdint.h>

void tm2_srand(uint32_t seed);
int32_t tm2_rand(void);          /* 0..0x7fff */

#endif /* TM2_RAND_H */
