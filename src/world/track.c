/* track.c: see track.h. */
#include "track.h"

#include <string.h>

#include "../math/tm2math.h"
#include "world_misc.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static uint32_t rdu32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void add32(uint8_t* p, int32_t v) { wr32(p, tm2_wadd(rd32(p), v)); }

#define REC(i) (env->table + (i) * WORLD_TRACK_REC)

static uint8_t* track_obj(const WorldTrackEnv* env, int32_t i)
{
    return env->resolve(env->user, rdu32(REC(i) + 0x34));
}

void world_track_surface_set(const WorldTrackEnv* env, int32_t i, const int32_t p[3], int32_t h, int32_t angle)
{
    uint8_t* t = env->tiles + (env->tile_count - 1 - i) * 28;
    wr16(t + 0x08, (int16_t)tm2_sin(angle));
    wr16(t + 0x0A, (int16_t)tm2_cos(angle));
    wr32(t + 0x14, p[0]);
    wr32(t + 0x18, p[1]);
    wr16(t + 0x0C, (int16_t)p[2]);
    wr16(t + 0x0E, (int16_t)p[2]);
    wr16(t + 0x10, (int16_t)h);
    wr16(t + 0x12, (int16_t)h);
}

void world_track_vehicle_ground(const WorldTrackEnv* env, int32_t i)
{
    uint8_t* r = REC(i);
    for (int k = 0; k < 3; k++) {
        wr32(r + 0x10 + 4 * k, rd32(track_obj(env, i) + 0xE8 + 4 * k));
        wr32(r + 0x1C + 4 * k, rd32(track_obj(env, i) + 0xE8 + 4 * k));
    }
    int32_t v[3] = {0, 0xC0, 0}, out[3];
    int32_t m[9];
    memcpy(m, track_obj(env, i) + 0xB0, sizeof m);
    tm2_mat_vec_mul_row(m, v, out);
    for (int k = 0; k < 3; k++)
        add32(r + 0x10 + 4 * k, out[k]);
    for (int k = 0; k < 3; k++)
        add32(r + 0x1C + 4 * k, -out[k]);
    int32_t p[3];
    memcpy(p, r + 0x10, sizeof p);
    int32_t h0 = env->ground(env->user, p);
    memcpy(p, r + 0x1C, sizeof p);
    int32_t h1 = env->ground(env->user, p);
    uint8_t* obj = track_obj(env, i);
    wr32(obj + 0xF0, tm2_wadd(h1, h0) / 2);
    add32(track_obj(env, i) + 0xF0, 0x30);
    int32_t d = tm2_wadd(h1, -h0);
    uint32_t root = world_isqrt_inline(tm2_wadd(0x24000, -tm2_wmul(d, d)));
    int32_t a = tm2_atan2(d, (int32_t)root);
    wr16(track_obj(env, i) + 0xE0, (int16_t)a);
    obj = track_obj(env, i);
    int16_t ang[3];
    memcpy(ang, obj + 0xE0, sizeof ang);
    tm2_build_matrix(ang, m);
    memcpy(obj + 0xB0, m, sizeof m);

    int32_t n = env->car_count(env->user);
    int hit = 0;
    int32_t front = h0;
    for (int32_t c = 0; c < n && !hit; c++) {
        const uint8_t* car = env->car(env->user, c);
        obj = track_obj(env, i);
        int32_t dx = tm2_wadd(rd32(car + 0x6E4), -rd32(obj + 0xE8));
        int32_t dy = tm2_wadd(rd32(car + 0x6E8), -rd32(obj + 0xEC));
        uint32_t ax = dx < 0 ? 0u - (uint32_t)dx : (uint32_t)dx;
        uint32_t ay = dy < 0 ? 0u - (uint32_t)dy : (uint32_t)dy;
        if ((int32_t)(ax + ay) >= 800)
            continue;
        hit = 1;
        int32_t w[3] = {-0x68, -0xF0, 0};
        memcpy(m, track_obj(env, i) + 0xB0, sizeof m);
        tm2_mat_vec_mul_row(m, w, out);
        obj = track_obj(env, i);
        int32_t q[3];
        q[0] = tm2_wadd(rd32(obj + 0xE8), out[0]);
        q[1] = tm2_wadd(rd32(obj + 0xEC), out[1]);
        q[2] = tm2_wadd(h1, 0x98);
        front = tm2_wadd(front, 0x98);
        if (rd32(r + 0x0C) == 0)
            world_track_surface_set(env, i, q, front, rd16(obj + 0xE4));
    }
}

