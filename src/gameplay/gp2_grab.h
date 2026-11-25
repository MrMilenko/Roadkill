/*
 * gp2_grab.h: Mr. Slam's claw grab update, ported from TM2.EXE 0x004316b4 (PS1 pair:
 * weaponSlamGrabUpdate). Runs every frame for a car whose claw is active.
 *
 * gp2_slam_grab_update(env, car, human): car is the game address of the car block.
 *   human != 0: claw record u32 +0x890, stats +0x138, body +0x6C4, key i16 +0x00, owner i16 +0x02
 *   human == 0: claw record u32 +0x87C, stats +0x4EC, body +0x708, key i16 +0x04, owner i16 +0x06
 *   ("key" only decides the sound branch; "owner" is the damage call's stack argument)
 * Body field read: +0x34 i16 (passed to the spin out). Sound position: body + 0x20.
 *
 * Claw record (u8 unless said): +0x00 active (cleared at the end of the retract), +0x02 running
 * (0: return at once), +0x03 lowering after a hit, +0x06 closing, +0x07 holding a car, +0x08 arm
 * phase, +0x09 done, +0x0A release swing, +0x0B light damage (5 instead of 10), +0x0C i16 hits
 * left, +0x0E i16 key of the held car (0xFFFF = none).
 *
 * Claw angle (0x00453ec8 / 0x00453f10) and arm angle (0x00453ec0 / 0x00453ed0) move by
 * ticks60 * rate per frame, wrapped by math_angle_wrap32:
 *   start: the grab sound (0x39), then
 *   closing (+6): carry the held car (0x004320d0, ecx 0, stack 0) or look for one (0x00431ce8);
 *     claw -68 to -0x2AA, arm +56 to 0x38E; both at their ends: +6 = 0, then holding: +3 = 1,
 *     +9 = +0x0A = 0, +8 = 1, else +9 = 1, +8 = 0.
 *   +8 == 0 (retract): claw +45 below 0x2E3, arm -34 above -0x71 (spin the held car above 0x238);
 *     at both ends set them exactly, +2 = +0 = 0.
 *   +3: arm -34 down to 0x71 and carry (1, 1); at 0x71 or below: +3 = 0.
 *   +0x0A: carry (1, 1), arm -34; under 0x71: arm 0x71, release (0x00432340 with 1), done.
 *   else: arm +68 up to 0x38E carrying (1, 0), the slam sound (2) at the top; at the top: damage
 *     (0x004210e4) the held car (0x0045b0b8 / 0x0045b0ec / 0x0045afbc / 0x0045b080 / 0x0045b154),
 *     one hit less; hits left: +3 = 1, else release (0x00432340 with 0), done, and rand() & 1
 *     sets +0x0A.
 * In every phase but closing, an arm above 0x238 spins out the held car (0x00431488 (key, 0, 1)).
 *
 * DECOMPILER TRAP: Ghidra drops the damage call's stack argument (0x004210e4 ends ret 4): the
 * attacking car's owner i16 (human +0x02 / AI +0x06), and its ecx is 2.
 *
 * Verified against the original under emulation (tools/emu/check_gp2_grab.sh).
 */
#ifndef TM2_GP2_GRAB_H
#define TM2_GP2_GRAB_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;     /* the car block and the claw record */
    void* user;
    /* every address below is a game address */
    int32_t (*sound_mode)(void* user);                                   /* 0x0046cc54 */
    void (*sound_locate0)(void* user, uint32_t pos);                     /* 0x0046f974 */
    void (*sound_locate1)(void* user, uint32_t pos, int32_t v);          /* 0x0046f980 */
    int32_t (*sound_vol)(void* user);                                    /* 0x0046f990 */
    int32_t (*sound_pan)(void* user);                                    /* 0x0046f988 */
    void (*sound_play)(void* user, int32_t id, int32_t pan, int32_t vol, int32_t prio); /* 0x0046f8b0 */
    void (*carry)(void* user, int32_t key, uint32_t stats, uint32_t body, int32_t a, int32_t b); /* 0x004320d0 */
    void (*find_target)(void* user, uint32_t car, uint8_t human);        /* 0x00431ce8 */
    int32_t (*claw_get)(void* user);                                     /* 0x00453ec8 */
    void (*claw_set)(void* user, int32_t a);                             /* 0x00453f10 */
    int32_t (*arm_get)(void* user);                                      /* 0x00453ec0 */
    void (*arm_set)(void* user, int32_t a);                              /* 0x00453ed0 */
    void (*spin)(void* user, int32_t key, int32_t a, int32_t b);         /* 0x00431488 */
    void (*release)(void* user, int32_t key, int32_t b34, int32_t front);/* 0x00432340 */
    uint32_t (*object_from_owner)(void* user, int32_t key);              /* 0x0045b0b8 */
    uint8_t (*find_by_object)(void* user, uint32_t obj, uint8_t* human, int32_t* index); /* 0x0045b0ec */
    uint32_t (*human_car)(void* user, int32_t i);                        /* 0x0045afbc */
    uint32_t (*ai_car)(void* user, int32_t i);                           /* 0x0045b080 */
    int32_t (*object_id)(void* user, int32_t i, uint8_t human);          /* 0x0045b154 */
    void (*damage)(void* user, int32_t id, int32_t amount, uint32_t pos, int32_t ecx, int32_t owner); /* 0x004210e4 */
    int32_t (*rand)(void* user);                                         /* 0x004899c2 */
} Gp2GrabEnv;

void gp2_slam_grab_update(Gp2GrabEnv* env, uint32_t car, uint8_t human);

#endif /* TM2_GP2_GRAB_H */
