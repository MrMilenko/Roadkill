/*
 * gp_spin.h: a car is hit hard: spin / bounce reaction, ported from TM2.EXE 0x00421920 (no PS1 pair;
 * called by the object spin out 0x00431488 and the weapon hits).
 *
 * gp_car_spin_hit(car, human, kind, keep_speed): blocks by layout (human / AI): W wheels +0x58C /
 * +0x5D0 (four 0x48 byte wheel records at W+0x18.., W+0x60.., W+0xA8.., W+0xF0..), weight i16 +0x1B2 /
 * +0x566, body B +0x6C4 / +0x708 (+0x00 i16 heading, +0x0A i16 speed, +0x08 i16, +0x40 i32 spin),
 * flags F +0x9A / +0x68 (+0x01, +0x0B busy, +0x27, +0x28), stats P +0x138 / +0x4EC (+0x3C, +0x62,
 * +0x70), O i32 +0x7EC / +0x830 (spin rate).
 *   Human kind 7 with stats +0x3C set and F+0x27 set, F+0x28 clear: F+0x28 = 1, F+0x0B = 0 and the
 *   four wheel tilts (W+0x2A, +0x72, +0xBA, +0x102) cleared. Nothing more while F+0x0B is set.
 *   Lift s (frame ticks scaled: 0x1C0 / t * t * 2 / 3 for kind 7, else (100 - weight) * (0x120 / t * t)
 *   / 100 at least 0x60 / t * t), then by kind 1/8: 0x60 / t * t / 2, 4: s / 3, 5: s * 2, 10: s / 2 and
 *   a random heading kick of +-(P+0x70 + rand % P+0x70) by the frame parity, 11: 0x60 / t * t * 2,
 *   12: 0x120 / t * t * 2, 13: 0x120 / t * t * 3 / 2. Into the wheels (+0x74, +0xBC, +0x104, +0x2C),
 *   wheel bases (+0x42 = +0x5C ...). Kinds 7 / 5 / 13 / 12 tilt the wheels against the speed sign by
 *   rand() bits and stop the spin rate; the others pick one of 7 tilts by rand() & 7 (7 counts as 0)
 *   with a spin change and a speed cut unless keep_speed, and a spin rate from P+0x62. Then all
 *   wheels are marked airborne (start heights copied where not airborne yet), F+0x0B = F+0x01 = 1,
 *   the spin rate scaled by kind (1: / 2, 2-3: * 2, 4: / 3, 5/7/11/12/13: 0); an AI car gets no spin
 *   rate and F+0x01 = 0, and in event types 5 / 6 (0x00474508) its speed and B+0x08 are zeroed.
 * Frame ticks (0x004e8300 via 0x004931d4) and parity (0x00a0364c via 0x004931f4) come from env.
 * The original faults on a tick count of 0 and on P+0x70 = 0 for kind 10; the callers never do.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_spin.sh).
 */
#ifndef TM2_GP_SPIN_H
#define TM2_GP_SPIN_H

#include <stdint.h>

typedef struct {
    int16_t ticks;            /* 0x004e8300 */
    int32_t parity;           /* 0x00a0364c */
    void* user;
    int32_t (*event_type)(void* user);   /* 0x00474508 (low byte compared) */
} GpSpinHitEnv;

void gp_car_spin_hit(GpSpinHitEnv* env, uint8_t* car, uint8_t human, uint8_t kind, uint8_t keep_speed);

#endif /* TM2_GP_SPIN_H */
