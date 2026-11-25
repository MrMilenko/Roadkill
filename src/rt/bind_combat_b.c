/*
 * bind_combat_b.c: runtime bindings of the combat ports cb_level_weapons, cb_pickup, cb_pickups,
 * cb_round, cb_round_start, cb_strike, cb_target, cb_track, cb_ua, cb_weapon_block, cb_world.
 *
 * Each adapter calls the port the way its harness (tools/emu/diff_combat_*.py) did: the WMem is
 * one region over the whole address space, every callback is rt_call of the original address with
 * the registers the harness hook read (eax, edx, ebx, ecx, then stack words). A callback that gets
 * a host pointer to a port local (a vector) passes it through a frame on the runtime's stack and
 * copies it back after the call.
 */
#include "rt_env.h"
#include "weapons/wmem.h"
#include "combat/cb_level_weapons.h"
#include "combat/cb_pickup.h"
#include "combat/cb_pickups.h"
#include "combat/cb_round.h"
#include "combat/cb_round_start.h"
#include "combat/cb_strike.h"
#include "combat/cb_target.h"
#include "combat/cb_track.h"
#include "combat/cb_ua.h"
#include "combat/cb_weapon_block.h"
#include "combat/cb_world.h"

/* ---- helpers ---- */

static WMem g_mem;
static const WMem* mem(void)
{
    g_mem.n = 1;
    g_mem.r[0].addr = 0;
    g_mem.r[0].size = RT_SIZE;
    g_mem.r[0].host = rt_base;
    return &g_mem;
}

static uint32_t c0(uint32_t f) { return rt_call(f, NULL, 0); }
static uint32_t c1(uint32_t f, uint32_t a) { uint32_t x[1] = {a}; return rt_call(f, x, 1); }
static uint32_t c2(uint32_t f, uint32_t a, uint32_t b) { uint32_t x[2] = {a, b}; return rt_call(f, x, 2); }
static uint32_t c3(uint32_t f, uint32_t a, uint32_t b, uint32_t c) { uint32_t x[3] = {a, b, c}; return rt_call(f, x, 3); }
static uint32_t c4(uint32_t f, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t x[4] = {a, b, c, d};
    return rt_call(f, x, 4);
}

/* a host buffer handed to a callee by game address: in place when it already lies in game
 * memory, else copied into a frame (and back when writable) */
typedef struct { void* p; uint32_t size, addr; int framed, ro; } Hb;
static int in_rt(const void* p)
{
    const uint8_t* q = (const uint8_t*)p;
    return q >= rt_base && q < rt_base + RT_SIZE;
}
static uint32_t hb_in(Hb* h, const void* p, uint32_t size, int ro)
{
    h->p = (void*)(uintptr_t)p; h->size = size; h->ro = ro;
    if (in_rt(p)) { h->framed = 0; h->addr = rt_addr(p); return h->addr; }
    h->framed = 1;
    h->addr = rt_frame_push(size);
    memcpy(rt_ptr(h->addr), p, size);
    return h->addr;
}
static void hb_out(Hb* h)
{
    if (!h->framed) return;
    if (!h->ro) memcpy(h->p, rt_ptr(h->addr), h->size);
    rt_frame_pop(h->size);
}

/* sound placement as the ports name it: view 0 -> 0x0046f974(pos), else 0x0046f980(pos, view) */
static void cb_locate(void* u, uint32_t pos, int32_t view)
{
    (void)u;
    if (view == 0) c1(0x0046f974u, pos);
    else c2(0x0046f980u, pos, (uint32_t)view);
}
static int32_t cb_split(void* u) { (void)u; return (int32_t)c0(0x0046cc54u); }
static int32_t cb_pan(void* u) { (void)u; return (int32_t)c0(0x0046f990u); }
static int32_t cb_volume(void* u) { (void)u; return (int32_t)c0(0x0046f988u); }
static void cb_play(void* u, int32_t id, int32_t vol, int32_t pan, int32_t prio)
{
    (void)u;
    c4(0x0046f8b0u, (uint32_t)id, (uint32_t)vol, (uint32_t)pan, (uint32_t)prio);
}
static int32_t cb_event_type(void* u) { (void)u; return (int32_t)c0(0x00474508u); }
static int32_t cb_timer(void* u) { (void)u; return (int32_t)c0(0x00493038u); }
static void cb_srand(void* u, uint32_t s) { (void)u; c1(0x004899e6u, s); }
static int32_t cb_rand(void* u) { (void)u; return (int32_t)c0(0x004899c2u); }
static uint32_t cb_human_get(void* u, int32_t i) { (void)u; return c1(0x0045afbcu, (uint32_t)i); }
static int32_t cb_human_count(void* u) { (void)u; return (int32_t)c0(0x0045afb4u); }
static void cb_hud_msg(void* u, int32_t a, int32_t b) { (void)u; c2(0x00436040u, (uint32_t)a, (uint32_t)b); }
static void cb_refuse(void* u, int32_t a) { (void)u; c1(0x0046fca0u, (uint32_t)a); }
static void cb_set_step(void* u, int32_t id, int32_t a, int32_t b)
{
    (void)u;
    c3(0x00470320u, (uint32_t)id, (uint32_t)a, (uint32_t)b);
}
static void cb_turbo(void* u, uint32_t car, int32_t on) { (void)u; c2(0x0043ac5cu, car, (uint32_t)on); }

