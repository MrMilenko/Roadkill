/* cb_area_damage.c: see cb_area_damage.h */
#include "combat/cb_area_damage.h"

static uint32_t rd32(const CbAreaEnv* e, uint32_t a) { return w_u32(wmem_ptr(e->mem, a, 4), 0); }
static uint8_t rd8(const CbAreaEnv* e, uint32_t a) { return *wmem_ptr(e->mem, a, 1); }

static int32_t iabs(uint32_t v) {
    uint32_t s = (uint32_t)((int32_t)v >> 31);
    return (int32_t)((v ^ s) - s);
}

/* the largest of |dx|, |dy|, |dz| (signed compares), >> 3 */
static int32_t dist(const CbAreaEnv* e, uint32_t a, uint32_t b) {
    int32_t ax = iabs(rd32(e, a) - rd32(e, b));
    int32_t ay = iabs(rd32(e, a + 4) - rd32(e, b + 4));
    int32_t az = iabs(rd32(e, a + 8) - rd32(e, b + 8));
    int32_t m = ay < az ? az : ay;
    return (ax >= m ? ax : m) >> 3;
}

static void hit_car(const CbAreaEnv* e, uint32_t car, uint32_t pos, int32_t j, int human, int32_t d,
                    int16_t shooter, uint32_t armour) {
    e->car_hit(e->user, car, human, 5, 0);
    int32_t amount = (rd8(e, car + armour) ? 0x32 : 0x19) - (d >> 2);
    int32_t id = e->object_id(e->user, j, human);
    e->damage(e->user, id, amount, 0x004e6a38, 2, shooter);
    int32_t v[3] = {(int32_t)rd32(e, pos), (int32_t)rd32(e, pos + 4), (int32_t)(rd32(e, pos + 8) + 0xC8)};
    e->explosion16(e->user, v, 0);
}

void cb_area_damage(const CbAreaEnv* e, uint32_t p, int16_t shooter, int16_t big) {
    int32_t range = big ? 100 : 0x4B;
    int32_t n = e->car_count(e->user);
    for (int32_t j = 0; j < n; j++) {
        uint32_t car = e->car_get(e->user, j);
        int32_t d = dist(e, car + 0x6E4, p);
        if (d < range && rd8(e, car + 0x13D) != 4)
            hit_car(e, car, car + 0x6E4, j, 1, d, shooter, 0xA4);
    }
    n = e->ai_count(e->user);
    for (int32_t j = 0; j < n; j++) {
        uint32_t car = e->ai_get(e->user, j);
        int32_t d = dist(e, car + 0x728, p);
        if (d < range && rd8(e, car + 0x4F1) != 4)
            hit_car(e, car, car + 0x728, j, 0, d, shooter, 0x72);
    }
    uint32_t t = e->destructibles(e->user);
    for (int k = 0; k < 50; k++) {
        uint32_t r = t + 0x14 * (uint32_t)k;
        if ((int16_t)(rd32(e, r + 2) >> 16) <= 0)
            continue;
        if (dist(e, r + 8, p) < 0x32)
            e->destructible_hit(e->user, (int16_t)rd32(e, r), (int32_t)rd32(e, r) >> 16, 0x19);
    }
    uint32_t q = e->props(e->user);
    for (int32_t k = 0; k < 6; k++) {
        uint32_t r = q + 0x4C * (uint32_t)k;
        if ((int32_t)rd32(e, r + 2) >> 16 == -1)
            continue;
        int32_t v[3] = {(int32_t)rd32(e, 0x004e6a50), (int32_t)rd32(e, 0x004e6a54), (int32_t)rd32(e, 0x004e6a58)};
        if (dist(e, rd32(e, r + 0x48) + 0xE8, p) < 0x32)
            e->prop_hit(e->user, k, v);
    }
}
