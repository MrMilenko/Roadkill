/*
 * ai_steps.c: AI car sensing and route geometry steps, ported from TM2.EXE (see ai.h).
 * Verified against the original under emulation (tools/emu/check_ai_steps.sh).
 */
#include "ai.h"
#include "ai_mem.h"

#include "../game/car.h"
#include "../math/tm2math.h"
#include "../physics/terrain_tile.h"

static int32_t iabs(int32_t v) { return v < 0 ? (int32_t)(0u - (uint32_t)v) : v; }

/* bearing and distance from the car position (+0x728/72C) to (x, y) */
static int16_t bearing_to(const uint8_t* ai, int32_t x, int32_t y, uint32_t* dist) {
    int32_t dx = wsub(x, i32(ai, 0x728)), dy = wsub(y, i32(ai, 0x72c));
    *dist = rmp_isqrt(tm2_wadd(tm2_wmul(dx, dx), tm2_wmul(dy, dy)));
    if (dy == 0) dy = 1;
    int16_t a = (int16_t)tm2_atan2(dx, dy);
    tm2_angle_wrap16(&a);
    return a;
}

void ai_goal_bearing(uint8_t* ai) {
    int32_t m[9], v[3] = { i32(ai, 0x49c), 0, 0 }, o[3];
    rd9(ai, 0x758, m);
    tm2_mat_vec_mul_row(m, v, o);
    int32_t gx = tm2_wadd(i32(ai, 0x4d4), o[0]), gy = tm2_wadd(i32(ai, 0x4d8), o[1]);
    w32(ai, 0x4b8, i32(ai, 0x4b4));
    uint32_t d;
    int16_t a = bearing_to(ai, gx, gy, &d);
    w32(ai, 0x4b4, (int32_t)d);
    if (ai[0x39] || i16(ai, 0x466) < 10) {
        ai[0x44f] = 0;
        w32(ai, 0x4b8, i32(ai, 0x4b4));
    }
    w16(ai, 0x488, a);
}

void ai_goal2_bearing(uint8_t* ai) {
    uint32_t d;
    int16_t a = bearing_to(ai, i32(ai, 0x4dc), i32(ai, 0x4e0), &d);
    w32(ai, 0x4bc, (int32_t)d);
    w16(ai, 0x482, a);
}

void ai_update_stuck(uint8_t* ai, const AiEnv* env) {
    ai[0x7f] = 0;
    if (i16(ai, 0x468) == 0) {
        ai[0x44e] = 0;
        w16(ai, 0x514, 0);
    } else {
        w16(ai, 0x514, i16(ai, 0x514) + 1);
        int32_t n = i16(ai, 0x514);
        if (n > 0x18) {
            if ((env->frame_parity & 5) == 1) w32(ai, 0x58c, i32(ai, 0x58c) - 1);
        } else if (n > 6) {
            ai[0x7f] = 1;
        }
    }
    w16(ai, 0x468, 0);
    if (ai[0x3b]) {
        ai[0x3b] = 0;
        w16(ai, 0x562, i16(ai, 0x564));
        w16(ai, 0x8, 0);
        car_stats_derive(ai + 0x4ec);
    }
    w16(ai, 0x462, 0);
    w16(ai, 0x460, 0);
    if (i16(ai, 0x466) > 600)
        w16(ai, 0x514, i16(ai, 0x514) + 1);
    else
        w16(ai, 0x466, 0);
}

/* mat_vec_mul_row by the rotation about Z by angle (z angle only matrix) */
static void rotate_z(int16_t angle, const int32_t v[3], int32_t out[3]) {
    int16_t ang[3] = { 0, 0, angle };
    int32_t m[9];
    tm2_build_matrix(ang, m);
    tm2_mat_vec_mul_row(m, v, out);
}

void ai_route_lateral_offset(const uint8_t* ai, int32_t out[3], int which) {
    int32_t dx, dy, gx, gy, cx, cy;
    uint8_t aligned_x, diagonal;
    if ((uint8_t)which) {
        dx = i32(ai, 0x48c); dy = i32(ai, 0x490); aligned_x = ai[0x43f]; diagonal = ai[0x43e];
        gx = i32(ai, 0x4cc); gy = i32(ai, 0x4d0); cx = i32(ai, 0x4a4); cy = i32(ai, 0x4a8);
    } else {
        dx = i32(ai, 0x494); dy = i32(ai, 0x498); aligned_x = ai[0x442]; diagonal = ai[0x441];
        gx = i32(ai, 0x4d4); gy = i32(ai, 0x4d8); cx = i32(ai, 0x4ac); cy = i32(ai, 0x4b0);
    }
    int32_t px = i32(ai, 0x728), py = i32(ai, 0x72c);
    if (diagonal) {
        /* distance from the line through the goal along (dx, dy), measured along x and along y;
         * the smaller one is kept */
        out[0] = dy == 0 ? 0 : wsub(wsub(px, gx), tm2_wmul(wsub(py, gy), dx) / dy);
        if (dx == 0) {
            out[1] = 0;
        } else {
            out[1] = wsub(wsub(py, gy), tm2_wmul(wsub(px, gx), dy) / dx);
            if (iabs(out[0]) > iabs(out[1])) out[0] = 0;
            else out[1] = 0;
        }
    } else if (aligned_x) {
        out[0] = wsub(px, cx);
        out[1] = 0;
    } else {
        out[0] = 0;
        out[1] = wsub(py, cy);
    }
    int32_t v[3] = { out[0], out[1], 0 };
    rotate_z((int16_t)-i16(ai, 0x47c), v, out);
}

