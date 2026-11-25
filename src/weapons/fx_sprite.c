/* fx_sprite.c: see fx_sprite.h */
#include "weapons/fx_sprite.h"

#include "core/tm2rand.h"

/* offsets of the frame tables from env->frames (0x00501f26) */
enum {
    FR_DEBRIS_A = 0x00, FR_DEBRIS_B = 0x20, FR_EXPLOSION = 0x80, FR_SMOKE = 0x100, FR_FREEZE = 0x180,
    FR_SPARK = 0x280, FR_PUFF = 0x2A0, FR_RICOCHET = 0x2C0, FR_GREY = 0x320, FR_FREEZE_GLOW = 0x420, FR_FLAME = 0x500
};

static uint32_t wadd(uint32_t a, uint32_t b) { return a + b; }

uint32_t fx_sprite_spawn(const FxEnv* env, uint32_t pos, int32_t kind, uint8_t step, uint8_t frames,
                         uint32_t table, uint8_t count, uint8_t flag) {
    uint8_t* s = 0;
    int i;
    for (i = 0; i < FX_SPRITE_COUNT; i++) {
        if (w_i8(env->sprites + i * FX_SPRITE_SIZE, 0x0A) < 0) {
            s = env->sprites + i * FX_SPRITE_SIZE;
            break;
        }
    }
    if (!s)
        return 0;
    const uint8_t* p = wmem_ptr(env->mem, pos, 12);
    s[0x09] = step;
    s[0x0A] = 0;
    w_s16(s, 0x0C, 0);
    s[0x00] = 0;
    s[0x02] = 1;
    s[0x03] = 0xFF;
    s[0x04] = 0xFF;
    s[0x05] = 0xFF;
    w_s32(s, 0x20, 0);
    w_s32(s, 0x24, 0);
    w_s32(s, 0x28, 0);
    s[0x0B] = (uint8_t)(frames * step);
    w_s32(s, 0x10, table);
    s[0x08] = (uint8_t)kind;
    s[0x06] = count;
    w_s32(s, 0x14, w_u32(p, 0));
    w_s32(s, 0x18, w_u32(p, 4));
    w_s32(s, 0x1C, kind < 0x60 ? w_u32(p, 8) : wadd(w_u32(p, 8), 0x78));
    w_s32(s, 0x2C, pos);
    s[0x01] = flag;
    return env->sprites_addr + (uint32_t)(i * FX_SPRITE_SIZE);
}

/* the explosion sound: once the cooldown has passed 5 */
/* vol_mode: 0 as placed, 1 / 10, 2 >> 1 (both unsigned) */
static void boom_sound(const FxEnv* env, uint32_t pos, int32_t id, int vol_mode, int32_t prio) {
    if (*env->boom_cooldown <= 5)
        return;
    int32_t pan;
    if (env->split == 0) {
        env->sound_locate(env->user, pos, 0);
        pan = *env->snd_pan;
    } else {
        env->sound_locate(env->user, pos, -1);
        pan = 0;
    }
    uint32_t vol = (uint32_t)*env->snd_volume;
    if (vol_mode == 1)
        vol /= 10u;
    else if (vol_mode == 2)
        vol >>= 1;
    env->sound_play(env->user, id, (int32_t)vol, pan, prio);
    *env->boom_cooldown = 0;
}

void fx_explosion8(const FxEnv* env, uint32_t pos, uint8_t flag) {
    boom_sound(env, pos, 8, 0, 0x4B);
    fx_sprite_spawn(env, pos, 8, 2, 0x10, env->frames + FR_EXPLOSION, 1, flag);
}

void fx_explosion16(const FxEnv* env, uint32_t pos, uint8_t flag) {
    boom_sound(env, pos, 7, 0, 0x4B);
    fx_sprite_spawn(env, pos, 0x10, 2, 0x10, env->frames + FR_EXPLOSION, 1, flag);
}

void fx_explosion48(const FxEnv* env, uint32_t pos, uint8_t flag) {
    boom_sound(env, pos, 7, 1, 0x63);
    fx_sprite_spawn(env, pos, 0x30, 1, 0x10, env->frames + FR_EXPLOSION, 8, flag);
}

