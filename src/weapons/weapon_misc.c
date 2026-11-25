/* weapon_misc.c: see weapon_misc.h */
#include "weapons/weapon_misc.h"

#include "core/tm2rand.h"
#include "math/tm2math.h"

void slam_records_reset(WMiscEnv* env) { *env->slam_cursor = 0; }

void slam_state_init(WMiscEnv* env, uint8_t* w) {
    int32_t c = *env->slam_cursor;
    if (w_u32(w, 0x48) == 0) {
        w_s32(w, 0x48, env->slam_records + (uint32_t)(c * 0x12));   /* the record at the cursor, then advance */
        c = c < 4 ? c + 1 : 0;
    }
    if (w_u32(w, 0x48) != 0) {
        uint8_t* r = wmem_ptr(env->mem, w_u32(w, 0x48), 0x12);
        r[0] = 0;
        r[1] = 1;
        r[2] = 0;
        r[7] = 0;
        r[3] = 1;
        w_s16(r, 0x10, 0);
        r[0x0B] = 0;
    }
    *env->slam_cursor = c;
}

uint32_t blast_anim_step(WMiscEnv* env, int32_t i) {
    return wmem_ptr(env->mem, env->blast_anims[i] + 7, 1)[0];
}

void shields_update(WMiscEnv* env) {
    for (int i = 0; i < 4; i++) {
        uint8_t* s = env->shields + i * 0x0C;
        if (w_i16(s, 0) <= 0)
            continue;
        w_s16(s, 0, (uint32_t)(w_i16(s, 0) - tm2_frame_ticks60()));
        if (w_i16(s, 0) < 0)
            w_s16(s, 0, 0);
        if (w_i16(s, 0) <= 0) {
            w_s32(wmem_ptr(env->mem, w_u32(s, 8), 0x0C), 8, 0);
            wmem_ptr(env->mem, env->car_data(env->user, w_i16(s, 2)), 0xA9)[0xA8] = 0;
        }
        uint8_t* dst = wmem_ptr(env->mem, w_u32(s, 8), 0xF4);
        const uint8_t* src = wmem_ptr(env->mem, w_u32(s, 4), 0xF4);
        for (int k = 0; k < 12; k++)
            dst[0xE8 + k] = src[0xE8 + k];
        w_s32(dst, 0xF0, w_u32(dst, 0xF0) - 0x40u);
        for (int k = 0; k < 8; k++)
            dst[0xE0 + k] = src[0xE0 + k];
        int16_t a[3] = {w_i16(dst, 0xE0), w_i16(dst, 0xE2), w_i16(dst, 0xE4)};
        int32_t m[9];
        tm2_build_matrix(a, m);
        for (int k = 0; k < 9; k++)
            w_s32(dst, 0xB0 + 4 * k, (uint32_t)m[k]);
    }
}

void screen_flash_start(WMiscEnv* env, int32_t frames, uint8_t r, uint8_t g, uint8_t b) {
    static const int16_t xy[8] = {0, 0, 0x140, 0, 0, 0xF0, 0x140, 0xF0};
    uint8_t* f = env->flash;
    for (int q = 0; q < 2; q++) {
        uint8_t* p = f + q * 0x18;
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = q ? 0x28 : 0x2A;
        for (int k = 0; k < 8; k++)
            w_s16(p, 4 + 2 * k, (uint16_t)xy[k]);
    }
    if (env->split != 0)
        frames *= 2;
    w_s32(f, 0x2C, (uint32_t)frames);
}

void pickup_respawn_tick(WMiscEnv* env) {
    if (*env->pickup_timer < *env->pickup_period) {
        *env->pickup_timer += 1;
        return;
    }
    *env->pickup_timer = 0;
    if (*env->pickup_count <= 0)
        return;
    int32_t n = tm2_rand() % *env->pickup_count;
    uint8_t* p = env->pickups + n * 16;
    int32_t id = w_i16(p, 0);
    if ((id == 0x1A1 || id == 0x1A2) && tm2_rand() >= 0x4000)
        return;
    w_s16(p, 2, 1);
    env->pickup_show(env->user, w_i16(p, 0), n, 1);
    env->event(env->user, 0x0B, p[0], n & 0xFF);
}
