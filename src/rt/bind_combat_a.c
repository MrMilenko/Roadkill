/*
 * bind_combat_a.c: combat ports bound the way their harnesses call them (tools/emu/diff_combat_
 * area / assets / actions / parts / dropplace / drops / flame / guns / invis / leap .py):
 * cb_area_damage, cb_assets, cb_car_actions, cb_car_parts, cb_drop_place, cb_drops, cb_flame,
 * cb_guns, cb_invis, cb_leap. Every env gets one WMem region over the whole address space; every
 * callback calls the original address with its arguments in register order (eax, edx, ebx, ecx,
 * then the stack words).
 *
 * Runtime equivalent of an emulator-only interface: several callbacks take a host pointer to a
 * port's local vector or out parameter (a position {x, y, z + 200}, a rotation matrix, car_find's
 * &human / &index); the original passes the address of a stack local. buf_* below copy the local
 * into a game frame for the call (and back for out parameters); a pointer that already lies in
 * game memory is passed as its game address.
 */
#include "rt_env.h"
#include "math/tm2math.h"
#include "combat/cb_area_damage.h"
#include "combat/cb_assets.h"
#include "combat/cb_car_actions.h"
#include "combat/cb_car_parts.h"
#include "combat/cb_drop_place.h"
#include "combat/cb_drops.h"
#include "combat/cb_flame.h"
#include "combat/cb_guns.h"
#include "combat/cb_invis.h"
#include "combat/cb_leap.h"

static WMem m_mem;

static const WMem* mem(void)
{
    m_mem.n = 1;
    m_mem.r[0].addr = 0;
    m_mem.r[0].size = RT_SIZE;
    m_mem.r[0].host = rt_base;
    return &m_mem;
}

/* the math ports (tm2_angle_step / interp ...) read the frame's ticks and fps from host statics */
static void sync_frame(void)
{
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
}

#define U(x) ((uint32_t)(x))

static uint32_t c0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static uint32_t c1(uint32_t fn, uint32_t a0) { uint32_t a[1] = {a0}; return rt_call(fn, a, 1); }
static uint32_t c2(uint32_t fn, uint32_t a0, uint32_t a1) { uint32_t a[2] = {a0, a1}; return rt_call(fn, a, 2); }
static uint32_t c3(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2)
{
    uint32_t a[3] = {a0, a1, a2};
    return rt_call(fn, a, 3);
}
static uint32_t c4(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    uint32_t a[4] = {a0, a1, a2, a3};
    return rt_call(fn, a, 4);
}

/* host buffers passed to the original: up to two per call */
typedef struct {
    const void* in;     /* host data */
    void* out;          /* copied back after the call (NULL: input only) */
    uint32_t size;
    uint32_t game;      /* the game address passed */
    int pushed;
} Buf;

static int in_game(const void* p)
{
    return (const uint8_t*)p >= rt_base && (const uint8_t*)p < rt_base + RT_SIZE;
}

static uint32_t buf_in(Buf* b, const void* in, void* out, uint32_t size)
{
    b->in = in;
    b->out = out;
    b->size = size;
    b->pushed = !in_game(in);
    if (b->pushed) {
        b->game = rt_frame_push((size + 15u) & ~15u);
        memcpy(rt_ptr(b->game), in, size);
    } else {
        b->game = rt_addr(in);
    }
    return b->game;
}

static void buf_done(Buf* b)
{
    if (!b->pushed)
        return;
    if (b->out)
        memcpy(b->out, rt_ptr(b->game), b->size);
    rt_frame_pop((b->size + 15u) & ~15u);
}

/* fn(vec, rest...) with vec a host vector of 3 i32 (input) */
static uint32_t call_vec(uint32_t fn, int slot, const int32_t* v, uint32_t* a, int n)
{
    Buf b;
    uint32_t r;
    a[slot] = buf_in(&b, v, NULL, 12);
    r = rt_call(fn, a, n);
    buf_done(&b);
    return r;
}

/* the sound placement both ports use: view 0 -> 0x0046f974(pos), else 0x0046f980(pos, view) */
static void locate_addr(uint32_t pos, int32_t view)
{
    if (view == 0)
        c1(0x0046f974u, pos);
    else
        c2(0x0046f980u, pos, U(view));
}

