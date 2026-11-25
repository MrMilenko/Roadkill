/* gp2_phit.c: see gp2_phit.h */
#include "gameplay/gp2_phit.h"

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}

/* largest axis distance >> 3 (32 bit wrap, |INT_MIN| stays negative) */
static int32_t dist8(const uint8_t* p, const uint8_t* q) {
    int32_t ax = iabs((int32_t)(w_u32(p, 0) - w_u32(q, 0)));
    int32_t ay = iabs((int32_t)(w_u32(p, 4) - w_u32(q, 4)));
    int32_t az = iabs((int32_t)(w_u32(p, 8) - w_u32(q, 8)));
    int32_t m = ay < az ? az : ay;
    return (ax < m ? m : ax) >> 3;
}

int32_t gp2_projectile_hit_test(Gp2PhitEnv* e, uint32_t pos_a, int32_t self, int32_t radius, int32_t kind) {
    const uint8_t* pos = wmem_ptr(e->mem, pos_a, 12);
    int32_t nh = e->human_count(e->user);
    int32_t na = e->ai_count(e->user);
    int32_t lim = (int32_t)((uint32_t)radius + 1);
    for (int32_t i = 0; i < nh; i++) {
        const uint8_t* w = wmem_ptr(e->mem, e->human_obj(e->user, i), 0x15C);
        uint32_t car_a = e->human_car(e->user, i);
        uint8_t* c = wmem_ptr(e->mem, car_a, 0x898);
        if (w_u32(w, 8) == 0 && c[0x9A] != 0)
            continue;
        if (dist8(pos, w + 0xE8) >= lim)
            continue;
        int32_t id = w_i32(w, 0x158);
        if (self == id) {
            if (kind == 0x33)
                return id;
            continue;
        }
        if (kind == 0x0E && (e->multiplayer(e->user) == 0 || w_i16(c, 0) <= 0)) {
            if (c[0xA7] == 0 && c[0xA8] == 0) {
                c[0xA7] = 1;
                w_s16(c, 0x152, 300);
                w_s16(c, 0x154, 1);
                e->freeze_state(e->user, w_i16(c, 0x90), 1);
                w_s16(c, 0x87C, 0);
            } else {
                w_s16(c, 0x152, 300);
            }
        }
        if (kind == 0x0F || kind == 0x2F) {
            if (c[0xB4] == 0 && c[0xA8] == 0 && c[0xBB] == 0 && c[0xBD] == 0) {
                c[0xB4] = 1;
                w_s32(c, 0x88C, e->flame(e->user, car_a + 0x6E4, 1));
            }
            w_s16(c, 0x15A, 0);
        }
        if (kind == 0x14)
            e->spin_hit(e->user, car_a, 1, 0, 1);
        return w_i32(w, 0x158);
    }
    for (int32_t i = 0; i < na; i++) {
        uint32_t w_a = e->ai_obj(e->user, i);
        uint32_t car_a = e->ai_car(e->user, i);
        if (w_a == 0)
            continue;
        const uint8_t* w = wmem_ptr(e->mem, w_a, 0x15C);
        uint8_t* c = wmem_ptr(e->mem, car_a, 0x880);
        if (w_u32(w, 8) == 0 && c[0x68] != 0)
            continue;
        int32_t extra = w_u16(c, 0x30) == 0xF0 ? 0x19 : 0;
        if (self == w_i32(w, 0x158))
            continue;
        if (dist8(pos, w + 0xE8) >= (int32_t)((uint32_t)lim + (uint32_t)extra))
            continue;
        if (kind == 0x0E) {
            if (c[0x75] == 0) {
                c[0x75] = 1;
                w_s16(c, 0x506, 300);
                w_s16(c, 0x508, 1);
                w_s16(c, 0x50A, 0);
                e->freeze_state(e->user, w_i16(c, 0x30), 1);
            } else {
                w_s16(c, 0x506, 300);
            }
        }
        if (kind == 0x0F || kind == 0x2F) {
            if (c[0x82] == 0 && c[0x89] == 0 && c[0x8B] == 0) {
                c[0x82] = 1;
                w_s32(c, 0x878, e->flame(e->user, car_a + 0x728, 1));
            }
            w_s16(c, 0x50E, 0);
        }
        if (kind == 0x14)
            e->spin_hit(e->user, car_a, 0, 0, 1);
        return w_i32(w, 0x158);
    }
    if (self < 0x32) {
        uint32_t pool = e->pool_get(e->user);
        for (int32_t i = 0; i < 0x32; i++) {
            const uint8_t* p = wmem_ptr(e->mem, pool + 0x14 * (uint32_t)i, 0x14);
            if (w_i16(p, 4) <= 0 || w_i16(p, 0) == (int32_t)(0u - (uint32_t)self))
                continue;
            if (dist8(pos, p + 8) < lim)
                return i + 500;
        }
    }
    uint32_t act = e->actors_get(e->user);
    for (int32_t i = 0; i < 6; i++) {
        const uint8_t* a = wmem_ptr(e->mem, act + 0x4C * (uint32_t)i, 0x4C);
        if (w_i16(a, 4) == -1 || (int8_t)a[2] == 2)
            continue;
        const uint8_t* o = wmem_ptr(e->mem, w_u32(a, 0x48) + 0xE8, 12);
        if (dist8(pos, o) < lim)
            return i + 6000;
    }
    return 0;
}
