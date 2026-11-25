/*
 * ai_route.c: AI route following: target lookup, node advance, next node choice, route roll.
 * Ported from TM2.EXE (see ai.h). Verified against the original under emulation
 * (tools/emu/check_ai_route.sh).
 *
 * PC differs from PS1: the PS1 picks the next node through a separate link picker (aiPickLinkNode);
 * the PC build inlines it and steers through three 35 x 35 zone route tables in the GRP file.
 */
#include "ai.h"
#include "ai_mem.h"

#include "../core/tm2rand.h"
#include "../math/tm2math.h"
#include "../physics/terrain_tile.h"

static int32_t iabs(int32_t v) { return v < 0 ? (int32_t)(0u - (uint32_t)v) : v; }
static const uint8_t* node_at(const AiEnv* env, int32_t i) { return env->nodes + AI_NODE_SIZE * i; }
static int32_t nx(const AiEnv* env, int32_t i) { return i32(node_at(env, i), 0); }
static int32_t ny(const AiEnv* env, int32_t i) { return i32(node_at(env, i), 4); }
static int32_t nzone(const AiEnv* env, int32_t i) { return i8(node_at(env, i), 0x17); }
static int32_t ncount(const AiEnv* env, int32_t i) { return i8(node_at(env, i), 0x16); }
static int32_t nsucc(const AiEnv* env, int32_t i, int32_t k) { return i16(node_at(env, i), 0xa + 2 * k); }
static int32_t nlane(const AiEnv* env, int32_t i, int32_t k) { return i8(node_at(env, i), 0x12 + k); }
/* GRP route tables, [from zone * 35 + to zone] */
static int32_t route_node(const AiEnv* env, int32_t idx) { return i8(env->grp, 0x354 + idx); }
static int32_t route_zone(const AiEnv* env, int32_t idx) { return i8(env->grp, 0x81d + idx); }
static int32_t route_take(const AiEnv* env, int32_t idx) { return i8(env->grp, 0xce6 + idx); }

int32_t ai_nearest_player(const AiEnv* env, const uint8_t* pos, int32_t skip) {
    int32_t best = 0x7fff, pick = 0;
    for (int32_t i = 0; i < env->player_count; i++) {
        const uint8_t* p = env->players + PLAYER_BLOCK_SIZE * i;
        if (i32(p, 0x1d8) <= 0) continue;
        int32_t d = tm2_wadd(iabs(wsub(i32(pos, 0), i32(p, 0x6e4))), iabs(wsub(i32(pos, 4), i32(p, 0x6e8))));
        if (d < best && i != skip) {
            best = d;
            pick = i;
        }
    }
    return pick;
}

uint8_t* ai_target_car(const AiEnv* env, int32_t index, uint8_t* is_player) {
    int32_t slot = env->target_slot[index];
    if (slot == -1) {
        *is_player = 1;
        int32_t i = ai_nearest_player(env, env->ai_cars + AI_BLOCK_SIZE * index + 0x728, -1);
        return env->players + PLAYER_BLOCK_SIZE * i;
    }
    *is_player = 0;
    return env->ai_cars + AI_BLOCK_SIZE * slot;
}

/* the target's zone: player block +0xC6, AI block +0x45E */
static int32_t target_zone(const uint8_t* t, uint8_t is_player) { return i16(t, is_player ? 0xc6 : 0x45e); }

/* 0x0045c300 */
static int boss_zone(int32_t z) { return (z > 3 && z < 12) || (z > 13 && z < 16) || (z > 16 && z < 19); }

void ai_boss_jump_check(uint8_t* ai, const AiEnv* env) {
    ai[0x451] = 0;
    ai[0x450] = 0;
    int32_t n = i16(ai, 0x458);
    if (n != 2 && n != 0x28 && n != 0xb && n != 0x2c) return;
    int32_t x = i32(ai, 0x728), y = i32(ai, 0x72c);
    if (!(y > -0x2260 && y < -0x15e0 && ((x > 0x10e0 && y < 0x14a0) || (x > -0x14a0 && y < -0x10e0)))) return;
    uint8_t pl;
    const uint8_t* t = ai_target_car(env, i16(ai, 4), &pl);
    int32_t z = target_zone(t, pl);
    if (boss_zone(z) && z > 7) ai[0x451] = 1;
    if (!ai[0x451]) ai[0x450] = 1;
}

