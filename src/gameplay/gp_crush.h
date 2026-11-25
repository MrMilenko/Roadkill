/*
 * gp_crush.h: a car under a crushing level object, ported from TM2.EXE 0x004305fc (PS1 pair:
 * carCrushContactUpdate).
 *
 * gp_car_crush_contact(car, human): F flags (+0x9A / +0x68), body (+0x6C4 / +0x708), wheels W
 * (+0x58C / +0x5D0, four 0x48 records: position i32 x, y at +0x54 / +0x58, push i16 at +0x30), stats
 * P (+0x138 / +0x4EC: +0x30 i16 radius, +0x32 i16 push, +0x96 i16 object type).
 * F+0x14 = 0; the object (0x0045b0b8(type), position +0xE8 / +0xEC) against every wheel: r = integer
 * square root (the original's inline bit loop, signed compares, 32 bit squares) of the squared
 * x / y distance; under the radius F+0x14 = 1 and the wheel push = push - push * r / radius / 2, else 0;
 * the nearest r (under 0x7FFF) is kept. The object's car (0x0045b0ec, human / AI block). Nearer than
 * radius / 2 (and touching): debris and smoke at the body like 0x00431488, a dust puff
 * (0x00450864(body position, 0)) when rand() & 15 == 1, and the object's car loses one of P2 +0xA0
 * (when P2 +0xA4 is set and the frame parity & 3 == 0). Not touching: all four pushes 0 and the
 * object car's F2 +0x15 = 0.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_crush.sh).
 */
#ifndef TM2_GP_CRUSH_H
#define TM2_GP_CRUSH_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;     /* entities */
    int32_t parity;      /* 0x00a0364c (0x004931f4) */
    void* user;
    uint32_t (*entity)(void* user, int32_t type);                                     /* 0x0045b0b8 */
    uint8_t (*entity_car)(void* user, uint32_t ent, uint8_t* human, int32_t* index);   /* 0x0045b0ec */
    uint8_t* (*human_car)(void* user, int32_t i);                                     /* 0x0045afbc */
    uint8_t* (*ai_car)(void* user, int32_t i);                                        /* 0x0045b080 */
    void (*debris)(void* user, int32_t kind, const int32_t p[3]);                     /* 0x004516bc */
    void (*fx)(void* user, uint32_t fn, const int32_t p[3], int32_t zero);            /* 0x00451014 / 0x004512b4 / 0x00451274 / 0x00450864 */
} GpCrushEnv;

void gp_car_crush_contact(GpCrushEnv* env, uint8_t* car, uint8_t human);

#endif /* TM2_GP_CRUSH_H */
