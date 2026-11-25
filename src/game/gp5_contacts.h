/*
 * gp5_contacts.h: the car contact loop of TM2.EXE 0x0041f40c (car_hits_gather, src/game/car_hit.h),
 * the code range 0x0041f80e..0x0041fa74, ported on its own (car_hit.c delegates it through
 * CarHitApplyEnv.car_contacts).
 *
 * The range runs when the active hit (hit base +0x34C, the 0x24 byte summary layout of car_hit.h)
 * is empty (+0) or consumed (+2). For k = 0 .. points - 1 it reads the car to car contact
 * summary k at hit base +0x274 + 0x24 * k and stops at the first one whose +0 is clear:
 *   - empty / consumed active hit: the contact becomes the active hit; hit base +0x12 / +0x14 =
 *     its i16 +0x12 / +0x14. The other car (car_by_id of the contact's i16 +0x14) may knock this
 *     one into a spin: when this car is faster (body i16 +0x0A) than its stats +0xC4, lighter
 *     (stats i16 +0x7A below the other's and below 0x1E) and its travel angle (body +0x34) differs
 *     from the other's by more than 0x5C7 (wrapped): controls +1 = +0x20 = 1, body +0x4C = 0,
 *     active +0x0C = +-0xAA and +8 = body +0x40 +- 0xAA (by the sign of active +0x0C), active +6 =
 *     stats +0xC4, active +0x12 = 0, body +2 = body +0x40, stats +0x20 = 0, body +0x12 = +-1 and
 *     body +4 = body +0x40 +- 0x400 (by the sign of the angle difference), then
 *     0x00421920(car, player, 0x0A, 1).
 *   - otherwise, a wall active hit (+1) goes to 0x0041fa98(contact, active); a car active hit is
 *     replaced by a deeper contact (+0x0E).
 * Block offsets as in car_hit.h (player / AI): hit base +0x21C / +0xCC, controls +0x9A / +0x68,
 * stats +0x138 / +0x4EC, body +0x6C4 / +0x708. points = the car model's collision point count.
 * Verified against the original under emulation (tools/emu/check_gp5_contacts.sh).
 */
#ifndef TM2_GP5_CONTACTS_H
#define TM2_GP5_CONTACTS_H

#include <stdint.h>

typedef struct {
    /* 0x0045b028(id, &is_player): the car block with that id (NULL when none); is_player set to
     * its kind */
    uint8_t* (*car_by_id)(void* u, int32_t id, uint8_t* is_player);
    /* 0x00421920(car, player, kind, flag) */
    void (*set_state)(void* u, uint8_t* car, int player, int kind, int flag);
    /* 0x0041fa98(contact, active) */
    void (*pass_hit)(void* u, uint8_t* contact, uint8_t* active);
    void* user;
} Gp5ContactEnv;

/* 0x0041f80e..0x0041fa74 of 0x0041f40c: car = the car block, player = its kind */
void gp5_car_contacts(uint8_t* car, int player, int32_t points, const Gp5ContactEnv* env);

#endif /* TM2_GP5_CONTACTS_H */