void ai_set_pass_lane(uint8_t* ai, int side) {
    int32_t off[3];
    ai_route_lateral_offset(ai, off, 1);
    w32(ai, 0x49c, 0);
    ai[0x48a] = 0;
    ai[0x43d] = 0;
    w32(ai, 0x4a0, off[0]);
    ai[0x44d] = 0;
    if ((uint8_t)side == 0 || i16(ai, 0x46e) <= 0) return;
    w32(ai, 0x49c, (uint8_t)side == 2 ? 0x48 : -0x48);
    if (i16(ai, 0x46e) > 1) w32(ai, 0x49c, tm2_wmul(i32(ai, 0x49c), 2));
    int32_t room = wsub(i32(ai, 0x4b4), i16(ai, 0x47a) + 400);
    if (room < 0) {
        w32(ai, 0x49c, 0);
        return;
    }
    if (room < 800) w32(ai, 0x49c, tm2_wmul(room, i32(ai, 0x49c)) / 800);
    ai[0x43d] = 1;
    ai[0x48a] = (uint8_t)side;
}

void ai_set_route_aim_point(uint8_t* ai, const AiEnv* env) {
    const uint8_t* nd = env->nodes + AI_NODE_SIZE * i16(ai, 0x458);
    ai[0x44b] = 1;
    int32_t x = i32(nd, 0), y = i32(nd, 4), o[3];
    if (i16(ai, 0x30) == 0xf0) {
        int32_t side;
        switch (i16(ai, 0x46e)) {
        case -2: side = 0; break;
        case -1: side = -40; break;
        case 0: side = -80; break;
        case 1: side = -120; break;
        default: side = -160; break;
        }
        int32_t v[3] = { 0, side, 0 };
        rotate_z(i16(ai, 0x488), v, o);
        x = tm2_wadd(x, o[0]);
        y = tm2_wadd(y, o[1]);
    }
    int32_t v[3] = { 0, i16(ai, 0x712) * 400 / i32(ai, 0x5a0) + 200, 0 };
    rotate_z(i16(ai, 0x47e), v, o);
    w32(ai, 0x4e4, tm2_wadd(o[0], x));
    w32(ai, 0x4e8, tm2_wadd(o[1], y));
    int32_t dx = wsub(i32(ai, 0x4e4), i32(ai, 0x728)), dy = wsub(i32(ai, 0x4e8), i32(ai, 0x72c));
    if (dy == 0) dy = 1;
    int16_t e = (int16_t)(i16(ai, 0x47e) - tm2_atan2(dx, dy));
    tm2_angle_wrap16(&e);
    w16(ai, 0x476, e);
    int16_t h = i16(ai, 0x73c);
    tm2_angle_wrap16(&h);
    w16(ai, 0x474, h);
}

int ai_near_zone_edge(const uint8_t* ai, const AiEnv* env) {
    if (env->event_type != 5 && env->event_type != 6) return 0;
    const uint8_t* g = env->grp;
    int32_t z = i16(ai, 0x45e);
    int32_t dx = wsub(i32(ai, 0x728), i32(g, 0xc + 12 * z));
    int32_t dy = wsub(i32(ai, 0x72c), i32(g, 0x10 + 12 * z));
    int32_t best = 8000;
    uint8_t shape = g[0x123b + z];
    if (!g[0x11d2 + z] && (shape == 0 || shape == 1)) {
        int32_t eu = i32(g, 0x1b0 + 4 * z), ev = i32(g, 0x23c + 4 * z);
        best = dx < eu / 2 ? dx : wsub(eu, dx);
        /* ORIGINAL BUG (kept): on the far half the test is dy < best, but best becomes ev - dy */
        if (dy < ev / 2) {
            if (dy < best) best = dy;
        } else if (dy < best) {
            best = wsub(ev, dy);
        }
    }
    return best < 0x1e0;
}

