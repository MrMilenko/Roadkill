/*
 * bind_ai.c: the computer cars' driving (src/ai), called the way tools/emu/diff_ai_*.py called
 * them: the AI block (or the first argument) in EAX, then EDX, EBX; the AiEnv / AiStartOps /
 * CarSlots context points into game memory at the original's globals.
 *
 * These ports keep some state on the host: src/math/tm2math's tables, frame ticks and fps, and
 * src/core/tm2rand's state. host_sync() loads the tables once and copies the ticks and fps from
 * game memory before each call; rand assumes crt_rand 0x004899c2 / crt_srand 0x004899e6 are bound
 * to tm2_rand / tm2_srand (one state for every caller).
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "ai/ai.h"
#include "ai/ai_reset.h"
#include "ai/ai_slots.h"
#include "math/tm2math.h"

static void host_sync(void)
{
    static int loaded;
    if (!loaded) {
        const char* dir = getenv("TM2_DATA");
        char s[1024], x[1024];
        if (!dir || !*dir) dir = "disc/files";
        snprintf(s, sizeof s, "%s/SINTAB.DAT", dir);
        snprintf(x, sizeof x, "%s/TM2.EXE", dir);
        if (tm2_math_load_tables(s, x) != 0) rt_stop("math tables (set TM2_DATA to the game's files)", 0);
        loaded = 1;
    }
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
}

static uint32_t ga(const void* p) { return p ? rt_addr(p) : 0; }

static uint32_t call2(uint32_t fn, uint32_t eax, uint32_t edx)
{
    uint32_t a[2] = {eax, edx};
    return rt_call(fn, a, 2);
}
static uint32_t call3(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    uint32_t a[3] = {eax, edx, ebx};
    return rt_call(fn, a, 3);
}

/* ---- AiEnv ---- */

/* 0x004543f8(kind, ai, 0) Dark Tooth warp; 0x00450984 / 0x0045109c(pos, 0) flash / smoke;
 * 0x0042d470(ai, 0) car_drive */
static void cb_warp(void* u, int32_t kind, uint8_t* ai) { (void)u; call3(0x004543f8u, (uint32_t)kind, ga(ai), 0); }
static void cb_flash(void* u, uint8_t* pos) { (void)u; call2(0x00450984u, ga(pos), 0); }
static void cb_smoke(void* u, uint8_t* pos) { (void)u; call2(0x0045109cu, ga(pos), 0); }
static void cb_drive(void* u, uint8_t* ai) { (void)u; call2(0x0042d470u, ga(ai), 0); }

static AiEnv* env(void)
{
    static AiEnv e;
    host_sync();
    e.nodes = rt_ptr(0x004fe1e4u);
    e.grp = rt_ptr(0x004fca18u);
    e.ai_cars = rt_ptr(0x0050760cu);
    e.players = rt_ptr(0x005030a8u);
    e.target_slot = (const int32_t*)(const void*)rt_ptr(0x00502fb4u);
    e.boss_warp = cb_warp;
    e.fx_flash = cb_flash;
    e.fx_smoke = cb_smoke;
    e.drive = cb_drive;
    e.user = NULL;
    /* the values the originals read through their getters, current at each call */
    e.event_type = rt_r8(0x00624ec7u);
    e.frame_parity = (int32_t)rt_r32(0x00a0364cu);
    e.difficulty = (int32_t)rt_r32(0x00502f98u);
    e.player_count = (int32_t)rt_r32(0x005075e0u);
    e.node_count = (int32_t)rt_r32(0x004fe1b0u);
    e.ai_count = (int32_t)rt_r32(0x0050c2b0u);
    e.game_mode = (int32_t)rt_r32(0x00502fa4u);
    e.flag_502f7c = rt_r8(0x00502f7cu);
    e.flag_50c2c4 = rt_r8(0x0050c2c4u);
    return &e;
}

#define A(i) rt_arg(a, n, (i))
#define P(i) rt_ptr(A(i))

/* ---- ai_steps.c ---- */

