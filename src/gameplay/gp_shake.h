/*
 * gp_shake.h: car shake start, ported from TM2.EXE 0x00420be8 (PS1 decomp pair: carShakeStart).
 *
 * gp_car_shake_start(car, kind, axes, frames, second): fills the shake record at car +0x98 (second
 * == 0) or car +0x108:
 *   +0x04 i16 frames; when frames <= 0: +0x00 u8 kind, +0x02 u8 axes, +0x04 = 3 - (rand & 3)
 *   +0x01 u8 kind, +0x06 i16 rand % 11, +0x0A / +0x0C i16 0x155
 *   +0x18 i16 x amplitude (axes 0 or 2), +0x1A i16 y amplitude (axes 1 or 2), else 0: 5 / 0x16 / 0x2D /
 *     0x44 / 0x71 for kind 1 (and any other) / 2 / 3 / 4 / 5, sign flipped when a rand() is odd; the
 *     y amplitude starts from the x one, flip included
 *   +0x20 / +0x22 i16: amplitude - rand % (amplitude / 2) (amplitude > 0), + it (< 0), unchanged at 0
 * Arguments as in the original: car EAX, kind DL, axes BL, frames CX, second on the stack (ret 4).
 * DECOMPILER TRAP: Ghidra tests the old frames at +0x04; the code stores the new frames first and
 *   tests those (the read is the i32 at +0x02 shifted down 16).
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_shake.sh).
 */
#ifndef TM2_GP_SHAKE_H
#define TM2_GP_SHAKE_H

#include <stdint.h>

void gp_car_shake_start(uint8_t* car, uint8_t kind, uint8_t axes, int16_t frames, uint8_t second);

#endif /* TM2_GP_SHAKE_H */
