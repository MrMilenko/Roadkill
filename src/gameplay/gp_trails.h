/*
 * gp_trails.h: missile trail setup, ported from TM2.EXE 0x004133d0 (PS1 pair: missileTrailsInit).
 *
 * For each of the 16 projectile slots (0x4BC bytes at 0x004f11dc, layout in src/weapons/projectile.h):
 *   18 trail quads Q = slot + 0x15C + 0x30 j (j = 0..17): +0x00 u8 = 4, +0x01 1, +0x02 0x0C, +0x03 9,
 *     +0x04..+0x0A i16 0, 1, 2, 3, +0x10 u32 0x10842000, +0x18..+0x1A u8 0x80, +0x1B u8 0x2E
 *   then the trail model header at +0xF4: +0xF4 u8 0, +0xF6 i16 0, +0xF8 u32 vertices (slot + 0x48,
 *     game address), +0xFC 0, +0x100 u32 polygons (slot + 0x15C), +0x104 i32 18, +0x108 i16 0,
 *     +0x10A i16 1, +0x10C u32 0 then byte +0x10D |= 0x20, +0x110 / +0x114 / +0x118 0, +0x11C 0x7FFF;
 *     a new render object (0x00492d90) in +0xF0 with model +0x15C = slot + 0xF4, hidden (+0x08 = 0),
 *     position 0 (+0xE8..+0xF0), registered with 0x00492c4c(obj).
 * Pass 1 runs over all slots before pass 2.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_trails.sh).
 */
#ifndef TM2_GP_TRAILS_H
#define TM2_GP_TRAILS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;            /* render objects */
    uint8_t* slots;             /* 0x004f11dc */
    uint32_t slots_addr;        /* 0x004f11dc */
    void* user;
    uint32_t (*obj_new)(void* user);               /* 0x00492d90: game address */
    void (*obj_add)(void* user, uint32_t obj);     /* 0x00492c4c */
} GpTrailEnv;

void gp_missile_trails_init(GpTrailEnv* env);

#endif /* TM2_GP_TRAILS_H */
