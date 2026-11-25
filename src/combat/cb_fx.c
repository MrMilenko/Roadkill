/* cb_fx.c: see cb_fx.h */
#include "combat/cb_fx.h"

static uint8_t* rec(const FxEnv* env, uint32_t a) { return env->sprites + (a - env->sprites_addr); }

static uint32_t spawn(const FxEnv* env, uint32_t pos, int32_t kind, uint8_t step, uint8_t frames,
                      uint32_t table, uint8_t count, uint8_t flag) {
    return fx_sprite_spawn(env, pos, kind, step, frames, env->frames + table, count, flag);
}

/* colour bytes +0x02..+0x05 and rise +0x28 */
static uint32_t tint(const FxEnv* env, uint32_t a, uint8_t r, uint8_t g, uint8_t b, int32_t rise) {
    if (a) {
        uint8_t* s = rec(env, a);
        s[0x02] = 1;
        s[0x03] = r;
        s[0x04] = g;
        s[0x05] = b;
        w_s32(s, 0x28, (uint32_t)rise);
    }
    return a;
}

/* burning flame: +0x00 = 1, +0x0E burn time, +0x0C = 0x32 */
static uint32_t burn(const FxEnv* env, uint32_t a, int32_t t) {
    if (a) {
        uint8_t* s = rec(env, a);
        s[0x00] = 1;
        w_s16(s, 0x0E, (uint32_t)t);
        w_s16(s, 0x0C, 0x32);
    }
    return a;
}

uint32_t cb_fx_spark_blue(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 5, 1, 4, 0x280, 1, flag), 0, 0x14, 100, 3);
}

uint32_t cb_fx_blast12(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return spawn(env, pos, 0x0C, 2, 0x10, 0x200, 1, flag);
}

uint32_t cb_fx_blast24(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return spawn(env, pos, 0x18, 1, 0x10, 0x200, 1, flag);
}

uint32_t cb_fx_flame5(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return burn(env, spawn(env, pos, 5, 1, 8, 0x500, 1, flag), 100);
}

uint32_t cb_fx_flame10(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return burn(env, spawn(env, pos, 10, 1, 8, 0x500, 1, flag), 200);
}

uint32_t cb_fx_flame_pair(const GroundEnv* genv, uint32_t pos, uint8_t flag) {
    const FxEnv* env = genv->fx;
    uint32_t a = 0;
    for (int k = 0; k < 2; k++) {
        uint32_t p = fx_jitter(genv, pos, 0x199, 0x199, 0x555);
        a = burn(env, spawn(env, p, 10, 1, 8, 0x500, 1, flag), 300);
    }
    return a;
}

uint32_t cb_fx_smoke_a2(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 2, 1, 0x10, 0x320, 1, flag), 0x40, 0x40, 0x40, 6);
}

uint32_t cb_fx_smoke_a8(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 8, 2, 0x10, 0x320, 0, flag), 0x40, 0x40, 0x40, 0x0F);
}

uint32_t cb_fx_smoke_b2(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 2, 1, 0x10, 0x3A0, 1, flag), 0x80, 0x80, 0x80, 6);
}

uint32_t cb_fx_smoke_b3(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 3, 2, 0x10, 0x3A0, 0, flag), 0x80, 0x80, 0x80, 10);
}

uint32_t cb_fx_smoke_b5(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 5, 2, 0x10, 0x3A0, 0, flag), 0x80, 0x80, 0x80, 0x0F);
}

uint32_t cb_fx_smoke_c1(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 1, 2, 0x10, 0x420, 1, flag), 0x10, 0x10, 0x10, 1);
}

uint32_t cb_fx_smoke_c2(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 2, 2, 0x10, 0x420, 1, flag), 0x10, 0x10, 0x10, 3);
}

uint32_t cb_fx_smoke_c3(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return tint(env, spawn(env, pos, 3, 2, 0x10, 0x420, 1, flag), 0x10, 0x10, 0x10, 6);
}

uint32_t cb_fx_lightning(const FxEnv* env, uint32_t pos, uint8_t flag) {
    uint32_t a = spawn(env, pos, 8, 1, 10, 0x558, 1, flag);
    if (a) {
        uint8_t* s = rec(env, a);
        s[0x02] = 1;
        s[0x03] = 0xFF;
        s[0x04] = 200;
        s[0x05] = 0x32;
        w_s16(s, 0x0C, 5);
    }
    return a;
}

uint32_t cb_fx_explosion_quiet8(const FxEnv* env, uint32_t pos, uint8_t flag) {
    return spawn(env, pos, 8, 1, 2, 0x80, 0, flag);
}

const uint32_t cb_anim_address[CB_ANIM_COUNT] = {
    0x004514ac, 0x004514c0, 0x004514d4, 0x004514e8, 0x004514fc, 0x00451510, 0x00451524,
    0x00451538, 0x0045154c, 0x00451560, 0x00451574, 0x00451498, 0x00451594
};
static const uint16_t anim_table[CB_ANIM_COUNT] = {
    0x080, 0x100, 0x180, 0x200, 0x320, 0x3A0, 0x420, 0x2C0, 0x280, 0x2A0, 0x4A0, 0x500, 0x558
};
static const uint8_t anim_frames[CB_ANIM_COUNT] = { 16, 16, 16, 16, 16, 16, 16, 8, 4, 4, 8, 10, 30 };

int32_t cb_fx_anim_store(const CbAnimEnv* env, int which, uint32_t src) {
    uint32_t dst = env->frames + anim_table[which];
    int32_t r = env->parse(env->user, src, dst, anim_frames[which]);
    if (which == CB_ANIM_CONTRAIL)
        r = env->set_contrail_cells(env->user, dst);
    return r;
}
