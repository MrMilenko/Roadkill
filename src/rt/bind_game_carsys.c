/*
 * bind_game_carsys.c: the car system accessors and helpers (src/game/carsys_*.c), called the way
 * tools/emu/diff_carsys_{records,misc,body,hits,input,section,world}.py called them.
 *
 * These ports take host pointers to the original's globals (CarRecords, CarsysMisc, ...). Plain
 * globals are rt_ptr(<address>). The tables of pointers (object slots 0x00503048, body objects
 * 0x00502910, ...) hold 32 bit game addresses in game memory, while the ports index them as host
 * pointer arrays: each call builds a host copy of the table on the C stack (game address 0 is
 * NULL, others rt_ptr) and after the call writes back only the entries the port changed (so a
 * callback that rewrote the table in game memory meanwhile is not undone). Everything is per
 * call, so a callback that re-enters one of these adapters gets its own copies.
 *
 * Callbacks call the original address with the registers the harness's stubs read (EAX, EDX,
 * EBX, ECX, then stack words), host pointers turned back into game addresses.
 */
#include <stdio.h>
#include <stdlib.h>

#include "rt_env.h"
#include "core/tm2rand.h"
#include "math/tm2math.h"
#include "game/carsys_body.h"
#include "game/carsys_hits.h"
#include "game/carsys_input.h"
#include "game/carsys_misc.h"
#include "game/carsys_records.h"
#include "game/carsys_section.h"
#include "game/carsys_world.h"

/* ---- shared helpers ---- */

/* tm2math keeps its tables, the frame's 60 Hz ticks and the fps on the host side: load the
 * tables once (from the player's files, TM2_DATA or disc/files) and copy ticks (0x004e8300) and
 * fps (0x004e8304) from game memory before every call. The rand state is tm2rand's own: the core
 * binds crt_rand 0x004899c2 / crt_srand 0x004899e6 to it. */
static void sync(void)
{
    static int loaded;
    if (!loaded) {
        const char* dir = getenv("TM2_DATA");
        char s[1024], x[1024];
        if (!dir || !*dir) dir = "disc/files";
        snprintf(s, sizeof s, "%s/SINTAB.DAT", dir);
        snprintf(x, sizeof x, "%s/TM2.EXE", dir);
        if (tm2_math_load_tables(s, x) != 0) rt_stop("cannot load the math tables (set TM2_DATA)", 0);
        loaded = 1;
    }
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
}

static uint8_t* hp(uint32_t a) { return a ? rt_ptr(a) : NULL; }
static uint32_t ga(const void* p) { return p ? rt_addr(p) : 0; }

/* a game table of n pointers at addr as host pointers, with what was read */
#define TAB_MAX 50
typedef struct { uint32_t addr; int n; uint8_t* h[TAB_MAX]; uint8_t* was[TAB_MAX]; } Tab;
static void tab_in(Tab* t, uint32_t addr, int n)
{
    t->addr = addr;
    t->n = n;
    for (int i = 0; i < n; i++) t->h[i] = t->was[i] = hp(rt_r32(addr + 4u * (uint32_t)i));
}
static void tab_out(const Tab* t)
{
    for (int i = 0; i < t->n; i++)
        if (t->h[i] != t->was[i]) rt_w32(t->addr + 4u * (uint32_t)i, ga(t->h[i]));
}

static uint32_t call4(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, int n)
{
    uint32_t a[4] = {eax, edx, ebx, ecx};
    return rt_call(fn, a, n);
}

#define A(i) rt_arg(a, n, (i))
#define UNUSED (void)a; (void)n

/* ---- CarRecords ---- */

/* the slot object table 0x00503048: 15 slots, the slot count 0x00503084 (read as entry 15 by
 * carsys_object_from_owner's miss), then the 8 human objects 0x00503088 */
typedef struct { CarRecords r; Tab slots; } Rec;

