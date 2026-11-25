/* fx_ground.c: see fx_ground.h */
#include "weapons/fx_ground.h"

#include "core/tm2rand.h"

uint32_t fx_jitter(const GroundEnv* env, uint32_t pos, int32_t dx, int32_t dy, int32_t dz) {
    const uint8_t* p = wmem_ptr(env->fx->mem, pos, 12);
    const int32_t d[3] = {dx, dy, dz};
    for (int i = 0; i < 3; i++) {
        if (d[i] == 0)
            continue;
        int up = tm2_rand() & 1;
        int32_t r = tm2_rand() / d[i];
        int32_t c = w_i32(p, 4 * i);
        env->jit[i] = (int32_t)(up ? (uint32_t)c + (uint32_t)r : (uint32_t)c - (uint32_t)r);
    }
    return env->jit_addr;
}

void ground_fire_spawn(const GroundEnv* env, uint32_t pos) {
    int i = 0;
    while (i < GROUND_FIRE_COUNT && env->fires[i * GROUND_FIRE_SIZE] != 0)
        i++;
    if (i >= GROUND_FIRE_COUNT)
        return;
    uint8_t* g = env->fires + i * GROUND_FIRE_SIZE;
    const uint8_t* p = wmem_ptr(env->fx->mem, pos, 12);
    g[0] = 1;
    w_s16(g, 0x04, 0);
    for (int k = 0; k < 3; k++)
        w_s32(g, 0x08 + 4 * k, w_u32(p, 4 * k));
    for (int k = 0; k < 5; k++) {
        uint32_t j = fx_jitter(env, pos, 0xCC, 0xCC, 0x555);
        w_s32(g, 0x14 + 4 * k, fx_ground_flame(env->fx, j, 0));
        fx_smoke(env->fx, j, 0);
    }
}
