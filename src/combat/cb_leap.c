/* cb_leap.c: see cb_leap.h */
#include "combat/cb_leap.h"

#include "math/tm2math.h"

static uint8_t* gp(const CbLeapEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbLeapEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static int16_t rd16(const CbLeapEnv* e, uint32_t a) { return w_i16(gp(e, a, 2), 0); }
static void wr32(const CbLeapEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }

uint32_t cb_isqrt(uint32_t s) {
    uint32_t r = 0, b = 0x40000000u;
    do {
        if ((int32_t)s >= (int32_t)b && (int32_t)(s - b) >= (int32_t)r) {
            s = s - b - r;
            r = (r >> 1) | b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    } while (b != 0);
    return r;
}

static int32_t iabs(uint32_t v) {
    uint32_t s = (uint32_t)((int32_t)v >> 31);
    return (int32_t)((v ^ s) - s);
}

void cb_homing_leap_update(const CbLeapEnv* e) {
    if (*gp(e, 0x004ffe34, 1) &&
        (rd16(e, 0x004ffe42) > 300 || rd16(e, rd32(e, 0x004ffe38)) < 2 ||
         (int32_t)(rd32(e, 0x004ffe50) - rd32(e, rd32(e, 0x004ffe4c))) > 5)) {
        *gp(e, 0x004ffe34, 1) = 0;
        e->set_step(e->user, 0x66, 0, 0);
        if (rd16(e, rd32(e, 0x004ffe38)) < 2) {
            e->explosion12(e->user, rd32(e, 0x004ffe60), 0);
            e->smoke_small(e->user, rd32(e, 0x004ffe60), 0);
        }
    }
    if (!*gp(e, 0x004ffe34, 1))
        return;
    uint32_t tp = rd32(e, 0x004ffe68), pp = rd32(e, 0x004ffe60);
    int32_t dx = (int32_t)(rd32(e, tp) - rd32(e, pp));
    int32_t dy = (int32_t)(rd32(e, tp + 4) - rd32(e, pp + 4));
    int32_t dz = (int32_t)(rd32(e, tp + 8) - rd32(e, pp + 8));
    uint32_t r = cb_isqrt((uint32_t)tm2_wmul(dy, dy) + (uint32_t)tm2_wmul(dx, dx));
    if (dy == 0)
        dy = 1;
    if (r == 0)
        r = 1;
    int32_t yaw = tm2_atan2(dx, dy);
    int32_t pitch = tm2_atan2(dz, (int32_t)r);
    uint32_t ob = rd32(e, 0x004ffe64);
    wr32(e, ob + 8, (uint32_t)tm2_angle_interp((int32_t)rd32(e, ob + 8), yaw, 0x0B));
    uint8_t* tm = gp(e, 0x004ffe42, 2);
    w_s16(tm, 0, (uint32_t)(w_u16(tm, 0) + (uint32_t)e->ticks60(e->user)));
    int32_t v[3] = {0, 0, 0};
    if (rd16(e, 0x004ffe42) > 6) {
        wr32(e, 0x004ffe58, (uint32_t)tm2_angle_step((int32_t)rd32(e, 0x004ffe58), (int32_t)rd32(e, 0x004ffe48), 5));
        int16_t ang[3] = {(int16_t)(0u - (uint32_t)pitch), 0, (int16_t)yaw};
        int32_t m[9], vel[3];
        tm2_build_matrix(ang, m);
        for (int k = 0; k < 3; k++)
            vel[k] = (int32_t)rd32(e, 0x004ffe54 + 4 * (uint32_t)k);
        tm2_mat_vec_mul_row(m, vel, v);
        uint32_t p = rd32(e, 0x004ffe60);
        for (int k = 0; k < 3; k++)
            wr32(e, p + 4 * (uint32_t)k, rd32(e, p + 4 * (uint32_t)k) + (uint32_t)v[k]);
        e->set_step(e->user, 0x66, 0, 1);
    }
    pp = rd32(e, 0x004ffe60);
    tp = rd32(e, 0x004ffe68);
    int32_t ax = iabs(rd32(e, pp) - rd32(e, tp));
    int32_t ay = iabs(rd32(e, pp + 4) - rd32(e, tp + 4));
    int32_t az = iabs(rd32(e, pp + 8) - rd32(e, tp + 8));
    int32_t m = ay < az ? az : ay;
    if (((ax >= m ? ax : m) >> 3) >= 0x0F)
        return;
    *gp(e, 0x004ffe34, 1) = 0;
    e->explosion12(e->user, rd32(e, 0x004ffe68), 0);
    e->smoke_small(e->user, rd32(e, 0x004ffe68), 0);
    e->damage(e->user, rd16(e, 0x004ffe44), 0x14, v, 2, rd16(e, 0x004ffe40));
    e->set_step(e->user, 0x66, 0, 0);
}