static void rec_in(Rec* c)
{
    tab_in(&c->slots, 0x00503048u, 24);
    CarRecords* r = &c->r;
    r->human = rt_ptr(0x005030a8u);
    r->human_count = (int32_t*)rt_ptr(0x005075e0u);
    r->ai = rt_ptr(0x0050760cu);
    r->ai_count = (int32_t*)rt_ptr(0x0050c2b0u);
    r->watched_ai = (int32_t*)rt_ptr(0x00502f90u);
    r->slot_count = (int32_t*)rt_ptr(0x00503084u);
    r->slot_objects = c->slots.h;
    r->slot_owner = (int16_t*)rt_ptr(0x00503028u);
    r->slot_index = (int32_t*)rt_ptr(0x00502fecu);
    r->slot_kind = rt_ptr(0x00502fdcu);
    r->human_objects = c->slots.h + 16;
    r->ai_targets = (int32_t*)rt_ptr(0x00502fb4u);
    r->human_slot = (int32_t*)rt_ptr(0x00507570u);
    r->ai_slot = (int32_t*)rt_ptr(0x0050c28cu);
    r->difficulty = (int32_t*)rt_ptr(0x00502f98u);
    r->boss_defeated = rt_ptr(0x0050c2c0u);
    r->hud_speed = (int32_t*)rt_ptr(0x005075c0u);
    sync();
}
static void rec_out(const Rec* c) { tab_out(&c->slots); }

#define REC_BEGIN Rec c; rec_in(&c)
#define REC_END rec_out(&c)

static uint32_t b_boss_defeated(const uint32_t* a, int n)
{ UNUSED; REC_BEGIN; uint32_t r = carsys_boss_defeated(&c.r); REC_END; return r; }
/* 0x0045ad38 (pos, exclude): the AI version takes pos only */
static uint32_t b_ai_nearest_alive(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = (uint32_t)carsys_ai_nearest_alive(&c.r, (const int32_t*)rt_ptr(A(0))); REC_END; return r; }
static uint32_t b_human_pos(const uint32_t* a, int n)
{ REC_BEGIN; carsys_human_pos(&c.r, (int32_t)A(0), (int32_t*)rt_ptr(A(1))); REC_END; return 0; }
static uint32_t b_ai_pos(const uint32_t* a, int n)
{ REC_BEGIN; carsys_ai_pos(&c.r, (int32_t)A(0), (int32_t*)rt_ptr(A(1))); REC_END; return 0; }
static uint32_t b_human_angles(const uint32_t* a, int n)
{ REC_BEGIN; carsys_human_angles(&c.r, (int32_t)A(0), (int16_t*)rt_ptr(A(1))); REC_END; return 0; }
static uint32_t b_human_object(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = ga(carsys_human_object(&c.r, (int32_t)A(0))); REC_END; return r; }
static uint32_t b_hud_speed_reset(const uint32_t* a, int n)
{ REC_BEGIN; carsys_hud_speed_reset(&c.r, (int32_t)A(0)); REC_END; return 0; }
static uint32_t b_hud_speed_smooth(const uint32_t* a, int n)
{ REC_BEGIN; carsys_hud_speed_smooth(&c.r, (int32_t)A(0)); REC_END; return 0; }
static uint32_t b_display_speed(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = (uint32_t)carsys_display_speed(&c.r, (int32_t)A(0)); REC_END; return r; }
static uint32_t b_ai_display_speed(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = (uint32_t)carsys_ai_display_speed(&c.r, (int32_t)A(0)); REC_END; return r; }
static uint32_t b_object_count(const uint32_t* a, int n)
{ UNUSED; REC_BEGIN; uint32_t r = (uint32_t)carsys_object_count(&c.r); REC_END; return r; }
static uint32_t b_human_count(const uint32_t* a, int n)
{ UNUSED; REC_BEGIN; uint32_t r = (uint32_t)carsys_human_count(&c.r); REC_END; return r; }
static uint32_t b_human_get(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = ga(carsys_human_get(&c.r, (int32_t)A(0))); REC_END; return r; }
static uint32_t b_from_slot(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = ga(carsys_from_slot(&c.r, (int32_t)A(0), rt_ptr(A(1)))); REC_END; return r; }
static uint32_t b_ai_count(const uint32_t* a, int n)
{ UNUSED; REC_BEGIN; uint32_t r = (uint32_t)carsys_ai_count(&c.r); REC_END; return r; }
static uint32_t b_ai_get(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = ga(carsys_ai_get(&c.r, (int32_t)A(0))); REC_END; return r; }
static uint32_t b_ai_watched(const uint32_t* a, int n)
{ UNUSED; REC_BEGIN; uint32_t r = ga(carsys_ai_watched(&c.r)); REC_END; return r; }
static uint32_t b_object_from_owner(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = ga(carsys_object_from_owner(&c.r, (int32_t)A(0))); REC_END; return r; }
static uint32_t b_find_by_object(const uint32_t* a, int n)
{
    REC_BEGIN;
    uint32_t r = carsys_find_by_object(&c.r, hp(A(0)), rt_ptr(A(1)), (int32_t*)rt_ptr(A(2)));
    REC_END;
    return r;
}
static uint32_t b_object_id(const uint32_t* a, int n)
{ REC_BEGIN; uint32_t r = (uint32_t)carsys_object_id(&c.r, (int32_t)A(0), (uint8_t)A(1)); REC_END; return r; }
static uint32_t b_find_by_id(const uint32_t* a, int n)
{
    REC_BEGIN;
    uint32_t r = carsys_find_by_id(&c.r, (int32_t)A(0), rt_ptr(A(1)), (int32_t*)rt_ptr(A(2)));
    REC_END;
    return r;
}
static uint32_t b_ai_set_difficulty(const uint32_t* a, int n)
{ REC_BEGIN; carsys_ai_set_difficulty(&c.r, A(0)); REC_END; return 0; }
static uint32_t b_ai_difficulty(const uint32_t* a, int n)
{ UNUSED; REC_BEGIN; uint32_t r = (uint32_t)carsys_ai_difficulty(&c.r); REC_END; return r; }
/* 0x0043a93c (stats, percent): tested inside 0x0045b370 */
static uint32_t b_stats_speed_percent(const uint32_t* a, int n)
{ sync(); carsys_stats_speed_percent(rt_ptr(A(0)), (int32_t)A(1)); return 0; }

