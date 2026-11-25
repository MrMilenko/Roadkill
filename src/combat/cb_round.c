/* cb_round.c: see cb_round.h */
#include "combat/cb_round.h"

static uint8_t* gp(const CbRoundEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbRoundEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static void wr32(const CbRoundEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }
static void wr16(const CbRoundEnv* e, uint32_t a, uint32_t v) { w_s16(gp(e, a, 2), 0, v); }
static void wr8(const CbRoundEnv* e, uint32_t a, uint32_t v) { w_s8(gp(e, a, 1), 0, v); }

void cb_weapons_round_reset(const CbRoundEnv* e) {
    int32_t d = e->difficulty(e->user);
    int32_t sh = d < 2 ? e->difficulty(e->user) : 2;
    int32_t ev = (int8_t)e->event_type(e->user);
    wr32(e, 0x004ffed8, (uint32_t)((int32_t)(300u - 4u * (uint32_t)ev) >> (sh & 31)));
    for (int k = 0; k < 30; k++) {
        uint32_t r = 0x004ff308 + 0x28 * k;
        wr8(e, r, 0);
        wr16(e, r + 4, 0);
        for (int j = 0; j < 8; j++)
            wr32(e, r + 8 + 4 * j, 0);
    }
    for (int k = 0; k < 5; k++)
        wr16(e, 0x004ff7fc + 0x14 * k, 0xFFFF);
    wr16(e, 0x004ff7b8, 0);
    wr16(e, 0x004ff7d8, 0);
    for (int k = 0; k < 10; k++) {
        uint32_t r = 0x004ff8d4 + 0x30 * k;
        uint32_t o = rd32(e, r + 0x28);
        wr16(e, r, 0);
        if (o) {
            wr32(e, o + 8, 0);
            wr32(e, r + 0x28, 0);
        }
    }
    for (int k = 0; k < 4; k++) {
        uint32_t r = 0x004ffac8 + 0xC * k;
        wr16(e, r + 2, 0);
        wr32(e, r + 4, 0);
        uint32_t o = rd32(e, r + 8);
        wr16(e, r, 0);
        if (o) {
            wr32(e, o + 8, 0);
            wr32(e, r + 8, 0);
        }
    }
    int32_t t = (int32_t)rd32(e, 0x004ffe1e) >> 16;
    wr16(e, 0x004ffe42, 0);
    wr8(e, 0x004ffe34, 0);
    if (t > 0) {
        wr16(e, 0x004ffe20, 0);
        uint32_t o = rd32(e, 0x004ffe1c);
        if (o)
            wr32(e, o + 8, 0);
    }
    int32_t n = (int32_t)rd32(e, 0x004ff8a4);
    wr16(e, 0x004ffe76, 0);
    wr16(e, 0x004ffe74, 0);
    if (n > 0) {
        int32_t end = (int32_t)((uint32_t)n * 0x12u);   /* wraps as the original */
        int32_t off = 0;
        do {
            uint32_t r = 0x004ff85c + (uint32_t)off;
            wr8(e, r, 0);
            wr8(e, r + 1, 1);
            wr8(e, r + 2, 0);
            wr8(e, r + 7, 0);
            wr8(e, r + 3, 1);
            wr16(e, r + 0x10, 0);
            wr8(e, r + 0x0B, 0);
            off += 0x12;
        } while (off < end);
    }
    e->scene_rotate_a(e->user, 0x2E3);
    e->scene_rotate_b(e->user, -0x71);
}
