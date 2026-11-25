/* cb_world.c: see cb_world.h */
#include "combat/cb_world.h"

static uint8_t* gp(const CbWorldEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbWorldEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static uint8_t rd8(const CbWorldEnv* e, uint32_t a) { return *gp(e, a, 1); }
static void wr32(const CbWorldEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }
static void wr16(const CbWorldEnv* e, uint32_t a, uint32_t v) { w_s16(gp(e, a, 2), 0, v); }
static void wr8(const CbWorldEnv* e, uint32_t a, uint32_t v) { w_s8(gp(e, a, 1), 0, v); }

static void locate(const CbWorldEnv* e, uint32_t pos) {
    if (e->split(e->user) == 0)
        e->locate(e->user, pos, 0);
    else
        e->locate(e->user, pos, -1);
}

void cb_warp_start(const CbWorldEnv* e, int32_t i, uint32_t car, int ai) {
    uint8_t a = (uint8_t)ai;
    int32_t type = (int32_t)rd32(e, car + (a ? 0x8E : 0x2E)) >> 16;
    if (type == 0xF0 && i == 0)
        return;
    uint32_t r = 0x00502ea0 + (uint32_t)i * 0x28;
    if (rd8(e, r) != 0)
        return;
    int32_t pair = (int8_t)rd8(e, r + 2);
    if (rd8(e, 0x00502ea0 + (uint32_t)pair * 0x28) != 0)
        return;
    wr16(e, r + 4, 0);
    wr8(e, r, 1);
    wr8(e, r + 1, a);
    uint32_t obj = rd32(e, r + 0x24);
    wr32(e, r + 0x20, car);
    wr32(e, obj + 8, 1);
    wr32(e, rd32(e, r + 0x24) + 0xE8, rd32(e, r + 8));
    wr32(e, rd32(e, r + 0x24) + 0xEC, rd32(e, r + 0x0C));
    wr32(e, rd32(e, r + 0x24) + 0xF0, rd32(e, r + 0x10));
    if (!a && rd8(e, car + 0x32) == 0)
        return;
    locate(e, rd32(e, r + 0x24) + 0xE8);
    int32_t pan = e->pan(e->user);
    int32_t vol = e->volume(e->user);
    e->play(e->user, 0x17, vol, pan, 0x63);
}

void cb_obj4b_place(const CbWorldEnv* e, uint32_t pos) {
    if ((int32_t)rd32(e, 0x004e7418) > 4)
        wr32(e, 0x004e7418, 1);
    uint32_t slot = 0x00502f34 + 4 * rd32(e, 0x004e7418);
    if (rd32(e, slot) == 0)
        return;
    wr32(e, rd32(e, slot) + 4, rd32(e, pos));
    wr32(e, rd32(e, slot) + 8, rd32(e, pos + 4));
    wr32(e, rd32(e, slot) + 0x0C, rd32(e, pos + 8));
    e->set_step(e->user, 0x2B3, (int32_t)rd32(e, 0x004e7418), 1);
    wr32(e, 0x004e7418, rd32(e, 0x004e7418) + 1);
    locate(e, pos);
    int32_t v = e->volume(e->user);
    uint32_t s = (uint32_t)(v >> 31);
    if ((int32_t)(((uint32_t)v ^ s) - s) >= 0x5DC)
        return;
    int32_t pan = e->pan(e->user);
    int32_t vol = e->volume(e->user);
    e->play(e->user, 0x22, vol, pan, 0x63);
}

void cb_car_tables_clear(const CbWorldEnv* e) {
    for (int k = 0; k < 15; k++) {
        wr8(e, 0x00502fdc + k, 2);
        wr32(e, 0x00502fec + 4 * k, 0xFFFFFFFFu);
    }
    for (int k = 0; k < 9; k++)
        wr32(e, 0x0050c28c + 4 * k, 0xFFFFFFFFu);
    for (int k = 0; k < 8; k++)
        wr32(e, 0x00503938 + 0x898 * k, 0);
    for (int k = 0; k < 9; k++)
        wr32(e, 0x00507e88 + 0x880 * k, 0);
    e->slam_records_reset(e->user);
    wr32(e, 0x0050c2b0, 0);
    wr32(e, 0x00503084, 0);
    wr32(e, 0x0050c2b8, 0);
    wr32(e, 0x0050c2bc, 0);
    e->collision_init(e->user);
    if ((int8_t)e->event_type(e->user) == 6)
        e->collapse_reset(e->user);
    else if ((int8_t)e->event_type(e->user) == 8)
        e->track_reset(e->user);
    wr32(e, 0x005075e4, 0xFFFFFFFFu);
}
