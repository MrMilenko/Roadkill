/*
 * gp_nodes.h: level zone tiles and waypoint areas changed at run time (moving floors, closed
 * roads), ported from TM2.EXE.
 *
 *   0x0042f7d0 gp_zone_tiles_raise  adds d to the four corner heights of every RMP tile of zone z
 *   0x0042f850 gp_zone_tiles_set    sets the four corner heights of every tile of zone z to h
 *   0x0042f520 gp_area_nodes_remove every waypoint node of area a is cut out of the graph: removed
 *                                   from every node's neighbour list, then from the spawn node list
 *                                   (0x0043bd74, src/world/world_misc.c world_section_list_remove)
 *
 * Tiles: 0x1C bytes from 0x004fb17c, +0x02 i16 zone, +0x0C..+0x12 i16 corner heights (16 bit wrap).
 *   Zone z's tiles are first .. last (u8 at GRP + 0x174A + z / + 0x176D + z, 0x004fe162 / 0x004fe185);
 *   only tiles whose zone field equals z change.
 * Waypoints (one block from 0x004fe1b0): +0x00 i32 node count, +0x04 spawn list (i16 entries, count
 *   byte at +0x31), +0x34 nodes of 24 bytes: +0x0A i16 neighbours[4], +0x16 i8 neighbour count,
 *   +0x17 i8 area.
 * ORIGINAL BUG (kept): removing a neighbour moves the following ones down (count - k moves, reading
 *   one i16 past the used ones, up to the bytes at +0x12) and the scan goes on at k + 1, so of two
 *   adjacent equal neighbours only the first goes. The counts are reread after every store.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_nodes.sh).
 */
#ifndef TM2_GP_NODES_H
#define TM2_GP_NODES_H

#include <stdint.h>

typedef struct {
    uint8_t* tiles;        /* 0x004fb17c */
    const uint8_t* grp;    /* 0x004fca18 */
    uint8_t* way;          /* 0x004fe1b0 .. (count, spawn list, nodes) */
} GpNodesEnv;

void gp_zone_tiles_raise(GpNodesEnv* env, int32_t zone, int32_t d);
void gp_zone_tiles_set(GpNodesEnv* env, int32_t zone, int32_t h);
void gp_area_nodes_remove(GpNodesEnv* env, int32_t area);

#endif /* TM2_GP_NODES_H */
