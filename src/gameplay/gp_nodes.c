/* gp_nodes.c: see gp_nodes.h */
#include "gameplay/gp_nodes.h"

#include "weapons/wmem.h"
#include "world/world_misc.h"

static void zone_tiles(GpNodesEnv* env, int32_t zone, int32_t v, int add) {
    uint32_t last = env->grp[0x176D + zone];
    for (uint32_t t = env->grp[0x174A + zone]; t <= last; t++) {
        uint8_t* p = env->tiles + t * 0x1C;
        if (w_i16(p, 0x02) != zone)
            continue;
        for (int k = 0; k < 4; k++)
            w_s16(p, 0x0C + 2 * k, add ? (uint32_t)(w_u16(p, 0x0C + 2 * k) + v) : (uint32_t)v);
    }
}

void gp_zone_tiles_raise(GpNodesEnv* env, int32_t zone, int32_t d) { zone_tiles(env, zone, d, 1); }
void gp_zone_tiles_set(GpNodesEnv* env, int32_t zone, int32_t h) { zone_tiles(env, zone, h, 0); }

void gp_area_nodes_remove(GpNodesEnv* env, int32_t area) {
    uint8_t* w = env->way;
    for (int32_t n = 0; n < w_i32(w, 0); n++) {
        if ((int8_t)w[0x34 + n * 24 + 0x17] != area)
            continue;
        for (int32_t j = 0; j < w_i32(w, 0); j++) {
            uint8_t* nd = w + 0x34 + j * 24;
            for (int32_t k = 0; k < (int8_t)nd[0x16]; k++) {
                if (w_i16(nd, 0x0A + 2 * k) != n)
                    continue;
                for (int32_t m = k; m < (int8_t)nd[0x16]; m++)
                    w_s16(nd, 0x0A + 2 * m, w_u16(nd, 0x0C + 2 * m));
                nd[0x16]--;
            }
        }
        world_section_list_remove(w + 4, n);
    }
}
