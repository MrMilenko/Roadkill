/* cb_drops.c: see cb_drops.h */
#include "combat/cb_drops.h"

static uint8_t* gp(const CbDropsEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbDropsEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static int16_t rd16(const CbDropsEnv* e, uint32_t a) { return w_i16(gp(e, a, 2), 0); }
static void wr32(const CbDropsEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }
static void wr16(const CbDropsEnv* e, uint32_t a, uint32_t v) { w_s16(gp(e, a, 2), 0, v); }

void cb_weapon_drops_update(const CbDropsEnv* e) {
    for (int k = 0; k < 25; k++) {
        uint32_t r = 0x004ffb5c + 0x1C * (uint32_t)k;
        if (rd16(e, r + 8) <= 0) {
            uint32_t o = rd32(e, r + 0x18);
            if (o && rd32(e, o + 8) == 1 && (int8_t)*gp(e, r, 1) != 0x13) {
                wr32(e, o + 8, 0);
                wr32(e, r + 0x18, 0);
            }
            continue;
        }
        int32_t ground = rd16(e, r + 0x0A);
        int32_t z = (int32_t)rd32(e, r + 0x14);
        if (ground < z) {
            int32_t nz = e->angle_step(e->user, z, ground, 5);
            uint32_t o = rd32(e, r + 0x18);
            wr32(e, r + 0x14, (uint32_t)nz);
            wr32(e, o + 0xF0, (uint32_t)nz);
        }
        int32_t t = e->ticks60(e->user);
        wr16(e, r + 8, (uint32_t)(rd16(e, r + 8) - t));
        int32_t owner = rd16(e, r + 4);
        int8_t kind = (int8_t)*gp(e, r, 1);
        if (kind == 0x14 && rd16(e, r + 8) < 0x30C)
            owner = 0;
        if ((int8_t)*gp(e, r, 1) == 0x13 && rd16(e, r + 8) < 0x7F87)
            owner = 0;
        uint32_t pos = r + 0x0C;
        wr32(e, r + 0x14, rd32(e, r + 0x14) + 0x1E);
        int32_t hit = e->hit_test(e->user, pos, owner, 0x0C, (int8_t)*gp(e, r, 1));
        wr32(e, r + 0x14, rd32(e, r + 0x14) - 0x1E);
        if (hit == 0 || hit == 9)
            continue;
        if ((int8_t)*gp(e, r, 1) == 0x14) {
            wr32(e, r + 0x14, rd32(e, r + 0x14) + 0x1E);
            e->explosion12(e->user, pos, 0);
            e->weapon_hit(e->user, hit, 0, 0, 0x0F, 0x004e6a38, rd16(e, r + 4));
            uint32_t o = rd32(e, r + 0x18);
            if (o)
                wr32(e, o + 8, 0);
        }
        if ((int8_t)*gp(e, r, 1) == 0x13) {
            wr32(e, r + 0x14, rd32(e, r + 0x14) + 0x1E);
            e->explosion16(e->user, pos, 0);
            if ((int8_t)e->event_type(e->user) == 2)
                e->event2(e->user, pos);
            if ((int8_t)e->event_type(e->user) == 1)
                e->event1(e->user, pos);
            if ((int8_t)e->event_type(e->user) == 3)
                e->event3(e->user, pos);
            e->area_damage(e->user, pos, rd16(e, r + 6), 1);
            int32_t c = rd16(e, r + 4) - 1;
            if (*gp(e, e->human_car_get(e->user, c) + 0x84E, 1)) {
                *gp(e, e->human_car_get(e->user, c) + 0x84E, 1) = 0;
                uint32_t a = e->human_car_get(e->user, c) + 0x86A;
                wr16(e, a, (uint32_t)(rd16(e, a) - 1));
                a = e->human_car_get(e->user, c) + 0x85C;
                wr16(e, a, (uint32_t)(rd16(e, a) - 1));
            }
            uint32_t o = rd32(e, r + 0x18);
            if (o)
                wr32(e, o + 8, 0);
            *gp(e, r + 1, 1) = 0;
        }
        wr16(e, r + 8, 0);
    }
}
