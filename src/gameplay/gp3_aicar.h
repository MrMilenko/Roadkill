/*
 * gp3_aicar.h: the AI car per frame update, TM2.EXE 0x00422e08 (Ghidra FUN_00422e08, old name
 * ai_car_update; the PS1 decomp has no confident pair). eax = AI car block (0x880 bytes, the
 * blocks at 0x0050760c). Ends at 0x00425478 (0x00425478..0x00425674 is another function).
 *
 * What it does, in machine order:
 *   1. wrecked AI car with explosion kind 4 (+0x68 set, i8 +0x4F1 == 4): nothing.
 *   2. +0x4EF: +0x5BC = 0x0042c3e0(car, 0), 0x00432940(car). Keeps the old heading +0x73C.
 *      0x0043a944(stats). When the fps changed (i16 +0x500): rescale +0x712 speed, +0x708 steer,
 *      +0x71C by fps / old, +0x500 = fps, +0x5AC = (i16 +0x562 * ((i16 +0x544 * 19 * ticks / 100)
 *      << 5)) / 100. Respawn timer +0x75 / +0x506 / +0x508 (0x00470940(id, 3)).
 *   3. alive and not wrecked: chase geometry against the chased car (ai_target_car 0x0045afc8,
 *      human body +0x6C4, AI body +0x708): +0x28 distance (bitwise isqrt of dx^2 + dy^2, 32 bit
 *      wrap), +0x26 height difference, approach / retreat timers +0x460 / +0x462, the range bytes
 *      +0x32..+0x38 against the i16 thresholds +0x16..+0x1E, 0x00425674(car), +0x22 bearing
 *      (atan2), +0x20 relative bearing, +0x24 facing difference, +0xC8 side class; the event 5
 *      boss bind (+0x449 / +0x454 / +0x45C); the behaviour roll (+0x01 mode 1..3 by rand % 100 and
 *      the skill blend of +0x568..+0x576 by +0x58C * 100 / i16 +0x526), 0x00410010(car).
 *   4. car_clear_controls(+0x40), the turn-around timers (+0x44C, +0x466, +0x3B, +0x08) with
 *      0x00427710(car) resets.
 *   5. +0x447 set (spin / recover state): recover steering and speed, then the common tail.
 *      Else, alive: attack approach (+0x39), ram trigger +0x5B, route following (+0x43C == 0:
 *      brake / gas decisions +0x52 / +0x53 against the planned speed, Dark Tooth (car type 0xF0)
 *      charge / jump states +0x450..+0x453 with 0x0043e6e0(car, 0), dodge rolls, side steer
 *      +0x48A) or the off route steering (+0x43C != 0) with the turn arcs +0x44A, then the
 *      control bytes +0x4E / +0x4F / +0x50 / +0x52 / +0x53 / +0x6E and the turbo / camera track
 *      0x00470320(id + 6, 0, on).
 *   6. tail: the per car steps 0x0042c4d4, 0x0042a020, 0x004220b8, 0x004305fc (+0x7C), 0x00420e48,
 *      0x0042d470, car_wreck_update 0x00422704 (wrecked, kind != 4); Dark Tooth close range turn;
 *      0x00453538(id, speed); the pickup record +0x87C (a 32 bit handle: 0x00432340 / 0x004316b4 /
 *      0x00430bb4); height step for wrecked cars or 0x00418450 / 0x0041858c; +0x474 += |turn|
 *      (16 bit); out of bounds flag +0x87 (0x0045c248 / 0x0045c2a0, +0x58C = -1); 0x00453c78,
 *      0x0043af58 when +0x584 is set, 0x00418d38, 0x00417640.
 *
 * Globals read: the GRP block 0x004fca18 (+0x1218 i8 per zone flags, +0xCE6 i8 zone x 0x23
 * table) and the PTS nodes 0x004fe1e4 (24 bytes: x i32, y i32, z i16 +8, zone i8 +0x17).
 * Frame ticks / fps through src/math (0x004931d4 / 0x004931dc). The math helpers (atan2, wraps,
 * interp / step) are the verified src/math ones. Every other callee goes through env->fn[].
 *
 * DECOMPILER TRAPS: Ghidra shows a difficulty compare before the event 5 test: the flags it sets
 * are overwritten by the call (dead). In the route branch with +0x3A set, the original re-reads
 * +0x3A after ai_target_car and only when it is now 0 runs a speed test (0x00424730..0x004247a6):
 * dead in the game (ai_target_car writes nothing); ported literally and covered with a stub that
 * clears +0x3A.
 *
 * Verified against the original under emulation (tools/emu/check_gp3_aicar.sh).
 */