/* ---- shared simple callbacks ---- */
static int32_t f_ticks60(void* u) { (void)u; return (int32_t)c0(0x004931d4u); }
static int32_t f_split(void* u) { (void)u; return (int32_t)c0(0x0046cc54u); }
static int32_t f_pan(void* u) { (void)u; return (int32_t)c0(0x0046f990u); }
static int32_t f_volume(void* u) { (void)u; return (int32_t)c0(0x0046f988u); }
static int32_t f_event_type(void* u) { (void)u; return (int32_t)c0(0x00474508u); }
static int32_t f_human_count(void* u) { (void)u; return (int32_t)c0(0x0045afb4u); }
static uint32_t f_human_get(void* u, int32_t i) { (void)u; return c1(0x0045afbcu, U(i)); }
static uint32_t f_ai_get(void* u, int32_t i) { (void)u; return c1(0x0045b080u, U(i)); }
static uint32_t f_human_object(void* u, int32_t i) { (void)u; return c1(0x0045ae24u, U(i)); }
static uint32_t f_ai_object(void* u, int32_t i) { (void)u; return c1(0x0045ae2cu, U(i)); }
static void f_locate_addr(void* u, uint32_t pos, int32_t view) { (void)u; locate_addr(pos, view); }
static void f_event1(void* u, uint32_t pos) { (void)u; c1(0x00411334u, pos); }
static void f_event2(void* u, uint32_t pos) { (void)u; c1(0x00410890u, pos); }
static void f_event3(void* u, uint32_t pos) { (void)u; c1(0x00411e28u, pos); }
static void f_area_damage(void* u, uint32_t pos, int32_t s, int32_t big) { (void)u; c3(0x00449f10u, pos, U(s), U(big)); }
static void f_explosion12(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x004508f4u, pos, U(f)); }
static void f_explosion16(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x00450984u, pos, U(f)); }
static void f_explosion48(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x00450aa0u, pos, U(f)); }

/* ---- 0x00449f10 cb_area_damage (eax = position, dx = shooter, bx = big) ---- */
static int32_t a_car_count(void* u) { (void)u; return (int32_t)c0(0x0045afb4u); }
static int32_t a_ai_count(void* u) { (void)u; return (int32_t)c0(0x0045b078u); }
static void a_car_hit(void* u, uint32_t car, int32_t x, int32_t y, int32_t z) { (void)u; c4(0x00421920u, car, U(x), U(y), U(z)); }
static int32_t a_object_id(void* u, int32_t i, int32_t h) { (void)u; return (int32_t)c2(0x0045b154u, U(i), U(h)); }
static void a_damage(void* u, int32_t id, int32_t amount, uint32_t kind, int32_t b, int32_t shooter)
{
    uint32_t a[5] = {U(id), U(amount), kind, U(b), U(shooter)};
    (void)u;
    rt_call(0x004210e4u, a, 5);
}
static void a_explosion16(void* u, const int32_t pos[3], int32_t f)
{
    uint32_t a[2] = {0, U(f)};
    (void)u;
    call_vec(0x00450984u, 0, pos, a, 2);
}
static uint32_t a_destructibles(void* u) { (void)u; return c0(0x00470d08u); }
static void a_destructible_hit(void* u, int32_t x, int32_t y, int32_t z) { (void)u; c3(0x00470d3cu, U(x), U(y), U(z)); }
static uint32_t a_props(void* u) { (void)u; return c0(0x00471aecu); }
static void a_prop_hit(void* u, int32_t i, int32_t* v)
{
    uint32_t a[2] = {U(i), 0};
    Buf b;
    (void)u;
    a[1] = buf_in(&b, v, v, 12);
    rt_call(0x004723bcu, a, 2);
    buf_done(&b);
}

static uint32_t b_area_damage(const uint32_t* a, int n)
{
    CbAreaEnv e = {mem(), a_car_count, f_human_get, a_ai_count, f_ai_get, a_car_hit, a_object_id, a_damage,
                   a_explosion16, a_destructibles, a_destructible_hit, a_props, a_prop_hit, NULL};
    cb_area_damage(&e, rt_arg(a, n, 0), (int16_t)rt_arg(a, n, 1), (int16_t)rt_arg(a, n, 2));
    return 0;
}

/* ---- cb_assets: 0x00447dbc (eax = asset, edx = id), 0x00454aa4 (eax = asset) ---- */
static uint32_t s_alloc(void* u, uint32_t id) { (void)u; return c1(0x00492d90u, id); }
static void s_insert(void* u, uint32_t obj) { (void)u; c1(0x00492c4cu, obj); }