/* ---- 0x0044f130 level_init_weapons ---- */

static uint32_t lw_base(void* u) { (void)u; return c0(0x0049ee98u); }
static void lw_4a1314(void* u, uint32_t v) { (void)u; c1(0x004a1314u, v); }
static void lw_reloc(void* u, uint32_t t, uint32_t f, int32_t a) { (void)u; c3(0x0049eee0u, t, f, (uint32_t)a); }
static void lw_walk(void* u, uint32_t t, uint32_t v, uint32_t h) { (void)u; c3(0x004a15d4u, t, v, h); }

static uint32_t b_level_init_weapons(const uint32_t* a, int n)
{
    (void)a; (void)n;
    CbLevelWeaponsEnv e = {mem(), lw_base, lw_4a1314, lw_reloc, lw_walk, NULL};
    cb_level_init_weapons(&e);
    return 0;
}

/* ---- 0x004499ac pickupCollect(car, is_human, id, slot) ---- */

static void pk_hud(void* u, int32_t a, int32_t b, int32_t c) { (void)u; c3(0x004735fcu, (uint32_t)a, (uint32_t)b, (uint32_t)c); }
static void pk_vis(void* u, int32_t a, int32_t b, int32_t c) { (void)u; c3(0x004707b0u, (uint32_t)a, (uint32_t)b, (uint32_t)c); }

static uint32_t b_pickup_collect(const uint32_t* a, int n)
{
    CbPickupEnv e = {mem(), cb_human_get, pk_hud, cb_hud_msg, cb_split, cb_locate, cb_pan, cb_volume,
                     cb_play, pk_vis, cb_refuse, NULL};
    cb_pickup_collect(&e, rt_arg(a, n, 0), (int)rt_arg(a, n, 1), rt_arg(a, n, 2), (int32_t)rt_arg(a, n, 3));
    return 0;
}

/* ---- 0x0044edb0 pickupCountersReset, 0x0044e060 subsys_game_data_init ---- */

static void pks_reg(void* u, uint32_t t, int32_t k, uint32_t h) { (void)u; c3(0x0049de80u, t, (uint32_t)k, h); }

static uint32_t b_pickup_counters_reset(const uint32_t* a, int n)
{
    (void)a; (void)n;
    CbPickupsEnv e = {mem(), cb_human_get, cb_turbo, pks_reg, NULL};
    cb_pickup_counters_reset(&e);
    return 0;
}

static uint32_t b_game_data_init(const uint32_t* a, int n)
{
    (void)a; (void)n;
    CbPickupsEnv e = {mem(), cb_human_get, cb_turbo, pks_reg, NULL};
    cb_game_data_init(&e);
    return 0;
}

/* ---- 0x00447b70 weaponsRoundReset ---- */

static int32_t rr_diff(void* u) { (void)u; return (int32_t)c0(0x0045b3dcu); }
static void rr_rot_a(void* u, int32_t v) { (void)u; c1(0x00453f10u, (uint32_t)v); }
static void rr_rot_b(void* u, int32_t v) { (void)u; c1(0x00453ed0u, (uint32_t)v); }

static uint32_t b_weapons_round_reset(const uint32_t* a, int n)
{
    (void)a; (void)n;
    CbRoundEnv e = {mem(), rr_diff, cb_event_type, rr_rot_a, rr_rot_b, NULL};
    cb_weapons_round_reset(&e);
    return 0;
}

/* ---- 0x00457c80 roundStart ---- */

static const uint32_t rs_calls[CB_RS_COUNT] = {
    0x00470900u, 0x00489a70u, 0x004325f8u, 0x004327fcu, 0x0046500cu, 0x004632b0u,
    0x00447b70u, 0x0045c56cu, 0x0045c350u,
};
static void rs_call(void* u, int which)
{
    (void)u;
    if (which >= 0 && which < CB_RS_COUNT) c0(rs_calls[which]);
}
static void rs_ksetup(void* u, int32_t a, int32_t b) { (void)u; c2(0x00463514u, (uint32_t)a, (uint32_t)b); }
static int32_t rs_kpoll(void* u, int32_t a) { (void)u; return (int32_t)c1(0x004632f4u, (uint32_t)a); }
static int32_t rs_kcheck(void* u) { (void)u; return (int32_t)c0(0x004632d4u); }
static void rs_cstart(void* u, uint32_t car, int32_t t, int32_t h) { (void)u; c3(0x00437850u, car, (uint32_t)t, (uint32_t)h); }
static void rs_cview(void* u, uint32_t car, int32_t v) { (void)u; c2(0x004225a0u, car, (uint32_t)v); }

