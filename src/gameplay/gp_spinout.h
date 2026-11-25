/*
 * gp_spinout.h: a level object spins out the car it belongs to, ported from TM2.EXE 0x00431488
 * (PS1 pair: carSpinOutByObject).
 *
 * gp_car_spin_out_by_object(id, spin, fx): the entity of type id (0x0045b0b8) and its car (0x0045b0ec
 * -> human flag, index; human block 0x0045afbc, its stats at +0x58C / body at +0x6C4; AI block
 * 0x0045b080, stats at +0x5D0 / body at +0x708). Nothing without an entity or a car.
 *   spin != 0: 0x00421920(car, human, (int8)spin, 0), then four i16 pairs of the stats add up:
 *              +0x2E += +0x32, +0x76 += +0x7A, +0xBE += +0xC2, +0x106 += +0x10A
 *   fx != 0:   a point near the body position (+0x20..+0x28): x, y +- 32 (rand & 63), z - 32 - (rand
 *              & 31), z at least 1; debris 0x004516bc(rand & 3, point), then by rand & 15: 0 smoke
 *              0x00451014, 1 0x004512b4, 2..5 0x00451274 (point, 0), else nothing
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_spinout.sh).
 */
#ifndef TM2_GP_SPINOUT_H
#define TM2_GP_SPINOUT_H

#include <stdint.h>

typedef struct {
    void* user;
    uint32_t (*entity)(void* user, int32_t id);                                   /* 0x0045b0b8 */
    uint8_t (*entity_car)(void* user, uint32_t ent, uint8_t* human, int32_t* index); /* 0x0045b0ec */
    uint8_t* (*human_car)(void* user, int32_t i);                                 /* 0x0045afbc */
    uint8_t* (*ai_car)(void* user, int32_t i);                                    /* 0x0045b080 */
    void (*spin)(void* user, uint8_t* car, int32_t human, int32_t spin, int32_t zero);   /* 0x00421920 */
    void (*debris)(void* user, int32_t kind, const int32_t p[3]);                 /* 0x004516bc */
    void (*fx)(void* user, uint32_t fn, const int32_t p[3], int32_t zero);
} GpSpinEnv;

void gp_car_spin_out_by_object(GpSpinEnv* env, int16_t id, int32_t spin, int32_t fx);

#endif /* TM2_GP_SPINOUT_H */