/* ---- CarsysMisc ---- */

static void m_bullets_move(void* u) { (void)u; call4(0x00412fb8u, 0, 0, 0, 0, 0); }
static uint8_t* m_human_object(void* u, int32_t i) { (void)u; return hp(call4(0x0045ae24u, (uint32_t)i, 0, 0, 0, 1)); }
static void m_anim_step(void* u, uint8_t* obj, uint32_t step) { (void)u; call4(0x004a0c94u, ga(obj), step, 0, 0, 2); }
static void m_fx(void* u, int32_t x, int32_t y) { (void)u; call4(0x00451f54u, (uint32_t)x, (uint32_t)y, 0, 0, 2); }
static void m_world_reset(void* u) { (void)u; call4(0x00454ae0u, 0, 0, 0, 0, 0); }
static void m_obj_finish(void* u, uint8_t* obj) { (void)u; call4(0x004a3494u, ga(obj), 0, 0, 0, 1); }

/* player one object 0x004f11d4 (1), body objects 0x00502910 (15), turbo objects 0x0062129c (16) */
typedef struct { CarsysMisc m; Tab p1, body, turbo; } Misc;

static void misc_in(Misc* c)
{
    tab_in(&c->p1, 0x004f11d4u, 1);
    tab_in(&c->body, 0x00502910u, 15);
    tab_in(&c->turbo, 0x0062129cu, 16);
    CarsysMisc* m = &c->m;
    m->event_byte = (const int8_t*)rt_ptr(0x004f07f9u);
    m->flame_markers = rt_ptr(0x004f087cu);
    m->bullets_parity = (int32_t*)rt_ptr(0x004f11d0u);
    m->player_one_object = c->p1.h;
    m->surface_count = (const int32_t*)rt_ptr(0x004fb174u);
    m->section_first = rt_ptr(0x004fe162u);
    m->section_last = rt_ptr(0x004fe185u);
    m->fx_args = (int32_t*)rt_ptr(0x005024d0u);
    m->body_objects = c->body.h;
    m->slam_arm = (const int32_t*)rt_ptr(0x00502e98u);
    m->slam_claw = (const int32_t*)rt_ptr(0x00502e9cu);
    m->game_mode = (const int32_t*)rt_ptr(0x00502fa4u);
    m->world_flags_i32 = (int32_t*)rt_ptr(0x0050e0f4u);
    m->world_flags_u8 = rt_ptr(0x0050e0fcu);
    m->dark_tooth_music = rt_ptr(0x00621181u);
    m->shared_loop = rt_ptr(0x00621188u);
    m->freeze_objects = rt_ptr(0x00621640u);
    m->turbo_objects = c->turbo.h;
    m->target_actors = rt_ptr(0x00622b58u);
    m->bullets_move = m_bullets_move;
    m->human_object = m_human_object;
    m->anim_step = m_anim_step;
    m->fx_451f54 = m_fx;
    m->world_reset = m_world_reset;
    m->obj_finish = m_obj_finish;
    m->user = NULL;
}
static void misc_out(const Misc* c) { tab_out(&c->p1); tab_out(&c->body); tab_out(&c->turbo); }

