/* cb_car_parts.c: see cb_car_parts.h */
#include "combat/cb_car_parts.h"

static uint8_t* gp(const CbCarPartsEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbCarPartsEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static void wr32(const CbCarPartsEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }

void cb_car_parts_spin(const CbCarPartsEnv* e, int32_t type, int32_t speed) {
    uint32_t angle, objs;
    int n;
    switch (type) {
    case 0x20: angle = 0x00502820; objs = 0x00502834; n = 4; break;
    case 0xB0: angle = 0x00502830; objs = 0x00502844; n = 2; break;
    case 0xC0: angle = 0x00502828; objs = 0x0050284c; n = 4; break;
    case 0xE0: angle = 0x00502824; objs = 0x0050285c; n = 6; break;
    case 0xF0: angle = 0x0050282c; objs = 0x00502874; n = 4; break;
    default: {
        int32_t i = type >> 4;
        if (i >= 15)
            return;
        uint32_t s = (uint32_t)(speed >> 31);
        uint32_t a = 0x00502884 + 4 * (uint32_t)i;
        uint32_t acc = rd32(e, a) + (((uint32_t)speed ^ s) - s);
        wr32(e, a, acc);
        if ((int32_t)acc > 0x140) {
            e->step_frames(e->user, i, speed);
            wr32(e, a, 0);
        }
        return;
    }
    }
    wr32(e, angle, rd32(e, angle) + (uint32_t)(speed / 7));
    int32_t m[12];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            m[3 * r + c] = r == c ? 0xFFF : 0;
    m[9] = m[10] = m[11] = 0;
    e->rot_x(e->user, (int32_t)(0u - rd32(e, angle)), m);
    for (int k = 0; k < n; k++) {
        uint32_t o = rd32(e, objs + 4 * k);
        if (!o)
            continue;
        for (int j = 0; j < 9; j++)
            wr32(e, rd32(e, objs + 4 * k) + 4 + 4 * j, (uint32_t)m[j]);
    }
}
