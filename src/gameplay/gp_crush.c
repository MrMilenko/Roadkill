/* gp_crush.c: see gp_crush.h */
#include "gameplay/gp_crush.h"

#include "core/tm2rand.h"

/* the original's inline square root: bit pairs from 2^30 down, signed compares */
static int32_t isqrt(int32_t n) {
    uint32_t res = 0, bit = 0x40000000u;
    while (bit != 0) {
        if (n >= (int32_t)bit && n - (int32_t)bit >= (int32_t)res) {
            n = n - (int32_t)bit - (int32_t)res;
            res = (res >> 1) | bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (int32_t)res;
}

void gp_car_crush_contact(GpCrushEnv* env, uint8_t* car, uint8_t human) {
    uint8_t* f = car + (human ? 0x9A : 0x68);
    uint8_t* body = car + (human ? 0x6C4 : 0x708);
    uint8_t* w = car + (human ? 0x58C : 0x5D0);
    const uint8_t* p = car + (human ? 0x138 : 0x4EC);
    f[0x14] = 0;
    int32_t best = 0x7FFF;
    uint32_t e = env->entity(env->user, w_i16(p, 0x96));
    if (e != 0) {
        const uint8_t* eo = wmem_ptr(env->mem, e + 0xE8, 8);
        for (int k = 0; k < 4; k++) {
            int32_t dx = (int32_t)(w_u32(eo, 0) - w_u32(w, 0x54 + 0x48 * k));
            int32_t dy = (int32_t)(w_u32(eo, 4) - w_u32(w, 0x58 + 0x48 * k));
            int32_t r = isqrt((int32_t)((uint32_t)dx * (uint32_t)dx + (uint32_t)dy * (uint32_t)dy));
            int32_t rad = w_i16(p, 0x30);
            if (r < rad) {
                f[0x14] = 1;
                int32_t push = w_i16(p, 0x32);
                int32_t q = (int32_t)((uint32_t)push * (uint32_t)r) / rad;
                w_s16(w, 0x30 + 0x48 * k, (uint32_t)(push - q / 2));
                if (r < best)
                    best = r;
            } else {
                w_s16(w, 0x30 + 0x48 * k, 0);
            }
        }
    }
    uint8_t* f2 = 0;
    uint8_t* p2 = 0;
    uint8_t h2 = 0;
    int32_t idx = 0;
    if (e != 0 && env->entity_car(env->user, e, &h2, &idx) != 0) {
        uint8_t* c2 = h2 ? env->human_car(env->user, idx) : env->ai_car(env->user, idx);
        f2 = c2 + (h2 ? 0x9A : 0x68);
        p2 = c2 + (h2 ? 0x138 : 0x4EC);
    }
    if (f[0x14] != 0 && w_i16(p, 0x30) / 2 > best) {
        int32_t pt[3];
        pt[0] = (int32_t)((uint32_t)(tm2_rand() & 0x3F) + w_u32(body, 0x20) - 0x20);
        pt[1] = (int32_t)((uint32_t)(tm2_rand() & 0x3F) + w_u32(body, 0x24) - 0x20);
        uint32_t z = w_u32(body, 0x28);
        pt[2] = (int32_t)(z - 0x20 - (uint32_t)(tm2_rand() & 0x1F));
        if (pt[2] < 1)
            pt[2] = 1;
        env->debris(env->user, tm2_rand() & 3, pt);
        uint32_t r = (uint32_t)tm2_rand() & 0xF;
        if (r == 0)
            env->fx(env->user, 0x00451014u, pt, 0);
        else if (r == 1)
            env->fx(env->user, 0x004512b4u, pt, 0);
        else if (r <= 5)
            env->fx(env->user, 0x00451274u, pt, 0);
        if ((tm2_rand() & 0xF) == 1) {
            int32_t bp[3] = {w_i32(body, 0x20), w_i32(body, 0x24), w_i32(body, 0x28)};
            env->fx(env->user, 0x00450864u, bp, 0);
        }
        if (f2 != 0 && w_i32(p2, 0xA4) != 0 && (env->parity & 3) == 0 && w_i32(p2, 0xA0) > 0)
            w_s32(p2, 0xA0, (uint32_t)(w_i32(p2, 0xA0) - 1));
    }
    if (f[0x14] == 0) {
        w_s16(w, 0x78, 0);
        w_s16(w, 0xC0, 0);
        w_s16(w, 0x108, 0);
        w_s16(w, 0x30, 0);
        if (f2 != 0)
            f2[0x15] = 0;
    }
}
