/* cb_car_actions.c: see cb_car_actions.h */
#include "combat/cb_car_actions.h"

static uint8_t* cp(const CbCarActionsEnv* e, uint32_t car) { return wmem_ptr(e->mem, car, 0x880); }

void cb_car_fire_special_slot(const CbCarActionsEnv* e, int32_t i, uint8_t slot) {
    uint32_t car = e->human_car_get(e->user, i);
    uint8_t* c = cp(e, car);
    w_s16(c, 0x86E, 0);
    w_s16(c, 0x152, 0);
    c[0xE7] = 1;
    c[0xEA] = 1;
    c[0x855] = slot;
    uint8_t saved = c[0xA7];
    c[0xA7] = 0;
    w_s16(wmem_ptr(e->mem, car + 0x856 + 2 * (uint32_t)(int32_t)(int8_t)slot, 2), 0, 1);
    e->fire_special(e->user, car, 1);
    cp(e, car)[0xA7] = saved;
}

void cb_car_fire_guns(const CbCarActionsEnv* e, int32_t i, uint8_t v) {
    uint32_t car = e->human_car_get(e->user, i);
    uint8_t* c = cp(e, car);
    w_s16(c, 0x870, 0);
    c[0x84F] = v;
    uint8_t saved = c[0xA7];
    w_s16(c, 0x152, 0);
    c[0xA7] = 0;
    e->pre_guns(e->user, car);
    e->guns_update(e->user, car, 1);
    c = cp(e, car);
    uint8_t f = c[0x9A];
    c[0xA7] = saved;
    if (f || saved)
        e->rumble(e->user, 9, i);
}

void cb_car_flag_84d(const CbCarActionsEnv* e, int32_t i) {
    cp(e, e->human_car_get(e->user, i))[0x84D] = 1;
}

void cb_car_fire_0044342c(const CbCarActionsEnv* e, int32_t i, uint8_t v) {
    uint32_t car = e->human_car_get(e->user, i);
    uint8_t* c = cp(e, car);
    w_s16(c, 0x86E, 0);
    w_s16(c, 0x152, 0);
    c[0xE9] = 1;
    c[0xEA] = 1;
    w_s16(c, 0x878, 3000);
    uint8_t saved = c[0xA7];
    c[0x854] = v;
    c[0xA7] = 0;
    e->fire_44342c(e->user, car, 1);
    cp(e, car)[0xA7] = saved;
}