static uint32_t b_goal_bearing(const uint32_t* a, int n) { host_sync(); ai_goal_bearing(P(0)); return 0; }
static uint32_t b_goal2_bearing(const uint32_t* a, int n) { host_sync(); ai_goal2_bearing(P(0)); return 0; }
static uint32_t b_update_stuck(const uint32_t* a, int n) { ai_update_stuck(P(0), env()); return 0; }
static uint32_t b_lateral_offset(const uint32_t* a, int n)
{
    host_sync();
    ai_route_lateral_offset(P(0), (int32_t*)(void*)P(1), (int)A(2));
    return 0;
}
static uint32_t b_pass_lane(const uint32_t* a, int n) { host_sync(); ai_set_pass_lane(P(0), (int)A(1)); return 0; }
static uint32_t b_aim_point(const uint32_t* a, int n) { ai_set_route_aim_point(P(0), env()); return 0; }
static uint32_t b_near_zone_edge(const uint32_t* a, int n) { return (uint32_t)ai_near_zone_edge(P(0), env()); }
static uint32_t b_type_top_speed(const uint32_t* a, int n) { (void)n; return (uint32_t)car_type_top_speed((uint16_t)a[0]); }
static uint32_t b_melee_special(const uint32_t* a, int n) { (void)n; return (uint32_t)car_has_melee_special((uint16_t)a[0]); }
static uint32_t b_set_top_speed(const uint32_t* a, int n)
{
    host_sync();
    car_stats_set_top_speed(P(0), (int16_t)A(1));
    return 0;
}
static uint32_t b_segment_speed(const uint32_t* a, int n) { ai_set_segment_speed(P(0), (int)A(1), env()); return 0; }

/* ---- ai_route.c ---- */

/* 0x0045afc8(index, &is_player): the chased car's block */
static uint32_t b_target_car(const uint32_t* a, int n)
{
    AiEnv* e = env();
    return ga(ai_target_car(e, (int32_t)A(0), P(1)));
}
/* 0x0045acb8(pos, skip) */
static uint32_t b_nearest_player(const uint32_t* a, int n)
{
    AiEnv* e = env();
    return (uint32_t)ai_nearest_player(e, P(0), (int32_t)A(1));
}
static uint32_t b_boss_jump(const uint32_t* a, int n) { ai_boss_jump_check(P(0), env()); return 0; }
static uint32_t b_advance_node(const uint32_t* a, int n) { ai_advance_route_node(P(0), env()); return 0; }
static uint32_t b_choose_next(const uint32_t* a, int n) { ai_choose_next_node(P(0), (int32_t)A(1), env()); return 0; }
static uint32_t b_roll_route(const uint32_t* a, int n) { ai_roll_take_route(P(0), env()); return 0; }

/* ---- ai_replan.c ---- */

/* 0x0042c3e0(car, kind). kind 0 (AI layout) is the port. The original's other path (kind != 0,
 * the player layout: body at +0x6C4, zone at +0xC6, no event 8 zone rule, the same loop) runs the
 * same port on a scratch AI block holding the player's position and zone, with the event type
 * seen as 0 so the event 8 rule does not apply. */
static uint32_t b_nearest_node(const uint32_t* a, int n)
{
    AiEnv* e = env();
    if ((A(1) & 0xffu) == 0) return (uint32_t)ai_nearest_node(P(0), e);
    static uint8_t tmp[AI_BLOCK_SIZE];
    const uint8_t* car = P(0);
    memcpy(tmp + 0x708 + 0x20, car + 0x6c4 + 0x20, 12);
    memcpy(tmp + 0x45e, car + 0xc6, 2);
    AiEnv e2 = *e;
    e2.event_type = 0;
    return (uint32_t)ai_nearest_node(tmp, &e2);
}
static uint32_t b_place_on_node(const uint32_t* a, int n) { ai_place_on_route_node(P(0), env()); return 0; }
static uint32_t b_replan(const uint32_t* a, int n) { ai_replan_route(P(0), env()); return 0; }
static uint32_t b_reroute_zone(const uint32_t* a, int n) { ai_reroute_zone((int32_t)A(0), (int32_t)A(1), env()); return 0; }

