/*
 * terrain_load.h: per arena terrain loaders, ported from TM2.EXE (PS1 decomp: terrainLoadRmp* /
 * terrainLoadGrp*, src/semantic/terrain_load*.c). One function per arena in the original:
 *
 *   arena   RMP loader   tile count (0x004fb174)  extra (0x004fb178)   GRP loader   0x004fe1ac
 *   FWY     0x0043b4c0   0x80                     0                    0x0043b940   0x13
 *   DISH    0x0043b528   0x38                     0                    0x0043b9a0   0x0C
 *   ICE     0x0043b590   0x8D                     0                    0x0043ba00   0x23
 *   LAVA    0x0043b5f8   0xCE                     0                    0x0043ba60   0x17
 *   PARIS   0x0043b660   0x43                     0                    0x0043bac0   0x16
 *   HKONG   0x0043b6c8   0xC8                     2                    0x0043bb20   0x19
 *   ROOF    0x0043b730   0x1A                     0                    0x0043bb80   0x18
 *   DEN     0x0043b798   0x08                     0                    0x0043bbe0   0x07
 *   SWAMP   0x0043b800   0x32                     0                    0x0043bc40   0x02
 *   CYB     0x0043b868   0x2E                     0                    0x0043bc9c   0x02
 *   SROOF   0x0043b8d0   0x0A                     0                    0x0043bcf8   0x0E
 *
 * Each one stores its constants, builds the path "TERRAIN\<ARENA>.RMP" / ".GRP" with
 * file_build_path 0x00499408 (kind 2) into a 300 byte stack buffer, reads the file with
 * file_load_resource 0x00498d18 into the shared load buffer (pointer at 0x00a3a5c8) and copies a
 * fixed size out of it: 0x189C bytes to the RMP tile table 0x004fb17c, 0x1794 bytes to the GRP
 * block 0x004fca18. The tile count is a constant in the code, not read from the file. The file
 * read's result is not checked.
 *
 * Verified against the original under emulation (tools/emu/check_world_terrain.sh).
 */
#ifndef TM2_WORLD_TERRAIN_LOAD_H
#define TM2_WORLD_TERRAIN_LOAD_H

#include <stdint.h>

#define WORLD_RMP_SIZE 0x189C
#define WORLD_GRP_SIZE 0x1794

enum {
    WORLD_ARENA_FWY, WORLD_ARENA_DISH, WORLD_ARENA_ICE, WORLD_ARENA_LAVA, WORLD_ARENA_PARIS,
    WORLD_ARENA_HKONG, WORLD_ARENA_ROOF, WORLD_ARENA_DEN, WORLD_ARENA_SWAMP, WORLD_ARENA_CYB,
    WORLD_ARENA_SROOF, WORLD_ARENA_COUNT
};

typedef struct {
    int32_t* rmp_tile_count;    /* 0x004fb174 */
    int32_t* rmp_extra;         /* 0x004fb178 */
    uint8_t* rmp;               /* 0x004fb17c, WORLD_RMP_SIZE bytes */
    int32_t* grp_param;         /* 0x004fe1ac */
    uint8_t* grp;               /* 0x004fca18, WORLD_GRP_SIZE bytes */
    uint8_t* load_buffer;       /* *(0x00a3a5c8) */
    /* file_build_path 0x00499408(out, name, kind): out has room for 300 bytes */
    void (*build_path)(void* user, char* out, const char* name, int32_t kind);
    /* file_load_resource 0x00498d18(buffer, path) */
    void (*load_resource)(void* user, uint8_t* buffer, const char* path);
    void* user;
} WorldTerrainEnv;

/* The RMP loader of `arena` (WORLD_ARENA_*). Returns -1 for an unknown arena, else 0. */
int world_terrain_load_rmp(int arena, const WorldTerrainEnv* env);
/* The GRP loader of `arena`. Returns -1 for an unknown arena, else 0. */
int world_terrain_load_grp(int arena, const WorldTerrainEnv* env);

/* The original's file name for an arena's terrain file ("TERRAIN\FWY.RMP", ...), or 0. */
const char* world_terrain_name(int arena, int grp);

#endif /* TM2_WORLD_TERRAIN_LOAD_H */
