/*
 * carsys_hits.h: car knock back, skid smoke and spin out by id, ported from TM2.EXE
 * 0x00421800, 0x00417640, 0x00432340. Car layouts as in car.h (player = 1, AI = 0):
 *
 *                 player   AI
 *   controls      +0x09A  +0x068   +0x01, +0x0B in air, +0x12, +0x15, +0x18 sliding, +0x19,
 *                                  +0x1C, +0x1D
 *   stats         +0x138  +0x4EC   +0x20 i16 spin count, +0x78 i32 (mass in the high half),
 *                                  +0xC4 i32 reverse speed cap
 *   probes        +0x58C  +0x5D0   wheel states 0x48 apart from +0x18: +0x1A i16 each,
 *                                  rear wheel positions at +0xE4 / +0x12C
 *   body          +0x6C4  +0x708   +0x02 / +0x04 i16, +0x08 i32 speed (i16 at +0x0A),
 *                                  +0x12 i16 spin direction, +0x34 i32 travel angle, +0x40 i32
 *                                  heading, +0x4C i32
 *   contact       +0x21C  +0x0CC   +0x03..05 u8, +0x06 i16 timer, +0x0A i16, +0x12 i16 push,
 *                                  +0x364 i32 x 3 hit position
 * Verified against the original under emulation (tools/emu/check_carsys_hits.sh).
 */
#ifndef TM2_CARSYS_HITS_H
#define TM2_CARSYS_HITS_H

#include <stdint.h>

#include "carsys_records.h"

typedef struct {
    const CarRecords* rec;
    /* 0x00421920(car, human, kind, 0): starts a spin out (kind 1) or a backwards one (kind 2) */
    void (*spin_out)(void* user, uint8_t* car, int human, int kind);
    /* 0x00420be8(car, 5, 2, 2, human): body shake */
    void (*shake)(void* user, uint8_t* car, int human);
    /* 0x0044a40c(pos, 0x555, 0x555, 0x555): a jittered copy of pos */
    uint8_t* (*jitter)(void* user, uint8_t* pos, int32_t range);
    /* 0x00451230(pos, 0): skid smoke puff */
    void (*smoke)(void* user, uint8_t* pos);
    void* user;
    const int32_t* slam_arm;    /* 0x00502e98 Mr. Slam's arm angle (carsys_slam_arm_angle) */
} CarsysHitOps;

/* 0x00421800: a car gets pushed from hit position pos by amount (halved for mass > 50): contact
 * +0x364 = pos, +0x12 = amount, +0x03..05 = 0, timer +0x06 = 120 below 40, else 300.
 * The original's formula for 40..800 is 300 - (800 - amount) / 800, which is always 300. */
void carsys_knock_back(uint8_t* car, int player, const int32_t* pos, int32_t amount);

/* 0x00417640: on the ground and sliding (controls +0x01, or +0x1C for a player car) faster than
 * half the reverse cap: four smoke puffs at the rear wheels (two on the PS1) and controls +0x18
 * = 1; otherwise +0x18 = 0. */
void carsys_skid_smoke(uint8_t* car, int player, const CarsysHitOps* ops);

/* 0x00432340: spins out the car owning object key id (carsys_object_from_owner /
 * carsys_find_by_object), facing angle (back: angle + 0x800 at a third of the speed). The front
 * spin also sets the spin direction from the heading vs travel angle, but only for cars with
 * mass below 35 (no such test on the PS1). */
void carsys_spin_out_by_id(int16_t id, int16_t angle, uint8_t back, const CarsysHitOps* ops);

/* 0x0041fa98 (= car_hit_pass, delegated by car_hit.c): an active car hit (active +0x14 i16 =
 * the other car's object slot, carsys_from_slot) handed to the other car as a wall summary
 * (0x24 bytes, summary +0x0A i16 its angle): the bounce angle 0x800 - 2 * (other body +0xF4 -
 * angle, + 0x800 when other body +0x1A < 0) picks the part (by the other car's travel angle
 * +0x34 against it: > 0x400 1, < -0x400 0, > 0 2, else 3); the summary is copied into that
 * part (hit base +0x19C + 0x24 part), marked (+1 = 1, +3 = part), its speed (+6) is the other
 * car's speed raised to at least ticks60 * 760 / 100 * 32, its angle (+0x0C) the bounce. */
void carsys_hit_pass(const CarRecords* rec, const uint8_t* active, const uint8_t* summary);

/* 0x004320d0 (id, stats, body, lifting, side; side is the one stack argument): Mr. Slam carries
 * the car owning object key id: its body x / y ease (tm2_angle_step, rate 0x14 lifting, else
 * 0x0D) to a point (0, stats +0x4E + 120 (+ or - for side, the arm's lift while lifting), 0)
 * turned by the slammer's body +0x50 matrix (transposed) from the slammer's body +0x20 / +0x24,
 * its travel angle eases (tm2_angle_interp, rate 9) to the slammer's +- 0x400; while lifting the
 * wheel heights (+0x1A of each wheel state) tilt toward the slammer by the arm angle and stats
 * +0x48. The lookup does not test for a missing object (carsys_find_by_object still runs). */
void carsys_slam_carry_target(int16_t id, const uint8_t* stats, const uint8_t* body, uint8_t lifting,
                              uint8_t side, const CarsysHitOps* ops);

#endif /* TM2_CARSYS_HITS_H */