static uint32_t b_asset_bind(const uint32_t* a, int n)
{
    CbAssetsEnv e = {mem(), s_alloc, s_insert, NULL};
    return cb_weapon_asset_bind(&e, rt_arg(a, n, 0), rt_arg(a, n, 1));
}

static uint32_t b_warp_objects(const uint32_t* a, int n)
{
    CbAssetsEnv e = {mem(), s_alloc, s_insert, NULL};
    return cb_warp_objects_create(&e, rt_arg(a, n, 0));
}

/* ---- cb_car_actions (eax = human index, dl = v / slot) ---- */
static void k_fire_special(void* u, uint32_t car, int32_t x) { (void)u; c2(0x004401fcu, car, U(x)); }
static void k_pre_guns(void* u, uint32_t car) { (void)u; c1(0x0046fea4u, car); }
static void k_guns(void* u, uint32_t car, int32_t x) { (void)u; c2(0x00443c9cu, car, U(x)); }
static void k_rumble(void* u, int32_t x, int32_t i) { (void)u; c2(0x0046f860u, U(x), U(i)); }
static void k_fire_342c(void* u, uint32_t car, int32_t x) { (void)u; c2(0x0044342cu, car, U(x)); }

static CbCarActionsEnv actions_env(void)
{
    CbCarActionsEnv e = {mem(), f_human_get, k_fire_special, k_pre_guns, k_guns, k_rumble, k_fire_342c, NULL};
    return e;
}

static uint32_t b_fire_special_slot(const uint32_t* a, int n)
{
    CbCarActionsEnv e = actions_env();
    cb_car_fire_special_slot(&e, (int32_t)rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1));
    return 0;
}

static uint32_t b_fire_guns(const uint32_t* a, int n)
{
    CbCarActionsEnv e = actions_env();
    cb_car_fire_guns(&e, (int32_t)rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1));
    return 0;
}

static uint32_t b_flag_84d(const uint32_t* a, int n)
{
    CbCarActionsEnv e = actions_env();
    cb_car_flag_84d(&e, (int32_t)rt_arg(a, n, 0));
    return 0;
}

static uint32_t b_fire_342c(const uint32_t* a, int n)
{
    CbCarActionsEnv e = actions_env();
    cb_car_fire_0044342c(&e, (int32_t)rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1));
    return 0;
}

/* ---- 0x00453538 cb_car_parts_spin (eax = type, edx = speed) ---- */
static void p_rot_x(void* u, int32_t angle, int32_t* m)
{
    uint32_t a[2] = {U(angle), 0};
    Buf b;
    (void)u;
    a[1] = buf_in(&b, m, m, 48);
    rt_call(0x0048c50cu, a, 2);
    buf_done(&b);
}
static void p_step(void* u, int32_t i, int32_t s) { (void)u; c2(0x0047070cu, U(i), U(s)); }