#define MISC_BEGIN Misc c; misc_in(&c)
#define MISC_END misc_out(&c)

static uint32_t b_event_byte(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; uint32_t r = (uint32_t)carsys_event_byte(&c.m); MISC_END; return r; }
static uint32_t b_flame_marker(const uint32_t* a, int n)
{ MISC_BEGIN; uint32_t r = rt_addr(carsys_flame_marker(&c.m, (int32_t)A(0))); MISC_END; return r; }
static uint32_t b_bullets_update(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; carsys_bullets_update(&c.m); MISC_END; return 0; }
static uint32_t b_body_spin_clear(const uint32_t* a, int n) { carsys_body_spin_clear(rt_ptr(A(0))); return 0; }
static uint32_t b_push_clear(const uint32_t* a, int n) { carsys_push_clear(rt_ptr(A(0))); return 0; }
static uint32_t b_eiffel_fallen(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; carsys_eiffel_fallen_surfaces(&c.m); MISC_END; return 0; }
static uint32_t b_track_surface_index(const uint32_t* a, int n)
{ MISC_BEGIN; uint32_t r = (uint32_t)carsys_track_surface_index(&c.m, (int32_t)A(0)); MISC_END; return r; }
static uint32_t b_fx_451a5c(const uint32_t* a, int n)
{
    MISC_BEGIN;
    carsys_fx_451a5c(&c.m, (int32_t)A(0), (int32_t)A(1), (int32_t)A(2), (int32_t)A(3));
    MISC_END;
    return 0;
}
static uint32_t b_body_object_register(const uint32_t* a, int n)
{ MISC_BEGIN; carsys_body_object_register(&c.m, (int32_t)A(0), rt_ptr(A(1))); MISC_END; return 0; }
static uint32_t b_slam_arm(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; uint32_t r = (uint32_t)carsys_slam_arm_angle(&c.m); MISC_END; return r; }
static uint32_t b_slam_claw(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; uint32_t r = (uint32_t)carsys_slam_claw_angle(&c.m); MISC_END; return r; }
static uint32_t b_world_teardown(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; carsys_world_teardown(&c.m); MISC_END; return 0; }
static uint32_t b_game_mode(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; uint32_t r = (uint32_t)carsys_game_mode(&c.m); MISC_END; return r; }
/* 0x00469cc4 (p, value): the port reads p[1], the original the word at p + 4 */
static uint32_t b_scene_choice_set(const uint32_t* a, int n)
{
    uint8_t* p[2] = {NULL, hp(rt_r32(A(0) + 4u))};
    carsys_scene_choice_set(p, (uint8_t)A(1));
    return 0;
}
static uint32_t b_dark_tooth_music(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; uint32_t r = carsys_dark_tooth_music(&c.m); MISC_END; return r; }
static uint32_t b_set_shared_loop(const uint32_t* a, int n)
{ MISC_BEGIN; carsys_set_shared_loop(&c.m, (uint8_t)A(0)); MISC_END; return 0; }
static uint32_t b_freeze_reset_all(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; carsys_freeze_reset_all(&c.m); MISC_END; return 0; }
/* the original returns whatever eax holds; callers ignore it */
static uint32_t b_freeze_set_state(const uint32_t* a, int n)
{ MISC_BEGIN; carsys_freeze_set_state(&c.m, (int32_t)A(0), A(1)); MISC_END; return 0; }
static uint32_t b_target_actors(const uint32_t* a, int n)
{ UNUSED; MISC_BEGIN; uint32_t r = ga(carsys_target_actors(&c.m)); MISC_END; return r; }
static uint32_t b_session_mode(const uint32_t* a, int n) { UNUSED; return carsys_session_mode(); }
static uint32_t b_obj_finish_if_8(const uint32_t* a, int n)
{ MISC_BEGIN; uint32_t r = (uint32_t)carsys_obj_finish_if_8(&c.m, rt_ptr(A(0))); MISC_END; return r; }

