/* gp_sway.c: see gp_sway.h */
#include "gameplay/gp_sway.h"

#include "gameplay/gp_shake.h"
#include "math/tm2math.h"
#include "weapons/wmem.h"

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}
static int32_t wmul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
/* idiv truncating toward zero, as the original's sbb idiom does for / 4096 */
static int32_t div4096(int32_t v) { return v < 0 ? -(int32_t)((uint32_t)(-(int64_t)v) >> 12) : v >> 12; }

void gp_car_body_sway(uint8_t* car, uint8_t second) {
    uint8_t* r = car + (second ? 0x108 : 0x98);
    const uint8_t* s = car + (second ? 0x138 : 0x4EC);
    const uint8_t* b = car + (second ? 0x6C4 : 0x708);
    int32_t limit = second ? w_i32(car, 0x8C) : 0;
    if (car[second ? 0xA6 : 0x74] != 0)
        return;
    int32_t d = second ? w_i16(s, 0x64) : w_i16(s, 0x64) / 10;
    int32_t m = w_i16(s, 0x74);
    int32_t v = wmul(m, w_i16(b, 0x00)) / d;
    v = wmul(v, iabs(w_i16(b, 0x0A))) / w_i32(s, 0xC4);
    if (iabs(v) > iabs(m)) {
        if (m > 0)
            v = v > 0 ? m : -m;
        else
            v = v > 0 ? -m : m;
    }
    w_s16(r, 0x10, (uint32_t)tm2_angle_step(w_i16(r, 0x10), v, 5));
    int32_t t = tm2_atan2(w_i16(s, 0x34), w_i16(s, 0x4A));
    int32_t c;
    if (limit > 0)
        c = t <= limit ? t : limit;
    else
        c = -t > limit ? -t : limit;
    w_s16(r, 0x0E, (uint32_t)tm2_angle_step(w_i16(r, 0x0E), -c, 5));
    if (r[0x01] != 0) {
        uint32_t ph = (uint32_t)(w_u16(r, 0x06) + w_u16(r, 0x0C));
        w_s16(r, 0x06, ph);
        if (w_i16(r, 0x06) < 0x1000) {
            int32_t lim2 = w_i32(s, 0xB4) / 8;
            int32_t sp = iabs(w_i16(b, 0x0A));
            int32_t x, y;
            if (sp > lim2) {
                x = w_i16(r, 0x20);
                y = w_i16(r, 0x22);
            } else {
                x = wmul(w_i16(b, 0x0A), w_i16(r, 0x20)) / lim2;
                y = wmul(w_i16(r, 0x22), w_i16(b, 0x0A)) / lim2;
            }
            w_s16(r, 0x28, (uint32_t)div4096(wmul(x, tm2_sin(w_i16(r, 0x06)))));
            w_s16(r, 0x2A, (uint32_t)div4096(wmul(tm2_sin(w_i16(r, 0x06)), y)));
            return;
        }
        w_s16(r, 0x06, ph - 0x1000);
        if (w_i16(r, 0x04) > 0) {
            w_s16(r, 0x20, (uint32_t)(int16_t)(w_i16(r, 0x20) / 2));
            int16_t h = (int16_t)(w_i16(r, 0x22) / 2);
            w_s16(r, 0x04, (uint32_t)(w_u16(r, 0x04) - 1));
            w_s16(r, 0x22, (uint32_t)h);
            return;
        }
    } else if (r[0x00] == 0) {
        return;
    }
    gp_car_shake_start(car, r[0x00], r[0x02], 0, second);
}
