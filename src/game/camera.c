/*
 * camera.c: the player's chase camera, ported from TM2.EXE 0x0043cdc0 (see camera.h).
 *
 * ORIGINAL BUG (kept): the steering ease rate clamps are inverted. The code computes
 * 13 - |steer| * 30 / limit, then "> 1 -> 1" and "< 20 -> 20", so the rate is 1 for small steering
 * and 20 otherwise (the PS1 version clamps 70 - .. to 40..98).
 */
#include "camera.h"

#include "../math/tm2math.h"
#include "../physics/terrain_tile.h"

#include <string.h>

static int32_t tm2_wsub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static void w32(uint8_t* b, int o, int32_t v) { for (int k = 0; k < 4; k++) b[o + k] = (uint8_t)((uint32_t)v >> (8 * k)); }
static void rd3(const uint8_t* b, int o, int32_t v[3]) { for (int k = 0; k < 3; k++) v[k] = i32(b, o + 4 * k); }
static void wr3(uint8_t* b, int o, const int32_t v[3]) { for (int k = 0; k < 3; k++) w32(b, o + 4 * k, v[k]); }
static void rd9(const uint8_t* b, int o, int32_t m[9]) { for (int k = 0; k < 9; k++) m[k] = i32(b, o + 4 * k); }

/* the original's wrapped blend: target brought to the same side of the wrap as current */
static int32_t unwrap(int32_t cur, int32_t t) {
    if (cur < -0x3ff && t > 0x3ff) return t - 0xfff;
    if (cur > 0x3ff && t < -0x3ff) return t + 0xfff;
    return t;
}
static int16_t blend_angle(int32_t cur, int32_t t, int32_t wc, int32_t wt) {
    int32_t v = (tm2_wmul(unwrap(cur, t), wt) + tm2_wmul(cur, wc)) / 100;
    tm2_angle_wrap32(&v);
    return (int16_t)v;
}

/* look direction: yaw, and pitch before the per view offset */
static void aim(const int32_t look[3], const int32_t pos[3], int16_t* yaw, int32_t* pitch_raw) {
    int32_t d[3];
    for (int k = 0; k < 3; k++) d[k] = tm2_wsub(look[k], pos[k]);
    int32_t dist = (int32_t)rmp_isqrt(tm2_wadd(tm2_wmul(d[0], d[0]), tm2_wmul(d[1], d[1])));
    if (d[1] == 0) d[1] = 1;
    if (dist == 0) dist = 1;
    *yaw = (int16_t)tm2_atan2(d[0], d[1]);
    *pitch_raw = tm2_atan2(d[2], dist);
}

static void fixed_view(uint8_t* c, const CamEnv* e) {
    int32_t m[9], look[3], src[3], cam[3];
    if (e->view_set) e->view_set(e->user, e->fixed_pos, e->fixed_ang);
    rd3(c[0x106] == 5 ? e->watched : c, c[0x106] == 5 ? 0x728 : 0x6e4, src);
    rd9(c, 0x714, m);
    tm2_mat_vec_mul_row(m, e->fixed_pos, look);
    for (int k = 0; k < 3; k++) look[k] = tm2_wadd(look[k], src[k]);
    wr3(c, 0x38, look);
    rd3(c, 0x14, cam);
    int16_t yaw;
    int32_t praw;
    aim(look, cam, &yaw, &praw);
    int16_t pitch = (int16_t)-praw;
    tm2_angle_wrap16(&yaw);
    tm2_angle_wrap16(&pitch);
    w16(c, 0xe, 0);
    int32_t diff = i16(c, 0x10) - yaw;
    if ((diff < 0 ? -diff : diff) < 0x201) {
        w16(c, 0xc, blend_angle(i16(c, 0xc), pitch, 9, 0x5b));
        w16(c, 0x10, blend_angle(i16(c, 0x10), yaw, 9, 0x5b));
    } else {
        w16(c, 0xc, pitch);
        w16(c, 0x10, yaw);
    }
}

