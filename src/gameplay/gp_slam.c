/* gp_slam.c: see gp_slam.h */
#include "gameplay/gp_slam.h"

#include "math/tm2math.h"

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}
static int32_t dist(const int32_t p[3], const uint8_t* c, int off) {
    uint32_t d = (uint32_t)iabs((int32_t)((uint32_t)p[0] - w_u32(c, off)));
    d += (uint32_t)iabs((int32_t)((uint32_t)p[1] - w_u32(c, off + 4)));
    d += (uint32_t)iabs((int32_t)((uint32_t)p[2] - w_u32(c, off + 8)));
    return (int32_t)d;
}

static void claw_set(uint8_t* claw, int32_t best, int lighter, uint8_t* ret) {
    if (lighter) {
        claw[7] = 1;
        w_s16(claw, 0x0C, best < 0x40 ? 3 : best < 0x88 ? 2 : 1);
    } else {
        *ret = 1;
        w_s16(claw, 0x0C, 0);
    }
}

uint8_t gp_slam_find_target(GpSlamEnv* env, uint8_t* car, uint8_t human) {
    uint8_t ret = 0, found = 0;
    uint32_t claw_a = w_u32(car, human ? 0x890 : 0x87C);
    int32_t self = human ? w_i16(car, 0x00) : w_i16(car, 0x04);
    const uint8_t* st = car + (human ? 0x138 : 0x4EC);
    const uint8_t* b = car + (human ? 0x6C4 : 0x708);
    int32_t m[9], v[3] = {0, 0, 0}, p1[3], p2[3];
    for (int i = 0; i < 9; i++)
        m[i] = w_i32(b, 0x50 + 4 * i);
    v[1] = w_i16(st, 0x4E) + 0x78;
    tm2_mat_vec_mul_row(m, v, p1);
    v[1] = w_i16(st, 0x4E) + 0x3C;
    tm2_mat_vec_mul_row(m, v, p2);
    for (int i = 0; i < 3; i++) {
        p1[i] = (int32_t)((uint32_t)p1[i] + w_u32(b, 0x20 + 4 * i));
        p2[i] = (int32_t)((uint32_t)p2[i] + w_u32(b, 0x20 + 4 * i));
    }
    int32_t best = 0x7FFF, bi = 0;
    int32_t nh = env->human_count(env->user);
    int32_t na = env->ai_count(env->user);
    for (int32_t i = 0; i < nh; i++) {
        if (i == self && human)
            continue;
        const uint8_t* c = env->human_car(env->user, i);
        int32_t d = dist(p1, c, 0x6E4);
        if (d < best)
            best = d, bi = i;
        d = dist(p2, c, 0x6E4);
        if (d < best)
            best = d, bi = i;
    }
    if (best < 0xA0) {
        uint8_t* c = env->human_car(env->user, bi);
        w_s16(c, 0x152, 0x3C);
        w_s16(c, 0x6CE, 0);
        c[0xA7] = 1;
        c[0xB7] = 1;
        found = 1;
        c[0xAF] = 1;
        uint8_t* claw = wmem_ptr(env->mem, claw_a, 0x10);
        claw_set(claw, best, w_i16(c, 0x1B2) < w_i16(st, 0x7A), &ret);
        w_s16(claw, 0x0E, w_u16(c, 0x90));
    }
    if (found)
        return ret;
    for (int32_t i = 0; i < na; i++) {
        if (i == self && !human)
            continue;
        const uint8_t* c = env->ai_car(env->user, i);
        int32_t d = dist(p1, c, 0x728);
        if (d < best)
            best = d, bi = i;
        d = dist(p2, c, 0x728);
        if (d < best)
            best = d, bi = i;
    }
    if (best < 0xA0) {
        uint8_t* c = env->ai_car(env->user, bi);
        c[0x75] = 1;
        w_s16(c, 0x506, 0x3C);
        w_s16(c, 0x712, 0);
        c[0x85] = 1;
        c[0x7D] = 1;
        uint8_t* claw = wmem_ptr(env->mem, claw_a, 0x10);
        claw_set(claw, best, w_i16(c, 0x566) < w_i16(st, 0x7A), &ret);
        w_s16(claw, 0x0E, w_u16(c, 0x30));
    }
    return ret;
}
