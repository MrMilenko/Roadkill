/*
 * grp.h: GRP terrain zones, ported from TM2.EXE.
 *
 * A level's GRP file (0x1794 bytes) is loaded raw to 0x004fca18. It splits the arena into up to
 * 35 zones; each zone names a range of RMP tiles (see terrain_probe.h). Offsets in the file:
 *   +0x000C  zone origin: i32 x, y, z, 12 bytes per zone
 *   +0x01B0  i32 extent u        +0x023C  i32 extent v
 *   +0x02C8  i16 sin(rotation)   +0x030E  i16 cos(rotation)     (as the old project's notes had)
 *   +0x11D2  u8 rotated          +0x11F5  u8 solid floor (ground search stops here)
 *   +0x123B  u8 shape: 0 rectangle, 1 cut corner octagon, 2 triangle
 *   +0x125E  u8 surface type (0x004fdc76, read by the vehicle code)
 *   +0x174A  u8 first tile, +0x176D u8 last tile (terrain_probe.h)
 *   +0x1790  u8 zone count
 * Verified against the original under emulation (tools/emu/check_grp.sh).
 */
#ifndef TM2_GRP_H
#define TM2_GRP_H

#include <stdint.h>

#include "terrain_probe.h"

#define GRP_SIZE 0x1794
#define GRP_MAX_ZONES 35

/* 0x0042cba8: zone containing (x, y) whose floor is below z + 80; else the last zone containing
 * the point; else the last zone. Shapes 1 and 2 divide by the extents: the original faults on a
 * zero extent there, the port treats the point as outside. */
int32_t grp_find_zone(const uint8_t* grp, int32_t x, int32_t y, int32_t z);

/* 0x0042c2d4: terrain height under p (x, y, z). Tests the RMP tiles of the point's zone with a
 * fresh probe (step height 120); without a hit it lowers z by 40 and retries until a hit, a solid
 * floor zone or z < 0. Returns the last height found (the zone's floor z without a hit). */
int32_t grp_ground_height(const RmpWorld* w, const uint8_t* grp, const int32_t p[3]);

#endif /* TM2_GRP_H */