/* ---- body: wheel effects, body pose ---- */

/* 0x0044a40c(pos, r, r, r) returns the game address of a jittered copy */
static uint8_t* w_jitter(void* u, uint8_t* pos, int32_t r)
{
    (void)u;
    return hp(call4(0x0044a40cu, ga(pos), (uint32_t)r, (uint32_t)r, (uint32_t)r, 4));
}
static void w_fx(void* u, int kind, uint8_t* pos)
{
    static const uint32_t fx[4] = {0x00451230u, 0x00451274u, 0x004512b4u, 0x004512f8u};
    (void)u;
    call4(fx[kind & 3], ga(pos), 0, 0, 0, 2);
}
static const CarsysWheelFx wheel_fx = {w_jitter, w_fx, NULL};

/* 0x0042b02c (probes, kind) */
static uint32_t b_wheels_spawn_fx(const uint32_t* a, int n)
{
    sync();
    carsys_wheels_spawn_fx(rt_ptr(A(0)), (uint8_t)A(1), &wheel_fx);
    return 0;
}
/* 0x00453c78 (car, player) */
static uint32_t b_body_pose_update(const uint32_t* a, int n)
{
    sync();
    Tab body;
    tab_in(&body, 0x00502910u, 15);
    carsys_body_pose_update(rt_ptr(A(0)), (int)A(1), body.h);
    tab_out(&body);
    return 0;
}

/* ---- hits ---- */

static void h_spin_out(void* u, uint8_t* car, int human, int kind)
{ (void)u; call4(0x00421920u, ga(car), (uint32_t)human, (uint32_t)kind, 0, 4); }
/* 0x00420be8(car, 5, 2, 2, human): one stack word */
static void h_shake(void* u, uint8_t* car, int human)
{
    (void)u;
    uint32_t a[5] = {ga(car), 5, 2, 2, (uint32_t)human};
    rt_call(0x00420be8u, a, 5);
}
static void h_smoke(void* u, uint8_t* pos) { (void)u; call4(0x00451230u, ga(pos), 0, 0, 0, 2); }

typedef struct { Rec rec; CarsysHitOps ops; } Hits;
static void hits_in(Hits* c)
{
    rec_in(&c->rec);
    c->ops.rec = &c->rec.r;
    c->ops.spin_out = h_spin_out;
    c->ops.shake = h_shake;
    c->ops.jitter = w_jitter;
    c->ops.smoke = h_smoke;
    c->ops.user = NULL;
    c->ops.slam_arm = (const int32_t*)rt_ptr(0x00502e98u);
}

/* 0x00421800 (car, player, pos, amount) */
static uint32_t b_knock_back(const uint32_t* a, int n)
{
    sync();
    carsys_knock_back(rt_ptr(A(0)), (int)A(1), (const int32_t*)rt_ptr(A(2)), (int32_t)A(3));
    return 0;
}
/* 0x00417640 (car, player): the original tests the low byte of the player flag */
static uint32_t b_skid_smoke(const uint32_t* a, int n)
{
    Hits c;
    hits_in(&c);
    carsys_skid_smoke(rt_ptr(A(0)), (int)(uint8_t)A(1), &c.ops);
    rec_out(&c.rec);
    return 0;
}
/* 0x00432340 (id, angle, back) */
static uint32_t b_spin_out_by_id(const uint32_t* a, int n)
{
    Hits c;
    hits_in(&c);
    carsys_spin_out_by_id((int16_t)A(0), (int16_t)A(1), (uint8_t)A(2), &c.ops);
    rec_out(&c.rec);
    return 0;
}
/* 0x0041fa98 (active, summary) */
static uint32_t b_hit_pass(const uint32_t* a, int n)
{
    REC_BEGIN;
    carsys_hit_pass(&c.r, rt_ptr(A(0)), rt_ptr(A(1)));
    REC_END;
    return 0;
}
/* 0x004320d0 (id, stats, body, lifting; side on the stack) */
static uint32_t b_slam_carry_target(const uint32_t* a, int n)
{
    Hits c;
    hits_in(&c);
    carsys_slam_carry_target((int16_t)A(0), rt_ptr(A(1)), rt_ptr(A(2)), (uint8_t)A(3), (uint8_t)A(4), &c.ops);
    rec_out(&c.rec);
    return 0;
}