/* ---- ai_decide.c ---- */

static uint32_t b_chase_direct(const uint32_t* a, int n) { return (uint32_t)ai_should_chase_direct(P(0), (int32_t)A(1), env()); }
/* 0x00429f7c(human, &is_player) */
static uint32_t b_nearest_to_human(const uint32_t* a, int n)
{
    AiEnv* e = env();
    return ga(car_nearest_to_human((int32_t)A(0), P(1), e));
}

/* ---- ai_setup.c / ai_misc.c ---- */

static uint32_t b_patch_boss(const uint32_t* a, int n)
{
    (void)a; (void)n;
    host_sync();
    ai_route_patch_boss_level(rt_ptr(0x004fe1e4u), rt_ptr(0x004fca18u));
    return 0;
}
static uint32_t b_setup_profile(const uint32_t* a, int n) { ai_car_setup_profile(P(0), env()); return 0; }
/* 0x0044a40c(pos, dx, dy, dz): into the original's static vector 0x004fef70, which it returns */
static uint32_t b_jitter(const uint32_t* a, int n)
{
    host_sync();
    math_jitter_pos((const int32_t*)(const void*)P(0), (int32_t)A(1), (int32_t)A(2), (int32_t)A(3),
                    (int32_t*)(void*)rt_ptr(0x004fef70u));
    return 0x004fef70u;
}
static uint32_t b_event_zones(const uint32_t* a, int n)
{
    (void)a; (void)n;
    host_sync();
    event_objects_find_zones(rt_ptr(0x004f004cu), rt_ptr(0x004fca18u));
    return 0;
}

/* ---- ai_reset.c ---- */

/* 0x0043846c(1, stats, type), 0x00438320(ai, 0), 0x0043807c(ai, weapons, 0), 0x0044c81c(weapons),
 * 0x00420be8(ai, 2, 2, 0, stack 0) */
static void cb_stats_init(void* u, uint8_t* st, int16_t type) { (void)u; call3(0x0043846cu, 1, ga(st), (uint32_t)(int32_t)type); }
static void cb_box(void* u, uint8_t* ai) { (void)u; call2(0x00438320u, ga(ai), 0); }
static void cb_weapons(void* u, uint8_t* ai, uint8_t* w) { (void)u; call3(0x0043807cu, ga(ai), ga(w), 0); }
static void cb_weapons_c0(void* u, uint8_t* w) { (void)u; uint32_t x[1] = {ga(w)}; rt_call(0x0044c81cu, x, 1); }
static void cb_hit_state(void* u, uint8_t* ai) { (void)u; uint32_t x[5] = {ga(ai), 2, 2, 0, 0}; rt_call(0x00420be8u, x, 5); }

static const AiStartOps* ops(void)
{
    static AiStartOps o;
    o.identity = (const int32_t*)(const void*)rt_ptr(0x004e69a4u);
    o.stats_init = cb_stats_init;
    o.collision_box = cb_box;
    o.weapons_init = cb_weapons;
    o.weapons_c0 = cb_weapons_c0;
    o.hit_state = cb_hit_state;
    o.spawns = (SpawnList*)(void*)rt_ptr(0x004fe1b4u);
    o.spawn_count = rt_r8(0x004fe1e1u);
    o.user = NULL;
    return &o;
}

static uint32_t b_car_reset(const uint32_t* a, int n)
{
    AiEnv* e = env();
    ai_car_reset(P(0), e, ops());
    return 0;
}
/* 0x0043af58(ai, type, reset_weapons) */
static uint32_t b_car_start(const uint32_t* a, int n)
{
    AiEnv* e = env();
    ai_car_start(P(0), (int16_t)A(1), (int)A(2), e, ops());
    return 0;
}

/* ---- ai_slots.c ---- */

