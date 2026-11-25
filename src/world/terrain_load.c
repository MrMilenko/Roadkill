/* terrain_load.c: see terrain_load.h. */
#include "terrain_load.h"

#include <string.h>

typedef struct {
    const char* rmp_name;
    const char* grp_name;
    int32_t tile_count, extra, grp_param;
} TerrainPreset;

static const TerrainPreset presets[WORLD_ARENA_COUNT] = {
    {"TERRAIN\\FWY.RMP", "TERRAIN\\FWY.GRP", 0x80, 0, 0x13},
    {"TERRAIN\\DISH.RMP", "TERRAIN\\DISH.GRP", 0x38, 0, 0x0C},
    {"TERRAIN\\ICE.RMP", "TERRAIN\\ICE.GRP", 0x8D, 0, 0x23},
    {"TERRAIN\\LAVA.RMP", "TERRAIN\\LAVA.GRP", 0xCE, 0, 0x17},
    {"TERRAIN\\PARIS.RMP", "TERRAIN\\PARIS.GRP", 0x43, 0, 0x16},
    {"TERRAIN\\HKONG.RMP", "TERRAIN\\HKONG.GRP", 0xC8, 2, 0x19},
    {"TERRAIN\\ROOF.RMP", "TERRAIN\\ROOF.GRP", 0x1A, 0, 0x18},
    {"TERRAIN\\DEN.RMP", "TERRAIN\\DEN.GRP", 0x08, 0, 0x07},
    {"TERRAIN\\SWAMP.RMP", "TERRAIN\\SWAMP.GRP", 0x32, 0, 0x02},
    {"TERRAIN\\CYB.RMP", "TERRAIN\\CYB.GRP", 0x2E, 0, 0x02},
    {"TERRAIN\\SROOF.RMP", "TERRAIN\\SROOF.GRP", 0x0A, 0, 0x0E},
};

const char* world_terrain_name(int arena, int grp)
{
    if (arena < 0 || arena >= WORLD_ARENA_COUNT)
        return 0;
    return grp ? presets[arena].grp_name : presets[arena].rmp_name;
}

static void load_copy(const WorldTerrainEnv* env, const char* name, uint8_t* dst, size_t size)
{
    char path[300];
    env->build_path(env->user, path, name, 2);
    env->load_resource(env->user, env->load_buffer, path);
    memcpy(dst, env->load_buffer, size);
}

int world_terrain_load_rmp(int arena, const WorldTerrainEnv* env)
{
    if (arena < 0 || arena >= WORLD_ARENA_COUNT)
        return -1;
    *env->rmp_tile_count = presets[arena].tile_count;
    *env->rmp_extra = presets[arena].extra;
    load_copy(env, presets[arena].rmp_name, env->rmp, WORLD_RMP_SIZE);
    return 0;
}

int world_terrain_load_grp(int arena, const WorldTerrainEnv* env)
{
    if (arena < 0 || arena >= WORLD_ARENA_COUNT)
        return -1;
    *env->grp_param = presets[arena].grp_param;
    load_copy(env, presets[arena].grp_name, env->grp, WORLD_GRP_SIZE);
    return 0;
}
