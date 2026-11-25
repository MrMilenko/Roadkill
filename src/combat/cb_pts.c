/* cb_pts.c: see cb_pts.h */
#include "combat/cb_pts.h"

#include <string.h>

typedef struct {
    const char* name;
    int32_t node_count;
    uint8_t n;
    uint8_t list[15];
} PtsPreset;

static const PtsPreset presets[CB_ARENA_COUNT] = {
    {"TERRAIN\\FWY.PTS", 0x4F, 10, {6, 10, 15, 21, 24, 28, 39, 50, 55, 64}},
    {"TERRAIN\\DISH.PTS", 0x21, 10, {5, 6, 7, 12, 17, 19, 21, 23, 25, 27}},
    {"TERRAIN\\ICE.PTS", 0x4F, 15, {65, 63, 61, 59, 57, 54, 52, 51, 49, 40, 36, 31, 19, 14, 8}},
    {"TERRAIN\\LAVA.PTS", 0x5E, 11, {2, 4, 6, 9, 11, 14, 37, 44, 55, 57, 82}},
    {"TERRAIN\\PARIS.PTS", 0x76, 11, {0, 13, 15, 31, 36, 44, 58, 62, 72, 91, 96}},
    {"TERRAIN\\HKONG.PTS", 0x8C, 14, {2, 4, 9, 11, 14, 20, 22, 28, 36, 49, 53, 13, 23, 48}},
    {"TERRAIN\\ROOF.PTS", 0x65, 10, {1, 17, 27, 37, 45, 55, 63, 67, 72, 78}},
    {"TERRAIN\\DEN.PTS", 0x3A, 15, {0, 2, 13, 15, 17, 19, 21, 25, 27, 30, 34, 43, 45, 46, 47}},
    {"TERRAIN\\SWAMP.PTS", 8, 8, {0, 1, 2, 3, 4, 5, 6, 7}},
    {"TERRAIN\\CYB.PTS", 10, 10, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}},
    {"TERRAIN\\SROOF.PTS", 9, 9, {0, 1, 2, 3, 4, 5, 6, 7, 8}},
};

void cb_pts_used_clear(const CbPtsEnv* env) {
    memset(env->sections + 0x1E, 0, 15);
}

int cb_pts_sections_setup(int arena, const CbPtsEnv* env) {
    if (arena < 0 || arena >= CB_ARENA_COUNT)
        return -1;
    const PtsPreset* p = &presets[arena];
    cb_pts_used_clear(env);
    env->sections[0x2D] = p->n;
    for (int i = 0; i < p->n; i++) {
        env->sections[2 * i] = p->list[i];
        env->sections[2 * i + 1] = 0;
    }
    return 0;
}

int cb_pts_load(int arena, const CbPtsEnv* env) {
    if (arena < 0 || arena >= CB_ARENA_COUNT)
        return -1;
    *env->node_count = presets[arena].node_count;
    cb_pts_sections_setup(arena, env);
    char path[300];
    env->build_path(env->user, path, presets[arena].name, 2);
    env->load_resource(env->user, env->load_buffer, path);
    memcpy(env->nodes, env->load_buffer, CB_PTS_SIZE);
    return 0;
}
