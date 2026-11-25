/*
 * gp_slam.h: the claw (Mr. Slam) target pick, ported from TM2.EXE 0x00431ce8 (PS1 pair:
 * weaponSlamFindTarget).
 *
 * gp_slam_find_target(car, human): human = the car is a human car block (claw record u32 +0x890,
 * own index i16 +0x00, stats +0x138, body +0x6C4), else an AI block (claw +0x87C, index i16 +0x04,
 * stats +0x4EC, body +0x708). Two grab points: the body matrix (+0x50) times (0, i16 stats+0x4E +
 * 0x78, 0) and (0, +0x3C, 0), plus the body position (+0x20). The nearest car by |dx| + |dy| + |dz|
 * to either point (32 bit wrap) among the human cars (0x0045afb4 / 0x0045afbc, position +0x6E4),
 * then, if none is under 160, among the AI cars (0x0045b078 / 0x0045b080, position +0x728); the own
 * car is skipped in its own list only. A hit under 160 is grabbed:
 *   human: +0x152 i16 60, +0x6CE i16 0, +0xA7 / +0xB7 / +0xAF u8 1, weight i16 +0x1B2, type +0x90
 *   AI:    +0x75 / +0x85 / +0x7D u8 1, +0x506 i16 60, +0x712 i16 0, weight i16 +0x566, type +0x30
 *   lighter than the claw's car (i16 stats+0x7A): claw +0x07 = 1, claw +0x0C i16 = 3 / 2 / 1 for
 *   distance < 64 / < 136 / else; heavier or equal: claw +0x0C = 0 and the result is 1. Claw +0x0E
 *   gets the target's type.
 * Returns 1 when a too heavy car was grabbed, else 0 (the original returns it in AL only).
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_slam.sh).
 */
#ifndef TM2_GP_SLAM_H
#define TM2_GP_SLAM_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;     /* claw records */
    void* user;
    int32_t (*human_count)(void* user);            /* 0x0045afb4 */
    int32_t (*ai_count)(void* user);               /* 0x0045b078 */
    uint8_t* (*human_car)(void* user, int32_t i);  /* 0x0045afbc */
    uint8_t* (*ai_car)(void* user, int32_t i);     /* 0x0045b080 */
} GpSlamEnv;

uint8_t gp_slam_find_target(GpSlamEnv* env, uint8_t* car, uint8_t human);

#endif /* TM2_GP_SLAM_H */
