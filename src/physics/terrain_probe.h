/*
 * terrain_probe.h: per probe terrain update, ported from TM2.EXE.
 *
 * Each vehicle has four terrain probes (the caller 0x0042a020 runs one update per probe, the state
 * blocks are 0x48 bytes apart). An update tests the probe against the RMP tiles of its zone, then
 * against the extra tiles some arenas add at the end of the table, and keeps the probe's on/off
 * ground state. Verified against the original under emulation (tools/emu/check_terrain_probe.sh).
 *
 * Probe state fields used here (byte offsets; meanings from these functions only):
 *   +0x00 u8  on terrain (set by the tile test)
 *   +0x01 u8  slope latch: set when the probe leaves the ground or changes tile
 *   +0x03 u8  forced contact
 *   +0x05 u8  have height (tile test)
 *   +0x08 u8  hit one of the extra tiles
 *   +0x0A i16 current tile           +0x0C i16 tile hit by the last test
 *   +0x0E i16 extra tile slot (count - index - 1)
 *   +0x10 i16 zone (index into the per level first/last tile tables)
 *   +0x12 i16 cleared with the latch  +0x14 i16 vertical rate (height change * 32 / ticks60)
 *   +0x16 i16 copy of +0x44          +0x1C i16 compared with +0x24 (+10 margin)
 *   +0x24 i16 terrain height (tile test) +0x26 i16 fallback height when nothing was hit
 *   +0x28 i16 cleared on most exits  +0x2A i16 copy of +0x44
 *   +0x3C i32 x, +0x40 i32 y, +0x44 i32 z
 */
#ifndef TM2_TERRAIN_PROBE_H
#define TM2_TERRAIN_PROBE_H

#include <stdint.h>

#define RMP_ZONE_COUNT 35

/* The level terrain globals these functions read. */
typedef struct {
    const uint8_t* tiles;       /* 0x004fb17c, RMP_TILE_SIZE bytes per tile */
    int32_t tile_count;         /* 0x004fb174, extra tiles are indexed back from here */
    const uint8_t* zone_first;  /* 0x004fe162 = GRP file + 0x174A, first tile of each zone */
    const uint8_t* zone_last;   /* 0x004fe185 = GRP file + 0x176D, last tile (inclusive);
                                   empty zones on the disc are first 1, last 0 */
    uint8_t event_type;         /* 0x00624ec7 (old name g_tournament_flag, unverified) */
} RmpWorld;

/* 0x0045c2b4: number of extra tiles to test in a zone: 2 in zones 4..11, 14, 15, 17, 18 when
 * event_type is 8, else 0. */
int32_t rmp_extra_tiles(uint8_t event_type, int32_t zone);

/* 0x0042b5b0: sets state +0x14 to the height difference between the probe and the point offset by
 * the body's second probe vector, on tile `tile`, times 32 / ticks60. */
void rmp_probe_slope(const RmpWorld* w, const uint8_t* body, uint8_t* state, int32_t tile);

/* 0x0042b1f0: one probe update. Returns 1 if the probe touches terrain this frame. */
int rmp_probe_update(const RmpWorld* w, const uint8_t* body, uint8_t* state);

/* 0x0042c17c: integrates the probe height +0x16 with rate +0x14 while the latch +0x01 is set
 * (+0x12 counts frames in the air and adds a quadratic pull into +0x2C), and drops the latch on
 * touch down. Without the latch it snaps +0x16 to the terrain height +0x24. on_terrain is the
 * rmp_probe_update result. */
void rmp_probe_integrate(uint8_t* state, int on_terrain);

/* 0x0042b8a4: per probe follow up after rmp_probe_update (caller 0x0042a020 passes the vehicle
 * speed and a car stat as span). Caps the rate +0x14 by speed, then integrates. */
void rmp_probe_finish(int32_t speed, int32_t span, uint8_t* state, int on_terrain);

#endif /* TM2_TERRAIN_PROBE_H */