void fx_explosion12(const FxEnv* env, uint32_t pos, uint8_t flag) {
    boom_sound(env, pos, 8, 0, 0x63);
    fx_sprite_spawn(env, pos, 0x0C, 2, 0x10, env->frames + FR_EXPLOSION, 1, flag);
}

void fx_explosion24(const FxEnv* env, uint32_t pos, uint8_t flag) {
    boom_sound(env, pos, 7, 2, 0x63);
    fx_sprite_spawn(env, pos, 0x18, 1, 0x10, env->frames + FR_EXPLOSION, 1, flag);
}

/* the plain coloured sprites: spawn, then colour bytes +0x02..+0x05 and rise +0x28 */
static uint32_t tinted(const FxEnv* env, uint32_t pos, int32_t kind, uint8_t step, uint8_t frames, int table,
                       uint8_t count, uint8_t flag, uint8_t r, uint8_t g, uint8_t b, int32_t rise) {
    uint32_t a = fx_sprite_spawn(env, pos, kind, step, frames, env->frames + (uint32_t)table, count, flag);
    if (a) {
        uint8_t* s = env->sprites + (a - env->sprites_addr);
        s[0x02] = 1;
        s[0x03] = r;
        s[0x04] = g;
        s[0x05] = b;
        w_s32(s, 0x28, (uint32_t)rise);
    }
    return a;
}

uint32_t fx_freeze_burst(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tinted(env, pos, 8, 2, 0x10, FR_FREEZE, 1, flag, 0, 0, 100, 6);
}

void fx_freeze_glow(const FxEnv* env, uint32_t pos, uint8_t flag) {
    tinted(env, pos, 4, 2, 0x10, FR_FREEZE_GLOW, 1, flag, 0x10, 0x10, 0x10, 10);
}

void fx_smoke_small(const FxEnv* env, uint32_t pos, uint8_t flag) {
    tinted(env, pos, 3, 2, 0x10, FR_GREY, 0, flag, 0x40, 0x40, 0x40, 10);
}

void fx_smoke(const FxEnv* env, uint32_t pos, uint8_t flag) {
    tinted(env, pos, 5, 2, 0x10, FR_GREY, 0, flag, 0x40, 0x40, 0x40, 0x0F);
}

void fx_puff(const FxEnv* env, uint32_t pos, uint8_t flag) {
    uint32_t a = fx_sprite_spawn(env, pos, 5, 1, 4, env->frames + FR_PUFF, 1, flag);
    if (a)
        w_s32(env->sprites + (a - env->sprites_addr), 0x28, 3);
}

void fx_bullet_spark(const FxEnv* env, uint32_t pos, uint8_t flag) {
    tinted(env, pos, 5, 1, 4, FR_SPARK, 1, flag, 0xFF, 0x75, 0, 3);
}

void fx_bullet_smoke(const FxEnv* env, uint32_t pos, uint8_t flag) {
    tinted(env, pos, 1, 1, 0x10, FR_GREY, 1, flag, 0x40, 0x40, 0x40, 3);
}

void fx_ricochet(const FxEnv* env, uint32_t pos, uint8_t flag) {
    uint32_t table = env->frames + FR_RICOCHET + (uint32_t)(tm2_rand() % 6) * 8;
    fx_sprite_spawn(env, pos, 8, 1, 3, table, 1, flag);
}

uint32_t fx_ground_flame(const FxEnv* env, uint32_t pos, uint8_t flag) {
    uint32_t a = fx_sprite_spawn(env, pos, 0x0A, 1, 8, env->frames + FR_FLAME, 1, flag);
    if (a) {
        uint8_t* s = env->sprites + (a - env->sprites_addr);
        s[0x00] = 1;
        w_s16(s, 0x0E, 300);
        w_s16(s, 0x0C, 0x32);
    }
    return a;
}

uint32_t fx_smoke_burst(const FxEnv* env, uint32_t pos, uint8_t flag) {
    boom_sound(env, pos, 8, 0, 0x4B);
    uint32_t a = fx_sprite_spawn(env, pos, 8, 2, 0x10, env->frames + FR_SMOKE, 1, flag);
    if (a) {
        uint8_t* s = env->sprites + (a - env->sprites_addr);
        s[0x02] = 1;
        s[0x03] = 0xFF;
        s[0x04] = 0;
        s[0x05] = 0x96;
        w_s32(s, 0x28, 6);      /* rises */
    }
    return a;
}

