/* cb_pickups.c: see cb_pickups.h */
#include "combat/cb_pickups.h"

static void wr32(const CbPickupsEnv* e, uint32_t a, uint32_t v) { w_s32(wmem_ptr(e->mem, a, 4), 0, v); }
static void wr8(const CbPickupsEnv* e, uint32_t a, uint32_t v) { w_s8(wmem_ptr(e->mem, a, 1), 0, v); }

static void car0_reset(const CbPickupsEnv* e) {
    wr32(e, e->human_car_get(e->user, 0) + 0x1DC, 1);
    wr32(e, e->human_car_get(e->user, 0) + 0x848, 0);
    wr8(e, e->human_car_get(e->user, 0) + 0x851, 0);
    wr8(e, e->human_car_get(e->user, 0) + 0x850, 0);
    e->turbo_set(e->user, e->human_car_get(e->user, 0), 0);
}

void cb_pickup_counters_reset(const CbPickupsEnv* e) {
    for (int k = 0; k < 31; k++)
        wr32(e, 0x004ffedc + 4 * k, 0);
    car0_reset(e);
}

void cb_game_data_init(const CbPickupsEnv* e) {
    e->register_codes(e->user, 0x004e6a84, 0x15D, 0x0044e1cc);
    cb_pickup_counters_reset(e);
    for (int k = 0; k < 8; k++)
        wr32(e, 0x004ffff4 + 4 * k, 0);
    car0_reset(e);
}