static uint32_t b_parts_spin(const uint32_t* a, int n)
{
    CbCarPartsEnv e = {mem(), p_rot_x, p_step, NULL};
    cb_car_parts_spin(&e, (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
    return 0;
}

/* ---- 0x00448118 cb_drop_place (eax = car, dl = kind, ebx = is human) ---- */
static int32_t dp_ground(void* u, const int32_t v[3])
{
    uint32_t a[1] = {0};
    (void)u;
    return (int32_t)call_vec(0x0042c2d4u, 0, v, a, 1);
}
static void dp_locate(void* u, const int32_t v[3], int32_t view)
{
    Buf b;
    uint32_t g = buf_in(&b, v, NULL, 12);
    (void)u;
    locate_addr(g, view);
    buf_done(&b);
}

static uint32_t b_drop_place(const uint32_t* a, int n)
{
    CbDropPlaceEnv e = {mem(), f_human_object, f_ai_object, dp_ground, f_split, dp_locate, f_event_type,
                        f_event2, f_event1, f_event3, f_area_damage, f_explosion48, NULL};
    cb_drop_place(&e, rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1), (int)rt_arg(a, n, 2));
    return 0;
}

/* ---- 0x00448b1c cb_weapon_drops_update ---- */
static int32_t dr_angle_step(void* u, int32_t c, int32_t t, int32_t r) { (void)u; return (int32_t)c3(0x0048caecu, U(c), U(t), U(r)); }
static int32_t dr_hit_test(void* u, uint32_t pos, int32_t owner, int32_t r, int32_t kind)
{
    (void)u;
    return (int32_t)c4(0x004126b4u, pos, U(owner), U(r), U(kind));
}
static void dr_weapon_hit(void* u, int32_t hit, int32_t x, int32_t y, int32_t z, uint32_t kind, int32_t owner)
{
    uint32_t a[6] = {U(hit), U(x), U(y), U(z), kind, U(owner)};
    (void)u;
    rt_call(0x00415d1cu, a, 6);
}

static uint32_t b_drops_update(const uint32_t* a, int n)
{
    CbDropsEnv e = {mem(), dr_angle_step, f_ticks60, dr_hit_test, f_explosion12, f_explosion16, dr_weapon_hit,
                    f_event_type, f_event2, f_event1, f_event3, f_area_damage, f_human_get, NULL};
    (void)a;
    (void)n;
    cb_weapon_drops_update(&e);
    return 0;
}

/* ---- 0x00449424 cb_flame_update ---- */
static void fl_bit(void* u, const int32_t p[3])
{
    uint32_t a[1] = {0};
    (void)u;
    call_vec(0x00450f44u, 0, p, a, 1);
}
static int32_t fl_rand(void* u) { (void)u; return (int32_t)c0(0x004899c2u); }
static void fl_bullet_smoke(void* u, const int32_t p[3], int32_t f)
{
    uint32_t a[2] = {0, U(f)};
    (void)u;
    call_vec(0x00450fd4u, 0, p, a, 2);
}
static int32_t fl_hit_test(void* u, const int32_t p[3], int32_t owner, int32_t r, int32_t kind)
{
    uint32_t a[4] = {0, U(owner), U(r), U(kind)};
    (void)u;
    return (int32_t)call_vec(0x004126b4u, 0, p, a, 4);
}
static void fl_weapon_hit(void* u, int32_t hit, int32_t x, int32_t y, int32_t z, const int32_t d[3], int32_t owner)
{
    uint32_t a[6] = {U(hit), U(x), U(y), U(z), 0, U(owner)};
    (void)u;
    call_vec(0x00415d1cu, 4, d, a, 6);
}
static int32_t fl_car_find(void* u, int32_t id, uint8_t* human, int32_t* index)
{
    uint32_t a[3] = {U(id), 0, 0};
    Buf bh, bi;
    uint32_t r;
    (void)u;
    a[1] = buf_in(&bh, human, human, 1);
    a[2] = buf_in(&bi, index, index, 4);
    r = rt_call(0x0045b178u, a, 3);
    buf_done(&bi);
    buf_done(&bh);
    return (int32_t)r;
}
static uint32_t fl_flame5(void* u, uint32_t pos, int32_t f) { (void)u; return c2(0x00450de4u, pos, U(f)); }
static void fl_smoke_a2(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x00451014u, pos, U(f)); }
static uint32_t fl_target(void* u, int32_t i) { (void)u; return c1(0x00411e14u, U(i)); }
static void fl_flame_pair(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x00450e64u, pos, U(f)); }
static void fl_notify(void* u, int32_t id, int32_t i, int32_t v) { (void)u; c3(0x00499984u, U(id), U(i), U(v)); }

static uint32_t b_flame_update(const uint32_t* a, int n)
{
    CbFlameEnv e = {mem(), f_ticks60, fl_bit, fl_rand, fl_bullet_smoke, fl_hit_test, fl_weapon_hit, fl_car_find,
                    f_human_get, f_ai_get, fl_flame5, fl_smoke_a2, f_explosion12, fl_target, fl_flame_pair,
                    f_explosion16, f_event_type, fl_notify, NULL};
    (void)a;
    (void)n;
    sync_frame();
    cb_flame_update(&e);
    return 0;
}

/* ---- 0x00443c9c cb_update_guns (eax = car, edx = is human) ---- */
static void g_play(uint32_t fn, int32_t id, int32_t vol, int32_t pan, int32_t prio, int32_t who)
{
    uint32_t a[5] = {U(id), U(vol), U(pan), U(prio), U(who)};
    rt_call(fn, a, 5);
}
static void g_play_human(void* u, int32_t id, int32_t v, int32_t p, int32_t q, int32_t w) { (void)u; g_play(0x0046f840u, id, v, p, q, w); }
static void g_play_ai(void* u, int32_t id, int32_t v, int32_t p, int32_t q, int32_t w) { (void)u; g_play(0x0046f878u, id, v, p, q, w); }
static void g_rumble(void* u, int32_t x, int32_t y) { (void)u; c2(0x00473568u, U(x), U(y)); }
static void g_bullet(void* u, int32_t owner, int32_t kind, const int32_t dir[3], const int32_t pos[3], int32_t cnt)
{
    uint32_t a[5] = {U(owner), U(kind), 0, 0, U(cnt)};
    Buf bd, bp;
    (void)u;
    a[2] = buf_in(&bd, dir, NULL, 12);
    a[3] = buf_in(&bp, pos, NULL, 12);
    rt_call(0x00412dc4u, a, 5);
    buf_done(&bp);
    buf_done(&bd);
}
static void g_muzzle(void* u, const int32_t pos[3], int32_t f)
{
    uint32_t a[2] = {0, U(f)};
    (void)u;
    call_vec(0x00451430u, 0, pos, a, 2);
}