/* one semicircle step: angle = atan2(dx, dy) about the centre (cx, cy), the object's heading is
 * angle + turn (16 bit), and the object moves onto the circle of radius rad */
static void track_arc(const WorldTrackEnv* env, int32_t i, int32_t cx, int32_t cy, int32_t rad,
                      int32_t turn, int normalize)
{
    uint8_t* obj = track_obj(env, i);
    int32_t dx = tm2_wadd(rd32(obj + 0xE8), -cx);
    int32_t dy = tm2_wadd(rd32(obj + 0xEC), -cy);
    int32_t a = tm2_atan2(dx, dy);
    wr16(track_obj(env, i) + 0xE4, (int16_t)(a + turn));
    if (normalize) {
        int16_t h = rd16(track_obj(env, i) + 0xE4);
        tm2_angle_wrap16(&h);
        wr16(track_obj(env, i) + 0xE4, h);
    }
    int32_t sx = tm2_wmul(tm2_sin(a), rad) / 0xFFF;
    int32_t cy2 = tm2_wmul(tm2_cos(a), rad) / 0xFFF;
    add32(track_obj(env, i) + 0xE8, -tm2_wadd(dx, -sx));
    add32(track_obj(env, i) + 0xEC, -tm2_wadd(dy, -cy2));
}

/* straight: fixed coordinate (axis 0 = x, 1 = y) and heading */
static void track_straight(const WorldTrackEnv* env, int32_t i, int axis, int32_t v, int16_t heading)
{
    wr32(track_obj(env, i) + 0xE8 + 4 * axis, v);
    wr16(track_obj(env, i) + 0xE4, heading);
}

static int32_t ox(const WorldTrackEnv* env, int32_t i) { return rd32(track_obj(env, i) + 0xE8); }
static int32_t oy(const WorldTrackEnv* env, int32_t i) { return rd32(track_obj(env, i) + 0xEC); }

