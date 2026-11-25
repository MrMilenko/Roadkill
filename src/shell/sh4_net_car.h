/*
 * sh4_net_car.h: a network player's car state and messages applied, TM2.EXE 0x00473714
 * (Ghidra's old name FUN_00473714; tools/match paired it with the PS1 aiCarDrive, which is wrong
 * for the PC code: this is the receiving side of the snapshot built by 0x00473254, see
 * sh2_events.h).
 *
 * Called with (eax) the received bytes, (edx) their length and (ebx) the player i. The bytes go
 * to player i's record 0x00622d24 + 0x40c * i (copy_words 0x0048ce88, (len + 3) / 4 words):
 *   +0 i16 id, +2 i16 health, +4 i16 speed, +6 i16 turn, +8 / +0x0c i32 x / y, +0x10 i32 armor,
 *   +0x14 u16 flag bits, +0x16 the message stream (up to 0x3de bytes).
 * The car (0x0045afbc(i)) takes them: health, heading from the move (0x0048b71c), position, armor
 * raised to the record's on all four sides, the 12 flag bytes, the frozen state change (bit 12,
 * 0x00470940), speeds scaled by the car's values / 100 and the frame ticks. Then the stream:
 * bytes 0x2a introduce a command 0..13 (car reset, player car id, finished flag, kill credit
 * with the answer appended to the send stream at 0x00624d84, damage / pickup calls, sounds);
 * 0x7f, any other byte or the end stops it.
 *
 * Verified against the original under emulation (tools/emu/check_sh4_net_car.sh).
 */
#ifndef TM2_SH4_NET_CAR_H
#define TM2_SH4_NET_CAR_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (esp after its prologue, 0x20 bytes). */
void sh4_net_car_apply(const ShEnv* e, uint32_t fb, uint32_t src, int32_t len, uint32_t i);

#endif /* TM2_SH4_NET_CAR_H */