static uint32_t b_update_guns(const uint32_t* a, int n)
{
    CbGunsEnv e = {mem(), f_human_get, f_ai_get, f_split, f_locate_addr, f_pan, f_volume, g_play_human, g_play_ai,
                   g_rumble, g_bullet, g_muzzle, NULL};
    sync_frame();
    cb_update_guns(&e, rt_arg(a, n, 0), (int)rt_arg(a, n, 1));
    return 0;
}

/* ---- 0x0044bbe4 cb_invisibility_update ---- */
static void i_hide(void* u, int32_t t) { (void)u; c1(0x00453c38u, U(t)); }
static void i_show(void* u, int32_t t) { (void)u; c1(0x00453c00u, U(t)); }
static uint32_t i_jitter(void* u, uint32_t pos, int32_t x, int32_t y, int32_t z) { (void)u; return c4(0x0044a40cu, pos, U(x), U(y), U(z)); }
static void i_play(void* u, int32_t id, int32_t v, int32_t p, int32_t q) { (void)u; c4(0x0046f8b0u, U(id), U(v), U(p), U(q)); }
static void i_spark(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x0045075cu, pos, U(f)); }

static uint32_t b_invis_update(const uint32_t* a, int n)
{
    CbInvisEnv e = {mem(), f_human_count, f_human_object, f_human_get, i_hide, i_show, f_ticks60, i_jitter, f_split,
                    f_locate_addr, f_pan, f_volume, i_play, i_spark, NULL};
    (void)a;
    (void)n;
    cb_invisibility_update(&e);
    return 0;
}

/* ---- 0x0044cee0 cb_homing_leap_update ---- */
static void l_set_step(void* u, int32_t id, int32_t x, int32_t y) { (void)u; c3(0x00470320u, U(id), U(x), U(y)); }
static void l_smoke_small(void* u, uint32_t pos, int32_t f) { (void)u; c2(0x00451058u, pos, U(f)); }
static void l_damage(void* u, int32_t id, int32_t amount, const int32_t v[3], int32_t b, int32_t shooter)
{
    uint32_t a[5] = {U(id), U(amount), 0, U(b), U(shooter)};
    (void)u;
    call_vec(0x004210e4u, 2, v, a, 5);
}

static uint32_t b_leap_update(const uint32_t* a, int n)
{
    CbLeapEnv e = {mem(), f_ticks60, l_set_step, f_explosion12, l_smoke_small, l_damage, NULL};
    (void)a;
    (void)n;
    sync_frame();
    cb_homing_leap_update(&e);
    return 0;
}

void rt_bind_combat_a(void)
{
    rt_bind(0x00449f10u, b_area_damage, "areaDamage");
    rt_bind(0x00447dbcu, b_asset_bind, "weaponAssetBind");
    rt_bind(0x00454aa4u, b_warp_objects, "worldWarpObjectsCreate");
    rt_bind(0x0044df18u, b_fire_special_slot, "carFireSpecialSlot");
    rt_bind(0x0044df78u, b_fire_guns, "carFireGuns");
    rt_bind(0x0044dfe8u, b_flag_84d, "carFlag84d");
    rt_bind(0x0044dff8u, b_fire_342c, "carFire0044342c");
    rt_bind(0x00453538u, b_parts_spin, "carPartsSpin");
    rt_bind(0x00448118u, b_drop_place, "weaponDropPlace");
    rt_bind(0x00448b1cu, b_drops_update, "weaponDropCheckHit");
    rt_bind(0x00449424u, b_flame_update, "weaponFlameUpdate");
    rt_bind(0x00443c9cu, b_update_guns, "UpdateGuns");
    rt_bind(0x0044bbe4u, b_invis_update, "weaponInvisibilityUpdate");
    rt_bind(0x0044cee0u, b_leap_update, "weaponHomingLeapUpdate");
}

RT_BIND_AREA(combat_a)
