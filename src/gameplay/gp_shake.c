/* gp_shake.c: see gp_shake.h */
#include "gameplay/gp_shake.h"

#include "core/tm2rand.h"
#include "weapons/wmem.h"

static int16_t settle(int16_t v) {
    int32_t half = v / 2;
    if (v > 0)
        return (int16_t)(v - tm2_rand() % half);
    return (int16_t)(v + tm2_rand() % half);
}

void gp_car_shake_start(uint8_t* car, uint8_t kind, uint8_t axes, int16_t frames, uint8_t second) {
    uint8_t* r = car + (second ? 0x108 : 0x98);
    w_s16(r, 0x04, (uint16_t)frames);
    r[0x01] = kind;
    if (frames <= 0) {
        r[0x00] = kind;
        r[0x02] = axes;
        w_s16(r, 0x04, (uint32_t)(3 - (tm2_rand() & 3)));
    }
    w_s16(r, 0x06, (uint32_t)(tm2_rand() % 11));
    int32_t a;
    switch (kind) {
    case 2: a = 0x16; break;
    case 3: a = 0x2D; break;
    case 4: a = 0x44; break;
    case 5: a = 0x71; break;
    default: a = 5; break;
    }
    if (axes == 0 || axes == 2) {
        if (tm2_rand() & 1)
            a = -a;
        w_s16(r, 0x18, (uint32_t)a);
    } else {
        w_s16(r, 0x18, 0);
    }
    if (axes == 1 || axes == 2) {
        if (tm2_rand() & 1)
            a = -a;
        w_s16(r, 0x1A, (uint32_t)a);
    } else {
        w_s16(r, 0x1A, 0);
    }
    w_s16(r, 0x0A, 0x155);
    w_s16(r, 0x0C, 0x155);
    int16_t x = w_i16(r, 0x18);
    if (x != 0)
        w_s16(r, 0x20, (uint16_t)settle(x));
    int16_t y = w_i16(r, 0x1A);
    if (y != 0)
        w_s16(r, 0x22, (uint16_t)settle(y));
}
