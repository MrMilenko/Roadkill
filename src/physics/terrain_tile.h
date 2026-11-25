/*
 * terrain_tile.h: RMP terrain tile tests, ported from TM2.EXE.
 *
 * RMP files hold 225 tiles x 28 bytes, copied raw to 0x004fb17c by the per level loaders
 * (0x0043b4c0..0x0043b8d0). Tile layout as these functions read it:
 *   +0  u8  shape: 0 rectangle, 1 triangle, 2 ring sector
 *   +1  u8  height mode: 0 = z0 plus one slope, else bilinear z0..z3
 *   +4  i16 rect/tri: extent u        ring: inner radius
 *   +6  i16 rect/tri: extent v        ring: outer radius
 *   +8  i16 rect/tri: sin(rotation)   ring: start angle
 *   +10 i16 rect/tri: cos(rotation)   ring: end angle
 *   +12 i16 z0, +14 z1, +16 z2, +18 z3
 *   +20 i32 world x, +24 i32 world y
 * Verified against the original under emulation (tools/emu/check_terrain.sh).
 */
#ifndef TM2_TERRAIN_TILE_H
#define TM2_TERRAIN_TILE_H

#include <stdint.h>

#define RMP_TILE_SIZE 28
#define RMP_TILE_COUNT 225

/* 0x0042bdd4: surface height on a rectangle/triangle tile at local (u, v). Returns z0 if an extent
 * is 0. Divides by extent v in the sloped case (the original faults if it is 0; port returns z0). */
int32_t rmp_height_rect(const uint8_t* tile, int32_t u, int32_t v);

/* 0x0042c098: surface height on a ring sector tile at radius r, angle a. The original faults on a
 * zero angle span or zero radial span; the port returns z0 there. */
int32_t rmp_height_ring(const uint8_t* tile, int32_t r, int32_t a);

/* Integer square root exactly as inlined in 0x0042b9a8 (bit by bit, signed compares). */
uint32_t rmp_isqrt(int32_t v);

/* 0x0042b9a8: test one tile against a body.
 *   body  (original param_1, int16 array): +0 i16 current tile index, +8 i32 step height,
 *         +12 i32 / +16 i32 second probe x/y offset, +20 i32 second probe z offset
 *   state (original param_2): +0 u8 hit, +5 u8 have height, +12 i16 hit tile, +36 i16 height,
 *         +60/+64/+68 i32 x/y/z
 * Updates state like the original and returns 1 on a hit. Field meanings are from this function
 * only; the callers are not verified yet. */
int rmp_tile_test(const uint8_t* body, uint8_t* state, const uint8_t* tile, int32_t tile_index);

#endif /* TM2_TERRAIN_TILE_H */
