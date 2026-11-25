/*
 * bind_helpers.c: the helpers that are their own functions in TM2.EXE but were ported only as
 * statics or inline code of a bigger port, now exported (tools/emu/check_helpers.sh verifies each
 * against its own original). Compact convention (docs/RUNTIME.md "Calls").
 *
 * Results: where the original leaves EAX as it came (the plain writers) the adapter returns it;
 * 0x00418b60 leaves the body address in EAX.
 *
 * Not bound here, on purpose: 0x0049a1b4 car_collide_walk and 0x0049a080 car_collide_push. Their
 * port keeps the walk state in a host struct (CarCollideState) and they are only reached from
 * inside car_collide 0x0049cec0 (bound, the port calls them directly) and from dead code
 * (unlisted 0x0049d7bc). Binding them alone needs that state mapped onto 0x00a3c890..0x00a3cfdc.
 * Port code, not a decompiled function.
 */
#include "rt_env.h"

#include "game/car_input.h"
#include "game/car_spawn.h"
#include "game/car_stats.h"
#include "game/car_update.h"
#include "scene/dpc_collide.h"

#define A(i) rt_arg(a, n, (i))
#define P(i) rt_ptr(A(i))

static uint8_t event_type(void) { return (uint8_t)rt_call(0x00474508u, NULL, 0); }

/* 0x0045c248(car, dl player) */
static uint32_t b_oob_low(const uint32_t* a, int n) { return (uint32_t)car_oob_low(P(0), event_type(), (uint8_t)A(1) != 0); }
/* 0x0045c2a0() */
static uint32_t b_oob_back(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)car_oob_back(event_type()); }
/* 0x0044f100(combo block) */
static uint32_t b_combo_reset(const uint32_t* a, int n) { car_combo_reset(P(0)); return A(0); }

/* the spectator globals as CarInputEnv; its view mode event is 0x004225a0(car or player car 0, mode) */
static void cb_event(void* u, int id, uint8_t* car, int32_t x, int32_t y, int32_t z)
{
    (void)u; (void)y; (void)z;
    if (id != CAR_IN_SET_STATE) rt_stop("bind_helpers: unexpected CarInputEnv event", (uint32_t)id);
    uint32_t v[2] = {car ? rt_addr(car) : 0x005030a8u, (uint32_t)x};
    rt_call(0x004225a0u, v, 2);
}
static void watch_in(CarInputEnv* e)
{
    memset(e, 0, sizeof *e);
    e->event = cb_event;
    e->watch_player = (int32_t)rt_r32(0x00502f8cu);
    e->watch_ai_flag = (int32_t)rt_r32(0x00502f88u);
    e->watch_ai = (int32_t)rt_r32(0x00502f90u);
    e->ai_count = (int32_t)rt_r32(0x0050c2b0u);
    e->player_count = (int32_t)rt_r32(0x005075e0u);
}
static void watch_out(const CarInputEnv* e)
{
    rt_w32(0x00502f8cu, (uint32_t)e->watch_player);
    rt_w32(0x00502f88u, (uint32_t)e->watch_ai_flag);
    rt_w32(0x00502f90u, (uint32_t)e->watch_ai);
}
/* 0x0045b5a8(v) / 0x0045b564(v) */
static uint32_t b_watch_player(const uint32_t* a, int n) { CarInputEnv e; watch_in(&e); car_watch_player(&e, (int32_t)A(0)); watch_out(&e); return 0; }
static uint32_t b_watch_ai(const uint32_t* a, int n) { CarInputEnv e; watch_in(&e); car_watch_ai(&e, (int32_t)A(0)); watch_out(&e); return 0; }

/* 0x00437ff8(controls) */
static uint32_t b_controls_clear(const uint32_t* a, int n) { car_controls_clear(P(0)); return A(0); }
/* 0x0043a754(probes, dx car id) */
static uint32_t b_step_height(const uint32_t* a, int n) { car_probe_step_height(P(0), (uint16_t)A(1)); return A(0); }
/* 0x00430368(probes, z) */
static uint32_t b_probes_floor(const uint32_t* a, int n) { car_probes_floor(P(0), (int32_t)A(1)); return A(0); }
/* 0x0043033c(probes, zone): the GRP file at 0x004fca18 */
static uint32_t b_probes_zone(const uint32_t* a, int n) { car_probes_zone(P(0), (int32_t)A(1), rt_ptr(0x004fca18u)); return A(0); }
/* 0x00418b60(car, dl player): EAX = the body */
static uint32_t b_motion_reset(const uint32_t* a, int n)
{
    int pl = (uint8_t)A(1) != 0;
    car_motion_reset(P(0), pl);
    return A(0) + (pl ? 0x6c4u : 0x708u);
}
/* 0x00439fa8(stats) */
static uint32_t b_stats_generic(const uint32_t* a, int n) { car_stats_generic(P(0)); return A(0); }
/* 0x0048d010(centre, radius, point), the slack 0x009f7888 */
static uint32_t b_group_sphere(const uint32_t* a, int n)
{
    return (uint32_t)dpc_group_sphere((const int32_t*)(const void*)P(0), (int32_t)A(1), (const int32_t*)(const void*)P(2),
                                      (int32_t)rt_r32(0x009f7888u));
}
/* 0x0048dc3c(volume node, point, result, ecx flag: the whole register is tested) */
static uint32_t b_volume_test(const uint32_t* a, int n)
{
    return (uint32_t)dpc_volume_test(P(0), (const int32_t*)(const void*)P(1), (DpcCollision*)(void*)P(2), A(3) != 0);
}

void rt_bind_helpers(void)
{
    rt_bind(0x0045c248u, b_oob_low, "car_oob_low");
    rt_bind(0x0045c2a0u, b_oob_back, "car_oob_back");
    rt_bind(0x0044f100u, b_combo_reset, "car_combo_reset");
    rt_bind(0x0045b5a8u, b_watch_player, "spectate_player");
    rt_bind(0x0045b564u, b_watch_ai, "spectate_ai");
    rt_bind(0x00437ff8u, b_controls_clear, "car_controls_clear");
    rt_bind(0x0043a754u, b_step_height, "car_probe_step_height");
    rt_bind(0x00430368u, b_probes_floor, "car_probes_floor");
    rt_bind(0x0043033cu, b_probes_zone, "car_probes_zone");
    rt_bind(0x00418b60u, b_motion_reset, "car_motion_reset");
    rt_bind(0x00439fa8u, b_stats_generic, "car_stats_generic");
    rt_bind(0x0048d010u, b_group_sphere, "dpc_group_sphere");
    rt_bind(0x0048dc3cu, b_volume_test, "dpc_volume_test");
}

RT_BIND_AREA(helpers)
