/*
 * car.c: car block steps, ported from TM2.EXE (see car.h).
 */
#include "car.h"

#include "../math/tm2math.h"
#include "../physics/terrain_probe.h"
#include "../physics/terrain_tile.h"
#include "../core/tm2rand.h"
#include "../physics/grp.h"

const CarLayout CAR_PLAYER = { 0x6c4, 0x09a, 0x138, 0x58c, 0x7ec, 0x09b, 0x0cc, 0x21c };
const CarLayout CAR_AI = { 0x708, 0x068, 0x4ec, 0x5d0, 0x830, 0x446, 0x040, 0x0cc };

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static void w32(uint8_t* b, int o, int32_t v) {
    uint32_t u = (uint32_t)v;
    b[o] = (uint8_t)u; b[o + 1] = (uint8_t)(u >> 8); b[o + 2] = (uint8_t)(u >> 16); b[o + 3] = (uint8_t)(u >> 24);
}
static int32_t iabs(int32_t v) { return (int32_t)(v < 0 ? 0u - (uint32_t)v : (uint32_t)v); }

void car_snapshot(uint8_t* b, uint8_t* c) {
    c[0x19] = 1;
    c[1] = 0;
    if (c[0xc]) { w32(b, 0x4c, 0); return; }
    w32(b, 0x44, i16(b, 0x10));
    w32(b, 0x48, i16(b, 0x12));
    w32(b, 0x4c, i16(b, 0x14));
    w32(b, 0x38, i32(b, 0x2c));
    w32(b, 0x3c, i32(b, 0x30));
    w32(b, 0x40, i32(b, 0x34));
}

void car_slide_check(uint8_t* car, int player) {
    const CarLayout* L = car_layout(player);
    uint8_t *b = car + L->body, *c = car + L->controls;
    if (car[L->flag] && (tm2_frame_ticks60() * 0x2f8 / 100) * 0x20 < i16(b, 0xa)) {
        int32_t d = tm2_wadd(i32(b, 0x40), -i32(b, 0x34));
        tm2_angle_wrap32(&d);
        if (0x200 < iabs(d)) { c[2] = 1; return; }
    }
    c[2] = 0;
}

void car_move(uint8_t* car, int player) {
    const CarLayout* L = car_layout(player);
    uint8_t *b = car + L->body, *s = car + L->stats;
    int32_t v[3] = { i16(b, 8) / 32, i16(b, 0xa) / 32, i16(b, 0xc) / 32 }, m[9], out[3];
    for (int i = 0; i < 9; i++) m[i] = i32(b, 0xb0 + 4 * i);
    tm2_mat_vec_mul_row(m, v, out);
    w32(b, 0x20, tm2_wadd(i32(b, 0x20), out[0]));
    w32(b, 0x24, tm2_wadd(i32(b, 0x24), out[1]));
    int32_t k = i16(s, 0x48) / 2 + 0x10;
    int32_t a = iabs(i32(b, 0x30));
    tm2_angle_wrap32(&a);
    int32_t dz;
    if (a < 0x400) {
        int32_t lo = i16(s, 0x42);
        dz = tm2_wmul(a, k) / 1024 + (lo - tm2_wmul(a, lo) / 1024);
    } else {
        int32_t hi = i16(s, 0x46);
        dz = (k - tm2_wmul(a - 0x400, k) / 1024) + tm2_wmul(hi, a - 0x400) / 1024;
    }
    w32(b, 0x28, tm2_wadd(i32(b, 0x28), dz));
}

void car_probe_latch(uint8_t* car, int player) {
    const CarLayout* L = car_layout(player);
    uint8_t *p = car + L->probes, *b = car + L->body, *c = car + L->controls;
    uint8_t was = c[0xf];
    c[0xf] = 0;
    for (int k = 0; k < 4; k++) {
        int latch = 0x1b + 0x48 * k, on = 0x19 + 0x48 * k;
        if (p[latch]) {
            if (!p[on]) p[latch] = 0;
            c[0xf] = 1;
        }
    }
    if (!c[0xf]) {
        c[0x27] = 0;
        c[0x28] = 0;
        return;
    }
    w16(b, 0x14, i16(b, 0x14) + i16(car, (int)L->turn));
    int32_t d = tm2_wadd(i32(b, 0x34), -i32(b, 0x40));
    tm2_angle_wrap32(&d);
    if (iabs(d) < 0xe3 && was && c[0x27]) car_snapshot(b, c);
}

void car_heading_follow(uint8_t* car, int player) {
    const CarLayout* L = car_layout(player);
    uint8_t *s = car + L->stats, *b = car + L->body, *c = car + L->controls;
    uint8_t *x = car + L->steer_flags, *y = car + L->steer_halve;
    int32_t t = tm2_frame_ticks60();
    int32_t spd = iabs(i16(b, 0xa));
    int32_t d = 0;
    if (spd >= i32(s, 0xb0)) {
        d = tm2_wadd(i32(b, 0x34), -i32(b, 0x40));
        tm2_angle_wrap32(&d);
        if (0x58e < iabs(d) && (t * 0x17c / 100) * 0x20 < i16(b, 0xa)) return;
        int32_t r, q;
        if (d < 1) r = x[0x10] ? t * 5 : (x[0xf] ? 0 : t * 2);
        else r = x[0x10] ? 0 : (x[0xf] ? t * 5 : t * 2);
        if (r < 1) q = 0;
        else if (i32(s, 0xbc) < spd) q = 0;
        else if (i32(s, 0xbc) / 2 < spd) q = t * 2;
        else q = t * 5;
        if (y[3]) { r /= 2; q /= 2; }
        int32_t a = (int32_t)((uint32_t)((q + r) / 2) << 12) / 0x168;
        if (x[3]) a *= 3;
        else if (c[4]) a *= 2;
        if (player && c[0x1c] && c[0xb]) a = t * 5;
        a -= tm2_wmul(i16(s, 0x7a), a) / 200;
        int32_t h = i32(b, 0x40);
        h = d < 1 ? tm2_wadd(h, -a) : tm2_wadd(h, a);
        tm2_angle_wrap32(&h);
        w32(b, 0x40, h);
        int32_t d2 = tm2_wadd(i32(b, 0x34), -h);
        tm2_angle_wrap32(&d2);
        if (d < 1 ? d2 < 1 : d2 >= 0) return;
    }
    car_snapshot(b, c);
}