static void chase_view(uint8_t* c, const CamEnv* e) {
    int view = c[0x106];
    const uint8_t* w = e->watched;
    const uint8_t* st;
    int32_t pos[3];
    int16_t pitch_src, heading;
    int lock = 0;

    if (e->view_set) e->view_set(e->user, e->fixed_pos, e->fixed_ang);
    if (view == 7) {
        st = w + 0x68;
        rd3(w, 0x728, pos);
        pitch_src = i16(w, 0x734);
        heading = i16(w, 0x73c);
        lock = w[0x87];
    } else {
        st = c + 0x9a;
        rd3(c, 0x6e4, pos);
        pitch_src = i16(c, 0x6f0);
        heading = i16(c, 0x6f8);
        if (!c[0xb9] && !c[0xb7] && !c[0xba]) {
            ++*e->lock_frames;   /* the original then reads +0xA6 / +0xA7 and keeps the lock clear */
        } else {
            lock = 1;
            w16(c, 4, 1);
            *e->lock_frames = 0;
        }
    }
    rd3(c, 0x14, e->prev_pos);

    int32_t mb[9];
    rd9(c, 0x714, mb);
    if (!lock) {
        int16_t ang[3] = { (int16_t)(pitch_src / 3), 0, heading };
        int32_t m[9], goal[3];
        tm2_build_matrix(ang, m);
        int have = 1;
        if (e->split == 0) {
            if (view == 2 || view == 7) {
                int32_t ofs[3];
                rd3(c, 0x44, ofs);
                if (view == 7) {
                    ofs[1] = tm2_wadd(ofs[1], i16(w, 0x53c));
                    ofs[2] = tm2_wadd(ofs[2], i16(w, 0x532) + i16(w, 0x52e));
                }
                tm2_mat_vec_mul_row(m, ofs, goal);
            } else {
                int32_t ofs[3];
                rd3(c, view == 3 ? 0x50 : 0x5c, ofs);
                tm2_mat_vec_mul_row(m, ofs, goal);
            }
        } else if (e->split == 1) {
            int32_t ofs[3];
            rd3(c, 0x68, ofs);
            tm2_mat_vec_mul_row(m, ofs, goal);
        } else if (e->split == 2) {
            int32_t ofs[3];
            rd3(c, 0x74, ofs);
            tm2_mat_vec_mul_row(m, ofs, goal);
        } else {
            have = 0;
        }
        if (!have) rd3(c, 0x2c, goal);
        for (int k = 0; k < 3; k++) goal[k] = tm2_wadd(goal[k], pos[k]);
        wr3(c, 0x2c, goal);
        if (view != 7) {   /* lead by the car's speed */
            int32_t v[3] = { 0, i16(c, 0x6ce) / 32, 0 }, out[3], cam[3];
            tm2_mat_vec_mul_row(mb, v, out);
            rd3(c, 0x14, cam);
            for (int k = 0; k < 3; k++) cam[k] = tm2_wadd(cam[k], out[k]);
            wr3(c, 0x14, cam);
        }
    }
    if (view != 7 && !st[0xc] && i32(c, 0x1c) < i32(c, 0x6ec)) w32(c, 0x1c, i32(c, 0x6ec));

    if (view == 4) {
        w16(c, 4, 5);
    } else {
        int32_t rate = 20;
        if (c[0xa0]) {
            int32_t s = i16(c, 0x6c4);
            rate = 13 - (s < 0 ? -s : s) * 30 / i16(c, 0x19c);
            if (rate > 1) rate = 1;
            else if (rate < 20) rate = 20;
        }
        int32_t old = i16(c, 4), v = (old * 3 + rate * 97) / 100;
        w16(c, 4, v == old ? rate : v);
    }
    for (int k = 0; k < 3; k++) {
        int32_t ease = i16(c, 4), p = i32(c, 0x14 + 4 * k), g = i32(c, 0x2c + 4 * k);
        int32_t v = tm2_wadd(tm2_wmul(100 - ease, g), tm2_wmul(ease, p)) / 100;
        w32(c, 0x14 + 4 * k, v == p ? g : v);
    }

    if (view != 7) {   /* keep the camera at least 40 behind the car, in car space */
        int32_t d[3], out[3], t[9], cam[3];
        rd3(c, 0x14, cam);
        for (int k = 0; k < 3; k++) d[k] = tm2_wsub(cam[k], pos[k]);
        tm2_matrix_transpose(mb, t);
        tm2_mat_vec_mul_row(t, d, out);
        if (out[1] > -0x29) {
            int32_t v[3] = { out[0], -0x28, out[2] };
            tm2_mat_vec_mul_row(mb, v, out);
            for (int k = 0; k < 3; k++) cam[k] = tm2_wadd(pos[k], out[k]);
            wr3(c, 0x14, cam);
        }
        int32_t lo = i32(c, 0x84), v = tm2_wadd(lo, 0xf78) / 100;
        w32(c, 0x84, v == lo ? 0x28 : v);
    }
    int32_t lofs[3], look[3], cam[3];
    rd3(c, 0x80, lofs);
    tm2_mat_vec_mul_row(mb, lofs, look);
    for (int k = 0; k < 3; k++) look[k] = tm2_wadd(look[k], pos[k]);
    wr3(c, 0x38, look);
    rd3(c, 0x14, cam);

    int16_t yaw;
    int32_t praw;
    aim(look, cam, &yaw, &praw);
    int32_t base;
    int16_t pitch;
    if (e->split == 0) base = (view == 2 || view == 7) ? -0x9f : view == 3 ? -0xcc : -0xe3;
    else base = e->split == 1 ? -0xc1 : -0x66;   /* split > 2: the original leaves pitch unset (not reached) */
    pitch = (int16_t)(base - praw);
    tm2_angle_wrap16(&yaw);
    tm2_angle_wrap16(&pitch);
    if (!lock) {
        w16(c, 0xe, 0);
        w16(c, 0xc, blend_angle(i16(c, 0xc), pitch, 0xd, 0x57));
        w16(c, 0x10, blend_angle(i16(c, 0x10), yaw, 0x14, 0x50));
    } else {
        w16(c, 0xc, st[0x1f] ? blend_angle(i16(c, 0xc), pitch, 9, 0x5b) : blend_angle(i16(c, 0xc), pitch, 1, 99));
        w16(c, 0xe, 0);
        w16(c, 0x10, blend_angle(i16(c, 0x10), yaw, 5, 0x5f));
    }
}

