/* cb_round_start.c: see cb_round_start.h */
#include "combat/cb_round_start.h"

static uint8_t* gp(const CbRoundStartEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbRoundStartEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static void wr32(const CbRoundStartEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }
static void wr8(const CbRoundStartEnv* e, uint32_t a, uint32_t v) { *gp(e, a, 1) = (uint8_t)v; }

void cb_round_start(const CbRoundStartEnv* e) {
    wr32(e, 0x00502f94, 0);
    wr32(e, 0x00502f78, 0);
    e->srand(e->user, (uint32_t)e->timer(e->user));
    e->call(e->user, CB_RS_470900);
    e->call(e->user, CB_RS_489A70);
    if ((uint8_t)e->event_type(e->user) == 8 && w_u16(gp(e, 0x0050763c, 2), 0) == 0xF0)
        e->call(e->user, CB_RS_4325F8);
    if ((int32_t)rd32(e, 0x00502fa4) >= 5) {
        wr8(e, 0x0050e0fc, 1);
        wr32(e, 0x0050e0f4, 1);
        wr8(e, 0x0050e0fd, 0);
        wr8(e, 0x0050f8f0, 0);
        wr32(e, 0x0050e0f8, 0);
        e->key_setup(e->user, 0x0D, 0x0D);
        while (e->key_poll(e->user, 0x0D) == 0) {
        }
        if (e->key_check(e->user) != 0) {
            e->call(e->user, CB_RS_4327FC);
            wr8(e, 0x0050f8f0, 1);
        } else {
            e->call(e->user, CB_RS_46500C);
        }
        e->key_setup(e->user, 0x0E, 0x0E);
        while (e->key_poll(e->user, 0x0E) == 0) {
        }
    }
    for (int32_t c = 0; c < (int32_t)rd32(e, 0x005075e0); c++) {
        uint32_t car = 0x005030a8 + 0x898 * (uint32_t)c;
        e->call(e->user, CB_RS_4632B0);
        e->car_start(e->user, car, w_i16(gp(e, car + 0x90, 2), 0), *gp(e, 0x00502f80 + (uint32_t)c, 1));
        wr32(e, 0x005075c0 + 4 * (uint32_t)(int32_t)(int16_t)c, 0);
        if (e->split(e->user) == 0)
            e->car_view(e->user, car, (int8_t)*gp(e, car + 0x107, 1));
    }
    e->call(e->user, CB_RS_ROUND_RESET);
    wr8(e, 0x00502f80, 0);
    wr8(e, 0x00502f81, 0);
    if (!*gp(e, 0x00502f9c, 1))
        return;
    wr32(e, 0x00502fa8, 0);
    wr32(e, 0x00502fac, 0);
    wr32(e, 0x00502fb0, 0);
    wr32(e, 0x00502fd8, 0);
    wr32(e, 0x00502f88, 1);
    int32_t r = e->rand(e->user);
    wr32(e, 0x00502f90, (uint32_t)(r % (int32_t)rd32(e, 0x0050c2b0)));
    e->car_view(e->user, 0x005030a8, 7);
    for (int k = 0; k < 4; k++)
        wr8(e, 0x00502fa0 + (uint32_t)k, 0);
    e->call(e->user, CB_RS_45C56C);
    e->call(e->user, CB_RS_45C350);
    for (int32_t k = 0; k < (int32_t)rd32(e, 0x0050c2b0); k++)
        wr8(e, 0x0050760d + 0x880 * (uint32_t)(int32_t)(int16_t)k, 1);
}