/* the derived values (int16 / int32 fields of the stats block) */
static int32_t rate(int32_t k76, int32_t t, int32_t base) { return tm2_wmul(tm2_wmul(k76, tm2_wmul(t, base) / 100), 0x20) / 100; }

void car_stats_derive(uint8_t* p) {
    int32_t k76 = i16(p, 0x76);
    w32(p, 0xac, i32(p, 0xa8));
    w32(p, 0xb0, tm2_wmul(tm2_wmul(k76, i32(p, 0xa8)), 5) / 100);
    w32(p, 200, tm2_wmul(i32(p, 0xac), i16(p, 0x5c)));
    if (!p[0]) w32(p, 0xb0, i32(p, 0xb0) / 2);
    w16(p, 0x60, i16(p, 0x5e));
    w16(p, 0x62, tm2_wmul(tm2_wmul(k76, i16(p, 0x6c)), i16(p, 0x60)) / 100);
    int16_t fps = (int16_t)tm2_frame_fps();
    if (fps != 0 && fps != 0x3c) {
        w32(p, 0xac, tm2_wmul(i32(p, 0xac), 0x3c) / fps);
        w32(p, 0xb0, tm2_wmul(i32(p, 0xb0), 0x3c) / fps);
        w32(p, 200, tm2_wmul(i32(p, 200), 0x3c) / fps);
        w16(p, 0x60, i16(p, 0x60) * 0x3c / fps);
        w16(p, 0x62, i16(p, 0x62) * 0x3c / fps);
    }
    w32(p, 0xb8, tm2_wmul(i32(p, 0xb0), 10));
    w16(p, 0x70, i16(p, 0x62) * 5);
    w16(p, 0x72, i16(p, 0x62) * 2);
    w16(p, 0x74, i16(p, 0x6a) * 0x2aa / 100);
    w16(p, 100, (int16_t)(i16(p, 0x60) * i16(p, 0x68)));
    if (!p[0]) w16(p, 100, (int16_t)(i16(p, 0x60) * i16(p, 0x68) * 10));
    int32_t t = tm2_frame_ticks60();
    int32_t e = i16(p, 0x52), f = i16(p, 0x56), g = i16(p, 0x58);
    w16(p, 0x66, i16(p, 0x64) / 2);
    k76 = i16(p, 0x76);
    w32(p, 0xb4, rate(k76, t, tm2_wmul(e, 0x14) - e));
    w32(p, 0xc4, tm2_wmul(tm2_wmul(k76, tm2_wmul(tm2_wmul(i16(p, 0x54), 0x13), t) / 100), 0x20) / 100);
    w32(p, 0xbc, rate(k76, t, tm2_wmul(f, 0x14) - f));
    w32(p, 0xc0, rate(k76, t, tm2_wmul(g, 0x14) - g));
    w32(p, 0xcc, tm2_wmul(tm2_wmul(k76, tm2_wmul(tm2_wmul(i16(p, 0x5a), 0x13), t) / 100), 0x20) / 100);
}

void car_ground_contact(uint8_t* car, int player, const CarEnv* env) {
    const CarLayout* L = car_layout(player);
    uint8_t *b = car + L->body, *s = car + L->stats, *p = car + L->probes, *c = car + L->controls;
    uint8_t *x = car + L->steer_flags, *y = car + L->steer_halve;

    for (int k = 0; k < 4; k++) {
        uint8_t* st = p + 0x18 + 0x48 * k;
        int hit = rmp_probe_update(env->world, p, st);
        rmp_probe_finish(i16(b, 0xa), i32(s, 0xb4), st, hit);
    }
    int any = 0, all = 1;
    for (int k = 0; k < 4; k++) { any |= p[0x19 + 0x48 * k] != 0; all &= p[0x19 + 0x48 * k] != 0; }
    c[0xb] = (uint8_t)any;
    c[0xc] = (uint8_t)all;
    if (all) {
        c[0x1b] = 1;
        int32_t d = iabs(tm2_wadd(i32(p, 0x5c), -i32(p, 0xa4)));
        if (i16(p, 2) < d) w16(p, 2, d);
    }
    if (c[0xb]) w32(b, 0x4c, 0);                       /* 0x00419160 */

    /* which probe gives the current tile and height */
    w16(p, 0, -1);
    int on = 0;
    for (int k = 0; k < 4; k++) {
        uint8_t* st = p + 0x18 + 0x48 * k;
        if (!st[0]) continue;
        on = 1;
        int32_t h = i16(p, k == 0 ? 0x84 : 0x18 + 0x48 * k + 0x24);   /* probe 0 reads probe 1's */
        if (!(st[1] == 0 || h + 10 < i32(st, 0x44))) continue;
        if (k > 0 && !(i16(p, 0) < 0 || i32(p, 4) < i16(st, 0x16))) continue;
        w16(p, 0, i16(st, 0xa));
        w32(p, 4, i16(st, 0x16));
    }
    if (on && !c[0xa]) w16(y, 0xe, 0xe);
    else if (!on && c[0xa]) {
        if (env->left_ground) env->left_ground(env->user, car, player);
        w16(y, 0xe, 0x27);
    }
    c[0xa] = (uint8_t)on;

    /* moving platforms (extra tiles) */
    int ride = 0, ride_all = 1;
    for (int k = 0; k < 4; k++) { ride |= p[0x20 + 0x48 * k] != 0; ride_all &= p[0x20 + 0x48 * k] != 0; }
    if (!ride) c[0x25] = 0;
    else if (!c[0xb] && !c[0x25]) {
        w16(b, 0xa, 0);
        c[0x25] = 1;
    } else if (c[0x25]) {
        int32_t slot = i16(p, 0x26);
        const uint8_t* e = env->platforms + (int64_t)slot * 0x38;
        for (int k = 0; k < 3; k++) w32(b, 0x20 + 4 * k, tm2_wadd(i32(b, 0x20 + 4 * k), i32(e, 0x28 + 4 * k)));
        w32(b, 0x34, tm2_wadd(i32(b, 0x34), i32(e, 8)));
        int32_t step = x[0x13] ? (ride_all ? 2 : -2) : (ride_all ? 1 : -1);
        const int32_t* t = env->platform_target(env->user, slot);
        for (int k = 0; k < 2; k++) {
            int32_t v = i32(b, 0x20 + 4 * k);
            w32(b, 0x20 + 4 * k, (t[k] == v || tm2_wadd(t[k], -v) < 0) ? tm2_wadd(v, -step) : tm2_wadd(v, step));
        }
    }

    /* air drag on player cars */
    if (player && 0x38 < iabs(i32(b, 0x30)) && !c[0xb] && i32(s, 0xb4) != 0) {
        int32_t q = tm2_wmul(i16(s, 0x60), i32(b, 0x30)) / 0x155;
        w16(b, 0x14, i16(b, 0x14) - tm2_wmul(i16(b, 0xa), q) / i32(s, 0xb4));
    }
}