void car_chase_camera(uint8_t* c, const CamEnv* e) {
    switch (c[0x106]) {
    case 4:
        if (e->multiplayer) break;
        /* fall through */
    case 1: case 2: case 3: case 7:
        chase_view(c, e);
        break;
    case 5: case 6:
        fixed_view(c, e);
        break;
    default:
        break;
    }
    if (e->view_out) {
        int32_t cam[3];
        rd3(c, 0x14, cam);
        e->view_out(e->user, c + 0xc, cam);
    }
}

void car_camera_init(uint8_t* c) {
    static const int32_t look[3] = { 0, 0x28, 0 };
    static const int32_t chase[3] = { 0, -0x118, 0x70 }, v3[3] = { 0, -0x1e0, 0xa8 }, other[3] = { 0, -0x4b0, 800 };
    int id = i16(c, 0x90);
    wr3(c, 0x80, look);
    wr3(c, 0x44, chase);
    wr3(c, 0x50, v3);
    wr3(c, 0x5c, other);
    int32_t s1[3] = { 0, -0xb0, (id == 0xb0 || id == 0xe0) ? 0xa0 : 0x50 };
    if (id == 0xb0 || id == 0xe0) s1[1] = -0xc0;
    wr3(c, 0x68, s1);
    int32_t s2[3] = { 0, -0xf0, 0x50 };
    if (id == 0x60 || id == 0x20 || id == 0xc0) { s2[2] = 0x48; s2[1] = -0xe0; }
    else if (id == 0xb0 || id == 0xe0) { s2[2] = 0x78; s2[1] = -400; }
    wr3(c, 0x74, s2);
}