#ifndef TM2_GP3_AICAR_H
#define TM2_GP3_AICAR_H

#include <stdint.h>

enum {
    GP3A_42C3E0,  /* (car, 0) -> i32 */
    GP3A_432940,  /* (car) */
    GP3A_43A944,  /* (car + 0x4EC) */
    GP3A_470940,  /* (id, 3) */
    GP3A_TARGET,  /* 0x0045afc8 (id, u8* human) -> car block */
    GP3A_425674,  /* (car) */
    GP3A_DIFF,    /* 0x0045b3dc () -> i32 */
    GP3A_EVENT,   /* 0x00474508 () -> al */
    GP3A_45B3FC,  /* (type) -> al */
    GP3A_427838,  /* (car) */
    GP3A_RAND,    /* 0x004899c2 () -> i32 */
    GP3A_410010,  /* (car) */
    GP3A_CLEAR,   /* 0x00429fd0 (car + 0x40) */
    GP3A_427710,  /* (car) */
    GP3A_4291DC,  /* (car, dl) */
    GP3A_42CE2C,  /* (car) -> al */
    GP3A_418CF0,  /* (car + 0x708, car + 0x68) */
    GP3A_42577C,  /* (car) */
    GP3A_4334C8,  /* (car, i) */
    GP3A_4259EC,  /* (car) */
    GP3A_433000,  /* (car, dl) */
    GP3A_427DD8,  /* (car) */
    GP3A_SINGLE,  /* 0x0045c348 () -> al */
    GP3A_430414,  /* (zone, zone) -> i32 */
    GP3A_432F2C,  /* (car) */
    GP3A_43E6E0,  /* (car, 0) Dark Tooth */
    GP3A_456C70,  /* () -> al */
    GP3A_421920,  /* (car, a, b, c) */
    GP3A_430460,  /* (car, i32* out, 1) */
    GP3A_4295FC,  /* (car, i) -> al */
    GP3A_432BA0,  /* (car) */
    GP3A_439588,  /* (type) -> i32 */
    GP3A_470320,  /* (id + 6, 0, on) */
    GP3A_4168E4, GP3A_42C4D4, GP3A_42A020, GP3A_4220B8, GP3A_4305FC, GP3A_420E48, GP3A_42D470,
    GP3A_422704,  /* all (car, 0) */
    GP3A_43953C,  /* (car + 0x4EC, 0x19) */
    GP3A_453538,  /* (id, speed) */
    GP3A_432340,  /* (a, b, 1) */
    GP3A_4316B4, GP3A_430BB4, GP3A_418450, GP3A_41858C, /* (car, 0) */
    GP3A_45C248,  /* (car, 0) -> i32 */
    GP3A_45C2A0,  /* () -> i32 */
    GP3A_453C78,  /* (car, 0) */
    GP3A_43AF58,  /* (car, id, 1) */
    GP3A_418D38, GP3A_417640, /* (car, 0) */
    GP3A_N
};

typedef uintptr_t (*Gp3AFn)(void* user, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d);

typedef struct {
    const uint8_t* grp;      /* 0x004fca18 */
    const uint8_t* nodes;    /* 0x004fe1e4 */
    uint8_t* (*resolve)(void* user, uint32_t handle);   /* the pickup record handle at +0x87C */
    Gp3AFn fn[GP3A_N];
    void* user;
} Gp3AiCarEnv;

/* 0x00422e08 */
void gp3_ai_car_update(const Gp3AiCarEnv* env, uint8_t* car);

#endif /* TM2_GP3_AICAR_H */
