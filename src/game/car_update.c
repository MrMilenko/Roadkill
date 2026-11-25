/*
 * car_update.c: per player car update, ported from TM2.EXE 0x00415f00 (see car_update.h).
 */
#include "car_update.h"

#include "car.h"
#include "../math/tm2math.h"

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static void w32(uint8_t* b, int o, int32_t v) {
    uint32_t u = (uint32_t)v;
    b[o] = (uint8_t)u; b[o + 1] = (uint8_t)(u >> 8); b[o + 2] = (uint8_t)(u >> 16); b[o + 3] = (uint8_t)(u >> 24);
}
static int32_t iabs(int32_t v) { return (int32_t)(v < 0 ? 0u - (uint32_t)v : (uint32_t)v); }

#define OP(name) do { if (o->name) o->name(o->user, car); } while (0)

/* 0x0045c248 / 0x0045c2a0: out of bounds heights by event type */
static int32_t oob_low(const uint8_t* car, uint8_t ev) {
    switch (ev) {
    case 5: case 0xc: return 6000;
    case 6: return 0xf00;
    case 8: return (i32(car, 0x6ec) < 0 && i32(car, 0x6e8) < -0x3660) ? 0 : -8000;
    default: return -8000;
    }
}
static int32_t oob_back(uint8_t ev) { return ev == 8 ? -800 : 0x50; }

/* the exported 0x0045c248 (EAX car, DL player flag: an AI car never gets event 8's tunnel case) */
int32_t car_oob_low(const uint8_t* car, uint8_t ev, int player) { return !player && ev == 8 ? -8000 : oob_low(car, ev); }
/* the exported 0x0045c2a0 */
int32_t car_oob_back(uint8_t ev) { return oob_back(ev); }