/* 0x00492e50(model, 0); 0x004583c8(arg, slot, type) */
static void cb_model_ref(void* u, int32_t model) { (void)u; call2(0x00492e50u, (uint32_t)model, 0); }
static void cb_slot_bind(void* u, int32_t arg, int32_t slot, int32_t type) { (void)u; call3(0x004583c8u, (uint32_t)arg, (uint32_t)slot, (uint32_t)type); }

static const CarSlots* slots(void)
{
    static CarSlots s;
    s.type = (int16_t*)(void*)rt_ptr(0x00503028u);
    s.model = (int32_t*)(void*)rt_ptr(0x00503048u);
    s.status = rt_ptr(0x00502fdcu);
    s.count = (int32_t*)(void*)rt_ptr(0x00503084u);
    s.ai_count = (int32_t*)(void*)rt_ptr(0x0050c2b0u);
    s.model_ref = cb_model_ref;
    s.bind = cb_slot_bind;
    s.user = NULL;
    return &s;
}

/* 0x00457428(model, type), 0x00458340(arg, type) */
static uint32_t b_register_model(const uint32_t* a, int n) { car_register_model(slots(), (int32_t)A(0), (int16_t)A(1)); return 0; }
static uint32_t b_bind_by_type(const uint32_t* a, int n) { car_bind_ai_by_type(slots(), (int32_t)A(0), (int16_t)A(1)); return 0; }

void rt_bind_ai(void)
{
    rt_bind(0x00425674u, b_goal_bearing, "ai_goal_bearing");
    rt_bind(0x0042577cu, b_goal2_bearing, "ai_goal2_bearing");
    rt_bind(0x004259ecu, b_update_stuck, "ai_update_stuck");
    rt_bind(0x00430460u, b_lateral_offset, "ai_route_lateral_offset");
    rt_bind(0x004291dcu, b_pass_lane, "ai_set_pass_lane");
    rt_bind(0x00427dd8u, b_aim_point, "ai_set_route_aim_point");
    rt_bind(0x0042ce2cu, b_near_zone_edge, "ai_near_zone_edge");
    rt_bind(0x00439588u, b_type_top_speed, "car_type_top_speed");
    rt_bind(0x0045b3fcu, b_melee_special, "car_has_melee_special");
    rt_bind(0x0043953cu, b_set_top_speed, "car_stats_set_top_speed");
    rt_bind(0x00433000u, b_segment_speed, "ai_set_segment_speed");
    rt_bind(0x0045afc8u, b_target_car, "ai_target_car");
    rt_bind(0x0045acb8u, b_nearest_player, "ai_nearest_player");
    rt_bind(0x00432f2cu, b_boss_jump, "ai_boss_jump_check");
    rt_bind(0x00432ba0u, b_advance_node, "ai_advance_route_node");
    rt_bind(0x004334c8u, b_choose_next, "ai_choose_next_node");
    rt_bind(0x00427838u, b_roll_route, "ai_roll_take_route");
    rt_bind(0x0042c3e0u, b_nearest_node, "car_nearest_node");
    rt_bind(0x00432940u, b_place_on_node, "ai_place_on_route_node");
    rt_bind(0x00427710u, b_replan, "ai_replan_route");
    rt_bind(0x0042f6d4u, b_reroute_zone, "ai_reroute_zone");
    rt_bind(0x004295fcu, b_chase_direct, "ai_should_chase_direct");
    rt_bind(0x00429f7cu, b_nearest_to_human, "car_nearest_to_human");
    rt_bind(0x004325f8u, b_patch_boss, "ai_route_patch_boss_level");
    rt_bind(0x004102f4u, b_setup_profile, "ai_car_setup_profile");
    rt_bind(0x0044a40cu, b_jitter, "math_jitter_pos");
    rt_bind(0x004107c0u, b_event_zones, "event_objects_find_zones");
    rt_bind(0x0043b0ccu, b_car_reset, "ai_car_reset");
    rt_bind(0x0043af58u, b_car_start, "ai_car_start");
    rt_bind(0x00457428u, b_register_model, "car_register_model");
    rt_bind(0x00458340u, b_bind_by_type, "car_bind_ai_by_type");
}

RT_BIND_AREA(ai)