/* raise probe state st to the floor lim (0x0042d470 per probe block) */
static void bottom_out(uint8_t* st, int32_t lim) {
    if (i32(st, 0x44) < lim) {
        st[7] = 1;
        w32(st, 0x44, lim);
        if (st[1]) {
            w16(st, 0x16, (int16_t)(i16(st, 0x44) - (i16(st, 0x18) + i16(st, 0x1a) + i16(st, 0x22))));
            w16(st, 0x12, i16(st, 0x12) - tm2_frame_ticks60());
        }
    } else if (!st[1]) {
        st[7] = 0;
    }
}

void car_suspension_limit(uint8_t* car, int player) {
    const CarLayout* L = car_layout(player);
    uint8_t *b = car + L->body, *s = car + L->stats, *p = car + L->probes, *c = car + L->controls;
    uint8_t* st[4];
    int32_t h[4];
    for (int k = 0; k < 4; k++) {
        st[k] = p + 0x18 + 0x48 * k;
        h[k] = i16(st[k], 0x1a) + i16(st[k], 0x16) + i16(st[k], 0x22) + i16(st[k], 0x18);
        if (!st[k][0]) h[k] += i16(st[k], 0x26);
        w32(st[k], 0x44, h[k]);
    }
    int32_t m01 = h[0] < h[1] ? h[1] : h[0];
    int32_t lim = m01 - i16(s, 0x48) * 7 / 8;
    bottom_out(st[0], lim);
    bottom_out(st[1], lim);
    int32_t m = m01;
    if (m < i32(st[2], 0x44)) m = i32(st[2], 0x44);
    if (m < i32(st[3], 0x44)) m = i32(st[3], 0x44);
    lim = m - i16(s, 0x4a) * 7 / 8;
    bottom_out(st[2], lim);
    bottom_out(st[3], lim);

    if (c[0xc]) return;
    int32_t v[3] = { 0, 0, 0 };
    int32_t gap = i16(s, 0x4a);
    if (st[0][7] || gap < i32(st[2], 0x44) - i32(st[0], 0x44)) { v[1] = 2; v[0] = -2; }
    if (st[1][7] || gap < i32(st[3], 0x44) - i32(st[1], 0x44)) { v[1] += 2; v[0] += 2; }
    if (st[2][7]) { v[1] -= 2; v[0] -= 2; }
    if (st[3][7]) { v[1] -= 2; v[0] += 2; }
    if (!v[0] && !v[1]) return;
    int16_t ang[3] = { 0, 0, i16(b, 0x34) };
    int32_t m3[9], out[3];
    tm2_build_matrix(ang, m3);
    tm2_mat_vec_mul_row(m3, v, out);
    for (int k = 0; k < 3; k++) w32(b, 0x20 + 4 * k, tm2_wadd(i32(b, 0x20 + 4 * k), out[k]));
}

static void fx_arg(const CarFx* f, int id, uint8_t* car, int player, int32_t arg) {
    if (f && f->fx) f->fx(f->user, id, car, player, arg);
}
static void fx(const CarFx* f, int id, uint8_t* car, int player) { fx_arg(f, id, car, player, id == CAR_FX_SCREEN ? 99 : 0); }

void car_roll_spin(uint8_t* car, int player, const CarFx* f, CarDriveTmp* tmp) {
    const CarLayout* L = car_layout(player);
    uint8_t *b = car + L->body, *s = car + L->stats, *p = car + L->probes, *c = car + L->controls;
    int32_t t = tm2_frame_ticks60();
    int32_t h0 = i32(p, 0x5c), h1 = i32(p, 0xa4), h2 = i32(p, 0xec), h3 = i32(p, 0x134);
    int32_t dsum = ((h1 - h0) + (h3 - h2)) / 2;
    int32_t k48 = i16(s, 0x48);
    int32_t roll = tm2_atan2(dsum, (int32_t)rmp_isqrt(tm2_wadd(tm2_wmul(k48, k48), -tm2_wmul(dsum, dsum))));
    int32_t spd = iabs(i16(b, 0xa));
    int32_t lim50 = i32(s, 0xb4);
    if (lim50 <= spd) lim50 = spd;
    int rough = c[0x14] || c[0x15] || i16(p, 0x40) < 0xf || i16(p, 0x88) < 0xf || i16(p, 0xd0) < 0xf ||
                i16(p, 0x118) < 0xf || i32(s, 0xb8) < spd || i16(s, 0x70) < iabs(i16(b, 0));
    if (tmp) { tmp->rough = rough; tmp->speed_ref = lim50; }

    if (!c[0x12]) {
        if (c[0x20] || c[0]) return;
        if (!rough) {
            int32_t rate = (i16(s, 0x60) < iabs(i16(b, 0x14)) || i32(s, 0xb0) <= spd) ? 9 : 5;
            w32(b, 0x30, tm2_angle_interp(i32(b, 0x30), roll, rate));
        } else {
            w32(b, 0x30, roll);
        }
        int32_t lim = 0x355 - tm2_wmul(spd, 0x1aa) / lim50;
        int32_t a = i32(b, 0x30);
        if (lim < a) w32(b, 0x30, lim);
        else if (a < -lim) w32(b, 0x30, -lim);
        if (i32(s, 0xcc) < spd && !p[0x19] && !p[0x61]) {
            int32_t d = h0 - h1, g = i16(s, 0x3e), dir = 0;
            if (g < d) dir = -1;
            else if (d < -g) dir = 1;
            if (dir) {
                c[0x12] = 1;
                w16(b, 2, i16(b, 0x40));
                w16(b, 4, i16(b, 0x40) + (dir < 0 ? -0x400 : 0x400));
                w16(b, 0x12, dir);
                w16(s, 0x20, 0);
            }
        }
        return;
    }

    /* spinning */
    int32_t who = player ? i16(car, 0) : i16(car, 4);
    if (!player && 0x32 < i16(car, 0x566)) car[0x7a] = 0;
    int32_t v = i16(b, 0xa);
    int32_t floor_v = (t * 0xb22 / 100) * 0x20;
    int32_t den = iabs(v) < floor_v ? (t * 0xb22 / 100) << 5 : iabs(v);
    int32_t r = t * 0x5b * v / den;
    if (r < 0) { if (-0x2d * t < r) r = -0x2d * t; }
    else if (r < 0x2d * t) r = 0x2d * t;
    if (!c[0xb]) {
        if (tm2_rand() & 1) fx(f, CAR_FX_SKID, car, player);
        if (!(tm2_rand() & 1)) fx(f, CAR_FX_SPARKS, car, player);
        if ((player && who == 0) || car[0x32]) fx(f, CAR_FX_SCREEN, car, player);
        fx(f, CAR_FX_SPIN_HIT, car, player);
    }
    int32_t ang = i32(b, 0x30);
    r += tm2_wmul(r, iabs(tm2_sin(ang))) / 4096;
    if (i16(b, 0x12) < 1) {
        w32(b, 0x30, ang - r);
        if (ang >= 0x6ab && ang - r < 0x6ab) w16(s, 0x20, i16(s, 0x20) + 1);
    } else {
        w32(b, 0x30, ang + r);
        if (ang < 0x6aa && 0x6a9 < ang + r) w16(s, 0x20, i16(s, 0x20) + 1);
    }
    int32_t av = iabs(i16(b, 0xa));
    if (av < (t * 0xbe / 100) * 0x20 || (av < (t * 0x3b6 / 100) * 0x20 && 0 < i16(s, 0x20))) {
        if (iabs(i32(b, 0x30)) < iabs(r * 3)) {
            c[0x12] = 0;
            w16(b, 0x12, 0);
            w32(b, 0x30, 0);
            w16(s, 0x20, 0);
        }
        return;
    }
    int32_t dh = tm2_wmul(i16(s, 0x48), tm2_sin(i32(b, 0x30))) / 4096;
    for (int k = 0; k < 4; k++) w32(p, 0x5c + 0x48 * k, i32(p, 0x5c + 0x48 * k) + dh);
    if (i16(b, 0xa) != 0) {
        int16_t v16 = i16(b, 0xa);
        int32_t bleed = i32(s, 0xc8) / 3;
        w16(b, 0xa, v16 < 1 ? (int16_t)(bleed + v16) : (int16_t)(v16 - bleed));
    }
    w32(b, 0x40, i16(b, 2));
    c[1] = 1;
    w32(b, 0x34, tm2_angle_interp(i32(b, 0x34), i16(b, 4), 3));
}

