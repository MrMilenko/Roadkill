/*
 * warps.h: arena warps (teleporters), ported from TM2.EXE 0x0045450c (PS1 decomp: worldWarpsUpdate,
 * src/semantic/world_collapse_warps.c).
 *
 * Three warp records of 0x28 bytes at 0x00502ea0:
 *   +0x00 u8 active   +0x01 u8 human car (else AI car)   +0x04 i16 frame counter
 *   +0x14 i32[3] destination   +0x20 u32 car (handle)   +0x24 u32 warp effect object (handle)
 * Car fields: human car block (0x898): body +0x6C4, probe block +0x58C, stats +0x138, camera
 * position +0x14 (i32[3]) and angles +0x0C (i16[3]); AI car block (0x880): body +0x708, probes
 * +0x5D0, stats +0x4EC, route fields +0x52C i16 and +0x5BC i32.
 * Body: +0x08/+0x0A/+0x0C i16 velocity, +0x20 i32[3] position, angles i16 at +0x2C/+0x30/+0x34.
 * Stats +0x9C i32: terrain probes on. Object: +0x08 i32 visible, +0xE8 i32[3] position.
 *
 * Per active record, the counter goes up by one; below 10 the car's velocity is held at 0; at 10
 * the car is put on the destination (probes cleared, zone from grp_find_zone, floor, one driving
 * model step), a human car's camera jumps there, an AI car gets a route exit that depends on the
 * event (arena) and the warp index, and the effect moves there; at 11 the probes come back on; at
 * 40 the effect is hidden and the record ends.
 *
 * The AI exit choice is an inlined copy of 0x004548a0 (aiWarpSetExitNode): event 1 (any warp)
 * 0x35 / 0x4A; 3: 0x31 / 0x56, 0x58 / 0x57; 4: 0x37 / 0x52, 0x3B / 0x3D; 5: 2 / 0, 0x3C / 0x3B;
 * 6: 0x45 / 0x43, 0x33 / 0x34; 7: 9 / 0x2E, 0x26 / 0x2F (warp 0 / others); 8: 0x51 / 0x4F,
 * 0xB / 0, 0x86 / 0x42 (warps 0, 1, 2); then aiPlaceOnRouteNode 0x00432940(car). Other events
 * (0, 2, above 8): nothing.
 *
 * Verified against the original under emulation (tools/emu/check_world_warps.sh).
 */
#ifndef TM2_WORLD_WARPS_H
#define TM2_WORLD_WARPS_H

#include <stdint.h>

#define WORLD_WARPS 3
#define WORLD_WARP_REC 0x28

typedef struct {
    uint8_t* records;           /* 0x00502ea0 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    void (*probes_clear)(void* user, uint8_t* probes);                   /* 0x004300e8 */
    int32_t (*find_zone)(void* user, int32_t x, int32_t y, int32_t z);   /* 0x0042cba8 */
    void (*probes_zone)(void* user, uint8_t* probes, int32_t zone);      /* 0x0043033c */
    void (*probes_floor)(void* user, uint8_t* probes, int32_t z);        /* 0x00430368 */
    void (*drive)(void* user, uint8_t* car, int32_t human);              /* 0x0042d470 */
    uint8_t (*event_type)(void* user);                                   /* 0x00474508 */
    void (*ai_place)(void* user, uint8_t* car);                          /* 0x00432940 */
    void* user;
} WorldWarpsEnv;

void world_warps_update(const WorldWarpsEnv* env);

#endif /* TM2_WORLD_WARPS_H */