void car_player_update(uint8_t* car, const CarUpdateOps* o) {
    uint8_t *b = car + 0x6c4, *c = car + 0x9a, *x = car + 0xcc;
    int32_t t = tm2_frame_ticks60();
    car_stats_derive(car + 0x138);
    int32_t fps = tm2_frame_fps();
    int32_t ref = i16(car, 0x14c);
    if (ref != fps) {
        /* the decompiled C reads +0x14C again after storing fps into it; the machine code uses the
         * value loaded before the store (checked under emulation), so all three use the old rate */
        w16(car, 0x6ce, ref * i16(car, 0x6ce) / fps);
        w16(car, 0x6c4, ref * i16(car, 0x6c4) / fps);
        w16(car, 0x14c, fps);
        w16(car, 0x6d8, ref * i16(car, 0x6d8) / fps);
    }
    car[0xa3] = 0;
    w16(car, 0x22a, 0);
    if (car[0xa7]) {
        w16(car, 0x152, i16(car, 0x152) - t);
        if (i16(car, 0x152) < 1) {
            car[0xa7] = 0;
            if (i16(car, 0x154) == 1 && (!o->multiplayer || i16(car, 0) < 1)) {
                if (o->respawn_obj) o->respawn_obj(o->user, i16(car, 0x90), 3);
                w16(car, 0x154, 0);
            }
        }
    }
    int32_t speed0 = i16(car, 0x6ce);
    OP(drive_input);

    /* steering state */
    int32_t sp = i16(car, 0x6ce);
    if (!car[0x9b]) {
        if (!car[0xd2] || iabs(sp) <= i32(car, 0x1f4)) {
            if ((i32(car, 0x1f4) < iabs(sp) && i16(car, 0x19e) < iabs(i16(car, 0x6d8))) || car[0xd2]) {
                car[0x9b] = 1;
                w32(car, 0x710, 0);
            } else {
                car_snapshot(b, c);
            }
        } else {
            car[0x9b] = 1;
            w32(car, 0x710, 0);
        }
        sp = i16(car, 0x6ce);
        int32_t g = (!car[0xcc] && !car[0xcd]) ? i16(car, 0x19c) : i16(car, 0x19c) * i16(car, 0x1a6);
        int32_t r = i32(car, 0x1ec);
        if (r <= sp) r = sp;
        if (i32(car, 0x1f4) < iabs(sp)) {
            int32_t k = g - i16(car, 0x1a8);
            if (g - tm2_wmul(k, iabs(sp)) / r < iabs(i16(car, 0x6c4))) {
                if (sp < 1) { if (sp < 0) w16(car, 0x6ce, i16(car, 0x6ce) + (int16_t)i32(car, 0x200)); }
                else w16(car, 0x6ce, i16(car, 0x6ce) - (int16_t)i32(car, 0x200));
            }
        }
    } else if (!car[0x221] || car[0xb3]) {
        int f = o->frame_parity & 1;
        int lat0 = car[0x5a5] != 0, lat1 = car[0x5ed] != 0;
        int go = (lat0 && lat1) ? 0 : ((lat0 || lat1) ? !f : 1);
        if (go) {
            if (car[0xd2]) { car[0x9b] = 1; w32(car, 0x710, 0); }
            if (car[0xb6] || !car[0xa5]) car_heading_follow(car, 1);
            int32_t d = tm2_wadd(i32(car, 0x6f8), -i32(car, 0x704));
            tm2_angle_wrap32(&d);
            int32_t ad = iabs(d), base = i32(car, 0x200), m;
            if (ad < 0x2aa) m = base * 2 / 3;
            else if (ad < 0x400) m = base * 3 / 4;
            else if (ad < 0x58e) m = base;
            else if (!x[3]) m = c[4] ? base * 2 : base * 3 / 2;
            else m = base * 3;
            if (i16(b, 0xa) != 0 && !c[0xb]) {
                int32_t v = (i16(car, 0x5b8) * t / 32) * 32;
                if (0 < v) {
                    int32_t w = base * 2 * v / iabs(i16(b, 0xa));
                    int32_t e = m;
                    if (w <= m) { e = w; if (w < -m) e = -m; }
                    int32_t mm = i16(car, 0x5cc);
                    if (i16(car, 0x614) <= mm) mm = i16(car, 0x614);
                    int32_t n = i16(car, 0x65c);
                    if (mm < n) n = mm;
                    if (i16(car, 0x6a4) <= n) n = i16(car, 0x6a4);
                    n /= t;
                    if (0 < n && n < 5) e = e * 5 / n;
                    m += e;
                }
            }
            if (i16(b, 0xa) < 1) {
                w16(b, 0xa, i16(b, 0xa) + (int16_t)m);
                if (0 < i16(b, 0xa)) car_snapshot(b, c);
            } else {
                w16(b, 0xa, i16(b, 0xa) - (int16_t)m);
                if (i16(b, 0xa) < 0) car_snapshot(b, c);
            }
        }
    } else if (iabs(sp) < i32(car, 0x1e8)) {
        car_snapshot(b, c);
    }

    w32(car, 0x8c, i16(car, 0x6ce) - speed0);
    OP(wheels_place);
    OP(terrain);
    car_probe_latch(car, 1);
    if (car[0xae]) OP(step_4305fc);
    OP(step_420e48);
    uint32_t pa = (uint32_t)i32(car, 0x890);
    if (pa && o->object) {
        uint8_t* p = o->object(o->user, pa);
        if (p[0] == 0) {
            if (p[7]) {
                if (o->pickup_432340) o->pickup_432340(o->user, i16(p, 0xe), i16(car, 0x6f8));
                w16(p, 0xe, -1);
                p[7] = 0;
            }
        } else {
            if (p[1]) OP(step_430bb4);
            if (p[2]) OP(step_4316b4);
        }
    }
    OP(drive);
    if (car[0x9a] && car[0x13d] != 4) OP(step_422704);
    if (o->step_453538) o->step_453538(o->user, i16(car, 0x90), i16(car, 0x6ce));
    if (!car[0x9b]) car_snapshot(b, c);
    car_orient(car, 1);
    car_move(car, 1);

    /* out of bounds */
    if (!car[0xb9]) {
        if (i32(car, 0x6ec) < oob_low(car, o->event_type)) car[0xb9] = 1;
    } else if (i32(car, 0x6ec) < oob_back(o->event_type) && i16(car, 0) == 0) {
        w32(car, 0x1d8, -1);
    } else if (oob_low(car, o->event_type) < i32(car, 0x6ec)) {
        car[0xb9] = 0;
    }
    OP(step_453c78);
    OP(step_43e6e0);
    car_slide_check(car, 1);
    OP(step_417640);
    if (i32(car, 0x1d0)) {
        if (o->init_model) o->init_model(o->user, car, i16(car, 0x90));
        w32(car, 0x1d0, 0);
    }

    /* timer: health / points tick */
    int done = 0;
    if (!car[0xbf]) {
        if (0 < i16(car, 0x1ca)) w16(car, 0x1ca, 0);
        w16(car, 0x1ca, i16(car, 0x1ca) - t);
        if (-0xe11 < i16(car, 0x1ca)) done = 1;
        else w16(car, 0x1cc, 0);
    } else {
        if (i16(car, 0x1ca) < 0) w16(car, 0x1ca, 0);
        w16(car, 0x1ca, i16(car, 0x1ca) + t);
        if (o->multiplayer || i16(car, 0x1ca) < 0x169) done = 1;
        else {
            w16(car, 0x1cc, i16(car, 0x1cc) + 1);
            if (i16(car, 0x1cc) < 5) {
                int got = 0;
                int32_t a = i16(car, 0x86c), bb = i16(car, 0x86a);
                if (i32(car, 0x848) == 0 && ((a != bb && a - bb >= 0) || o->land_flag != 0)) {
                    int k = car[0x855];
                    w16(car, 0x856 + 2 * k, i16(car, 0x856 + 2 * k) + 1);
                    car[0xad] = 1;
                    if (o->sound) o->sound(o->user, 0x22);
                    got = 1;
                }
                if (i32(car, 0x1dc) != 0 && i32(car, 0x1d8) < i16(car, 0x172)) {
                    int32_t h = i32(car, 0x1d8) + 5;
                    w32(car, 0x1d8, h);
                    if (i16(car, 0x172) < h) {
                        w32(car, 0x1d8, i16(car, 0x172));
                        if (!got) {
                            if (o->sound) o->sound(o->user, 0x22);
                            car[0xad] = 1;
                        }
                    }
                }
            }
        }
    }
    if (!done) w16(car, 0x1ca, 0);

    /* the hidden spot: stopped exactly there, full health */
    if (!car[0x139] && i16(car, 0x6ce) == 0) {
        int32_t sx, sy;
        switch (o->event_type) {
        case 1: sy = 0xd88; sx = 0x1fa8; break;
        case 2: sy = -0x3d8; sx = -0x910; break;
        case 3: sy = -0xbc0; sx = -0xad0; break;
        case 4: sy = 0x2df0; sx = 0xa8; break;
        case 5: sy = 800; sx = 0xc0; break;
        case 6: sy = -0xa90; sx = 0x39e8; break;
        case 7: sy = -0x1838; sx = -0x490; break;
        case 8: sy = 0xa60; sx = -0xaa0; break;
        case 10: sy = -0xa70; sx = 800; break;
        case 0xb: sy = 0x2d40; sx = 0x5860; break;
        default: sy = 2000; sx = 0x2a30; break;
        }
        int32_t px = i32(car, 0x6e4), py = i32(car, 0x6e8);
        if (sx <= px && px <= sx + 0x40 && sy <= py && py <= sy + 0x40) {
            car[0x139] = 1;
            w32(car, 0x1d8, i16(car, 0x172));
        }
    }
}