void car_wheel_bounce(uint8_t* st, int32_t cap) {
    int32_t t = tm2_frame_ticks60();
    int32_t v = (i32(st, 0x44) - i16(st, 0x1c)) / t / 8;
    int32_t u = i16(st, 0x1e);
    if (u < v) {
        if (cap <= v) v = cap;
        st[6] = 0;
        w16(st, 0x1e, v);
        w16(st, 0x20, -(int16_t)v);
        return;
    }
    if (st[1]) {
        int32_t g = i16(st, 0x12);
        w16(st, 0x1e, g < 0x3d ? g * cap / 0x3c : cap);
        w16(st, 0x20, -i16(st, 0x1e));
        int32_t w = i16(st, 0x22);
        st[6] = 0;
        if (w < 0) {
            w16(st, 0x22, w + t);
            if (0 < i16(st, 0x22)) w16(st, 0x22, 0);
        } else if (w > 0) {
            w16(st, 0x22, w - t);
            if (i16(st, 0x22) < 0) w16(st, 0x22, 0);
        }
        return;
    }
    int16_t s1e = i16(st, 0x1e);
    if (s1e == 0) {
        int32_t w = i16(st, 0x22);
        if (w == 0) return;
        if (w < 0) {
            w16(st, 0x22, w + t);
            if (i16(st, 0x22) < 1) return;
        } else {
            w16(st, 0x22, w - t);
            if (i16(st, 0x22) >= 0) return;
        }
        w16(st, 0x22, 0);
        return;
    }
    int32_t a = iabs(u) / 2;
    if (a < 2) a = 2;
    if (!st[6]) {
        int16_t n = (int16_t)(i16(st, 0x22) - a);
        w16(st, 0x22, n);
        if (n < i16(st, 0x20)) {
            w16(st, 0x20, i16(st, 0x20) / 2);
            if (i16(st, 0x20) < -2) { st[6] = 1; return; }
            w16(st, 0x22, 0);
            w16(st, 0x1e, 0);
        }
    } else {
        int16_t n = (int16_t)(i16(st, 0x22) + a);
        w16(st, 0x22, n);
        if (i16(st, 0x1e) < n) {
            w16(st, 0x1e, i16(st, 0x1e) / 2);
            if (2 < i16(st, 0x1e)) { st[6] = 0; return; }
            w16(st, 0x22, 0);
            w16(st, 0x1e, 0);
        }
    }
}

void car_wheel_dust(uint8_t* st, int32_t thr, const CarFx* f, uint8_t* car, int player) {
    (void)car;
    if (st[1]) {
        if (0x3c < i16(st, 0x12)) st[2] = 1;
        return;
    }
    /* the probe state stands in for the car: the effect needs its position (+0x3C) */
    if (thr < (i32(st, 0x44) - i16(st, 0x1c)) / tm2_frame_ticks60()) fx(f, CAR_FX_DUST, st, player);
    if (st[2]) {
        fx(f, CAR_FX_DUST, st, player);
        st[2] = 0;
    }
}

/* wheel tilt angle from two height differences (0x0042d470 tail) */
static int16_t tilt(int32_t d, int32_t len) {
    return (int16_t)tm2_atan2(d, (int32_t)rmp_isqrt(tm2_wadd(tm2_wmul(len, len), -tm2_wmul(d, d))));
}

