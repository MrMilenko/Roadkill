/* cb_ua.c: see cb_ua.h */
#include "combat/cb_ua.h"

static void wr32(const CbUaEnv* e, uint32_t a, uint32_t v) { w_s32(wmem_ptr(e->mem, a, 4), 0, v); }
static void wr16(const CbUaEnv* e, uint32_t a, uint32_t v) { w_s16(wmem_ptr(e->mem, a, 2), 0, v); }
static void wr8(const CbUaEnv* e, uint32_t a, uint32_t v) { w_s8(wmem_ptr(e->mem, a, 1), 0, v); }

void cb_ua_init(const CbUaEnv* e) {
    wr32(e, 0x00502f7c, 0);
    wr32(e, 0x00502f88, 0);
    wr32(e, 0x00502f8c, 0);
    wr32(e, 0x00502f90, 0);
    wr32(e, 0x00502f94, 0);
    wr32(e, 0x00502f98, 1);
    wr32(e, 0x00502f78, 0);
    for (uint32_t k = 0; k < 8; k++) {
        uint32_t o = 0x898 * k;
        wr8(e, 0x00502f80 + k, 1);
        wr32(e, 0x00503284 + o, 1);
        wr32(e, 0x0050393c + o, k);
        wr32(e, 0x0050e0f4, 1);
        wr32(e, 0x0050378c + o, 0);
        wr32(e, 0x0050e0f8, 0);
        wr32(e, 0x00503790 + o, 0);
        wr32(e, 0x00503794 + o, 0);
        wr8(e, 0x00507568 + k, 1);
        for (uint32_t j = 0; j < 6; j++)
            wr8(e, 0x0050313c + o + j, 0);
    }
    e->car_tables_clear(e->user);
    for (uint32_t k = 0; k < 8; k++)
        wr32(e, 0x00507590 + 4 * k, 1);
    wr32(e, 0x0050c2b8, 0);
    wr32(e, 0x0050c2bc, 0);
    e->fn_473160(e->user);
    wr32(e, 0x0050e0f4, 1);
    wr32(e, 0x0050e0f8, 0);
    wr8(e, 0x0050e0fc, 1);
    wr8(e, 0x0050e0fd, 0);
    wr32(e, 0x005075e0, 1);
    wr16(e, 0x005075b0, 0x90);
    e->fx_sprites_reset(e->user);
    e->srand(e->user, (uint32_t)e->timer(e->user));
    wr32(e, 0x00502fa4, 1);
    wr8(e, 0x00502f9c, 0);
    wr32(e, 0x00502fa8, 0);
    wr32(e, 0x00502fac, 0);
    wr32(e, 0x00502fb0, 0);
    wr32(e, 0x00502fd8, 0);
    for (uint32_t k = 0; k < 9; k++)
        wr32(e, 0x00502fb4 + 4 * k, 0xFFFFFFFFu);
}