/* ---- input ---- */

/* the mouse button queries 0x004c41c8 / 0x004c4208 / 0x004c41e8 (mouse) */
static int32_t i_button(void* u, void* mouse, int which)
{
    static const uint32_t fn[3] = {0x004c41c8u, 0x004c4208u, 0x004c41e8u};
    (void)u;
    return (int32_t)call4(fn[which], ga(mouse), 0, 0, 0, 1);
}

typedef struct { CarsysInput in; Tab models; } Input;
static void input_in(Input* c)
{
    tab_in(&c->models, 0x00a81c4cu, 20);
    c->in.device_types = rt_ptr(0x00a03a78u);
    c->in.keys = rt_ptr(0x00a5d810u);
    c->in.models = (void**)c->models.h;
    c->in.mouse = rt_ptr(0x00a82164u);
    c->in.button = i_button;
    c->in.user = NULL;
}

static uint32_t b_device_type(const uint32_t* a, int n)
{ Input c; input_in(&c); return carsys_input_device_type(&c.in, (int32_t)A(0)); }
static uint32_t b_first_key(const uint32_t* a, int n)
{ UNUSED; Input c; input_in(&c); return carsys_input_first_key(&c.in); }
static uint32_t b_model_add(const uint32_t* a, int n)
{ Input c; input_in(&c); carsys_world_model_add(&c.in, hp(A(0))); tab_out(&c.models); return 0; }
static uint32_t b_model_remove(const uint32_t* a, int n)
{ Input c; input_in(&c); carsys_world_model_remove(&c.in, hp(A(0))); tab_out(&c.models); return 0; }
static uint32_t b_mouse_button(const uint32_t* a, int n)
{ UNUSED; Input c; input_in(&c); return carsys_input_mouse_button(&c.in); }

/* ---- section ---- */

static void s_node_removed(void* u, int32_t node) { (void)u; call4(0x0043bd74u, (uint32_t)node, 0, 0, 0, 1); }

/* 0x0042f2dc (section) */
static uint32_t b_section_disable(const uint32_t* a, int n)
{
    REC_BEGIN;
    CarsysSectionEnv e = {
        rt_ptr(0x004fca2cu), (int32_t*)rt_ptr(0x004fcbc8u), (int32_t*)rt_ptr(0x004fcc54u),
        rt_ptr(0x004fe162u), rt_ptr(0x004fe185u), rt_ptr(0x004fb17cu), rt_ptr(0x004fe1e4u),
        (const int32_t*)rt_ptr(0x004fe1b0u), rt_ptr(0x004fdc0du), &c.r,
        s_node_removed, h_spin_out, NULL};
    carsys_world_section_disable(&e, (int32_t)A(0));
    REC_END;
    return 0;
}

/* ---- world ---- */

static uint8_t* wd_resolve(void* u, uint32_t handle) { (void)u; return hp(handle); }
static void wd_particles(void* u) { (void)u; call4(0x004517e0u, 0, 0, 0, 0, 0); }
static void wd_reset(void* u, uint8_t* car) { (void)u; call4(0x0043ac5cu, ga(car), 0, 0, 0, 2); }
static void wd_stats_init(void* u, uint8_t* car, int32_t owner)
{ (void)u; call4(0x0043af58u, ga(car), (uint32_t)owner, 1, 0, 3); }
static void wd_register(void* u, uint8_t* obj) { (void)u; call4(0x00492e50u, ga(obj), 1, 0, 0, 2); }
static void wd_init(void* u, uint8_t* car) { (void)u; call4(0x004102f4u, ga(car), 0, 0, 0, 1); }

/* pickups 0x00621388 (50), AI car objects 0x005075e8 (9); the platform table is the pointer at
 * 0x00502f18, read on every call */
