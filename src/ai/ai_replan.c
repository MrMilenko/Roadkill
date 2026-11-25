/*
 * ai_replan.c: AI route replanning: nearest node, placing a car back on the route, rerouting the
 * cars of a zone. Ported from TM2.EXE (see ai.h). Verified against the original under emulation
 * (tools/emu/check_ai_replan.sh).
 */
#include "ai.h"
#include "ai_mem.h"

#include "../core/tm2rand.h"
#include "../game/car.h"
#include "../game/car_spawn.h"
#include "../math/tm2math.h"

static int32_t iabs(int32_t v) { return v < 0 ? (int32_t)(0u - (uint32_t)v) : v; }
static const uint8_t* node_at(const AiEnv* env, int32_t i) { return env->nodes + AI_NODE_SIZE * i; }

int32_t ai_nearest_node(const uint8_t* ai, const AiEnv* env) {
    const uint8_t* b = ai + 0x708;
    int32_t zone = i16(ai, 0x45e);
    if (env->event_type == 8 && zone > 3 && zone < 8) zone = 0x18;
    int32_t best = 0x7fffffff, pick = 0;
    for (int32_t i = 0; i < env->node_count; i++) {
        const uint8_t* n = node_at(env, i);
        if (i8(n, 0x17) != zone) continue;
        int32_t d = tm2_wadd(iabs(wsub(i32(b, 0x28), i16(n, 8))),
                             tm2_wadd(iabs(wsub(i32(b, 0x20), i32(n, 0))), iabs(wsub(i32(b, 0x24), i32(n, 4)))));
        if (d < best) {
            best = d;
            pick = i;
        }
    }
    return pick;
}

/* 0x00430368: probe floor heights */
static void probes_floor(uint8_t* p, int32_t z) {
    for (int k = 0; k < 4; k++) w32(p, 0x5c + 0x48 * k, z);
    for (int k = 0; k < 4; k++) {
        int16_t h = i16(p, 0x5c + 0x48 * k);
        w16(p, 0x34 + 0x48 * k, h);
        w16(p, 0x3c + 0x48 * k, h);
        w16(p, 0x3e + 0x48 * k, h);
    }
}

/* choose node, stuck bookkeeping, plain segment speed, advance: the step the placement repeats */
static void step_to(uint8_t* ai, int32_t node, const AiEnv* env) {
    ai_choose_next_node(ai, node, env);
    ai_update_stuck(ai, env);
    ai_set_segment_speed(ai, 0, env);
    ai_advance_route_node(ai, env);
}

void ai_place_on_route_node(uint8_t* ai, const AiEnv* env) {
    int32_t home = i32(ai, 0x5bc);
    const uint8_t* h = node_at(env, home);
    int32_t cnt = i8(h, 0x16), k = 0;
    if (cnt > 0) k = tm2_rand() % cnt;
    step_to(ai, i16(h, 0xa + 2 * k), env);
    step_to(ai, home, env);
    /* the successor equal to +0x52C (else the first), then the one after it */
    int32_t j = 0;
    for (int32_t i = 0; i < i8(h, 0x16); i++) {
        if (i16(h, 0xa + 2 * i) == i16(ai, 0x52c)) {
            j = i;
            break;
        }
    }
    w16(ai, 0x52c, i16(h, 0xa + 2 * j));
    step_to(ai, i16(ai, 0x52c), env);
    if (++j >= i8(h, 0x16)) j = 0;
    w16(ai, 0x52c, i16(h, 0xa + 2 * j));
    w32(ai, 0x728, i32(h, 0));
    w32(ai, 0x72c, i32(h, 4));
    int32_t zone = i8(h, 0x17);
    w16(ai, 0x712, 0);
    w32(ai, 0x730, i32(env->grp, 0x14 + 12 * zone));
    ai[0x69] = 0;
    w32(ai, 0x73c, i16(ai, 0x47c));
    ai[0x452] = 0;
    uint8_t* p = ai + 0x5d0;
    car_probes_clear(p);
    for (int s = 0; s < 4; s++) w16(p, 0x28 + 0x48 * s, zone);
    probes_floor(p, i32(env->grp, 0x14 + 12 * zone));
    probes_floor(p, i16(h, 8));
    env->drive(env->user, ai);
}

void ai_replan_route(uint8_t* ai, const AiEnv* env) {
    ai_choose_next_node(ai, ai_nearest_node(ai, env), env);
    ai_update_stuck(ai, env);
    ai_set_segment_speed(ai, 0, env);
    if (ai[0x7f]) {
        w32(ai, 0x5bc, ai_nearest_node(ai, env));
        ai_place_on_route_node(ai, env);
        env->fx_flash(env->user, ai + 0x728);
        env->fx_smoke(env->user, ai + 0x728);
    }
    ai_advance_route_node(ai, env);
    ai_goal_bearing(ai);
    ai[0x44f] = 1;
    ai[0x43c] = 0;
    ai[0x44b] = 0;
    w16(ai, 0x484, 0x800);
    w16(ai, 0x486, 0x7fff);
    w16(ai, 0x472, 0);
    ai[0x44a] = 0;
    ai[0x43d] = 0;
    ai[0x48a] = 0;
    ai[0x453] = 0;
    ai[0x39] = 0;
    ai[0x452] = 0;
    ai[0x3d] = 0;
    if (!ai[0x69]) car_snapshot(ai + 0x708, ai + 0x68);
}

void ai_reroute_zone(int32_t zone, int32_t node, const AiEnv* env) {
    int32_t n = env->ai_count;
    for (int32_t i = 0; i < n; i++) {
        uint8_t* c = env->ai_cars + AI_BLOCK_SIZE * (i < 0 || i > 8 ? 0 : i);   /* 0x0045b080 clamps */
        if (i16(c, 0x45e) != zone && i8(node_at(env, i16(c, 0x458)), 0x17) != zone &&
            i8(node_at(env, i16(c, 0x45a)), 0x17) != zone)
            continue;
        ai_roll_take_route(c, env);
        ai_choose_next_node(c, node, env);
        ai_update_stuck(c, env);
        ai_set_segment_speed(c, 0, env);
    }
}