static uint32_t b_round_start(const uint32_t* a, int n)
{
    (void)a; (void)n;
    CbRoundStartEnv e = {mem(), cb_timer, cb_srand, cb_rand, cb_event_type, rs_call, rs_ksetup, rs_kpoll,
                         rs_kcheck, rs_cstart, cb_split, rs_cview, NULL};
    cb_round_start(&e);
    return 0;
}

/* ---- 0x0044d1f4 weaponShadowStrikeUpdate ---- */

static int32_t st_ticks(void* u) { (void)u; return (int32_t)c0(0x004931d4u); }
static int32_t st_ground(void* u, const int32_t v[3])
{
    (void)u;
    Hb h;
    uint32_t p = hb_in(&h, v, 12, 1);
    int32_t r = (int32_t)c1(0x0042c2d4u, p);
    hb_out(&h);
    return r;
}
static int32_t st_hit(void* u, uint32_t pos, int32_t o, int32_t r, int32_t k)
{
    (void)u;
    return (int32_t)c4(0x004126b4u, pos, (uint32_t)o, (uint32_t)r, (uint32_t)k);
}
static void st_x16(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x00450984u, pos, (uint32_t)f); }
static void st_x24(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x00450a10u, pos, (uint32_t)f); }
static void st_whit(void* u, int32_t hit, int32_t a, int32_t b, int32_t c, uint32_t vel, int32_t owner)
{
    (void)u;
    uint32_t x[6] = {(uint32_t)hit, (uint32_t)a, (uint32_t)b, (uint32_t)c, vel, (uint32_t)owner};
    rt_call(0x00415d1cu, x, 6);
}

static uint32_t b_strike_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    CbStrikeEnv e = {mem(), st_ticks, st_ground, st_hit, cb_set_step, st_x16, st_x24, st_whit, NULL};
    cb_strike_update(&e);
    return 0;
}

/* ---- 0x00448de4 weaponTargetFront / 0x00449048 weaponTargetRear(i, &out, is_human) ---- */

static int32_t tg_single(void* u) { (void)u; return (int32_t)c0(0x0045c348u); }
static uint32_t tg_hobj(void* u, int32_t i) { (void)u; return c1(0x0045ae24u, (uint32_t)i); }
static uint32_t tg_aobj(void* u, int32_t i) { (void)u; return c1(0x0045ae2cu, (uint32_t)i); }
static int32_t tg_acount(void* u) { (void)u; return (int32_t)c0(0x0045b078u); }
static uint32_t tg_aget(void* u, int32_t i) { (void)u; return c1(0x0045b080u, (uint32_t)i); }

static uint32_t target_pick(const uint32_t* a, int n, int rear)
{
    CbTargetEnv e = {mem(), tg_single, tg_hobj, tg_aobj, tg_acount, tg_aget, cb_human_count, cb_human_get, NULL};
    uint32_t outa = rt_arg(a, n, 1);
    uint32_t out = rt_r32(outa);   /* the port may leave it alone: keep the caller's value */
    if (rear) cb_target_pick_rear(&e, (int32_t)rt_arg(a, n, 0), &out, (int)rt_arg(a, n, 2));
    else cb_target_pick_front(&e, (int32_t)rt_arg(a, n, 0), &out, (int)rt_arg(a, n, 2));
    rt_w32(outa, out);
    return 0;
}
static uint32_t b_target_front(const uint32_t* a, int n) { return target_pick(a, n, 0); }
static uint32_t b_target_rear(const uint32_t* a, int n) { return target_pick(a, n, 1); }

/* ---- 0x00456fb0 trackVehicleDestroy(i) ---- */

static int32_t tr_index(void* u, int32_t i) { (void)u; return (int32_t)c1(0x0043b4b0u, (uint32_t)i); }
static void tr_hit(void* u, uint32_t car, int32_t a, int32_t b, int32_t c)
{
    (void)u;
    c4(0x00421920u, car, (uint32_t)a, (uint32_t)b, (uint32_t)c);
}
static void tr_set(void* u, int32_t i, const int32_t* pos, int32_t a, int32_t h)
{
    (void)u;
    Hb hb;
    uint32_t p = hb_in(&hb, pos, 12, 1);
    c4(0x0043b440u, (uint32_t)i, p, (uint32_t)a, (uint32_t)h);
    hb_out(&hb);
}