static void track_run(const WorldTrackEnv* env, int32_t i)
{
    uint8_t* r = REC(i);
    uint8_t* obj = track_obj(env, i);
    int32_t sx = rd32(obj + 0xE8), sy = rd32(obj + 0xEC), sz = rd32(obj + 0xF0);
    int32_t v[3] = {0, tm2_frame_ticks60() * 0x5F0 / 100, 0}, out[3], m[9];
    wr32(r + 0x04, v[1]);
    memcpy(m, obj + 0xB0, sizeof m);
    tm2_mat_vec_mul_row(m, v, out);
    memcpy(r + 0x28, out, sizeof out);
    add32(track_obj(env, i) + 0xE8, rd32(r + 0x28));
    add32(track_obj(env, i) + 0xEC, rd32(r + 0x2C));
    int32_t heading = rd16(track_obj(env, i) + 0xE4);
    uint32_t state = (uint32_t)rd32(r);
    if (i == 0) {
        switch (state) {
        case 0: track_straight(env, i, 0, -0x2D40, 0); if (oy(env, i) > -800) wr32(r, 1); break;
        case 1: track_arc(env, i, -0x2120, -800, 0xC20, 0x400, 0); if (ox(env, i) > -0x2120) wr32(r, 2); break;
        case 2: track_straight(env, i, 1, 0x900, 0x400); if (ox(env, i) > 0x2120) wr32(r, 3); break;
        case 3: track_arc(env, i, 0x2120, -800, 0xC20, 0x400, 0); if (oy(env, i) < -800) wr32(r, 4); break;
        case 4: track_straight(env, i, 0, 0x2D40, 0x800); if (oy(env, i) < -4000) wr32(r, 5); break;
        case 5: track_arc(env, i, 0x2120, -4000, 0xC20, 0x400, 0); if (ox(env, i) < 0x2120) wr32(r, 6); break;
        case 6: track_straight(env, i, 1, -0x1BC0, (int16_t)0xFC00); if (ox(env, i) < -0x2120) wr32(r, 7); break;
        case 7: track_arc(env, i, -0x2120, -4000, 0xC20, 0x400, 0); if (oy(env, i) > -4000) wr32(r, 0); break;
        default: break;
        }
    } else {
        switch (state) {
        case 0: track_straight(env, i, 0, -0x2E00, 0x800); if (oy(env, i) < -4000) wr32(r, 1); break;
        case 1: track_arc(env, i, -0x2120, -4000, 0xCE0, -0x400, 1); if (ox(env, i) > -0x2120) wr32(r, 2); break;
        case 2: track_straight(env, i, 1, -0x1C80, 0x400); if (ox(env, i) > 0x2120) wr32(r, 3); break;
        case 3: track_arc(env, i, 0x2120, -4000, 0xCE0, -0x400, 0); if (oy(env, i) > -4000) wr32(r, 4); break;
        case 4: track_straight(env, i, 0, 0x2E00, 0); if (oy(env, i) > -800) wr32(r, 5); break;
        case 5: track_arc(env, i, 0x2120, -800, 0xCE0, -0x400, 0); if (ox(env, i) < 0x2120) wr32(r, 6); break;
        case 6: track_straight(env, i, 1, 0x9C0, (int16_t)0xFC00); if (ox(env, i) < -0x2120) wr32(r, 7); break;
        case 7: track_arc(env, i, -0x2120, -800, 0xCE0, -0x400, 0); if (oy(env, i) < -800) wr32(r, 0); break;
        default: break;
        }
    }
    world_track_vehicle_ground(env, i);
    obj = track_obj(env, i);
    wr32(r + 0x08, tm2_wadd(rd16(obj + 0xE4), -heading));
    wr32(r + 0x28, tm2_wadd(rd32(track_obj(env, i) + 0xE8), -sx));
    wr32(r + 0x2C, tm2_wadd(rd32(track_obj(env, i) + 0xEC), -sy));
    wr32(r + 0x30, tm2_wadd(rd32(track_obj(env, i) + 0xF0), -sz));
    env->sound(env->user, i == 0 ? 0x1FA : 0x1FB, track_obj(env, i) + 0xE8);
}

void world_track_vehicles_update(const WorldTrackEnv* env)
{
    int32_t n = env->car_count(env->user);
    int busy = 0;
    for (int32_t c = 0; c < n; c++)
        if (world_section_has_moving_surfaces(rd32(env->car(env->user, c) + 0xC4) >> 16))
            busy = 1;
    if (busy) {
        for (int32_t i = 0; i < 2; i++) {
            uint8_t* r = REC(i);
            if (rd32(r + 0x0C) != 0)
                continue;
            uint8_t* obj = track_obj(env, i);
            if (rd32(obj + 8) != 0)
                continue;
            int32_t x = rd32(obj + 0xE8);
            /* ORIGINAL BUG: the snap for x <= 0xC80 is unreachable (the code compares -0x1900
             * with x and jumps to the same place both ways) */
            if (x <= 0x1900 && x > 0xC80)
                wr32(obj + 0xE8, rd32(r) == 2 ? 0x1900 : 0xC80);
            wr32(track_obj(env, i) + 8, 1);
        }
    } else {
        for (int32_t i = 0; i < 2; i++) {
            uint8_t* r = REC(i);
            if (rd32(r + 0x0C) != 0) {
                env->place(env->user, i);
                wr32(REC(i) + 0x0C, 0);
                continue;
            }
            uint8_t* obj = track_obj(env, i);
            if (rd32(obj + 8) == 0)
                continue;
            int32_t x = rd32(obj + 0xE8);
            if (x > 0x1900 || (x <= 0xC80 && (x > -0xC80 || x < -0x1900)))
                wr32(track_obj(env, i) + 8, 0);
        }
    }
    if (rdu32(REC(0) + 0x34) != 0)
        track_run(env, 0);
    if (rdu32(REC(1) + 0x34) != 0)
        track_run(env, 1);
}