void fx_flame_bit(const FxEnv* env, uint32_t pos) {
    uint32_t table = env->frames + FR_EXPLOSION + 8 + (uint32_t)(tm2_rand() & 3) * 8;
    uint32_t a = fx_sprite_spawn(env, pos, 3, 1, 2, table, 0, 0);
    if (!a)
        return;
    uint8_t* s = env->sprites + (a - env->sprites_addr);
    w_s32(s, 0x20, (uint32_t)((tm2_rand() & 7) - 3));
    w_s32(s, 0x24, (uint32_t)((tm2_rand() & 7) - 3));
    int32_t vz = (tm2_rand() & 0xF) - 5;
    vz = vz > 0 ? 0 : (tm2_rand() & 0xF) - 5;     /* a second draw when the first is <= 0 */
    s[0x02] = 1;
    s[0x03] = 0xC6;
    s[0x04] = 0x42;
    s[0x05] = 0x21;
    w_s32(s, 0x28, (uint32_t)vz);
}

void fx_impact_debris(const FxEnv* env, int32_t n, uint32_t pos) {
    if (n > 2) {
        env->sound_locate(env->user, pos, env->split == 0 ? 0 : -1);
        int32_t prio = n > 6 ? 0x63 : 0x4B;
        int32_t pan = *env->snd_pan;     /* PC: the pan is used in split screen too */
        env->sound_play(env->user, 6, *env->snd_volume, pan, prio);
    }
    for (int k = 0; k < n; k++) {
        uint8_t* q = 0;
        for (int i = 0; i < FX_PARTICLE_COUNT; i++) {
            if (w_i32(env->particles + i * FX_PARTICLE_SIZE, 0x1C) <= 0) {
                q = env->particles + i * FX_PARTICLE_SIZE;
                break;
            }
        }
        if (!q)
            return;
        const uint8_t* p = wmem_ptr(env->mem, pos, 12);
        w_s32(q, 0x1C, 0x28);
        q[0x18] = 0;
        w_s32(q, 0x00, w_u32(p, 0));
        w_s32(q, 0x04, w_u32(p, 4));
        w_s32(q, 0x08, w_u32(p, 8));
        w_s32(q, 0x0C, (uint32_t)((tm2_rand() & 0x3F) - 0x20));
        w_s32(q, 0x10, (uint32_t)((tm2_rand() & 0x3F) - 0x20));
        w_s32(q, 0x14, (uint32_t)((tm2_rand() & 0x1F) + 0x10));
        int r = tm2_rand() & 3;
        w_s32(q, 0x20, env->frames + ((r & 1) ? FR_DEBRIS_B : FR_DEBRIS_A));
    }
}

void fx_impact_particles_update(const FxEnv* env) {
    for (int i = 0; i < FX_PARTICLE_COUNT; i++) {
        uint8_t* q = env->particles + i * FX_PARTICLE_SIZE;
        if (w_i32(q, 0x1C) <= 0)
            continue;
        w_s32(q, 0x00, wadd(w_u32(q, 0x00), w_u32(q, 0x0C)));
        w_s32(q, 0x04, wadd(w_u32(q, 0x04), w_u32(q, 0x10)));
        w_s32(q, 0x08, wadd(w_u32(q, 0x08), w_u32(q, 0x14)));
        int32_t vz = (int32_t)(w_u32(q, 0x14) - 4u);
        w_s32(q, 0x14, (uint32_t)vz);
        if (w_i32(q, 0x08) <= 0 && vz < 0) {
            if (vz <= -6)
                w_s32(q, 0x14, (uint32_t)((int32_t)(0u - (uint32_t)vz) / 2));   /* bounce */
            else
                w_s32(q, 0x1C, 0);
        }
        int32_t life = w_i32(q, 0x1C);
        if (life > 0) {
            w_s32(q, 0x1C, (uint32_t)(life - 1));
            q[0x18] = (uint8_t)((q[0x18] + 1) & 3);
        }
    }
}