static uint32_t b_track_destroy(const uint32_t* a, int n)
{
    CbTrackEnv e = {mem(), tr_index, cb_human_count, cb_human_get, tr_hit, tr_set, NULL};
    cb_track_vehicle_destroy(&e, (int32_t)rt_arg(a, n, 0));
    return 0;
}

/* ---- 0x00457464 uaInit ---- */

static void ua_clear(void* u) { (void)u; c0(0x004575c0u); }
static void ua_473160(void* u) { (void)u; c0(0x00473160u); }
static void ua_fxreset(void* u) { (void)u; c0(0x00451458u); }

static uint32_t b_ua_init(const uint32_t* a, int n)
{
    (void)a; (void)n;
    CbUaEnv e = {mem(), ua_clear, ua_473160, ua_fxreset, cb_timer, cb_srand, NULL};
    cb_ua_init(&e);
    return 0;
}

/* ---- 0x0043807c weaponBlockInit(car, block, ai_layout), 0x0043ac5c carTurboSet(car, on) ---- */

static int32_t wb_slam(void* u, uint32_t b) { (void)u; return (int32_t)c1(0x0044c81cu, b); }
static void wb_derive(void* u, uint8_t* stats) { (void)u; c1(0x0043a944u, rt_addr(stats)); }

static uint32_t b_weapon_block_init(const uint32_t* a, int n)
{
    CbWeaponBlockEnv e = {mem(), wb_slam, NULL};
    return cb_weapon_block_init(&e, rt_ptr(rt_arg(a, n, 0)), rt_arg(a, n, 1), (int)rt_arg(a, n, 2));
}

static uint32_t b_car_turbo_set(const uint32_t* a, int n)
{
    cb_car_turbo_set(rt_ptr(rt_arg(a, n, 0)), (int)rt_arg(a, n, 1), wb_derive, NULL);
    return 0;
}

/* ---- 0x004543f8 worldWarpStart(i, car, ai), 0x0045723c fxObject4BPlace(pos), 0x004575c0 carTablesClear ---- */

static void wd_slam(void* u) { (void)u; c0(0x0044c7e4u); }
static void wd_coll(void* u) { (void)u; c0(0x00470b20u); }
static void wd_collapse(void* u) { (void)u; c0(0x00454ae0u); }
static void wd_track(void* u) { (void)u; c0(0x00455e94u); }

static CbWorldEnv world_env(void)
{
    CbWorldEnv e = {mem(), cb_split, cb_locate, cb_volume, cb_pan, cb_play, cb_set_step, wd_slam, wd_coll,
                    cb_event_type, wd_collapse, wd_track, NULL};
    return e;
}

static uint32_t b_warp_start(const uint32_t* a, int n)
{
    CbWorldEnv e = world_env();
    cb_warp_start(&e, (int32_t)rt_arg(a, n, 0), rt_arg(a, n, 1), (int)rt_arg(a, n, 2));
    return 0;
}

static uint32_t b_obj4b_place(const uint32_t* a, int n)
{
    CbWorldEnv e = world_env();
    cb_obj4b_place(&e, rt_arg(a, n, 0));
    return 0;
}

static uint32_t b_car_tables_clear(const uint32_t* a, int n)
{
    (void)a; (void)n;
    CbWorldEnv e = world_env();
    cb_car_tables_clear(&e);
    return 0;
}

void rt_bind_combat_b(void)
{
    rt_bind(0x0044f130u, b_level_init_weapons, "level_init_weapons");
    rt_bind(0x004499acu, b_pickup_collect, "pickupCollect");
    rt_bind(0x0044edb0u, b_pickup_counters_reset, "pickupCountersReset");
    rt_bind(0x0044e060u, b_game_data_init, "subsys_game_data_init");
    rt_bind(0x00447b70u, b_weapons_round_reset, "weaponsRoundReset");
    rt_bind(0x00457c80u, b_round_start, "roundStart");
    rt_bind(0x0044d1f4u, b_strike_update, "weaponShadowStrikeUpdate");
    rt_bind(0x00448de4u, b_target_front, "weaponTargetFront");
    rt_bind(0x00449048u, b_target_rear, "weaponTargetRear");
    rt_bind(0x00456fb0u, b_track_destroy, "trackVehicleDestroy");
    rt_bind(0x00457464u, b_ua_init, "uaInit");
    rt_bind(0x0043807cu, b_weapon_block_init, "weaponBlockInit");
    rt_bind(0x0043ac5cu, b_car_turbo_set, "carTurboSet");
    rt_bind(0x004543f8u, b_warp_start, "worldWarpStart");
    rt_bind(0x0045723cu, b_obj4b_place, "fxObject4BPlace");
    rt_bind(0x004575c0u, b_car_tables_clear, "carTablesClear");
}

RT_BIND_AREA(combat_b)
