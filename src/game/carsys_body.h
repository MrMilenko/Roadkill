/*
 * carsys_body.h: wheel effects and the car body pose, ported from TM2.EXE 0x0042b02c and
 * 0x00453c78. Probe block layout as in car.h / terrain_probe.h: wheel states 0x48 apart from
 * +0x18; wheel k: +0x3C + 0x48 k i32 x, y, z (positions at +0x54, +0x9C, +0xE4, +0x12C) and
 * i16 height at +0x3C + 0x48 k (+0x3C, +0x84, +0xCC, +0x114).
 * Verified against the original under emulation (tools/emu/check_carsys_body.sh).
 */
#ifndef TM2_CARSYS_BODY_H
#define TM2_CARSYS_BODY_H

#include <stdint.h>

typedef struct {
    /* 0x0044a40c(pos, 0x555, 0x555, 0x555): a jittered copy of pos */
    uint8_t* (*jitter)(void* user, uint8_t* pos, int32_t range);
    /* kind 0..3: 0x00451230 / 0x00451274 / 0x004512b4 / 0x004512f8 (pos, 0) */
    void (*fx)(void* user, int kind, uint8_t* pos);
    void* user;
} CarsysWheelFx;

/* 0x0042b02c (probe block, kind): one time in five (tm2_rand() % 5 == 0) an effect of kind
 * (0, 1, 2, anything else = 3) at each wheel, jittered. The original returns whatever the last
 * call left in eax; callers do not use it. */
void carsys_wheels_spawn_fx(uint8_t* probes, uint8_t kind, const CarsysWheelFx* fx);

/* 0x00453c78: poses the car's body object (body_objects[(car key >> 4) - 1], key = i16 player
 * +0x90 / AI +0x30): rotation from the rear axle and left side wheel vectors in the body frame
 * (body +0x50 matrix) and the heading (body +0x34) into object +4, x / y from body +0x20 into
 * +0x28 / +0x2C, mean wheel height into +0x30 (+3 for keys 0x40, 0x60, 0xA0, 0xB0, 0xC0).
 * ORIGINAL BUG not reproduced: keys outside 0x10..0xFF index outside the 15 entry table; the
 * port ignores them. */
void carsys_body_pose_update(uint8_t* car, int player, uint8_t* const* body_objects);

#endif /* TM2_CARSYS_BODY_H */
