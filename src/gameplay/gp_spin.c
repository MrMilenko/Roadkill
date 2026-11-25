/* gp_spin.c: see gp_spin.h */
#include "gameplay/gp_spin.h"

#include "core/tm2rand.h"
#include "weapons/wmem.h"

static void add16(uint8_t* p, int o, int32_t v) { w_s16(p, o, (uint32_t)(w_u16(p, o) + (uint32_t)v)); }
static int32_t div2(int32_t v) { return v / 2; }
static int32_t div4(int32_t v) { return v / 4; }

void gp_car_spin_hit(GpSpinHitEnv* env, uint8_t* car, uint8_t human, uint8_t kind, uint8_t keep_speed) {
    uint8_t* w = car + (human ? 0x58C : 0x5D0);
    int32_t weight = w_i16(car, human ? 0x1B2 : 0x566);
    uint8_t* b = car + (human ? 0x6C4 : 0x708);
    uint8_t* f = car + (human ? 0x9A : 0x68);
    const uint8_t* p = car + (human ? 0x138 : 0x4EC);
    uint8_t* o = car + (human ? 0x7EC : 0x830);
    int32_t t = env->ticks;

    if (human && w_i16(p, 0x3C) != 0 && kind == 7 && f[0x27] != 0 && f[0x28] == 0) {
        f[0x28] = 1;
        f[0x0B] = 0;
        w_s16(w, 0x2A, 0);
        w_s16(w, 0x72, 0);
        w_s16(w, 0xBA, 0);
        w_s16(w, 0x102, 0);
    }
    if (f[0x0B] != 0)
        return;
    int32_t s;
    if (kind == 7) {
        s = (0x1C0 / t) * t * 2 / 3;
        f[0x27] = 1;
    } else {
        f[0x27] = 0;
        s = (100 - weight) * (t * (0x120 / t)) / 100;
        if (s < t * (0x60 / t))
            s = (0x60 / t) * t;
    }
    switch (kind) {
    case 1:
    case 8: s = div2((0x60 / t) * t); break;
    case 4: s = s / 3; break;
    case 5: s *= 2; f[0x27] = 1; break;
    case 10: {
        s = div2(s);
        int32_t m = w_i16(p, 0x70);
        if (env->parity & 1)
            w_s16(b, 0x00, (uint32_t)(tm2_rand() % m + m));
        else
            w_s16(b, 0x00, (uint32_t)(-m - tm2_rand() % m));
        break;
    }
    case 11: s = (0x60 / t) * t * 2; break;
    case 12: s = (0x120 / t) * t * 2; break;
    case 13: s = (0x120 / t) * t * 3 / 2; break;
    default: break;
    }
    w_s16(w, 0x74, (uint32_t)s);
    w_s16(w, 0xBC, (uint32_t)s);
    w_s16(w, 0x104, (uint32_t)s);
    w_s16(w, 0x42, w_u16(w, 0x5C));
    w_s16(w, 0x8A, w_u16(w, 0xA4));
    w_s16(w, 0xD2, w_u16(w, 0xEC));
    w_s16(w, 0x11A, w_u16(w, 0x134));
    w_s16(w, 0x2C, (uint32_t)s);
    int32_t rate = w_i16(p, 0x62);
    if (kind == 7 || kind == 5 || kind == 13 || kind == 12) {
        int32_t sg = w_i16(b, 0x0A) < 0 ? 1 : -1;
        add16(w, 0xBA, sg);
        add16(w, 0x102, sg);
        if (tm2_rand() & 1) {
            add16(w, 0xBA, sg);
            if (tm2_rand() & 1) {
                add16(w, 0xBA, sg);
                add16(w, 0x72, -sg);
            }
        } else {
            add16(w, 0x102, sg);
            if (tm2_rand() & 1) {
                add16(w, 0x102, sg);
                add16(w, 0x2A, -sg);
            }
        }
        w_s32(o, 0, 0);
    } else {
        uint32_t r = (uint32_t)tm2_rand() & 7;
        if (r > 6)
            r = 0;
        int32_t q = w_i16(b, 0x0A);
        switch (r) {
        case 0:
            add16(w, 0x2A, -6);
            if (!keep_speed) {
                w_s32(b, 0x40, w_u32(b, 0x40) - 0x200);
                w_s16(b, 0x0A, (uint32_t)(q - div4(q)));
            }
            w_s32(o, 0, (uint32_t)-rate);
            break;
        case 1:
            add16(w, 0x102, -5);
            add16(w, 0xBA, -6);
            if (!keep_speed) {
                w_s32(b, 0x40, w_u32(b, 0x40) - 0x155);
                w_s16(b, 0x0A, (uint32_t)(q - div2(q)));
            }
            w_s32(o, 0, (uint32_t)div4(-rate));
            break;
        case 2:
            add16(w, 0x72, -7);
            if (!keep_speed) {
                w_s32(b, 0x40, w_u32(b, 0x40) + 0x200);
                w_s16(b, 0x0A, (uint32_t)(q - div4(q)));
            }
            w_s32(o, 0, (uint32_t)rate);
            break;
        case 3:
            add16(w, 0x72, -7);
            add16(w, 0x102, -4);
            if (!keep_speed) {
                w_s32(b, 0x40, w_u32(b, 0x40) + 0x400);
                w_s16(b, 0x0A, (uint32_t)(q - div2(q)));
            }
            w_s32(o, 0, (uint32_t)(rate * 2));
            break;
        case 4:
            add16(w, 0x102, -9);
            if (!keep_speed) {
                w_s32(b, 0x40, w_u32(b, 0x40) + 0x600);
                w_s16(b, 0x0A, (uint32_t)(q - div4(q)));
            }
            w_s32(o, 0, (uint32_t)rate);
            break;
        case 5:
            add16(w, 0x102, -5);
            add16(w, 0xBA, -7);
            if (!keep_speed)
                w_s32(b, 0x40, w_u32(b, 0x40) + 0x155);
            w_s32(o, 0, (uint32_t)div4(rate));
            break;
        default:
            add16(w, 0xBA, -6);
            if (!keep_speed) {
                w_s32(b, 0x40, w_u32(b, 0x40) - 0x600);
                w_s16(b, 0x0A, (uint32_t)(q - div4(q)));
            }
            w_s32(o, 0, (uint32_t)-rate);
            break;
        }
    }
    w[0x61] = 1; w[0xA9] = 1; w[0xF1] = 1; w[0x1B] = 1;
    w[0x63] = 1; w[0xAB] = 1; w[0xF3] = 1;
    uint8_t air0 = w[0x18];
    w[0x19] = 1;
    if (air0 == 0) {
        w_s16(w, 0x24, w_u16(w, 0x22));
        w_s16(w, 0x2E, (uint32_t)(w_u16(w, 0x5C) + 1));
    }
    if (w[0x60] == 0) {
        w_s16(w, 0x6C, w_u16(w, 0x6A));
        w_s16(w, 0x76, (uint32_t)(w_u16(w, 0xA4) + 1));
    }
    if (w[0xA8] == 0) {
        w_s16(w, 0xB4, w_u16(w, 0xB2));
        w_s16(w, 0xBE, (uint32_t)(w_u16(w, 0xEC) + 1));
    }
    if (w[0xF0] == 0) {
        w_s16(w, 0xFC, w_u16(w, 0xFA));
        w_s16(w, 0x106, (uint32_t)(w_u16(w, 0x134) + 1));
    }
    w[0x60] = 1; w[0xA8] = 1; w[0xF0] = 1; w[0x18] = 1;
    f[0x0B] = 1;
    f[0x01] = 1;
    switch (kind) {
    case 1: w_s32(o, 0, (uint32_t)div2(w_i32(o, 0))); break;
    case 2:
    case 3: w_s32(o, 0, w_u32(o, 0) * 2); break;
    case 4: w_s32(o, 0, (uint32_t)(w_i32(o, 0) / 3)); break;
    case 5: case 7: case 11: case 12: case 13: w_s32(o, 0, 0); break;
    default: break;
    }
    if (!human) {
        w_s32(o, 0, 0);
        f[0x01] = 0;
        if ((env->event_type(env->user) & 0xFF) == 5 || (env->event_type(env->user) & 0xFF) == 6) {
            w_s16(b, 0x0A, 0);
            w_s16(b, 0x08, 0);
        }
    }
}
