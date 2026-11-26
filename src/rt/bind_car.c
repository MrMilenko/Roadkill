/*
 * bind_car.c: car block steps of src/game whose addresses tools/rt/unbound.tsv listed without a
 * source (car.h, car_stats.h, car_hit.h, car_spawn.h), called the way tools/emu/diff_car.py /
 * diff_car_stats.py / diff_car_hit_apply.py called the originals: the car block (or stats block)
 * in eax as a game address, the kind flag (player 1, AI 0) in edx.
 *
 * Host state: tm2math's tables and frame ticks / fps (copied from 0x004e8300 / 0x004e8304 before
 * each call), the car stats table read once from TM2.EXE (tm2_car_stats_load), both from TM2_DATA
 * (default disc/files).
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "assets/exe_carstats.h"
#include "game/car.h"
#include "game/car_hit.h"
#include "game/car_spawn.h"
#include "game/car_stats.h"
#include "math/tm2math.h"
#include "qol_cars.h"

#define A(i) rt_arg(a, n, (i))

static void data_path(char* out, size_t n, const char* file)
{
    const char* dir = getenv("TM2_DATA");
    if (!dir || !*dir) dir = "disc/files";
    snprintf(out, n, "%s/%s", dir, file);
}

static void host(void)
{
    static int loaded;
    if (!loaded) {
        char s[1024], x[1024];
        data_path(s, sizeof s, "SINTAB.DAT");
        data_path(x, sizeof x, "TM2.EXE");
        if (tm2_math_load_tables(s, x) != 0) rt_stop("math tables (set TM2_DATA to the game's files)", 0);
        loaded = 1;
    }
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
}

/* 0x00418d38 / 0x0041858c / 0x004220b8(car, player in dl) */
static uint32_t b_car_slide_check(const uint32_t* a, int n) { host(); car_slide_check(rt_ptr(A(0)), (uint8_t)A(1) != 0); return 0; }
static uint32_t b_car_move(const uint32_t* a, int n) { host(); car_move(rt_ptr(A(0)), (uint8_t)A(1) != 0); return 0; }
static uint32_t b_car_probe_latch(const uint32_t* a, int n) { host(); car_probe_latch(rt_ptr(A(0)), (uint8_t)A(1) != 0); return 0; }
/* 0x0043a944(stats) */
static uint32_t b_car_stats_derive(const uint32_t* a, int n) { host(); car_stats_derive(rt_ptr(A(0))); return 0; }
/* 0x004300e8(probes) */
static uint32_t b_car_probes_clear(const uint32_t* a, int n) { car_probes_clear(rt_ptr(A(0))); return 0; }

/* 0x00438320(car, player): the entity is 0x0045ae24(i16 car +0) for a player car, 0x0045ae2c(i16
 * car +4) for an AI car, as the original looks it up */
static uint32_t b_car_collision_box(const uint32_t* a, int n)
{
    uint32_t car = A(0);
    int player = (uint8_t)A(1) != 0;   /* test dl, dl */
    uint32_t v[1] = {(uint32_t)(int32_t)(int16_t)rt_r16(car + (player ? 0u : 4u))};
    uint32_t ent = rt_call(player ? 0x0045ae24u : 0x0045ae2cu, v, 1);
    car_collision_points(rt_ptr(car), player, rt_ptr(ent));
    return 0;
}

/* 0x0042f06c(probe state, cap) */
static uint32_t b_car_wheel_bounce(const uint32_t* a, int n) { host(); car_wheel_bounce(rt_ptr(A(0)), (int32_t)A(1)); return 0; }
/* 0x0042ca04(probe state, new zone, climb, shared in cl) on the loaded GRP 0x004fca18 */
static uint32_t b_car_zone_cross(const uint32_t* a, int n)
{
    host();
    car_zone_cross(rt_ptr(A(0)), (int32_t)A(1), (int32_t)A(2), (uint8_t)A(3), rt_ptr(0x004fca18u));
    return 0;
}

/* the effects car_wheels_place raises: only vehicle_set_state 0x004225a0(car, arg) */
static void cb_fx(void* u, int id, uint8_t* car, int player, int32_t arg)
{
    (void)u; (void)player;
    if (id != CAR_FX_SET_STATE) rt_stop("bind_car: unexpected car effect", (uint32_t)id);
    uint32_t v[2] = {rt_addr(car), (uint32_t)arg};
    rt_call(0x004225a0u, v, 2);
}

/* 0x0042c4d4(car, player in dl) on the loaded GRP */
static uint32_t b_car_wheels_place(const uint32_t* a, int n)
{
    static CarFx fx = {cb_fx, NULL, 0, {0, 0}, 0};
    host();
    car_wheels_place(rt_ptr(A(0)), (uint8_t)A(1) != 0, rt_ptr(0x004fca18u), &fx);
    return 0;
}

/* car 0xC0's two scene object turns 0x00453f10 / 0x00453ed0(angle) */
static void cb_rotate(void* u, int which, int32_t angle)
{
    (void)u;
    uint32_t v[1] = {(uint32_t)angle};
    rt_call(which ? 0x00453ed0u : 0x00453f10u, v, 1);
}

/* 0x0043846c(slot, stats, car id): multiplayer = 0x004632b0() */
static uint32_t b_car_stats_init(const uint32_t* a, int n)
{
    static int table;
    host();
    if (!table) {
        char x[1024];
        data_path(x, sizeof x, "TM2.EXE");
        if (tm2_car_stats_load(x) != 0) rt_stop("car stats (set TM2_DATA to the game's files)", 0);
        table = 1;
    }
    int multi = (int)rt_call(0x004632b0u, NULL, 0);
    car_stats_init(rt_ptr(A(1)), (int16_t)A(0), (uint16_t)A(2), multi, cb_rotate, NULL);
    qolc_stats_done(A(1), (uint16_t)A(2));   /* [gameplay] play_dark_tooth (qol_cars.h) */
    return 0;
}

void rt_bind_car(void)
{
    rt_bind(0x00418d38u, b_car_slide_check, "car_slide_check");
    rt_bind(0x0041858cu, b_car_move, "car_move");
    rt_bind(0x004220b8u, b_car_probe_latch, "car_probe_latch");
    rt_bind(0x0043a944u, b_car_stats_derive, "car_stats_derive");
    rt_bind(0x004300e8u, b_car_probes_clear, "car_probes_clear");
    rt_bind(0x00438320u, b_car_collision_box, "car_collision_box");
    rt_bind(0x0043846cu, b_car_stats_init, "car_stats_init");
    rt_bind(0x0042f06cu, b_car_wheel_bounce, "car_wheel_bounce");
    rt_bind(0x0042ca04u, b_car_zone_cross, "car_zone_cross");
    rt_bind(0x0042c4d4u, b_car_wheels_place, "car_wheels_place");
}

RT_BIND_AREA(car)