void ai_advance_route_node(uint8_t* ai, const AiEnv* env) {
    w16(ai, 0x456, i16(ai, 0x458));
    w16(ai, 0x458, i16(ai, 0x45a));
    w16(ai, 0x47c, i16(ai, 0x47e));
    w32(ai, 0x4c0, i32(ai, 0x4c4));
    w16(ai, 0x46e, i16(ai, 0x470));
    if (!ai[0x6d]) {
        int32_t top = car_type_top_speed((uint16_t)i16(ai, 0x30));
        switch (i16(ai, 0x46e)) {
        case -2: top /= 4; break;
        case -1: top /= 3; break;
        case 0: top /= 2; break;
        case 1: top = top * 2 / 3; break;
        default: break;
        }
        car_stats_set_top_speed(ai + 0x4ec, (int16_t)top);
    }
    int32_t lane = i16(ai, 0x46e);
    if (lane < 1) {
        ai[0x444] = 0;
        ai[0x445] = 0;
    } else {
        w16(ai, 0x46c, i16(ai, 0x30) != 0xf0 && lane == 3 ? 0x48 : 0);
        if (ai[4] & 1) {
            ai[0x444] = 0;
            ai[0x445] = 1;
        } else {
            ai[0x444] = 1;
            ai[0x445] = 0;
        }
    }
    w32(ai, 0x4cc, i32(ai, 0x4d4));
    w32(ai, 0x4d0, i32(ai, 0x4d8));
    w32(ai, 0x4d4, i32(ai, 0x4dc));
    w32(ai, 0x4d8, i32(ai, 0x4e0));
    int32_t prev = i16(ai, 0x456), cur = i16(ai, 0x458);
    ai[0x448] = node_at(env, prev)[0x17] != node_at(env, cur)[0x17];
    ai_set_segment_speed(ai, 0, env);
    w32(ai, 0x4c8, i16(ai, 0x478));
    w32(ai, 0x48c, wsub(nx(env, cur), nx(env, prev)));
    w32(ai, 0x490, wsub(ny(env, cur), ny(env, prev)));
    ai[0x43f] = iabs(i32(ai, 0x48c)) < 0x28;
    ai[0x440] = iabs(i32(ai, 0x490)) < 0x28;
    ai[0x43e] = !ai[0x43f] && !ai[0x440];
    if (ai[0x43e]) {
        w32(ai, 0x4a4, tm2_wadd(nx(env, prev), i32(ai, 0x48c) / 2));
        w32(ai, 0x4a8, tm2_wadd(ny(env, prev), i32(ai, 0x490) / 2));
    } else if (ai[0x43f]) {
        w32(ai, 0x4a4, nx(env, prev));
    } else {
        w32(ai, 0x4a8, ny(env, prev));
    }
    ai_set_pass_lane(ai, 0);
    ai[0x43c] = 0;
    w16(ai, 0x466, 0);
    w16(ai, 0x472, 0);
    w16(ai, 0x45a, i16(ai, 0x456));
    if (i16(ai, 0x30) == 0xf0) ai_boss_jump_check(ai, env);
}