typedef struct { Rec rec; CarsysWorld w; Tab pickups, ai_objects; } World;
static void world_in(World* c)
{
    rec_in(&c->rec);
    tab_in(&c->pickups, 0x00621388u, 50);
    tab_in(&c->ai_objects, 0x005075e8u, 9);
    CarsysWorld* w = &c->w;
    w->rec = &c->rec.r;
    w->fx_sprites = rt_ptr(0x00500304u);
    w->fx_cooldown = (int32_t*)rt_ptr(0x00500030u);
    w->smoke_tables[0] = 0x00502246u;
    w->smoke_tables[1] = 0x005022c6u;
    w->smoke_tables[2] = 0x00502346u;
    w->unlock = rt_ptr(0x009f6c1cu);
    w->session = rt_ptr(0x00624ea9u);
    w->platforms = rt_ptr(rt_r32(0x00502f18u));
    w->pickups = c->pickups.h;
    w->player_one_flags = (int32_t*)rt_ptr(0x004ffff4u);
    w->ai_objects = c->ai_objects.h;
    w->resolve = wd_resolve;
    w->particles_update = wd_particles;
    w->car_reset_43ac5c = wd_reset;
    w->stats_init = wd_stats_init;
    w->obj_register = wd_register;
    w->car_init_4102f4 = wd_init;
    w->user = NULL;
}
static void world_out(const World* c) { rec_out(&c->rec); tab_out(&c->pickups); tab_out(&c->ai_objects); }

#define WORLD_BEGIN World c; world_in(&c)
#define WORLD_END world_out(&c)

static uint32_t b_fx_sprites_update(const uint32_t* a, int n)
{ UNUSED; WORLD_BEGIN; carsys_fx_sprites_update(&c.w); WORLD_END; return 0; }
static uint32_t b_car_is_selectable(const uint32_t* a, int n)
{ WORLD_BEGIN; uint32_t r = (uint32_t)carsys_car_is_selectable(&c.w, (uint8_t)A(0)); WORLD_END; return r; }
static uint32_t b_track_vehicle_place(const uint32_t* a, int n)
{ WORLD_BEGIN; carsys_track_vehicle_place(&c.w, (int32_t)A(0)); WORLD_END; return 0; }
static uint32_t b_pickup_set_visible(const uint32_t* a, int n)
{ WORLD_BEGIN; carsys_pickup_set_visible(&c.w, (int32_t)A(0), (int32_t)A(1), (uint8_t)A(2)); WORLD_END; return 0; }
static uint32_t b_player_one_reset(const uint32_t* a, int n)
{ UNUSED; WORLD_BEGIN; carsys_player_one_reset(&c.w); WORLD_END; return 0; }
static uint32_t b_bind_ai_slot(const uint32_t* a, int n)
{ WORLD_BEGIN; carsys_bind_ai_slot(&c.w, (int32_t)A(0), (int32_t)A(1)); WORLD_END; return 0; }