void car_pitch_land(uint8_t* car, int player, const uint8_t* surfaces, const CarFx* f, const CarDriveTmp* tmp) {
    const CarLayout* L = car_layout(player);
    uint8_t *b = car + L->body, *s = car + L->stats, *p = car + L->probes, *c = car + L->controls;
    int32_t t = tm2_frame_ticks60();
    uint8_t* st[4] = { p + 0x18, p + 0x60, p + 0xa8, p + 0xf0 };
#define H(k) i32(st[k], 0x44)

    if (!c[0x20]) {
        if (!c[0] || c[0xa]) {
            int32_t d = ((H(2) - H(0)) + (H(3) - H(1))) / 2;
            int32_t pitch = tm2_atan2(d, (int32_t)rmp_isqrt(tm2_wadd(tm2_wmul(i16(s, 0x4a), i16(s, 0x4a)), -tm2_wmul(d, d))));
            if (!tmp->rough) {
                int32_t rate = (i16(s, 0x60) < iabs(i16(b, 0x14)) || i32(s, 0xb0) <= iabs(i16(b, 0xa))) ? 9 : 5;
                w32(b, 0x2c, tm2_angle_interp(i32(b, 0x2c), pitch, rate));
            } else {
                w32(b, 0x2c, pitch);
            }
            int32_t lim = 0x355 - tm2_wmul(iabs(i16(b, 0xa)), 0x1aa) / tmp->speed_ref;
            int32_t a = i32(b, 0x2c);
            if (lim / 2 < a) w32(b, 0x2c, lim / 2);
            else if (a < -lim) w32(b, 0x2c, -lim);
        } else {
            w32(b, 0x2c, tm2_angle_interp(i32(b, 0x2c), 0, 1));
        }
    } else {
        /* flip: the spin out of part 2 on the pitch axis, then on the roll axis */
        if (!player && 0x32 < i16(car, 0x566)) car[0x88] = 0;
        int32_t v = i16(b, 0xa);
        int32_t den = iabs(v) < (t * 0xb22 / 100) * 0x20 ? (t * 0xb22 / 100) << 5 : iabs(v);
        int32_t r = v * t * 0x5b / den;
        if (r < 0) { if (-0x2d * t < r) r = -0x2d * t; }
        else if (r < 0x2d * t) r = 0x2d * t;
        if (!c[0xb]) {
            if (tm2_rand() & 1) fx(f, CAR_FX_SKID, car, player);
            if (!(tm2_rand() & 1)) fx(f, CAR_FX_SPARKS, car, player);
            fx(f, CAR_FX_SPIN_HIT, car, player);
        }
        int32_t pa = i32(b, 0x2c);
        r += tm2_wmul(r, iabs(tm2_sin(pa))) / 4096;
        w32(b, 0x2c, pa + r);
        if (pa < 0x6aa && 0x6a9 < pa + r) w16(s, 0x20, i16(s, 0x20) + 1);
        int32_t ra = i32(b, 0x30);
        r += tm2_wmul(r, iabs(tm2_sin(ra))) / 4096;
        if (i16(b, 0x12) < 1) {
            w32(b, 0x30, ra - r);
            if (ra >= 0x6ab && ra - r < 0x6ab) w16(s, 0x20, i16(s, 0x20) + 1);
        } else {
            w32(b, 0x30, ra + r);
            if (ra < 0x6aa && 0x6a9 < ra + r) w16(s, 0x20, i16(s, 0x20) + 1);
        }
        int32_t av = iabs(i16(b, 0xa));
        if (av < (t * 0xbe / 100) * 0x20 || (av < (t * 0x3b6 / 100) * 0x20 && 0 < i16(s, 0x20))) {
            if (iabs(i32(b, 0x2c)) < iabs(r * 3)) {
                c[0x20] = 0;
                w16(b, 0x12, 0);
                w32(b, 0x2c, 0);
                w16(s, 0x20, 0);
            }
        } else {
            int32_t dh = tm2_wmul(i16(s, 0x4a), tm2_sin(i32(b, 0x2c))) / 4096;
            for (int k = 0; k < 4; k++) w32(st[k], 0x44, H(k) + dh);
            if (i16(b, 0xa) != 0) {
                int16_t v16 = i16(b, 0xa);
                int32_t bleed = i32(s, 0xc8) / 3;
                w16(b, 0xa, v16 < 1 ? (int16_t)(bleed + v16) : (int16_t)(v16 - bleed));
            }
            w32(b, 0x40, i16(b, 2));
            c[1] = 1;
            w32(b, 0x34, tm2_angle_interp(i32(b, 0x34), i16(b, 4), 3));
        }
    }

    /* ride height, wheels */
    w32(b, 0x28, tm2_wadd(tm2_wadd(H(2), H(0)), tm2_wadd(H(1), H(3))) / 4);
    uint8_t* tiltw = car + (player ? 0x108 : 0x98);
    if (player)
        for (int k = 0; k < 4; k++) car_wheel_bounce(st[k], i16(car, 0x16c));
    int32_t la = i16(st[0], 0x22), lb = i16(st[1], 0x22), lc = i16(st[2], 0x22), ld = i16(st[3], 0x22);
    w16(tiltw, 0x14, tilt(((ld - lb) + (lc - la)) / 2, i16(s, 0x4a)));
    w16(tiltw, 0x16, tilt(((ld - lc) + (lb - la)) / 2, i16(s, 0x48)));
    if (!c[0x1f] && !st[0][9] && !st[1][9] && !st[2][9] && !st[3][9])
        for (int k = 0; k < 4; k++) car_wheel_dust(st[k], i16(s, 0x34), f, car, player);

    /* landing */
    if (!c[0x1b] || c[0xc]) return;
    c[0x1b] = 0;
    int32_t drop = i16(st[0], 0x2a) - H(0);
    for (int k = 1; k < 4; k++) {
        int32_t dk = i16(st[k], 0x2a) - H(k);
        if (k == 1 ? drop <= dk : drop < dk) drop = dk;
    }
    for (int k = 0; k < 4; k++) w16(st[k], 0x2a, (int16_t)H(k));
    if (drop >= 1) {
        fx_arg(f, CAR_FX_LAND, car, player, f && f->land_flag ? -1 : 0);
        uint8_t surf = surfaces[i16(st[0], 0x10)];
        if (surf != 2 && surf != 3 && !c[0]) {
            if (0x1c2 <= drop && drop <= 999) fx_arg(f, CAR_FX_SHAKE_HARD, car, player, drop * 0x43 / 0x225 + 5);
            else if (999 < drop) fx_arg(f, CAR_FX_SHAKE_HARD, car, player, 99);
            if (0x5a <= drop && drop <= 0x1c1) fx_arg(f, CAR_FX_SHAKE, car, player, drop * 0x43 / 0x167 + 0x2b);
            else if (0x1c2 <= drop) fx_arg(f, CAR_FX_SHAKE, car, player, 99);
        }
    }
    w16(p, 2, 0);
    if (c[0x1e]) {
        c[0x12] = 1;
        w16(b, 2, i16(b, 0x40));
        int32_t d = tm2_wadd(i32(b, 0x40), -i32(b, 0x34));
        tm2_angle_wrap32(&d);
        if (d < 0) { w16(b, 0x12, 1); w16(b, 4, i16(b, 0x40) + 0x400); }
        else { w16(b, 0x12, -1); w16(b, 4, i16(b, 0x40) - 0x400); }
        w16(s, 0x20, 0);
        c[0x1e] = 0;
    }
#undef H
}