/* the part of 0x004334c8 that picks the node and lane for node == -1 */
static void pick_next(uint8_t* ai, const AiEnv* env) {
    int32_t cur = i16(ai, 0x458);
    int8_t mood = i8(ai, 1);
    if (mood == 3) {
        int32_t k = 0;
        if (ncount(env, cur) > 1) {
            do k = tm2_rand() % ncount(env, cur);
            while ((int16_t)nsucc(env, cur, k) == i16(ai, 0x456));
        }
        w16(ai, 0x45a, nsucc(env, cur, k));
        w16(ai, 0x470, nlane(env, cur, k));
        return;
    }
    uint8_t pl;
    uint8_t* t = ai_target_car(env, i16(ai, 4), &pl);
    const uint8_t* tbody = t + (pl ? 0x6c4 : 0x708);
    const uint8_t* tstats = t + (pl ? 0x138 : 0x4ec);
    int32_t tz = target_zone(t, pl);
    int32_t myz = nzone(env, cur);
    int32_t zone = myz;   /* zone the successors must be in (-1: any) */
    int32_t via, via_zone;
    int direct;           /* heading straight for the route node */
    int32_t idx = myz * 35 + tz;
    int32_t pinned = i16(ai, 0x45c);
    if (ai[0x449] && pinned > 0) {
        via = pinned;
        if (via == route_take(env, idx)) {
            via_zone = route_zone(env, myz * 35 + nzone(env, route_take(env, idx)));
            direct = 1;
        } else {
            ai[0x449] = 0;
            via = route_node(env, idx);
            via_zone = route_zone(env, idx);
            direct = 0;
        }
    } else {
        via = route_node(env, idx);
        via_zone = route_zone(env, idx);
        direct = 0;
    }
    if (direct && nzone(env, via) == myz) {
        ai[0x3e] = 0;
    } else if (tz == myz) {
        ai[0x3e] = 0;
    } else if (via < 0) {
        ai[0x3e] = 0;
        if (i16(ai, 0x30) == 0xf0 && tz > 4 && myz < 4) {
            if (cur == 0x42 || cur == 0x44 || cur == 0x46 || cur == 0x48) {
                env->boss_warp(env->user, 1, ai);
                ai[0x3e] = 0;
                zone = 0x18;
            }
        }
    } else {
        ai[0x3e] = 1;
        if (i16(ai, 0x30) == 0xf0 && tz < 4 && myz > 3 && cur == 0) {
            env->boss_warp(env->user, 2, ai);
            ai[0x3e] = 0;
            zone = 3;
        }
    }

    int32_t pt[3];
    int filter_any = 1;
    if (ai[0x3e]) {
        if (mood > 1) {
            pt[0] = i32(tbody, 0x20);
            pt[1] = i32(tbody, 0x24);
            pt[2] = i32(tbody, 0x28);
        } else {
            if (cur == via) {
                /* first successor of the route node in the route's zone (the last one on even
                 * frames: the scan only stops early when the frame parity is odd) */
                int32_t pick = nsucc(env, via, 0);
                for (int32_t k = 0; k < ncount(env, via); k++) {
                    int32_t s = nsucc(env, via, k);
                    if (nzone(env, s) != via_zone) continue;
                    pick = s;
                    zone = via_zone;
                    if (env->frame_parity & 1) break;
                }
                via = pick;
            }
            pt[0] = nx(env, via);
            pt[1] = ny(env, via);
            pt[2] = i32(tbody, 0x28);
            filter_any = env->grp[0x1218 + zone] == 0;
        }
    } else if (direct) {
        pt[0] = nx(env, via);
        pt[1] = ny(env, via);
        pt[2] = i16(node_at(env, via), 8);
    } else {
        int32_t v[3] = { 0, i32(ai, 0x28) / 2, 0 }, m[9], o[3];
        if (mood <= 1 && i16(ai, 0x1e) > i32(ai, 0x28)) {
            int32_t sp = i16(tbody, 0xa), div = sp < i32(tstats, 0xb4) ? i32(tstats, 0xb4) : sp;
            v[1] += tm2_wmul(iabs(sp), 0x640) / div;
        }
        rd9(tbody, 0x50, m);
        tm2_mat_vec_mul_row(m, v, o);
        pt[0] = tm2_wadd(i32(tbody, 0x20), o[0]);
        pt[1] = tm2_wadd(i32(tbody, 0x24), o[1]);
        pt[2] = tm2_wadd(i32(tbody, 0x28), o[2]);
    }
    if (filter_any) zone = -1;

    int32_t choice = nsucc(env, cur, 0), lane = nlane(env, cur, 0);
    int32_t best = mood == 2 ? 0 : 0x7fffffff;
    int32_t prev = i16(ai, 0x456);
    for (int32_t k = 0; k < ncount(env, cur); k++) {
        int32_t s = nsucc(env, cur, k);
        if (mood <= 1 && zone >= 0 && nzone(env, s) != zone) continue;
        int32_t dx = wsub(pt[0], nx(env, s)), dy = wsub(pt[1], ny(env, s));
        int32_t d = tm2_wadd(tm2_wmul(dy, dy), tm2_wmul(dx, dx));
        int32_t l = nlane(env, cur, k);
        if (mood == 2) {
            if (d > best && (s != prev || i16(ai, 0x464) < 3)) {
                best = d;
                choice = s;
                lane = l;
            }
        } else if (d < best) {
            if (s != prev) {
                best = d;
                choice = s;
                lane = l;
            } else if (i16(ai, 0x464) < 3) {
                best = tm2_wadd(d, 0x50);
                choice = s;
                lane = l;
            }
        }
    }
    if (prev == choice) w16(ai, 0x464, i16(ai, 0x464) + 1);
    else w16(ai, 0x464, 0);
    w16(ai, 0x45a, choice);
    w16(ai, 0x470, lane);
}

