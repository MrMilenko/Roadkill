/* cb_strike.c: see cb_strike.h */
#include "combat/cb_strike.h"

#include "math/tm2math.h"

static uint8_t* gp(const CbStrikeEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbStrikeEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static int16_t rd16(const CbStrikeEnv* e, uint32_t a) { return w_i16(gp(e, a, 2), 0); }
static void wr32(const CbStrikeEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }
static void wr16(const CbStrikeEnv* e, uint32_t a, uint32_t v) { w_s16(gp(e, a, 2), 0, v); }

static uint32_t obj(const CbStrikeEnv* e) { return rd32(e, 0x004ffed0); }

static void rebuild(const CbStrikeEnv* e) {
    uint32_t o = obj(e);
    int16_t a[3] = {rd16(e, o + 0xE0), rd16(e, o + 0xE2), rd16(e, o + 0xE4)};
    int32_t m[9];
    tm2_build_matrix(a, m);
    for (int k = 0; k < 9; k++)
        wr32(e, o + 0xB0 + 4 * (uint32_t)k, (uint32_t)m[k]);
}

static void mat(const CbStrikeEnv* e, int32_t m[9]) {
    for (int k = 0; k < 9; k++)
        m[k] = (int32_t)rd32(e, obj(e) + 0xB0 + 4 * (uint32_t)k);
}

void cb_strike_update(const CbStrikeEnv* e) {
    if (rd16(e, 0x004ffe76) <= 0)
        return;
    wr16(e, 0x004ffe76, (uint32_t)(rd16(e, 0x004ffe76) - e->ticks60(e->user)));
    rebuild(e);
    int32_t v[3], w[3], m[9];
    v[0] = (int32_t)rd32(e, 0x004ffe78);
    v[1] = tm2_wmul((int32_t)rd32(e, 0x004ffe7c), (int16_t)e->ticks60(e->user));
    v[2] = (int32_t)rd32(e, 0x004ffe80);
    mat(e, m);
    tm2_mat_vec_mul_row(m, v, w);
    for (int k = 0; k < 3; k++)
        wr32(e, 0x004ffe84 + 4 * (uint32_t)k, (uint32_t)w[k]);
    for (int k = 0; k < 3; k++)
        wr32(e, 0x004ffe90 + 4 * (uint32_t)k, rd32(e, obj(e) + 0xE8 + 4 * (uint32_t)k));
    for (int k = 0; k < 3; k++) {
        uint32_t o = obj(e);
        int32_t t = (int16_t)e->ticks60(e->user);
        wr32(e, o + 0xE8 + 4 * (uint32_t)k,
             rd32(e, o + 0xE8 + 4 * (uint32_t)k) + (uint32_t)tm2_wmul(t, (int32_t)rd32(e, 0x004ffe84 + 4 * (uint32_t)k)));
    }
    int32_t c[4][3];
    for (int k = 0; k < 4; k++) {
        int32_t p[3];
        for (int j = 0; j < 3; j++)
            p[j] = (int32_t)rd32(e, 0x004ffe9c + 12 * (uint32_t)k + 4 * (uint32_t)j);
        mat(e, m);
        tm2_mat_vec_mul_row(m, p, c[k]);
        for (int j = 0; j < 3; j++)
            c[k][j] = (int32_t)((uint32_t)c[k][j] + rd32(e, obj(e) + 0xE8 + 4 * (uint32_t)j));
    }
    int32_t h[4];
    for (int k = 0; k < 4; k++)
        h[k] = e->ground(e->user, c[k]);
    int32_t a0 = tm2_atan2((int32_t)((uint32_t)h[0] - (uint32_t)h[1]), 0x50);
    wr16(e, obj(e) + 0xE0, (uint32_t)(0u - (uint32_t)a0));
    int32_t a1 = tm2_atan2((int32_t)((uint32_t)h[3] - (uint32_t)h[2]), 0x50);
    wr16(e, obj(e) + 0xE2, (uint32_t)a1);
    rebuild(e);
    int32_t owner = (int32_t)rd32(e, 0x004ffe70) >> 16;
    int32_t hit = e->hit_test(e->user, obj(e) + 0xE8, owner, 0x14, 0x915);
    if (hit > 0x1F4 || (hit > 0 && *gp(e, 0x004ffe70, 1) == 0)) {
        wr16(e, 0x004ffe76, 0);
        wr16(e, 0x004ffe74, 0);
        e->set_step(e->user, 0x915, 0, 0);
        e->explosion16(e->user, obj(e) + 0xE8, 0);
        e->weapon_hit(e->user, hit, 0, 0, 0x19, 0x004ffe84, (int32_t)rd32(e, 0x004ffe70) >> 16);
    }
    if ((int32_t)rd32(e, 0x004ffe74) >> 16 > 0)
        return;
    wr16(e, 0x004ffe76, 0);
    wr16(e, 0x004ffe74, 0);
    wr32(e, obj(e) + 8, 0);
    e->set_step(e->user, 0x915, 0, 0);
    uint32_t o = obj(e);
    wr32(e, o + 0xF0, rd32(e, o + 0xF0) + 0x50);
    e->explosion24(e->user, o + 0xE8, 0);
    uint32_t r = rd32(e, 0x004ffecc) + 0x0E;
    wr16(e, r, (uint32_t)(rd16(e, r) - 1));
}