void car_drive(uint8_t* car, int player, const uint8_t* surfaces, const CarFx* f) {
    CarDriveTmp tmp;
    car_suspension_limit(car, player);
    car_roll_spin(car, player, f, &tmp);
    car_pitch_land(car, player, surfaces, f, &tmp);
}

static void wrap16_at(uint8_t* b, int o) { int16_t v = i16(b, o); tm2_angle_wrap16(&v); w16(b, o, v); }
static void wrap32x3_at(uint8_t* b, int o) {
    int32_t v[3] = { i32(b, o), i32(b, o + 4), i32(b, o + 8) };
    tm2_angle_wrap32x3(v);
    for (int k = 0; k < 3; k++) w32(b, o + 4 * k, v[k]);
}
static void build_at(uint8_t* b, int o, int16_t a0, int16_t a1, int16_t a2) {
    int16_t a[3] = { a0, a1, a2 };
    int32_t m[9];
    tm2_build_matrix(a, m);
    for (int k = 0; k < 9; k++) w32(b, o + 4 * k, m[k]);
}

void car_orient(uint8_t* car, int player) {
    uint8_t* b = car + car_layout(player)->body;
    uint8_t* w = car + (player ? 0x108 : 0x98);
    for (int k = 0; k < 3; k++) {
        w32(b, 0x2c + 4 * k, tm2_wadd(i32(b, 0x2c + 4 * k), i16(b, 0x10 + 2 * k)));
        w32(b, 0x38 + 4 * k, tm2_wadd(i32(b, 0x38 + 4 * k), i32(b, 0x44 + 4 * k)));
    }
    wrap16_at(b, 0);
    wrap32x3_at(b, 0x2c);
    wrap32x3_at(b, 0x38);
    build_at(b, 0xb0, i16(b, 0x38), i16(b, 0x3c), i16(b, 0x40));
    build_at(b, 0x50, i16(b, 0x2c), i16(b, 0x30), i16(b, 0x34));
    build_at(b, 0x80, (int16_t)(i16(b, 0x2c) + i16(w, 0x28) + i16(w, 0x14) + i16(w, 0xe)),
             (int16_t)(i16(b, 0x30) + i16(w, 0x2a) + i16(w, 0x16) + i16(w, 0x10)), i16(b, 0x34));
}

static int32_t floor_z(const uint8_t* g, int32_t zone) { return g[0x11f5 + zone] ? i32(g, 0x14 + 12 * zone) : 0; }

void car_zone_cross(uint8_t* st, int32_t nz, int32_t climb, int shared, const uint8_t* g) {
    int32_t cur = i16(st, 0x10);
    int32_t fc = floor_z(g, cur), fn = floor_z(g, nz);
    if (cur == nz) {
        if (i16(st, 0x26) != fn) w16(st, 0x26, fn);
        return;
    }
    int32_t d = fn - fc;
    if (!g[0x1218 + cur] && d < 0) d = 0;
    int32_t tile = i16(st, 0xa);
    int outside = tile < g[0x174a + nz] || g[0x176d + nz] < tile;
    if (d < 0) {
        w16(st, 0x26, fn);
        w16(st, 0x10, nz);
        if (!st[0]) {
            if (!st[1]) {
                st[0x12] = 0; st[0x13] = 0;
                st[1] = 1;
                w16(st, 0x2a, i16(st, 0x44));
            }
            st[0] = 1;
            st[0x28] = 0; st[0x29] = 0;
            w16(st, 0x14, -32 / tm2_frame_ticks60());
            w16(st, 0x16, i16(st, 0x44));
            w16(st, 0x24, i16(st, 0x26));
            return;
        }
        if (outside) w16(st, 0x24, i16(st, 0x26));
    } else if (d == 0) {
        w16(st, 0x26, fn);
        w16(st, 0x10, nz);
        if (st[0] && outside) w16(st, 0x24, i16(st, 0x26));
    } else if (-climb < i32(st, 0x44) - fn || shared) {
        w16(st, 0x26, fn);
        w16(st, 0x10, nz);
        if (st[1]) {
            w16(st, 0x16, i16(st, 0x44));
            w16(st, 0x24, i16(st, 0x26));
        }
    }
}

void car_wheels_place(uint8_t* car, int player, const uint8_t* g, const CarFx* f) {
    const CarLayout* L = car_layout(player);
    uint8_t *b = car + L->body, *s = car + L->stats, *p = car + L->probes;
    int32_t m[9], ms[9];
    for (int k = 0; k < 9; k++) { m[k] = i32(b, 0x50 + 4 * k); ms[k] = i32(b, 0xb0 + 4 * k); }
    int32_t hw = i16(s, 0x48), h = -i16(s, 0x42);
    for (int k = 0; k < 4; k++) {
        uint8_t* st = p + 0x18 + 0x48 * k;
        int32_t v[3] = { (k & 1) ? hw / 2 : -hw / 2, i16(s, k < 2 ? 0x4e : 0x50), h }, o[3];
        tm2_mat_vec_mul_row(m, v, o);
        for (int j = 0; j < 3; j++) {
            w32(st, 0x30 + 4 * j, o[j]);
            w32(st, 0x3c + 4 * j, tm2_wadd(i32(b, 0x20 + 4 * j), o[j]));
        }
    }
    int32_t v[3] = { i16(b, 8) / 32, i16(b, 0xa) / 32, i16(b, 0xc) / 32 }, look[3];
    tm2_mat_vec_mul_row(ms, v, look);
    for (int j = 0; j < 3; j++) w32(p, 0xc + 4 * j, look[j]);
    int32_t zone[4];
    for (int k = 0; k < 4; k++) {
        uint8_t* st = p + 0x18 + 0x48 * k;
        for (int j = 0; j < 3; j++) w32(st, 0x3c + 4 * j, tm2_wadd(i32(st, 0x3c + 4 * j), look[j]));
        w16(st, 0x1c, (int16_t)i32(st, 0x44));
    }
    for (int k = 0; k < 4; k++) {
        uint8_t* st = p + 0x18 + 0x48 * k;
        zone[k] = grp_find_zone(g, i32(st, 0x3c), i32(st, 0x40), i32(b, 0x28));
    }
    for (int k = 0; k < 4; k++) {
        int shared = 0;
        for (int j = 0; j < 4; j++)
            if (j != k && i16(p + 0x18 + 0x48 * j, 0x10) == zone[k]) shared = 1;
        car_zone_cross(p + 0x18 + 0x48 * k, zone[k], i16(s, 0x3e), shared, g);
    }
    if (!player) {
        w16(car, 0x45e, zone[0]);
        return;
    }
    if (zone[0] == zone[1] && zone[0] == zone[2] && zone[0] == zone[3]) w16(car, 0xc6, zone[0]);
    if (g[0x125e + zone[0]] == 1 && (car[0x106] == 3 || car[0x106] == 4)) fx_arg(f, CAR_FX_SET_STATE, car, player, 2);
}

