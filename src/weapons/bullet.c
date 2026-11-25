/* bullet.c: see bullet.h */
#include "weapons/bullet.h"

#include "core/tm2rand.h"
#include "math/tm2math.h"

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}

void bullet_spawn(BulletEnv* env, int32_t owner, uint8_t flag, uint32_t dir, uint32_t pos, int32_t damage) {
    uint8_t* b = 0;
    for (int i = 0; i < BULLET_COUNT; i++) {
        if (w_i32(env->bullets + i * BULLET_SIZE, 0x28) < 1) {
            b = env->bullets + i * BULLET_SIZE;
            break;
        }
    }
    if (!b)
        return;
    const uint8_t* d = wmem_ptr(env->mem, dir, 12);
    int32_t dx = w_i32(d, 0), dy = w_i32(d, 4), dz = w_i32(d, 8);
    int32_t m = iabs(dy) < iabs(dz) ? iabs(dz) : iabs(dy);
    int32_t n = iabs(dx) < m ? m : iabs(dx);
    if (n < 1)
        n = 1;
    int32_t v[3] = {tm2_wmul(dx, 200) / n, tm2_wmul(dy, 200) / n, tm2_wmul(dz, 200) / n};
    w_s32(b, 0x28, (owner != 1 && owner >= 0x32) ? 0x14 : 0x0F);
    const uint8_t* p = wmem_ptr(env->mem, pos, 12);
    for (int i = 0; i < 3; i++) {
        w_s32(b, 0x0C + 4 * i, w_u32(p, 4 * i));
        w_s32(b, 0x18 + 4 * i, (uint32_t)v[i]);
    }
    for (int i = 0; i < 3; i++)
        w_s32(b, 4 * i, w_u32(p, 4 * i));
    w_s32(b, 0x30, 0);
    b[0x35] = 0;
    w_s32(b, 0x24, (uint32_t)owner);
    b[0x34] = flag;
    w_s32(b, 0x2C, (uint32_t)damage);
}

void bullets_move(BulletEnv* env) {
    uint32_t rec = 0;     /* kept across bullets, as in the original */
    for (int i = 0; i < BULLET_COUNT; i++) {
        uint8_t* b = env->bullets + i * BULLET_SIZE;
        if (w_i32(b, 0x28) <= 0)
            continue;
        for (int k = 0; k < 3; k++)
            w_s32(b, 0x0C + 4 * k, w_u32(b, 4 * k));
        w_s32(b, 0x28, w_u32(b, 0x28) - 1u);
        for (int k = 0; k < 3; k++)
            w_s32(b, 4 * k, w_u32(b, 4 * k) + w_u32(b, 0x18 + 4 * k));
        int32_t pt[3], prev[3];
        for (int k = 0; k < 3; k++)
            pt[k] = w_i32(b, 0x0C + 4 * k);
        for (int32_t k = 7; k >= 0; k--) {
            for (int c = 0; c < 3; c++) {
                prev[c] = pt[c];
                pt[c] = (int32_t)(w_u32(b, 4 * c) - (uint32_t)(tm2_wmul(w_i32(b, 0x18 + 4 * c), k) >> 3));
            }
            int32_t owner = w_i32(b, 0x24);
            w_s32(b, 0x30, (uint32_t)env->hit_test(env->user, pt, owner, owner < 0x32 ? 10 : 5, -1));
            if (w_i32(b, 0x24) == w_i32(b, 0x30))
                w_s32(b, 0x30, 0);
            if (k != 1 && w_i32(b, 0x30) == 0) {
                int32_t h = 0;
                rec = env->level_hit(env->user, w_i32(b, 0x24), 0xFFFFFFFFu, pt, &h);
                w_s32(b, 0x30, (uint32_t)h);
                uint8_t* r = wmem_ptr(env->mem, rec, 0x10);
                int32_t rt = w_i16(r, 0x0A);
                if (rt == -w_i32(b, 0x24) || rt == 0x29B || rt == 0x29C || rt == 0xCE5 ||
                    (rt >= 0x191 && rt <= 0x1B8) || (rt >= 0xDAC && rt < 0xDB6)) {
                    w_s16(r, 0x0A, 0);
                    w_s32(b, 0x30, 0);
                }
            }
            int32_t hit = w_i32(b, 0x30);
            if (hit <= 0)
                continue;
            if (hit == 1)
                for (int c = 0; c < 3; c++)
                    prev[c] = pt[c];
            env->fx(env->user, BULLET_FX_SPARK, prev);
            if (tm2_rand() & 1)
                env->fx(env->user, BULLET_FX_SMOKE, prev);
            if (w_i32(b, 0x30) != 9)
                env->fx(env->user, BULLET_FX_RICOCHET, prev);
            k = -1;
            w_s32(b, 0x28, 0);
            if (tm2_rand() & 1) {
                int32_t vel[3] = {w_i32(b, 0x18), w_i32(b, 0x1C), w_i32(b, 0x20)};
                if (rec == 0) {
                    env->apply_hit(env->user, w_i32(b, 0x30), 0, 0, w_i32(b, 0x2C), vel, w_i32(b, 0x24));
                } else {
                    const uint8_t* r = wmem_ptr(env->mem, rec, 0x10);
                    env->apply_hit(env->user, w_i32(b, 0x30), w_i16(r, 0x0A), w_i16(r, 0x0C), w_i32(b, 0x2C), vel,
                                   w_i32(b, 0x24));
                }
            }
        }
    }
}
