/*
 * carsys_section.c: switching a world section off (see carsys_section.h).
 */
#include "carsys_section.h"

#include <string.h>

static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

void carsys_world_section_disable(const CarsysSectionEnv* env, int32_t section) {
    wr32(env->heights + section * 12, 32000);
    env->sec_a[section] = 0;
    env->sec_b[section] = 0;
    for (int32_t s = env->first[section]; s <= (int32_t)env->last[section]; s++) {
        uint8_t* r = env->surfaces + s * 0x1C;
        if (rd16(r + 2) != section) continue;
        wr32(r + 0x18, 32000);
        for (int k = 0; k < 4; k++) wr16(r + 0x0C + 2 * k, 32000);
        wr16(r + 6, 0);
        wr16(r + 4, 0);
        wr32(r + 0x14, 32000);
    }
    for (int32_t i = 0; i < *env->node_count; i++) {
        if ((int8_t)env->nodes[i * 24 + 0x17] != section) continue;
        for (int32_t j = 0; j < *env->node_count; j++) {
            uint8_t* nd = env->nodes + j * 24;
            for (int32_t k = 0; k < (int8_t)nd[0x16]; k++) {
                if (rd16(nd + 0x0A + 2 * k) != i) continue;
                for (int32_t m = k; m < (int8_t)nd[0x16]; m++) wr16(nd + 0x0A + 2 * m, rd16(nd + 0x0C + 2 * m));
                nd[0x16]--;
            }
        }
        env->node_removed(env->user, i);
    }
    env->section_flags[section] = 0;
    int32_t n = carsys_human_count(env->rec);
    for (int32_t i = 0; i < n; i++) {
        uint8_t* car = carsys_human_get(env->rec, i);
        if (rd16(car + 0xC6) == section) env->spin_out(env->user, car, 1, 5);
    }
}
