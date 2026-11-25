/*
 * track.h: track vehicles (the two rail cars that run round an arena and carry cars as moving
 * floors), ported from TM2.EXE (PS1 decomp: trackVehicle*, src/semantic/track_vehicle*.c).
 *
 * Two records of 0x38 bytes; the table pointer is at 0x00502f18 (it points at 0x00502988):
 *   +0x00 i32 route state 0..7   +0x04 i32 speed   +0x08 i32 heading change this frame
 *   +0x0C i32 needs placing (0x00455ef0)   +0x10 i32[3] front probe   +0x1C i32[3] back probe
 *   +0x28 i32[3] movement this frame   +0x34 u32 object (handle)
 * Object: +0x08 i32 visible, +0xB0 i32[9] matrix, +0xE0 i16[3] angles (pitch, roll, heading),
 * +0xE8 i32[3] position.
 * Each record owns one of the RMP "extra" tiles at the end of the tile table (tile count - 1 - i):
 * the deck surface that cars on it drive over (see terrain_probe.h).
 *
 * Verified against the original under emulation (tools/emu/check_world_track.sh).
 */
#ifndef TM2_WORLD_TRACK_H
#define TM2_WORLD_TRACK_H

#include <stdint.h>

#define WORLD_TRACK_REC 0x38

typedef struct {
    uint8_t* table;             /* *(0x00502f18) */
    uint8_t* tiles;             /* 0x004fb17c, 28 bytes per tile */
    int32_t tile_count;         /* 0x004fb174 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    int32_t (*ground)(void* user, const int32_t p[3]);      /* grp_ground_height 0x0042c2d4 */
    int32_t (*car_count)(void* user);                       /* 0x0045afb4 */
    const uint8_t* (*car)(void* user, int32_t i);           /* 0x0045afbc */
    void (*place)(void* user, int32_t i);                   /* 0x00455ef0 */
    void (*sound)(void* user, int32_t id, uint8_t* pos);    /* 0x00472e50(id, object + 0xE8) */
    void* user;
} WorldTrackEnv;

/* 0x0043b440 [trackVehicleSurfaceSet]: deck tile of record i: +0x08 i16 sin(angle), +0x0A i16
 * cos(angle), +0x14 / +0x18 i32 p x, y, +0x0C / +0x0E i16 p z (low half), +0x10 / +0x12 i16 h. */
void world_track_surface_set(const WorldTrackEnv* env, int32_t i, const int32_t p[3], int32_t h, int32_t angle);

/* 0x00456cc4 [trackVehicleGround]: puts the front and back probes 0xC0 ahead of and behind the
 * object, sets its z to the mean ground height + 0x30 and its pitch from the height difference
 * (atan2(d, isqrt(0x24000 - d * d))), rebuilds its matrix; when a player car is within 800
 * (|dx| + |dy|) and the record does not need placing, sets the deck tile at the point
 * (-0x68, -0xF0) in object space, at the back height + 0x98, with the front height + 0x98 and
 * the object's heading. */
void world_track_vehicle_ground(const WorldTrackEnv* env, int32_t i);

/* 0x0045606c [worldTrackVehiclesUpdate]: while a player car is on a section with moving surfaces
 * (world_section_has_moving_surfaces of its +0xC6), hidden track cars that do not wait to be
 * placed are shown again, moved to x 0x1900 (state 2) / 0xC80 when x is in (0xC80, 0x1900]
 * (ORIGINAL BUG: the matching snap for the other end is unreachable); otherwise cars waiting to
 * be placed are placed (+0x0C cleared) and visible ones outside x (0xC80, 0x1900] and
 * [-0x1900, -0xC80] are hidden. Then each car with an object drives its route: straights at a
 * fixed x or y and semicircles (radius 0xC20 / 0xCE0) round (+-0x2120, -800 / -4000), speed
 * 0x5F0 * ticks60 / 100 along its matrix; ground follow, per frame movement and heading change,
 * and a sound at its position (ids 0x1FA, 0x1FB). The second car's first semicircle also wraps
 * its heading (math_angle_normalize); the others do not. */
void world_track_vehicles_update(const WorldTrackEnv* env);

#endif /* TM2_WORLD_TRACK_H */
