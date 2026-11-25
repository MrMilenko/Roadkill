/*
 * en8_joy.h: the joystick state snapshot, TM2.EXE 0x004a28d0, in the En2Env of en2_env.h.
 * Verified against the original under emulation (tools/emu/check_en8_joy.sh).
 */
#ifndef TM2_EN8_JOY_H
#define TM2_EN8_JOY_H

#include <stdint.h>
#include "en2_env.h"

/* 0x004a28d0: when input device 4 is a joystick (0x004932a0(4) == 1): with the hold flag
 * 0x00a8255c clear (0x0049e108 nonzero clears it first), the DirectInput device 0x00a82560 is
 * polled and byte per control stored from 0x00a824fd: buttons 0..3, 4..9 / 10..17 / 18..31 when
 * the device has more than 4 / 10 / 18 buttons (0x004a8240), the axes 0 / 1 and, when present
 * (0x00a82540..0x00a8254c), 2..5 as +/- pairs, the hat (0x00a8253c) at 0 / 18000 / 27000 / 9000 /
 * centered; then unless 0x0049e108: hold = 1 and the time (timeGetTime) to 0x00a82550. With the
 * hold set: the time since into 0x00a82554 / 0x00a82558, the hold cleared after 0x004e84a8 ms. */
void en8_joy_snapshot(En2Env* e);

#endif
