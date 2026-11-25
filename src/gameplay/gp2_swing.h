/*
 * gp2_swing.h: Mr. Slam's claw swing update, ported from TM2.EXE 0x00430bb4 (PS1 pair:
 * weaponSlamSwingUpdate). The swing is the claw's attack: lower the claw, swing the arm up,
 * hit what it meets, then retract.
 *
 * gp2_slam_swing_update(env, car, human): car is the game address of the car block.
 *   human != 0: claw record u32 +0x890, stats +0x138, body +0x6C4, flags +0x9A, misc +0xCC,
 *               key i16 +0x00, owner i16 +0x02
 *   human == 0: claw record u32 +0x87C, stats +0x4EC, body +0x708, flags +0x68, misc +0x40,
 *               key i16 +0x04, owner i16 +0x06
 * Fields read: flags +0x0D (stunned), misc +0x1F (hold the claw down), stats +0x4E i16 (reach),
 * body +0x20 position, +0x34 i16, +0x50 matrix.
 *
 * Claw record (u8): +0x00 active, +0x01 swinging (0: return at once), +0x02 grab running, +0x03
 * lowering, +0x04 swinging up, +0x05 holding down, +0x06 grab closing, +0x07 holding a car,
 * +0x0B light hits, +0x0C i16, +0x0E i16 key of the held car, +0x10 i16 hold timer.
 *
 *   stunned: carry the held car (0x004320d0 (key, stats, body, 0, stack 0)) and return.
 *   +3: the claw sound (0x39); claw angle -34 / tick down to -0x2AA (at the stop: +5 = 1,
 *       +3 = 0), arm -5 / tick.
 *   +5: misc +0x1F clear -> +4 = 1, +5 = 0; else the timer +0x10 counts ticks, above 600 the same.
 *   +4: arm +79 / tick. Past 0x38E: arm 0x38E, +4 = 0; holding: spin out (key, 1, 1), damage the
 *       held car 6 (+0x0B) or 12, spin out 0xC0 (1, 1); not holding: the swing sound (5), spin
 *       out 0xC0 (1, 0). Then the claw tip (body matrix x (0, reach + 0x78, 0) + position) hits
 *       target pool entries (0x00470d08, 50 x 20 bytes, count i16 +2 > 0, position +8) and target
 *       actors (0x00471aec, 6 x 0x4C, i16 +2 != -1, object u32 +0x48 with position +0xE8) whose
 *       largest axis distance >> 3 is under 15: 0x00470d3c (i16 +0, i16 +2, 0x19), 0x004723bc
 *       (index, {0, 0, 0xFFF}). Above 0x238 (not past 0x38E): carry if holding, else search
 *       (0x00431ce8); a catch: +4 = 0, spin out (key, 1, 1), damage 12 (+0x0B) or 25, spin out
 *       0xC0 (1, 1). Every +4 frame ends with the held car's spin out (key, 0, 1) and the arm set.
 *   else (retract): claw +45 / tick below 0x2E3, arm -34 / tick above -0x71; at or below -0x71
 *       and not holding: search, a catch spins it out (key, 0, 1). Both at their stops: set them
 *       exactly, +1 = 0, then holding: +6 = +2 = 1 (the grab starts), else +0 = 0, release
 *       (0x00432340 (key, body +0x34, 0)), +0x0E = 0xFFFF.
 * The {0, 0, 0xFFF} vector is the original's data at 0x004e66e8 (checked by the test).
 *
 * DECOMPILER TRAP: the damage calls (0x004210e4, ret 4) take the owner i16 as a stack argument
 * Ghidra drops. The retract's "arm above 0x238" test after "arm at or below -0x71" is dead code
 * (three instructions never run).
 *
 * Verified against the original under emulation (tools/emu/check_gp2_swing.sh).
 */
#ifndef TM2_GP2_SWING_H
#define TM2_GP2_SWING_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;     /* the car block, the claw record, the pool and actor tables */
    void* user;
    int32_t (*sound_mode)(void* user);                                   /* 0x0046cc54 */
    void (*sound_locate0)(void* user, uint32_t pos);                     /* 0x0046f974 */
    void (*sound_locate1)(void* user, uint32_t pos, int32_t v);          /* 0x0046f980 */
    int32_t (*sound_vol)(void* user);                                    /* 0x0046f990 */
    int32_t (*sound_pan)(void* user);                                    /* 0x0046f988 */
    void (*sound_play)(void* user, int32_t id, int32_t pan, int32_t vol, int32_t prio); /* 0x0046f8b0 */
    void (*carry)(void* user, int32_t key, uint32_t stats, uint32_t body, int32_t a, int32_t b); /* 0x004320d0 */
    uint8_t (*find_target)(void* user, uint32_t car, uint8_t human);     /* 0x00431ce8 */
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
    uint32_t (*pool_get)(void* user);                                    /* 0x00470d08 */
    void (*pool_hit)(void* user, int32_t id, int32_t count, int32_t v);  /* 0x00470d3c */
    uint32_t (*actors_get)(void* user);                                  /* 0x00471aec */
    void (*actor_hit)(void* user, int32_t i, const int32_t dir[3]);      /* 0x004723bc */
} Gp2SwingEnv;

void gp2_slam_swing_update(Gp2SwingEnv* env, uint32_t car, uint8_t human);

#endif /* TM2_GP2_SWING_H */