void rt_bind_game_carsys(void)
{
    /* records */
    rt_bind(0x0043a93cu, b_stats_speed_percent, "carsys_stats_speed_percent");
    rt_bind(0x0045a8bcu, b_boss_defeated, "carsys_boss_defeated");
    rt_bind(0x0045ad38u, b_ai_nearest_alive, "carsys_ai_nearest_alive");
    rt_bind(0x0045adacu, b_human_pos, "carsys_human_pos");
    rt_bind(0x0045add0u, b_ai_pos, "carsys_ai_pos");
    rt_bind(0x0045adf8u, b_human_angles, "carsys_human_angles");
    rt_bind(0x0045ae24u, b_human_object, "carsys_human_object");
    rt_bind(0x0045ae34u, b_hud_speed_reset, "carsys_hud_speed_reset");
    rt_bind(0x0045ae44u, b_hud_speed_smooth, "carsys_hud_speed_smooth");
    rt_bind(0x0045aee8u, b_display_speed, "carsys_display_speed");
    rt_bind(0x0045af58u, b_ai_display_speed, "carsys_ai_display_speed");
    rt_bind(0x0045afacu, b_object_count, "carsys_object_count");
    rt_bind(0x0045afb4u, b_human_count, "carsys_human_count");
    rt_bind(0x0045afbcu, b_human_get, "carsys_human_get");
    rt_bind(0x0045b028u, b_from_slot, "carsys_from_slot");
    rt_bind(0x0045b078u, b_ai_count, "carsys_ai_count");
    rt_bind(0x0045b080u, b_ai_get, "carsys_ai_get");
    rt_bind(0x0045b0a0u, b_ai_watched, "carsys_ai_watched");
    rt_bind(0x0045b0b8u, b_object_from_owner, "carsys_object_from_owner");
    rt_bind(0x0045b0ecu, b_find_by_object, "carsys_find_by_object");
    rt_bind(0x0045b154u, b_object_id, "carsys_object_id");
    rt_bind(0x0045b178u, b_find_by_id, "carsys_find_by_id");
    rt_bind(0x0045b370u, b_ai_set_difficulty, "carsys_ai_set_difficulty");
    rt_bind(0x0045b3dcu, b_ai_difficulty, "carsys_ai_difficulty");
    /* misc */
    rt_bind(0x00411dbcu, b_event_byte, "carsys_event_byte");
    rt_bind(0x00411e14u, b_flame_marker, "carsys_flame_marker");
    rt_bind(0x004133a8u, b_bullets_update, "carsys_bullets_update");
    rt_bind(0x00419160u, b_body_spin_clear, "carsys_body_spin_clear");
    rt_bind(0x00420bacu, b_push_clear, "carsys_push_clear");
    rt_bind(0x004325a8u, b_eiffel_fallen, "carsys_eiffel_fallen_surfaces");
    rt_bind(0x0043b4b0u, b_track_surface_index, "carsys_track_surface_index");
    rt_bind(0x00451a5cu, b_fx_451a5c, "carsys_fx_451a5c");
    rt_bind(0x00453c5cu, b_body_object_register, "carsys_body_object_register");
    rt_bind(0x00453ec0u, b_slam_arm, "carsys_slam_arm_angle");
    rt_bind(0x00453ec8u, b_slam_claw, "carsys_slam_claw_angle");
    rt_bind(0x00458004u, b_world_teardown, "carsys_world_teardown");
    rt_bind(0x0045c338u, b_game_mode, "carsys_game_mode");
    rt_bind(0x00469cc4u, b_scene_choice_set, "carsys_scene_choice_set");
    rt_bind(0x0046fc04u, b_dark_tooth_music, "carsys_dark_tooth_music");
    rt_bind(0x0046fc88u, b_set_shared_loop, "carsys_set_shared_loop");
    rt_bind(0x00470900u, b_freeze_reset_all, "carsys_freeze_reset_all");
    rt_bind(0x00470940u, b_freeze_set_state, "carsys_freeze_set_state");
    rt_bind(0x00471aecu, b_target_actors, "carsys_target_actors");
    rt_bind(0x004a28a8u, b_session_mode, "carsys_session_mode");
    rt_bind(0x004a341cu, b_obj_finish_if_8, "carsys_obj_finish_if_8");
    /* body */
    rt_bind(0x0042b02cu, b_wheels_spawn_fx, "carsys_wheels_spawn_fx");
    rt_bind(0x00453c78u, b_body_pose_update, "carsys_body_pose_update");
    /* hits */
    rt_bind(0x00417640u, b_skid_smoke, "carsys_skid_smoke");
    rt_bind(0x0041fa98u, b_hit_pass, "carsys_hit_pass");
    rt_bind(0x00421800u, b_knock_back, "carsys_knock_back");
    rt_bind(0x004320d0u, b_slam_carry_target, "carsys_slam_carry_target");
    rt_bind(0x00432340u, b_spin_out_by_id, "carsys_spin_out_by_id");
    /* input */
    rt_bind(0x004932a0u, b_device_type, "carsys_input_device_type");
    rt_bind(0x0049ecd0u, b_first_key, "carsys_input_first_key");
    rt_bind(0x004a14a0u, b_model_add, "carsys_world_model_add");
    rt_bind(0x004a14c0u, b_model_remove, "carsys_world_model_remove");
    rt_bind(0x004a2380u, b_mouse_button, "carsys_input_mouse_button");
    /* section, world */
    rt_bind(0x0042f2dcu, b_section_disable, "carsys_world_section_disable");
    rt_bind(0x0044ee0cu, b_player_one_reset, "carsys_player_one_reset");
    rt_bind(0x00450454u, b_fx_sprites_update, "carsys_fx_sprites_update");
    rt_bind(0x00455ef0u, b_track_vehicle_place, "carsys_track_vehicle_place");
    rt_bind(0x004583c8u, b_bind_ai_slot, "carsys_bind_ai_slot");
    rt_bind(0x004707b0u, b_pickup_set_visible, "carsys_pickup_set_visible");
    rt_bind(0x0047766cu, b_car_is_selectable, "carsys_car_is_selectable");
}

RT_BIND_AREA(game_carsys)
