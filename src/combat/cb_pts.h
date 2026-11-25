/*
 * cb_pts.h: per arena route node loaders (PTS files) of TM2.EXE 0x0043bd60..0x0043cd88
 * (PS1 decomp: the PTS half of the terrain loaders; they sit next to the RMP / GRP loaders ported
 * in src/world/terrain_load.c, same scheme).
 *
 *   arena  loader      node count  start sections (i16 list, in order)                          setup
 *   FWY    0x0043be70  0x4F   6 10 15 21 24 28 39 50 55 64                                      inline
 *   DISH   0x0043bffc  0x21   5 6 7 12 17 19 21 23 25 27                                        inline
 *   ICE    0x0043c1c4  0x4F   65 63 61 59 57 54 52 51 49 40 36 31 19 14 8                        0x0043c0ec
 *   LAVA   0x0043c2d0  0x5E   2 4 6 9 11 14 37 44 55 57 82                                      inline
 *   PARIS  0x0043c470  0x76   0 13 15 31 36 44 58 62 72 91 96                                   inline
 *   HKONG  0x0043c634  0x8C   2 4 9 11 14 20 22 28 36 49 53 13 23 48                            0x0043c568
 *   ROOF   0x0043c734  0x65   1 17 27 37 45 55 63 67 72 78                                      inline
 *   DEN    0x0043c8f8  0x3A   0 2 13 15 17 19 21 25 27 30 34 43 45 46 47                        0x0043c824
 *   SWAMP  0x0043c9dc  8      0..7                                                               inline
 *   CYB    0x0043cb48  10     0..9                                                               inline
 *   SROOF  0x0043ccc0  9      0..8                                                               inline
 *
 * Each loader: node count i32 0x004fe1b0 = count; the section setup (inline, or the separate
 * function for ICE / HKONG / DEN, called after the count is stored): the 15 "used" bytes
 * 0x004fe1d2..0x004fe1e0 = 0, the list count byte 0x004fe1e1 = n, the first n i16 of the list
 * 0x004fe1b4 (entries past n keep their values); then "TERRAIN\<ARENA>.PTS" through file_build_path
 * 0x00499408 (kind 2, 300 byte stack buffer), file_load_resource 0x00498d18 into the shared load
 * buffer (*0x00a3a5c8) and 0xD20 bytes copied from it to the node table 0x004fe1e4 (140 nodes of
 * 24 bytes). The read's result is not checked.
 *   0x0043bd60: only the 15 used bytes = 0 (the PS1 startNodesClearUsed).
 *
 * The list is the section list that worldSectionPolyReset (0x0043bd74) removes from.
 *
 * Verified against the original under emulation (tools/emu/check_combat_pts.sh).
 */
#ifndef TM2_CB_PTS_H
#define TM2_CB_PTS_H

#include <stdint.h>

#define CB_PTS_SIZE 0xD20

enum {
    CB_ARENA_FWY, CB_ARENA_DISH, CB_ARENA_ICE, CB_ARENA_LAVA, CB_ARENA_PARIS, CB_ARENA_HKONG,
    CB_ARENA_ROOF, CB_ARENA_DEN, CB_ARENA_SWAMP, CB_ARENA_CYB, CB_ARENA_SROOF, CB_ARENA_COUNT
};

typedef struct {
    int32_t* node_count;        /* 0x004fe1b0 */
    uint8_t* sections;          /* 0x004fe1b4: i16[15] list, +0x1E u8[15] used, +0x2D u8 list count */
    uint8_t* nodes;             /* 0x004fe1e4, CB_PTS_SIZE bytes */
    uint8_t* load_buffer;       /* *(0x00a3a5c8) */
    /* file_build_path 0x00499408(out, name, kind): out has room for 300 bytes */
    void (*build_path)(void* user, char* out, const char* name, int32_t kind);
    /* file_load_resource 0x00498d18(buffer, path) */
    void (*load_resource)(void* user, uint8_t* buffer, const char* path);
    void* user;
} CbPtsEnv;

/* 0x0043bd60 */
void cb_pts_used_clear(const CbPtsEnv* env);
/* the section setup of an arena (0x0043c0ec ICE, 0x0043c568 HKONG, 0x0043c824 DEN; inline in the
 * other loaders). Returns -1 for an unknown arena, else 0. */
int cb_pts_sections_setup(int arena, const CbPtsEnv* env);
/* the PTS loader of an arena. Returns -1 for an unknown arena, else 0. */
int cb_pts_load(int arena, const CbPtsEnv* env);

#endif /* TM2_CB_PTS_H */
