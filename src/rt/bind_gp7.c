/*
 * bind_gp7.c: the two car functions completed in src/game/gp7_car.c, as the whole originals.
 *
 *   0x00429fd0 car_clear_controls(EAX block): gp7_clear_controls.
 *   0x00437850 player_car_setup(EAX car, EDX type, EBX full weapon setup flag): gp7_player_car_setup.
 *   0x0042a020 car_terrain_step(EAX car, EDX player flag): the verified first part
 *     car_ground_contact (src/game/car.c, up to 0x0042a4e7) then gp7_car_terrain_surface (from
 *     0x0042a4e7), which reads only the car and the player flag from the first part's state.
 *
 * car_ground_contact's CarEnv, the real equivalents of its harness: the terrain globals as
 * bind_physics.c reads them; the platform table at [0x00502f18]; a platform's target is the x, y
 * of its entity (table +0x34 -> entity +0xE8); leaving the ground calls 0x00420be8(car, 3, 2, 1,
 * stack player) as the original does. Its frame ticks / fps come from 0x004e8300 / 0x004e8304.
 * Port code, not a decompiled function.
 */
#include "rt_env.h"

#include "game/car.h"
#include "game/gp7_car.h"
#include "math/tm2math.h"
#include "physics/terrain_probe.h"

static void sync(void)
{
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
}

static uint32_t ga(const void* p) { return p ? rt_addr(p) : 0; }

/* 0x00429fd0(block) */
static uint32_t b_clear_controls(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    gp7_clear_controls(&e, rt_arg(a, n, 0));
    rt_en2env_done();
    return 0;
}

/* 0x00437850 */
static uint32_t b_player_car_setup(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    uint32_t r = gp7_player_car_setup(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2));
    rt_en2env_done();
    return r;
}

static const int32_t* cb_platform_target(void* u, int32_t slot)
{
    (void)u;
    uint32_t table = rt_r32(0x00502f18u);
    uint32_t ent = rt_r32(table + 0x38u * (uint32_t)slot + 0x34u);
    return (const int32_t*)(const void*)rt_ptr(ent + 0xe8u);
}

static void cb_left_ground(void* u, uint8_t* car, int player)
{
    (void)u;
    uint32_t x[5] = {ga(car), 3, 2, 1, (uint32_t)player};
    rt_call(0x00420be8u, x, 5);
}

/* 0x0042a020(car, player) */
static uint32_t b_car_terrain_step(const uint32_t* a, int n)
{
    sync();
    uint32_t car = rt_arg(a, n, 0);
    int player = (uint8_t)rt_arg(a, n, 1) != 0;
    RmpWorld w = {rt_ptr(0x004fb17cu), (int32_t)rt_r32(0x004fb174u), rt_ptr(0x004fe162u), rt_ptr(0x004fe185u),
                  rt_r8(0x00624ec7u)};
    CarEnv ce = {&w, rt_ptr(rt_r32(0x00502f18u)), cb_platform_target, cb_left_ground, NULL};
    car_ground_contact(rt_ptr(car), player, &ce);
    En2Env e = rt_en2env();
    gp7_car_terrain_surface(&e, car, player);
    rt_en2env_done();
    return 0;
}

void rt_bind_gp7(void)
{
    rt_bind(0x00429fd0u, b_clear_controls, "car_clear_controls");
    rt_bind(0x00437850u, b_player_car_setup, "player_car_setup");
    rt_bind(0x0042a020u, b_car_terrain_step, "car_terrain_step");
}

RT_BIND_AREA(gp7)
