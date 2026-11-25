/*
 * gp_sway.h: car body sway / shake step, ported from TM2.EXE 0x00420e48 (PS1 pair: carBodySwayUpdate).
 *
 * gp_car_body_sway(car, second): R = shake record (car +0x98, or +0x108 when second), S = stats
 * (car +0x4EC / +0x138), B = body (car +0x708 / +0x6C4), limit = 0 (or i32 car +0x8C when second).
 * Nothing while the byte car +0x74 (second: +0xA6) is set. Else:
 *   lean: v = ((i16 S+0x74 * i16 B+0) / d) * |i16 B+0x0A| / i32 S+0xC4, d = i16 S+0x64 (/ 10 for the
 *         first record), clamped to +-|S+0x74| with v's sign; R+0x10 eases to v (math_angle_step, 5)
 *   tilt: t = atan2(i16 S+0x34, i16 S+0x4A) clamped to the limit (min(t, limit) when limit > 0, else
 *         max(-t, limit) negated back); R+0x0E eases to -that
 *   shake (R+0x01 set): phase R+0x06 += R+0x0C; under 0x1000 the offsets R+0x28 / R+0x2A are
 *         amplitude (R+0x20 / R+0x22, scaled by |speed B+0x0A| / (S+0xB4 / 8) below that speed)
 *         x sin(phase) / 4096; at 0x1000 the phase wraps and, with frames R+0x04 left, the
 *         amplitudes halve (16 bit, toward zero) and one frame goes; else (and when R+0x01 is clear
 *         but R+0x00 set) a new shake starts: 0x00420be8(car, R+0x00, R+0x02, 0, second)
 *         (src/gameplay/gp_shake.c).
 * The original faults on d, S+0xC4 or S+0xB4 / 8 being 0; the callers never pass that.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_sway.sh).
 */
#ifndef TM2_GP_SWAY_H
#define TM2_GP_SWAY_H

#include <stdint.h>

void gp_car_body_sway(uint8_t* car, uint8_t second);

#endif /* TM2_GP_SWAY_H */
