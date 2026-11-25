/* fx_world.c: see fx_world.h */
#include "weapons/fx_world.h"

#include "core/tm2rand.h"
#include "math/tm2math.h"

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}

static void place(const WorldEnv* env, uint32_t obj, uint32_t pos) {
    uint8_t* o = wmem_ptr(env->fx->mem, obj, 0x10);
    const uint8_t* p = wmem_ptr(env->fx->mem, pos, 12);
    for (int i = 0; i < 3; i++)
        w_s32(o, 4 + 4 * i, w_u32(p, 4 * i));
}

void fx_splash_at(const WorldEnv* env, uint32_t pos, int32_t speed) {
    const FxEnv* fx = env->fx;
    if (*env->splash_next > 4)
        *env->splash_next = 1;
    uint32_t obj = env->splash_objs[*env->splash_next];
    if (obj == 0)
        return;
    place(env, obj, pos);
    env->anim_start(env->user, 0x2AC, *env->splash_next, 1);
    *env->splash_next += 1;
    fx->sound_locate(fx->user, pos, fx->split == 0 ? 0 : -1);
    if (iabs(*fx->snd_volume) >= 0x5DC)
        return;
    fx->sound_play(fx->user, iabs(speed) < 0x4B0 ? 0x21 : 0x22, *fx->snd_volume, *fx->snd_pan, 0x63);
}

uint8_t fx_pooled_blast(const WorldEnv* env, uint32_t pos) {
    int32_t i = *env->blast_next;
    if (wmem_ptr(env->fx->mem, env->blast_anims[i] + 7, 1)[0] != 0)
        return 0;
    uint32_t obj = env->blast_objs[i];
    if (obj == 0)
        return 0;
    place(env, obj, pos);
    env->anim_start(env->user, 0x2AE, *env->blast_next, 1);
    *env->blast_next += 1;
    if (*env->blast_next > 4)
        *env->blast_next = 1;
    return 1;
}

void light_flicker_tick(const WorldEnv* env) {
    for (int i = 0; i < 10; i++) {
        uint8_t* f = env->flicker + i * 8;
        if (w_i32(f, 0) <= 0)
            continue;
        int32_t t = w_i32(f, 0) - 1;
        w_s32(f, 0, (uint32_t)t);
        uint32_t m;
        if (t == 0)
            m = env->model_variant(env->user, 0);
        else
            m = env->model_variant(env->user, tm2_rand() % 3 == 0 ? 1 : 0);
        w_s32(wmem_ptr(env->fx->mem, w_u32(f, 4), 8), 4, m);
    }
}

void fxw_collapse_unlink(const WorldEnv* env, int32_t piece) {
    for (int r = 0; r < 35; r++)
        for (int j = 0; j < 6; j++)
            if (w_i8(env->collapse, r * 0x24 + j) == piece)
                env->collapse[r * 0x24 + j] = 0xFF;
}

void eiffel_tower_start_fall(const WorldEnv* env, uint32_t pos) {
    uint8_t* t = env->tower;
    const uint8_t* p = wmem_ptr(env->fx->mem, pos, 12);
    if (iabs((int32_t)(w_u32(t, 0x1C) - w_u32(p, 0))) >= 0x4B0)
        return;
    if (iabs((int32_t)(w_u32(t, 0x20) - w_u32(p, 4))) >= 0x4B0)
        return;
    if (t[0] != 0 || t[2] != 0)
        return;
    t[0] = 1;
    t[1] = 0x14;
    w_s16(t, 0x08, 0);
    int32_t r = tm2_rand() / 0x290;
    w_s16(t, 0x06, 0x4B);
    w_s32(t, 0x20, w_u32(t, 0x20) + 600u);
    w_s32(t, 0x24, w_u32(t, 0x24) + 800u);
    w_s16(t, 0x0A, (uint32_t)(r + 0x32));
    w_s16(t, 0x04, 0);
    fx_explosion48(env->fx, env->tower_addr + 0x1C, 0);
    w_s32(t, 0x24, 0x540);
    int16_t a[3] = {w_i16(t, 0x28), w_i16(t, 0x2A), w_i16(t, 0x2C)};
    int32_t m[9];
    tm2_build_matrix(a, m);
    uint8_t* o = wmem_ptr(env->fx->mem, w_u32(t, 0x0C) + 4, 36);
    for (int i = 0; i < 9; i++)
        w_s32(o, 4 * i, (uint32_t)m[i]);
}