void ai_choose_next_node(uint8_t* ai, int32_t node, const AiEnv* env) {
    if (node != -1) {
        w16(ai, 0x470, 0);
        w16(ai, 0x45a, node);
    } else {
        pick_next(ai, env);
    }
    int32_t nn = i16(ai, 0x45a), cur = i16(ai, 0x458);
    int32_t dx = wsub(nx(env, nn), nx(env, cur)), dy = wsub(ny(env, nn), ny(env, cur));
    w32(ai, 0x4c4, (int32_t)rmp_isqrt(tm2_wadd(tm2_wmul(dx, dx), tm2_wmul(dy, dy))));
    if (dy == 0) dy = 1;
    int16_t h = (int16_t)tm2_atan2(dx, dy);
    tm2_angle_wrap16(&h);
    w16(ai, 0x47e, h);
    int32_t side = i16(ai, 0x46c);
    if (ai[0x445]) {
        w32(ai, 0x4dc, tm2_wadd(nx(env, nn), side));
        w32(ai, 0x4e0, tm2_wadd(ny(env, nn), side));
    } else if (ai[0x444]) {
        w32(ai, 0x4dc, wsub(nx(env, nn), side));
        w32(ai, 0x4e0, wsub(ny(env, nn), side));
    } else {
        w32(ai, 0x4dc, nx(env, nn));
        w32(ai, 0x4e0, ny(env, nn));
    }
    int16_t turn = (int16_t)(i16(ai, 0x47e) - i16(ai, 0x47c));
    tm2_angle_wrap16(&turn);
    w16(ai, 0x480, turn);
    ai[0x446] = iabs(i16(ai, 0x480)) > 0x638;
}

void ai_roll_take_route(uint8_t* ai, const AiEnv* env) {
    ai[0x43d] = 0;
    w16(ai, 0x484, 0x800);
    w16(ai, 0x486, 0x7fff);
    w16(ai, 0x472, 0);
    ai[0x44a] = 0;
    ai[0x44b] = 0;
    ai[0x48a] = 0;
    ai[0x453] = 0;
    ai[0x43c] = 1;
    if (ai[0x449] || i8(ai, 1) != 1 || i16(ai, 0x466) >= 600) return;
    int32_t myz = i16(ai, 0x45e);
    uint8_t pl;
    const uint8_t* t = ai_target_car(env, i16(ai, 4), &pl);
    const uint8_t* tbody = t + (pl ? 0x6c4 : 0x708);
    const uint8_t* tstats = t + (pl ? 0x138 : 0x4ec);
    int32_t tz = target_zone(t, pl);
    int32_t range = myz == tz ? i8(env->grp, 0x11af + tz) * 64 : 0x41a0;
    int32_t take = route_take(env, myz * 35 + tz);
    if (i16(tbody, 0xa) > i32(tstats, 0xb8) && i16(ai, 0x510) < i16(ai, 0x460)) range /= 3;
    if (range <= i32(ai, 0x28) || take == 0) {
        ai[0x449] = 0;
        return;
    }
    uint8_t roll = 0;
    if (env->event_type == 8) {
        roll = 1;
    } else if (!car_has_melee_special((uint16_t)i16(ai, 0x30))) {
        if (env->event_type == 5) {
            int32_t mask = env->difficulty < 1 ? 0xf : env->difficulty < 2 ? 7 : 3;
            roll = (tm2_rand() & mask) == 1;
        } else if (env->event_type == 3 && i32(ai, 0x730) > 400 && iabs(i16(ai, 0x26)) < 400) {
            roll = (uint8_t)(tm2_rand() & 1);
        }
    }
    ai[0x449] = roll;
    if (!roll) return;
    w16(ai, 0x45c, take);
    if (take < 0) w16(ai, 0x45c, i16(ai, 0x458));
    else if (take == 0x51) (void)tm2_rand();   /* ORIGINAL: result unused */
}