int32_t car_type_top_speed(uint16_t t) {
    switch (t) {
    case 0x10: return 0x5c;
    case 0x20: return 0x60;
    case 0x30: return 0x7f;
    case 0x40: return 0x65;
    case 0x50: return 0x7d;
    case 0x60: return 0x79;
    case 0x70: return 0x6f;
    case 0x80: return 0x83;
    case 0xa0: return 0x8d;
    case 0xb0: return 0x67;
    case 0xc0: return 0x5c;
    case 0xd0: return 0x71;
    case 0xe0: return 0x7f;
    case 0xf0: return 0x7f;
    default: return 0x77;
    }
}

int car_has_melee_special(uint16_t t) {
    return t == 0 || t == 0x30 || t == 0x70 || t == 0xa0 || t == 0xb0 || t == 0xc0;
}

void car_stats_set_top_speed(uint8_t* st, int16_t top) {
    w16(st, 0x52, top);
    w16(st, 0x54, i16(st, 0x52) * 3 / 4);
    w16(st, 0x5a, i16(st, 0x52) * 2 / 3);
    car_stats_derive(st);
}

static int32_t difficulty_percent(const AiEnv* env) {
    return env->difficulty == 1 ? 0x78 : env->difficulty == 2 ? 0x8c : 0x4b;   /* 0x0045b488 */
}

void ai_set_segment_speed(uint8_t* ai, int turn, const AiEnv* env) {
    int16_t type = i16(ai, 0x30);
    if ((uint8_t)turn == 0) {
        w16(ai, 0x478, i16(ai, 0x516) * 2);
        w16(ai, 0x544, i16(ai, 0x53e));
    } else {
        int32_t cruise = i16(ai, 0x516);
        if (ai[0x446]) {
            if (i16(ai, 0x470) < 1) {
                w16(ai, 0x478, cruise);
                w16(ai, 0x544, car_type_top_speed((uint16_t)type) / 3);
            } else {
                w16(ai, 0x478, cruise * 2);
                w16(ai, 0x544, car_type_top_speed((uint16_t)type) * 2 / 3);
            }
        } else {
            int32_t at = iabs(i16(ai, 0x480));
            if (at < 0x1c7) {
                w16(ai, 0x478, cruise / 2);
                w16(ai, 0x544, i16(ai, 0x53e));
            } else if (at < 0x505) {
                w16(ai, 0x544, car_type_top_speed((uint16_t)type));
                if (i16(ai, 0x470) < 0) {
                    w16(ai, 0x478, cruise / 2);
                    w16(ai, 0x544, i16(ai, 0x544) / 4);
                } else if (i16(ai, 0x46e) == 0) {
                    w16(ai, 0x478, cruise / 2);
                    w16(ai, 0x544, i16(ai, 0x544) / 3);
                } else if (i16(ai, 0x46e) == 1) {
                    w16(ai, 0x478, cruise * 2 / 3);
                    w16(ai, 0x544, i16(ai, 0x544) / 2);
                } else {
                    w16(ai, 0x478, cruise);
                    w16(ai, 0x544, i16(ai, 0x544) * 3 / 4);
                }
            } else {
                w16(ai, 0x544, car_type_top_speed((uint16_t)type) / 3);
                w16(ai, 0x478, cruise * 3 / 2);
            }
        }
        if (i16(ai, 0x478) < 0xa0) w16(ai, 0x478, 0xa0);
        int32_t pct = difficulty_percent(env);
        if (pct == 0x4b) w16(ai, 0x478, i16(ai, 0x478) * 2 / 3);
        else if (pct == 0x8c) w16(ai, 0x478, i16(ai, 0x478) * 6 / 5);
    }
    int32_t k = i16(ai, 0x562);
    int32_t lim = 2000 / k;
    if (i16(ai, 0x544) < lim) w16(ai, 0x544, lim);
    int32_t r = i16(ai, 0x544) * 19 * tm2_frame_ticks60() / 100;
    w32(ai, 0x5ac, tm2_wmul(k, r * 32) / 100);

    const uint8_t* np = env->nodes + AI_NODE_SIZE * i16(ai, 0x458);
    const uint8_t* nc = env->nodes + AI_NODE_SIZE * i16(ai, 0x45a);
    w32(ai, 0x494, wsub(i32(nc, 0), i32(np, 0)));
    w32(ai, 0x498, wsub(i32(nc, 4), i32(np, 4)));
    ai[0x442] = iabs(i32(ai, 0x494)) < 0x28;
    ai[0x443] = iabs(i32(ai, 0x498)) < 0x28;
    ai[0x441] = !ai[0x442] && !ai[0x443];
    if (ai[0x441]) {
        w32(ai, 0x4ac, tm2_wadd(i32(np, 0), i32(ai, 0x494) / 2));
        w32(ai, 0x4b0, tm2_wadd(i32(np, 4), i32(ai, 0x498) / 2));
    } else if (ai[0x442]) {
        w32(ai, 0x4ac, i32(np, 0));
    } else {
        w32(ai, 0x4b0, i32(np, 4));
    }
    w16(ai, 0x47a, i16(ai, 0x478));
}
