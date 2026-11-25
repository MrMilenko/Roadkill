/* cb_flame.c: see cb_flame.h */
#include "combat/cb_flame.h"

#include "math/tm2math.h"

static uint8_t* gp(const CbFlameEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbFlameEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static int16_t rd16(const CbFlameEnv* e, uint32_t a) { return w_i16(gp(e, a, 2), 0); }
static uint8_t rd8(const CbFlameEnv* e, uint32_t a) { return *gp(e, a, 1); }

static int32_t iabs(int32_t v) {
    uint32_t s = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ s) - s);
}

/* field k (0 x, 1 y, 2 z) of target i's position, fetched through a fresh 0x00411e14 call */
static int32_t tp(const CbFlameEnv* e, int32_t i, int k) {
    return (int32_t)rd32(e, e->target(e->user, i) + 8 + 4 * (uint32_t)k);
}

/* the original's distance: the getter calls in its order */
static int32_t target_dist(const CbFlameEnv* e, int32_t i, const int32_t p[3]) {
    int32_t ay = iabs((int32_t)((uint32_t)p[1] - (uint32_t)tp(e, i, 1)));
    int32_t az = iabs((int32_t)((uint32_t)p[2] - (uint32_t)tp(e, i, 2)));
    int32_t m = ay >= az ? iabs((int32_t)((uint32_t)p[1] - (uint32_t)tp(e, i, 1)))
                         : iabs((int32_t)((uint32_t)p[2] - (uint32_t)tp(e, i, 2)));
    int32_t ax = iabs((int32_t)((uint32_t)p[0] - (uint32_t)tp(e, i, 0)));
    if (ax >= m)
        return iabs((int32_t)((uint32_t)p[0] - (uint32_t)tp(e, i, 0)));
    ay = iabs((int32_t)((uint32_t)p[1] - (uint32_t)tp(e, i, 1)));
    az = iabs((int32_t)((uint32_t)p[2] - (uint32_t)tp(e, i, 2)));
    return ay >= az ? iabs((int32_t)((uint32_t)p[1] - (uint32_t)tp(e, i, 1)))
                    : iabs((int32_t)((uint32_t)p[2] - (uint32_t)tp(e, i, 2)));
}

void cb_flame_update(const CbFlameEnv* e) {
    for (uint32_t off = 0; off < 0x40; off += 0x20) {
        if (rd16(e, 0x004ff7b8 + off) <= 0)
            continue;
        int32_t v[3] = {0, 0x20, 0}, d[3], m[9];
        uint8_t* t = gp(e, 0x004ff7b8 + off, 2);
        w_s16(t, 0, (uint32_t)(w_u16(t, 0) - (uint32_t)e->ticks60(e->user)));
        uint32_t ob = rd32(e, 0x004ff7d4 + off);
        for (int k = 0; k < 9; k++)
            m[k] = (int32_t)rd32(e, ob + 0xB0 + 4 * (uint32_t)k);
        tm2_mat_vec_mul_row(m, v, d);
        uint32_t pa = rd32(e, 0x004ff7d4 + off) + 0xE8;
        int32_t p[3];
        p[0] = (int32_t)(rd32(e, pa) + 2u * (uint32_t)d[0]);
        p[1] = (int32_t)(rd32(e, pa + 4) + 2u * (uint32_t)d[1]);
        p[2] = (int32_t)(rd32(e, pa + 8) + (uint32_t)d[2]);
        int hit_done = 0;
        uint8_t* ln = gp(e, 0x004ff7ba + off, 2);
        uint8_t n = ln[0];
        w_s16(ln, 0, (uint32_t)(w_u16(ln, 0) + 1));
        if (n > 0x0F)
            n = 0x0F;
        for (int32_t j = 0; j < (int32_t)n; j++) {
            p[0] = (int32_t)((uint32_t)p[0] + (uint32_t)d[0]);
            p[1] = (int32_t)((uint32_t)p[1] + (uint32_t)d[1]);
            p[2] = (int32_t)((uint32_t)p[2] + (uint32_t)d[2] + 1u);
            if (p[2] < 8 && (int32_t)rd32(e, pa + 8) > 0)
                p[2] = 7;
            e->flame_bit(e->user, p);
            if (e->rand(e->user) % 0x32 == 0)
                e->bullet_smoke(e->user, p, 0);
            if ((j & 2) && !hit_done) {
                int32_t owner = (int32_t)rd32(e, rd32(e, 0x004ff7d4 + off) + 0x158);
                int32_t hit = e->hit_test(e->user, p, owner, 0x14, -1);
                if (hit > 0) {
                    e->weapon_hit(e->user, hit, 0, 0, 1, d, (int32_t)rd32(e, rd32(e, 0x004ff7d4 + off) + 0x158));
                    hit_done = 1;
                    uint8_t human = 0;
                    int32_t idx = 0;
                    if ((uint8_t)e->car_find(e->user, hit, &human, &idx)) {
                        uint32_t car, body, gun, stats, fl, blk;
                        if (human) {
                            car = e->human_get(e->user, idx);
                            body = car + 0x6C4; gun = car + 0x848; stats = car + 0x138; fl = car + 0xCC; blk = car + 0x9A;
                        } else {
                            car = e->ai_get(e->user, idx);
                            body = car + 0x708; gun = car + 0x834; stats = car + 0x4EC; fl = car + 0x40; blk = car + 0x68;
                        }
                        if (!rd8(e, blk + 0x1A) && !rd8(e, blk + 0x0E) && !rd8(e, blk + 0x21) && !rd8(e, blk + 0x23) &&
                            (!rd8(e, fl + 3) || rd16(e, gun + 0x32) < 0x32)) {
                            *gp(e, blk + 0x1A, 1) = 1;
                            w_s16(gp(e, stats + 0x22, 2), 0, 0);
                            w_s32(gp(e, gun + 0x44, 4), 0, e->flame5(e->user, body + 0x20, 1));
                            e->smoke_a2(e->user, body + 0x20, 0);
                            e->explosion12(e->user, body + 0x20, 0);
                        }
                    }
                }
            }
            for (int32_t i = 1; i <= 3; i++) {
                if (target_dist(e, i, p) >> 3 >= 0x23)
                    continue;
                if (rd32(e, e->target(e->user, i)) != 0)
                    continue;
                w_s32(gp(e, e->target(e->user, i), 4), 0, 1);
                e->flame_pair(e->user, e->target(e->user, i) + 8, 0);
                e->explosion16(e->user, e->target(e->user, i) + 8, 0);
                if ((uint8_t)e->event_type(e->user) == 3)
                    e->notify(e->user, 0x13EC, i, 0);
                else
                    e->notify(e->user, 0x13EC, i, 2);
            }
        }
    }
}
