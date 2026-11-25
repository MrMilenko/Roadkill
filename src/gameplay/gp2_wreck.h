/*
 * gp2_wreck.h: a destroyed car's per frame update (burning, wreck timer, final explosion),
 * ported from TM2.EXE 0x00422704 (no PS1 pair found). Called by the AI car update with human 0
 * and by the human car update with human 1; its return value (whatever eax holds) is never used.
 *
 * gp2_car_wreck_update(env, car, human): car is the game address of the car block.
 *   human:  stats S +0x138, body B +0x6C4, flags F +0x9A, F2 +0x848, type u16 +0x90, key i16 +0x00
 *   AI:     stats S +0x4EC, body B +0x708, flags F +0x68, F2 +0x834, type u16 +0x30, key i16 +0x04
 * AI only, first: the car it chases (0x0045afc8(key) -> human body +0x6C4 / AI body +0x708):
 *   +0x28 u32 = isqrt(dx^2 + dy^2) (bitwise integer square root, 32 bit wrapping squares, signed
 *   compares), +0x32 u8 "near" = (int)root < 0x3020; then (F +0x1A, +0x0B, +0x21 clear) the
 *   burning flame 0x00450e24(B + 0x20, 1) into F2 +0x44 (its +0x0C i16 = 500), and S +0x22 = 0.
 *   Humans count as near.
 * Smoke: rand() < 0x800 with F +0x1A: 0x0044a40c(B + 0x20, 0x333 x3) -> vector, its z += 0x78,
 *   then 0x00451058 (rand() < 0x4000) or 0x00451014.
 * Timer S +0x26 i16 -= ticks60. Above 0: pitch / roll response from B +0x30 (F +0x11 picks the
 *   side) with S +0x60 / +0x62 into B +0x12, B +0x0A halved; then F +0x10 set: cleared when i16
 *   B +0x0A < i32 S +0xB0; else flame again (F2 +0x44 from 0x00450e24(B + 0x20 with z - 0x28, 0)),
 *   S +0x22 = 0 and, below 3 explosions (S +0x16), on every 4th frame (0x004931f4) and rand() & 1:
 *   S +0x16 += 1, F +0x13 = 1, an explosion (0x004508f4 / 0x00450984 by rand() & 1) and smoke
 *   0x0045109c at B + 0x20 with z + 0x78.
 *   At or below 0: not near or below -1200: (AI only) the car block +0x32 = 1, damage
 *   0x004210e4(id of 0x0045b0b8(type), 15, 0, 2, stack 0), explosion 0x00450984 and smoke at
 *   B + 0x20, z + 0x78 (smoke z + i16 S +0x42), the flame's +0x0C = 0, F +0x1A = 0. Below -960:
 *   (AI, F +0x26 clear) 0x004508f4 at z + 0x18 and smoke, F +0x26 = 1. Else the wreck settles:
 *   B +0x08 / +0x0C / +0x10 / +0x12 / +0x14 i16 = 0, B +0x0A -= B +0x0A / 16; game mode 7 with
 *   the human car key 0: below -300 the car restarts (0x00437850(car, type, 1), 0x004735b4(0,
 *   u32 car +0x894), 0x00493748, 0x0049364c); otherwise the roll B +0x30 eases (0x0048ca54(a, t, 1))
 *   toward 0x400 / 0x7F4 / 0 by range.
 *
 * ORIGINAL BUGs kept: the smoke burst draws three random offsets and then overwrites them (the
 * rand() calls stay); the code for rolls in (-0x78E, -0x471] can never run (that range returns
 * at the <= -0x38E test), and rolls in (-0x38E, -0x200] ease toward +0x400; the "7 explosions"
 * branch and divisor 12 are dead (the limit is the constant 3).
 * DECOMPILER TRAP: the damage call's stack argument (0) is dropped by Ghidra.
 *
 * Verified against the original under emulation (tools/emu/check_gp2_wreck.sh).
 */
#ifndef TM2_GP2_WRECK_H
#define TM2_GP2_WRECK_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    void* user;
    uint32_t (*target_car)(void* user, int32_t key, uint8_t* human);     /* 0x0045afc8 */
    uint32_t (*flame)(void* user, uint32_t pos, int32_t v);              /* 0x00450e24 */
    int32_t (*rand)(void* user);                                         /* 0x004899c2 */
    uint32_t (*jitter)(void* user, uint32_t pos, int32_t a, int32_t b, int32_t c); /* 0x0044a40c */
    void (*smoke_small)(void* user, uint32_t pos, int32_t z);            /* 0x00451058 */
    void (*smoke_a2)(void* user, uint32_t pos, int32_t z);               /* 0x00451014 */
    uint32_t (*frame)(void* user);                                       /* 0x004931f4 */
    void (*expl12)(void* user, const int32_t pos[3], int32_t z);         /* 0x004508f4 */
    void (*expl16)(void* user, const int32_t pos[3], int32_t z);         /* 0x00450984 */
    void (*smoke)(void* user, const int32_t pos[3], int32_t z);          /* 0x0045109c */
    uint32_t (*object_from_owner)(void* user, int32_t key);              /* 0x0045b0b8 */
    void (*damage)(void* user, int32_t id, int32_t amount, uint32_t pos, int32_t ecx, int32_t owner); /* 0x004210e4 */
    int32_t (*game_mode)(void* user);                                    /* 0x0045c338 */
    void (*car_setup)(void* user, uint32_t car, int32_t type, int32_t v); /* 0x00437850 */
    int32_t (*net_msg)(void* user, int32_t a, uint32_t b);               /* 0x004735b4 */
    void (*net_send)(void* user, int32_t msg);                           /* 0x00493748 */
    void (*net_flush)(void* user);                                       /* 0x0049364c */
    int32_t (*approach)(void* user, int32_t a, int32_t target, int32_t rate); /* 0x0048ca54 */
} Gp2WreckEnv;

void gp2_car_wreck_update(Gp2WreckEnv* env, uint32_t car, uint8_t human);

#endif /* TM2_GP2_WRECK_H */