static int8_t sb(const uint8_t* b, int o) { return (int8_t)b[o]; }

void car_drive_input(uint8_t* car, int player, const CarFx* f) {
    const CarLayout* L = car_layout(player);
    uint8_t *b = car + L->body, *s = car + L->stats, *c = car + L->controls;
    uint8_t *x = car + L->steer_flags, *tb = car + (player ? 0x848 : 0x834);
    int32_t who = player ? i16(car, 0) : i16(car, 4);
    int32_t t = tm2_frame_ticks60();
    int32_t spd = iabs(i16(b, 0xa));
    int32_t ref = spd < i32(s, 0xb4) ? i32(s, 0xb4) : spd;
    int32_t acc = i32(s, 0xb0);
    int32_t mr;
    if (!x[6] && !x[2]) {
        mr = i16(s, 0x64);
        if (c[0xb]) mr /= 2;
    } else {
        mr = i16(s, 0x6e) * i16(s, 0x64);
        if (!c[0xb]) { if (x[2] && x[6]) mr += tm2_wmul(mr * 2, spd) / ref; }
        else mr /= 4;
    }
    if (c[0xd]) { x[0x10] = 0; x[0xf] = 0; }

    /* steering -> yaw rate */
    if (!x[0x10] && !x[0xf]) {
        int16_t y = i16(b, 0);
        if (y != 0) {
            if (iabs(y) < i16(s, 0x62)) w16(b, 0, 0);
            else if (!c[0xb]) w16(b, 0, (int16_t)(y / t));
            else w16(b, 0, (int16_t)(y - y / 16));
        }
    } else {
        int32_t a = sb(x, 0x10);
        if (a < 2 && -2 < a) {
            int32_t bb = sb(x, 0xf);
            if (bb < 2 && -2 < bb) {
                int32_t lim = (t * 0x5f0 / 100) * 0x20;
                int16_t r;
                if (lim < spd) r = i16(s, 0x60);
                else r = (int16_t)(i16(s, 0x62) - (int16_t)(tm2_wmul(spd, i16(s, 0x62) - i16(s, 0x60)) / ((t * 0x5f0 / 100) << 5)));
                if (!c[1] && ((x[0x10] && i16(b, 0) < 1) || (x[0xf] && -1 < i16(b, 0)))) r = (int16_t)(r * 2);
                if (i32(s, 0xb8) < spd && (x[6] || x[2])) {
                    r = (int16_t)(r * i16(s, 0x6e));
                    if (x[2] && x[6]) r = (int16_t)(r + i16(s, 0x60));
                }
                int16_t m16 = (int16_t)mr;
                if (!x[0x10]) {
                    int16_t n = (int16_t)(i16(b, 0) - r);
                    w16(b, 0, n);
                    if (n < -mr) w16(b, 0, (int16_t)-m16);
                } else {
                    int16_t n = (int16_t)(i16(b, 0) + r);
                    w16(b, 0, n);
                    if (mr < n) w16(b, 0, m16);
                }
            } else {
                if (bb < 0x75) w16(b, 0, (int16_t)(tm2_wmul(i16(s, 0x66), -bb) / 0x7f));
                else w16(b, 0, (int16_t)(-(int16_t)(tm2_wmul(i16(s, 0x64) - i16(s, 0x66), spd) / ref) - i16(s, 0x66)));
                if (x[6]) w16(b, 0, (int16_t)(i16(b, 0) * 3));
            }
        } else {
            int16_t v;
            if (a < 0x75) v = (int16_t)(tm2_wmul(i16(s, 0x66), a) / 0x7f);
            else v = (int16_t)((int16_t)(tm2_wmul(i16(s, 0x64) - i16(s, 0x66), spd) / ref) + i16(s, 0x66));
            w16(b, 0, v);
            if (x[6]) w16(b, 0, (int16_t)(v * 3));
        }
    }

    /* body +0x14 from the yaw rate */
    if (i32(s, 0xb0) < spd) {
        if (spd / 32 < i16(s, 0x4c)) w16(b, 0x14, (int16_t)(i16(b, 0) * (i16(b, 0xa) / 32) / i16(s, 0x4c)));
        else {
            w16(b, 0x14, i16(b, 0));
            if (i16(b, 0xa) < 0) w16(b, 0x14, (int16_t)-i16(b, 0x14));
        }
    } else if (!c[6]) {
        w16(b, 0x14, 0);
    } else {
        w16(b, 0x14, (int16_t)tm2_angle_interp(i16(b, 0x14), i16(b, 0) / 2, 3));
    }
    if (c[0xd]) { x[0x12] = 0; x[0x13] = 1; }

    /* turbo meter, turbo end */
    uint32_t obj = (uint32_t)i32(tb, 0x44);
    if (c[0x1a]) {
        if (!player) {
            int32_t lim = f->ai_turbo_mode < 1 ? 0x78 : 0x3c;
            if (tm2_rand() & 1) lim *= 2;
            if (lim < i16(s, 0x22)) c[0x1a] = 0;
        } else if (!x[3] || i16(tb, 0x32) < 0x33) {
            if (x[3] && who == 0) fx_arg(f, CAR_FX_TRANSITION, car, player, 0x3f);
        } else {
            if (who == 0) w16(tb, 0x32, i16(tb, 0x32) - 0x32);
            c[0x1a] = 0;
        }
        if (!c[0x1a]) {
            if (obj) fx_arg(f, CAR_FX_TURBO_OBJ, car, player, (int32_t)obj);
            if (player) { fx(f, CAR_FX_TURBO_FLAME, car, player); fx(f, CAR_FX_TURBO_SMOKE, car, player); }
        }
    }
    if (c[0xe] && c[0x1a]) {
        c[0x1a] = 0;
        if (obj) fx_arg(f, CAR_FX_TURBO_OBJ, car, player, (int32_t)obj);
    }

    /* turbo button */
    if (!x[3]) {
        if (i32(s, 0xb4) < i16(b, 0xa)) x[0x12] = 0;
        if (player && who == 0) {
            if (i16(car, 0) == 0 && f->rumble[0]) fx_arg(f, CAR_FX_RUMBLE_STOP, car, player, 0);
            if (i16(car, 0) == 1 && f->rumble[1]) fx_arg(f, CAR_FX_RUMBLE_STOP, car, player, 1);
        }
    } else {
        x[0x12] = 0x7f;
        if (i16(tb, 0x32) < 1) {
            ref = i32(s, 0xb4);
            acc = i32(s, 0xb0);
            if (ref < i16(b, 0xa)) x[0x12] = 0;
            if (player && who == 0) {
                if (i16(car, 0) == 0 && f->rumble[0]) fx_arg(f, CAR_FX_RUMBLE_STOP, car, player, 0);
                if (i16(car, 0) == 1 && f->rumble[1]) fx_arg(f, CAR_FX_RUMBLE_STOP, car, player, 1);
                fx_arg(f, CAR_FX_TRANSITION, car, player, 0x3f);
            }
        } else {
            ref = tm2_wmul(i32(s, 0xb4), 4) / 3;
            if ((ref / 32) * 100 / (t * 0x13) < 0x8c) ref = (t * 0xa64 / 100) << 5;
            if (0xb4 < (ref / 32) * 100 / (t * 0x13)) ref = (t * 0xd5c / 100) << 5;
            acc = tm2_wmul(i32(s, 0xb0), 4);
            if (!player || who == 0) w16(tb, 0x32, i16(tb, 0x32) - 1);
            if (player && who == 0) {
                fx_arg(f, CAR_FX_RUMBLE, car, player, i16(car, 0));
                if (((!x[0xe] && i16(b, 0xa) < 1) || (x[0xe] && -1 < i16(b, 0xa))) && !c[0x22] && !c[0x21] && !c[0xb])
                    fx_arg(f, CAR_FX_TRANSITION, car, player, 1);
            }
        }
    }

    /* gas / brake */
    if (!x[0x13]) {
        if (!x[0x12]) c[4] = 0;
        else {
            c[4] = 1;
            c[8] = 0;
            c[7] = (!x[0xe] || 0 < i16(b, 0xa)) ? 0 : 1;
            int32_t sp = i16(b, 0xa);
            if (((c[7] && -i32(s, 0xc4) < sp) || (!c[7] && sp < ref)) && !c[1]) {
                if ((c[7] && sp < 0) || (!c[7] && 0 < sp)) acc -= tm2_wmul(acc - i32(s, 0xac), spd) / ref;
                w16(b, 0xa, c[7] ? (int16_t)(sp - (int16_t)acc) : (int16_t)(sp + (int16_t)acc));
            }
        }
    } else {
        if (i16(b, 0xa) != 0 && !c[0xc]) {
            int32_t d = i32(s, 0xc8);
            if (i16(b, 0xa) < 1) {
                if (x[0xe]) d /= 2;
                w16(b, 0xa, (int16_t)(i16(b, 0xa) + (int16_t)d));
                if (0 < i16(b, 0xa)) w16(b, 0xa, 0);
            } else {
                if (x[0x11]) d /= 2;
                w16(b, 0xa, (int16_t)(i16(b, 0xa) - (int16_t)d));
                if (i16(b, 0xa) < 0) w16(b, 0xa, 0);
            }
        }
        c[8] = 0;
        c[4] = 0;
        c[7] = i16(b, 0xa) < 0;
    }

    /* coasting drag, analog throttle */
    int32_t th = sb(x, 0xd);
    if (th < 2 && -2 < th) {
        if (i16(b, 0xa) != 0 && !c[4] && !c[8] && !c[0xb]) {
            int32_t d = tm2_wmul(spd, i32(s, 0xc8) - i32(s, 0xac)) / ref / 2;
            if (d < i32(s, 0xac)) d = i32(s, 0xac);
            if (i16(b, 0xa) < 1) {
                w16(b, 0xa, (int16_t)(i16(b, 0xa) + (int16_t)d));
                if (0 < i16(b, 0xa)) w16(b, 0xa, 0);
            } else {
                w16(b, 0xa, (int16_t)(i16(b, 0xa) - (int16_t)d));
                if (i16(b, 0xa) < 0) w16(b, 0xa, 0);
            }
        }
    } else {
        int32_t target = tm2_wmul(th, ref) / 100;
        int32_t diff = target - i16(b, 0xa);
        if (diff < 0) {
            int rate = 0;
            if (diff < (t * -0x3b6 / 100) * 0x20) rate = 5;
            else if (diff < (t * -0x23a / 100) * 0x20) rate = 3;
            else if (diff < (t * -0x11d / 100) * 0x20) rate = 1;
            else if (diff < (t * -0x85 / 100) * 0x20) rate = 1;
            if (rate) w16(b, 0xa, (int16_t)tm2_angle_step(i16(b, 0xa), target, rate));
        }
    }

    /* reverse */
    if (!x[0xe]) {
        c[7] = 0;
    } else {
        if (i16(b, 0xa) < 1) {
            c[7] = 1;
            if (-i32(s, 0xc4) < i16(b, 0xa)) { c[8] = 0; c[4] = 0; }
        }
        int32_t r = sb(x, 0xe);
        if (1 < r || r < -1) {
            int32_t lim = r < 10 ? 0 : tm2_wmul(i32(s, 0xb4), r - 10) / 0x75;
            int32_t v = i16(b, 0xa);
            if (v < -lim) w16(b, 0xa, (int16_t)tm2_angle_step(v, -lim, x[6] ? 1 : 5));
        }
    }

    /* engine braking on slopes */
    if (player ? (c[0x12] || c[0xb]) : c[0x12]) return;
    int push;
    if (c[0x25]) push = 0;
    else if (player) push = !(spd < i32(s, 0xb8) && x[0x14]);
    else push = c[0] != 0;
    if (push) {
        int32_t a = tm2_wmul(tm2_wmul(i32(s, 0xa8), t), 10) / 5;
        int32_t cap = tm2_wmul(tm2_wmul(i32(s, 0xa8), t), 7) / 5;
        int32_t v = tm2_wmul(i32(b, 0x38), a) / 0xaa;
        if (cap < v) v = cap;
        else if (v < -cap) v = -cap;
        w16(b, 0xa, (int16_t)(i16(b, 0xa) + (int16_t)v));
    }
}
