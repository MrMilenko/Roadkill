/* cb_invis.c: see cb_invis.h */
#include "combat/cb_invis.h"

static int16_t rd16(const CbInvisEnv* e, uint32_t a) { return w_i16(wmem_ptr(e->mem, a, 2), 0); }
static uint32_t rd32(const CbInvisEnv* e, uint32_t a) { return w_u32(wmem_ptr(e->mem, a, 4), 0); }
static void wr32(const CbInvisEnv* e, uint32_t a, uint32_t v) { w_s32(wmem_ptr(e->mem, a, 4), 0, v); }

void cb_invisibility_update(const CbInvisEnv* e) {
    int32_t n = e->human_count(e->user);
    for (int32_t j = 0; j < n; j++) {
        uint32_t obj = e->human_object(e->user, j);
        uint32_t car = e->human_get(e->user, j);
        if (!*wmem_ptr(e->mem, car + 0x84C, 1))
            continue;
        if (rd16(e, car + 0x87C) == 0x168)
            e->body_hide(e->user, rd16(e, car + 0x90));
        int32_t t = rd16(e, car + 0x87C);
        if (t <= 0x14F && t > 0x19)
            wr32(e, obj + 8, 0);
        if (rd16(e, car + 0x87C) <= 0x19 && rd32(e, obj + 8) == 0) {
            e->body_show(e->user, rd16(e, car + 0x90));
            wr32(e, obj + 8, 1);
        }
        int32_t tk = e->ticks60(e->user);
        w_s16(wmem_ptr(e->mem, car + 0x87C, 2), 0, (uint32_t)(rd16(e, car + 0x87C) - tk));
        t = rd16(e, car + 0x87C);
        if (t >= 0x14F || t <= 0x19) {
            uint32_t pos = obj + 0xE8;
            uint32_t p = e->jitter(e->user, pos, 0x333, 0x333, 0x333);
            e->locate(e->user, pos, e->split(e->user) == 0 ? 0 : -1);
            int32_t pan = e->pan(e->user);
            int32_t vol = e->volume(e->user);
            e->play(e->user, 0x16, vol, pan, 0x63);
            e->spark_blue(e->user, p, 1);
        }
        if (rd16(e, car + 0x87C) <= 0)
            *wmem_ptr(e->mem, car + 0x84C, 1) = 0;
    }
}
