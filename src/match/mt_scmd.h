/*
 * mt_scmd.h: scene commands (the C++ like objects run by the scene engine, src/game/scene.h and
 * src/match/mt_scene.h), ported from TM2.EXE 0x0046a728..0x0046ab34.
 *
 * Command record (handle cmd):
 *   +0x00 i32 duration (frames)   +0x04 i32 frames left (0 = idle)   +0x0c u32 target object
 *   move / turn / move+turn:  +0x10 / +0x14 / +0x18 i32 step (move+turn: +0x1c..+0x24 turn step)
 *   choice:                   +0x10 u8 mode (0 step, 1 set, 2 random), +0x14 i32 value,
 *                             +0x18 u32 validator function (value in eax, 0 = rejected)
 * Target object: +0x04 u32 choice node (u8 +0 type 9, +6 count, +7 current),
 *   +0x1c i32[3] position, +0x28 i32[3] angles.
 * A finished command posts event 2 (source cmd) to the shell scene (0x0050fad8) and removes
 * itself from that scene's running list (+0xaa0 count, +0xaa4 list), then rearms (+4 = +0).
 *
 * Verified against the original under emulation (tools/emu/check_match_scmd.sh).
 */
#ifndef MT_SCMD_H
#define MT_SCMD_H

#include <stdint.h>

typedef struct {
    uint8_t* scene;             /* 0x0050fad8 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    int32_t (*rand_fn)(void* user);                              /* 0x004899c2 (tm2_rand) */
    uint32_t (*validate)(void* user, uint32_t fn, uint32_t value); /* the choice validator */
    void* user;
} MtSceneCmd;

/* 0x0046a728 / 0x0046a744: target position / angles = the command's three values */
void mt_scmd_place(uint32_t cmd, const MtSceneCmd* e);
void mt_scmd_orient(uint32_t cmd, const MtSceneCmd* e);
/* 0x0046a760: move: while running, position += step; 0x0046a878: turn: angles += step, each
 * wrapped with the signed remainder % 0x1000; 0x0046a9c0: move and turn (angles not wrapped). Then
 * frames left: 0 -> finish (see above), > 0 -> one less. */
void mt_scmd_move(uint32_t cmd, const MtSceneCmd* e);
void mt_scmd_turn(uint32_t cmd, const MtSceneCmd* e);
void mt_scmd_move_turn(uint32_t cmd, const MtSceneCmd* e);
/* 0x0046ab34: choice: on a type 9 node: mode 0: current = (current + count + value) % count,
 * mode 2: (current + rand()) % count, repeated while the validator (if any) rejects it; mode 1:
 * current = value % count if the validator (if any) accepts value. Signed remainders, stored as
 * a byte. */
void mt_scmd_choice(uint32_t cmd, const MtSceneCmd* e);

#endif /* MT_SCMD_H */
