/*
 * bind_weapons.c: the weapon and effect ports (src/weapons) on the runtime.
 *
 * Every adapter calls its port the way tools/emu/diff_weapons_*.py did: registers EAX, EDX, EBX,
 * ECX then the stack words, one WMem region over the whole address space, the context struct's
 * globals as rt_ptr(<original address>), scalar copies of globals (split screen mode, the trail
 * frame table pointer) read fresh on every call, and every callback an rt_call of the address the
 * harness stubbed, with the registers that stub read.
 *
 * Real equivalents of emulator only interfaces:
 *   - A callback that gets a host pointer from the port (a C local: a test point, a normalized
 *     velocity, the &is_car / &index outputs) passes the original a game address: in place when
 *     the pointer is in game memory, else a copy on the frame stack (copied back for outputs).
 *   - The ports read the frame rate / 60 Hz ticks from src/math/tm2math.c statics, which the
 *     harness set by hand; the adapters set them from the game's 0x004e8300 / 0x004e8304 first.
 *   - The ports draw from src/core/tm2rand.c, and their matrices use tm2math's sin tables: those
 *     are only the game's when the core binds rand / srand 0x004899c2 / 0x004899e6 to tm2_rand /
 *     tm2_srand and loads the tables (tm2_math_load_tables) at start up.
 * Port code, not a decompiled function.
 */
#include "rt_env.h"
#include "math/tm2math.h"
#include "weapons/bullet.h"
#include "weapons/fx_ground.h"
#include "weapons/fx_sprite.h"
#include "weapons/fx_world.h"
#include "weapons/projectile.h"
#include "weapons/weapon_hit.h"
#include "weapons/weapon_misc.h"

/* ---- shared helpers ---------------------------------------------------------------------- */

static WMem s_mem;

static const WMem* wm(void)
{
    s_mem.n = 1;
    s_mem.r[0].addr = 0;
    s_mem.r[0].size = RT_SIZE;
    s_mem.r[0].host = rt_base;
    return &s_mem;
}

static void sync_frame_rate(void)
{
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
}

/* the game address of n bytes the port passes by host pointer: in place when they lie in game
 * memory, else a copy in a new frame (*fr = its size, 0 for none; gdone pops it) */
static uint32_t gaddr(const void* p, uint32_t n, uint32_t* fr)
{
    uintptr_t q = (uintptr_t)p, b = (uintptr_t)rt_base;
    *fr = 0;
    if (!p)
        return 0;
    if (q >= b && q + n <= b + RT_SIZE)
        return (uint32_t)(q - b);
    *fr = (n + 15u) & ~15u;
    uint32_t a = rt_frame_push(*fr);
    memcpy(rt_ptr(a), p, n);
    return a;
}
static void gdone(uint32_t fr) { if (fr) rt_frame_pop(fr); }

static uint32_t call0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static uint32_t call1(uint32_t fn, uint32_t a) { uint32_t v[1] = {a}; return rt_call(fn, v, 1); }
static uint32_t call2(uint32_t fn, uint32_t a, uint32_t b) { uint32_t v[2] = {a, b}; return rt_call(fn, v, 2); }
static uint32_t call3(uint32_t fn, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t v[3] = {a, b, c};
    return rt_call(fn, v, 3);
}
static uint32_t call4(uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t v[4] = {a, b, c, d};
    return rt_call(fn, v, 4);
}

/* a vector by host pointer to a function taking (EAX = its address, EDX, EBX, ECX) */
static uint32_t call_vec(uint32_t fn, const int32_t* p, uint32_t edx, uint32_t ebx, uint32_t ecx, int n)
{
    uint32_t fr, v[4];
    v[0] = gaddr(p, 12, &fr);
    v[1] = edx; v[2] = ebx; v[3] = ecx;
    uint32_t r = rt_call(fn, v, n);
    gdone(fr);
    return r;
}

/* 0x0048df8c(EAX owner, EDX model, EBX pos, ECX &hit): the collision record, sets *hit */
static uint32_t level_hit_call(int32_t owner, uint32_t model, const int32_t pos[3], int32_t* hit)
{
    uint32_t fp, fh;
    uint32_t pa = gaddr(pos, 12, &fp);
    uint32_t ha = gaddr(hit, 4, &fh);
    uint32_t r = call4(0x0048df8cu, (uint32_t)owner, model, pa, ha);
    memcpy(hit, rt_ptr(ha), 4);
    gdone(fh);
    gdone(fp);
    return r;
}

/* 0x00415d1c(EAX hit, EDX rtype, EBX rc, ECX damage, stack velocity, owner) */
static void apply_hit_call(int32_t hit, int32_t rtype, int32_t rc, int32_t dmg, const int32_t vel[3], int32_t owner)
{
    uint32_t fr, v[6];
    v[0] = (uint32_t)hit; v[1] = (uint32_t)rtype; v[2] = (uint32_t)rc; v[3] = (uint32_t)dmg;
    v[4] = gaddr(vel, 12, &fr);
    v[5] = (uint32_t)owner;
    rt_call(0x00415d1cu, v, 6);
    gdone(fr);
}

/* ---- fx_sprite.c / fx_ground.c ------------------------------------------------------------ */

/* sound placement 0x004b8010(EAX pos, EDX view); play 0x0046f8b0(EAX id, EDX volume, EBX pan, ECX prio) */
static void fx_cb_locate(void* u, uint32_t pos, int32_t view) { (void)u; call2(0x004b8010u, pos, (uint32_t)view); }
static void fx_cb_play(void* u, int32_t id, int32_t vol, int32_t pan, int32_t prio)
{
    (void)u;
    call4(0x0046f8b0u, (uint32_t)id, (uint32_t)vol, (uint32_t)pan, (uint32_t)prio);
}

static FxEnv s_fx;

static const FxEnv* fx_env(void)
{
    s_fx.mem = wm();
    s_fx.sprites = rt_ptr(0x00500304u);
    s_fx.sprites_addr = 0x00500304u;
    s_fx.particles = rt_ptr(0x00500034u);
    s_fx.boom_cooldown = (int32_t*)rt_ptr(0x00500030u);
    s_fx.split = (int32_t)rt_r32(0x00520e90u);
    s_fx.snd_volume = (const int32_t*)rt_ptr(0x00c8500cu);
    s_fx.snd_pan = (const int32_t*)rt_ptr(0x00c85058u);
    s_fx.frames = 0x00501f26u;
    s_fx.sound_locate = fx_cb_locate;
    s_fx.sound_play = fx_cb_play;
    s_fx.user = NULL;
    return &s_fx;
}

/* 0x00450380 fx_sprite_spawn(EAX pos, EDX kind, EBX step, ECX frames, stack table, count, flag) */
static uint32_t b_fx_sprite_spawn(const uint32_t* a, int n)
{
    return fx_sprite_spawn(fx_env(), rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1), (uint8_t)rt_arg(a, n, 2),
                           (uint8_t)rt_arg(a, n, 3), rt_arg(a, n, 4), (uint8_t)rt_arg(a, n, 5), (uint8_t)rt_arg(a, n, 6));
}

/* the sprite starters (EAX pos, EDX flag) */
#define FX_VOID(name, fn)                                                    \
    static uint32_t b_##fn(const uint32_t* a, int n)                         \
    {                                                                        \
        fn(fx_env(), rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1));             \
        return 0;                                                            \
    }
#define FX_RET(name, fn)                                                     \
    static uint32_t b_##fn(const uint32_t* a, int n)                         \
    {                                                                        \
        return fn(fx_env(), rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1));      \
    }
FX_VOID(0x00450864, fx_explosion8)
FX_VOID(0x00450984, fx_explosion16)
FX_VOID(0x00450aa0, fx_explosion48)
FX_RET(0x00450c50, fx_smoke_burst)
FX_VOID(0x004508f4, fx_explosion12)
FX_VOID(0x00450a10, fx_explosion24)
FX_RET(0x00450c08, fx_freeze_burst)
FX_VOID(0x004512f8, fx_freeze_glow)
FX_VOID(0x00451058, fx_smoke_small)
FX_VOID(0x0045109c, fx_smoke)
FX_VOID(0x004507a0, fx_puff)
FX_RET(0x00450f04, fx_ground_flame)
FX_VOID(0x00450718, fx_bullet_spark)
FX_VOID(0x00450fd4, fx_bullet_smoke)
FX_VOID(0x004513c8, fx_ricochet)

/* 0x00450f44 fx_flame_bit(EAX pos) */
static uint32_t b_fx_flame_bit(const uint32_t* a, int n)
{
    fx_flame_bit(fx_env(), rt_arg(a, n, 0));
    return 0;
}

/* 0x004516bc fx_impact_debris(EAX n, EDX pos) */
static uint32_t b_fx_impact_debris(const uint32_t* a, int n)
{
    fx_impact_debris(fx_env(), (int32_t)rt_arg(a, n, 0), rt_arg(a, n, 1));
    return 0;
}

/* 0x004517e0 fx_impact_particles_update() */
static uint32_t b_fx_impact_particles_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    fx_impact_particles_update(fx_env());
    return 0;
}

/* 0x0044a4cc ground_fire_spawn(EAX pos) */
static uint32_t b_ground_fire_spawn(const uint32_t* a, int n)
{
    GroundEnv g = {fx_env(), rt_ptr(0x004ff308u), (int32_t*)rt_ptr(0x004fef70u), 0x004fef70u};
    ground_fire_spawn(&g, rt_arg(a, n, 0));
    return 0;
}

/* ---- fx_world.c ---------------------------------------------------------------------------- */

/* 0x00470320(EAX id, EDX i, EBX on); 0x00495864(EAX i) */
static void w_cb_anim(void* u, int32_t id, int32_t i, int32_t on)
{
    (void)u;
    call3(0x00470320u, (uint32_t)id, (uint32_t)i, (uint32_t)on);
}
static uint32_t w_cb_variant(void* u, int32_t i) { (void)u; return call1(0x00495864u, (uint32_t)i); }

static WorldEnv world_env(void)
{
    WorldEnv w;
    w.fx = fx_env();
    w.splash_next = (int32_t*)rt_ptr(0x004e7414u);
    w.splash_objs = (const uint32_t*)rt_ptr(0x00502f20u);
    w.blast_next = (int32_t*)rt_ptr(0x004e741cu);
    w.blast_objs = (const uint32_t*)rt_ptr(0x00502f48u);
    w.blast_anims = (const uint32_t*)rt_ptr(0x00621370u);
    w.flicker = rt_ptr(0x005028c0u);
    w.collapse = rt_ptr(0x005029a4u);
    w.tower = rt_ptr(0x004f08e0u);
    w.tower_addr = 0x004f08e0u;
    w.user = NULL;
    w.anim_start = w_cb_anim;
    w.model_variant = w_cb_variant;
    return w;
}

/* 0x00457138 fx_splash_at(EAX pos, EDX speed) */
static uint32_t b_fx_splash_at(const uint32_t* a, int n)
{
    WorldEnv w = world_env();
    fx_splash_at(&w, rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
    return 0;
}

/* 0x00457314 fx_pooled_blast(EAX pos): the result is AL only; the rest of EAX is 0 here */
static uint32_t b_fx_pooled_blast(const uint32_t* a, int n)
{
    WorldEnv w = world_env();
    return fx_pooled_blast(&w, rt_arg(a, n, 0));
}

/* 0x004534c8 light_flicker_tick() */
static uint32_t b_light_flicker_tick(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WorldEnv w = world_env();
    light_flicker_tick(&w);
    return 0;
}

/* 0x00411e28 eiffel_tower_start_fall(EAX pos) */
static uint32_t b_eiffel_tower_start_fall(const uint32_t* a, int n)
{
    WorldEnv w = world_env();
    eiffel_tower_start_fall(&w, rt_arg(a, n, 0));
    return 0;
}

/* ---- bullet.c ------------------------------------------------------------------------------ */

/* 0x004126b4(EAX pos, EDX owner, EBX radius, ECX type) */
static int32_t cb_hit_test(void* u, const int32_t pos[3], int32_t owner, int32_t radius, int32_t type)
{
    (void)u;
    return (int32_t)call_vec(0x004126b4u, pos, (uint32_t)owner, (uint32_t)radius, (uint32_t)type, 4);
}
static uint32_t cb_level_hit(void* u, int32_t owner, uint32_t model, const int32_t pos[3], int32_t* hit)
{
    (void)u;
    return level_hit_call(owner, model, pos, hit);
}
static void cb_apply_hit(void* u, int32_t hit, int32_t rtype, int32_t rc, int32_t dmg, const int32_t vel[3],
                         int32_t owner)
{
    (void)u;
    apply_hit_call(hit, rtype, rc, dmg, vel, owner);
}
/* the bullet effects (EAX pos, EDX flag 0) */
static void bl_cb_fx(void* u, uint32_t fn, const int32_t pos[3]) { (void)u; call_vec(fn, pos, 0, 0, 0, 2); }

static BulletEnv bullet_env(void)
{
    BulletEnv b = {wm(), rt_ptr(0x004f0910u), NULL, cb_hit_test, cb_level_hit, bl_cb_fx, cb_apply_hit};
    return b;
}

/* 0x00412dc4 bullet_spawn(EAX owner, EDX flag, EBX dir, ECX pos, stack damage) */
static uint32_t b_bullet_spawn(const uint32_t* a, int n)
{
    BulletEnv b = bullet_env();
    bullet_spawn(&b, (int32_t)rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3),
                 (int32_t)rt_arg(a, n, 4));
    return 0;
}

/* 0x00412fb8 bullets_move() */
static uint32_t b_bullets_move(const uint32_t* a, int n)
{
    (void)a; (void)n;
    BulletEnv b = bullet_env();
    bullets_move(&b);
    return 0;
}

/* ---- weapon_hit.c -------------------------------------------------------------------------- */

/* 0x0045b178(EAX owner, EDX &is_car, EBX &index): nonzero (AL) when the owner is a car */
static int h_cb_car_lookup(void* u, int32_t owner, uint8_t* is_car, int32_t* index)
{
    (void)u;
    uint32_t f = rt_frame_push(16);
    rt_w8(f, *is_car);
    rt_w32(f + 4, (uint32_t)*index);
    uint32_t r = call3(0x0045b178u, (uint32_t)owner, f, f + 4);
    *is_car = rt_r8(f);
    *index = (int32_t)rt_r32(f + 4);
    rt_frame_pop(16);
    return (int)(r & 0xFFu);   /* the harness port stub answered found & 0xFF */
}
static uint32_t cb_car_data(void* u, int32_t i) { (void)u; return call1(0x0045afbcu, (uint32_t)i); }
static uint32_t h_cb_ai_data(void* u, int32_t i) { (void)u; return call1(0x0045b080u, (uint32_t)i); }
static uint32_t h_cb_hit_table(void* u) { (void)u; return call0(0x00470d08u); }
/* 0x004723bc(EAX id, EDX velocity) */
static void h_cb_object_hit(void* u, int32_t id, const int32_t vel[3])
{
    (void)u;
    uint32_t fr;
    uint32_t va = gaddr(vel, 12, &fr);
    call2(0x004723bcu, (uint32_t)id, va);
    gdone(fr);
}
/* 0x004210e4(EAX car, EDX damage, EBX direction, ECX 1, stack attacker) */
static void h_cb_car_damage(void* u, int32_t car, int32_t dmg, const int32_t dir[3], int32_t one, int32_t attacker)
{
    (void)u;
    uint32_t fr, v[5];
    v[0] = (uint32_t)car; v[1] = (uint32_t)dmg; v[2] = gaddr(dir, 12, &fr); v[3] = (uint32_t)one;
    v[4] = (uint32_t)attacker;
    rt_call(0x004210e4u, v, 5);
    gdone(fr);
}
static void h_cb_level_damage(void* u, int32_t rtype, int32_t rc, int32_t dmg)
{
    (void)u;
    call3(0x00470d3cu, (uint32_t)rtype, (uint32_t)rc, (uint32_t)dmg);
}
static void h_cb_dish(void* u, int32_t dmg) { (void)u; call1(0x00411b30u, (uint32_t)dmg); }
static void h_cb_eiffel(void* u, int32_t which, int32_t dmg) { (void)u; call2(0x00472b70u, (uint32_t)which, (uint32_t)dmg); }

/* 0x00415d1c weapon_apply_hit(EAX hit, EDX rtype, EBX rc, ECX damage, stack velocity, owner).
 * The original returns in AL only (0 leaves EAX's upper bytes as they were); 0 / 1 here. */
static uint32_t b_weapon_apply_hit(const uint32_t* a, int n)
{
    HitEnv h = {wm(), NULL, h_cb_car_lookup, cb_car_data, h_cb_ai_data, h_cb_hit_table, h_cb_object_hit,
                h_cb_car_damage, h_cb_level_damage, h_cb_dish, h_cb_eiffel};
    return weapon_apply_hit(&h, (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1), (int32_t)rt_arg(a, n, 2),
                            (int32_t)rt_arg(a, n, 3), rt_arg(a, n, 4), (int32_t)rt_arg(a, n, 5));
}

/* ---- weapon_misc.c ------------------------------------------------------------------------- */

/* 0x004707b0(EAX id, EDX n, EBX on); 0x004735fc(EAX kind, EDX a, EBX b) */
static void m_cb_show(void* u, int32_t id, int32_t n, int32_t on)
{
    (void)u;
    call3(0x004707b0u, (uint32_t)id, (uint32_t)n, (uint32_t)on);
}
static void m_cb_event(void* u, int32_t kind, int32_t a, int32_t b)
{
    (void)u;
    call3(0x004735fcu, (uint32_t)kind, (uint32_t)a, (uint32_t)b);
}

static WMiscEnv misc_env(void)
{
    WMiscEnv m;
    m.mem = wm();
    m.slam_cursor = (int32_t*)rt_ptr(0x004ff8a4u);   /* in the same memory as the fifth claw record */
    m.slam_records = 0x004ff85cu;
    m.blast_anims = (const uint32_t*)rt_ptr(0x00621370u);
    m.shields = rt_ptr(0x004ffac8u);
    m.flash = rt_ptr(0x004f5dc4u);
    m.split = (int32_t)rt_r32(0x00520e90u);
    m.pickup_timer = (int32_t*)rt_ptr(0x004fef6cu);
    m.pickup_period = (const int32_t*)rt_ptr(0x004e6a30u);
    m.pickup_count = (const int32_t*)rt_ptr(0x004ff2dcu);
    m.pickups = rt_ptr(0x004fefbcu);
    m.user = NULL;
    m.car_data = cb_car_data;
    m.pickup_show = m_cb_show;
    m.event = m_cb_event;
    return m;
}

/* 0x0044c7e4 slam_records_reset() */
static uint32_t b_slam_records_reset(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WMiscEnv m = misc_env();
    slam_records_reset(&m);
    return 0;
}

/* 0x0044c81c slam_state_init(EAX weapon block) */
static uint32_t b_slam_state_init(const uint32_t* a, int n)
{
    WMiscEnv m = misc_env();
    slam_state_init(&m, rt_ptr(rt_arg(a, n, 0)));
    return 0;
}

/* 0x00470640 blast_anim_step(EAX i) */
static uint32_t b_blast_anim_step(const uint32_t* a, int n)
{
    WMiscEnv m = misc_env();
    return blast_anim_step(&m, (int32_t)rt_arg(a, n, 0));
}

/* 0x0044bb18 shields_update() */
static uint32_t b_shields_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    sync_frame_rate();
    WMiscEnv m = misc_env();
    shields_update(&m);
    return 0;
}

/* 0x00415bc4 screen_flash_start(EAX frames, EDX r, EBX g, ECX b) */
static uint32_t b_screen_flash_start(const uint32_t* a, int n)
{
    WMiscEnv m = misc_env();
    screen_flash_start(&m, (int32_t)rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1), (uint8_t)rt_arg(a, n, 2),
                       (uint8_t)rt_arg(a, n, 3));
    return 0;
}

/* 0x004498d8 pickup_respawn_tick() */
static uint32_t b_pickup_respawn_tick(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WMiscEnv m = misc_env();
    pickup_respawn_tick(&m);
    return 0;
}

/* ---- projectile.c -------------------------------------------------------------------------- */

static void p_cb_locate(void* u, const int32_t pos[3], int32_t view)
{
    (void)u;
    call_vec(0x004b8010u, pos, (uint32_t)view, 0, 0, 2);
}
/* 0x0046f934(EAX type, EDX slot + 1) */
static void p_cb_end(void* u, int32_t type, int32_t slot1) { (void)u; call2(0x0046f934u, (uint32_t)type, (uint32_t)slot1); }
/* an effect (EAX pos, EDX flag 0): the render object's position when the port names it */
static void p_cb_fx(void* u, uint32_t fn, const int32_t pos[3], uint32_t pos_addr)
{
    (void)u;
    if (pos_addr || !pos)
        call2(fn, pos_addr, 0);
    else
        call_vec(fn, pos, 0, 0, 0, 2);
}
/* 0x0044a40c(EAX pos, EDX a, EBX b, ECX c): the jitter point's game address */
static uint32_t p_cb_jitter(void* u, const int32_t pos[3], uint32_t pos_addr, int32_t x, int32_t y, int32_t z)
{
    (void)u;
    if (pos_addr || !pos)
        return call4(0x0044a40cu, pos_addr, (uint32_t)x, (uint32_t)y, (uint32_t)z);
    return call_vec(0x0044a40cu, pos, (uint32_t)x, (uint32_t)y, (uint32_t)z, 4);
}
/* 0x00474508: the game event type (low byte) */
static int32_t p_cb_event_type(void* u) { (void)u; return (int32_t)call0(0x00474508u); }
static uint32_t p_cb_entity(void* u, int32_t type) { (void)u; return call1(0x0045b0b8u, (uint32_t)type); }
static void p_cb_steer(void* u, uint32_t slot) { (void)u; call1(0x004147c8u, slot); }
/* 0x0042c2d4(EAX pos): ground height */
static int32_t p_cb_ground(void* u, const int32_t pos[3], uint32_t pos_addr)
{
    (void)u;
    if (pos_addr || !pos)
        return (int32_t)call1(0x0042c2d4u, pos_addr);
    return (int32_t)call_vec(0x0042c2d4u, pos, 0, 0, 0, 1);
}
static int32_t p_cb_car_count(void* u) { (void)u; return (int32_t)call0(0x0045afb4u); }
static void p_cb_car_slot(void* u, int32_t i) { (void)u; call1(0x0045ae24u, (uint32_t)i); }

static ProjEnv s_proj;

static ProjEnv* proj_env(void)
{
    sync_frame_rate();
    s_proj.mem = wm();
    s_proj.slots = rt_ptr(PROJ_SLOTS_ADDR);
    s_proj.models = (uint32_t*)rt_ptr(PROJ_MODELS_ADDR);
    s_proj.trail_frames = rt_r32(0x004f11d8u);
    s_proj.split = (int32_t)rt_r32(0x00520e90u);
    s_proj.snd_volume = (const int32_t*)rt_ptr(0x00c8500cu);
    s_proj.snd_pan = (const int32_t*)rt_ptr(0x00c85058u);
    s_proj.user = NULL;
    s_proj.sound_locate = p_cb_locate;
    s_proj.sound_play = fx_cb_play;
    s_proj.sound_end = p_cb_end;
    s_proj.fx = p_cb_fx;
    s_proj.jitter = p_cb_jitter;
    s_proj.event_type = p_cb_event_type;
    s_proj.entity_by_type = p_cb_entity;
    s_proj.steer = p_cb_steer;
    s_proj.hit_test = cb_hit_test;
    s_proj.level_hit = cb_level_hit;
    s_proj.ground = p_cb_ground;
    s_proj.apply_hit = cb_apply_hit;
    s_proj.car_count = p_cb_car_count;
    s_proj.car_model_slot = p_cb_car_slot;
    s_proj.car_data = cb_car_data;
    return &s_proj;
}

/* 0x00412670 proj_store_model(EAX model, EDX id) */
static uint32_t b_proj_store_model(const uint32_t* a, int n)
{
    proj_store_model(proj_env(), rt_arg(a, n, 0), rt_arg(a, n, 1));
    return 0;
}

/* 0x00413e90 proj_object_init(EAX slot, EDX angles, EBX pos) */
static uint32_t b_proj_object_init(const uint32_t* a, int n)
{
    proj_object_init(proj_env(), rt_ptr(rt_arg(a, n, 0)), rt_arg(a, n, 1), rt_arg(a, n, 2));
    return 0;
}

/* 0x0041354c proj_trail_start(EAX index) */
static uint32_t b_proj_trail_start(const uint32_t* a, int n)
{
    proj_trail_start(proj_env(), (int32_t)rt_arg(a, n, 0));
    return 0;
}

/* 0x004136d4 proj_trail_uvs(EAX slot, EDX index unused) */
static uint32_t b_proj_trail_uvs(const uint32_t* a, int n)
{
    proj_trail_uvs(proj_env(), rt_ptr(rt_arg(a, n, 0)));
    return 0;
}

/* 0x00413a44 proj_trail_step(EAX slot, EDX index unused) */
static uint32_t b_proj_trail_step(const uint32_t* a, int n)
{
    proj_trail_step(proj_env(), rt_ptr(rt_arg(a, n, 0)));
    return 0;
}

/* 0x00414000 proj_spawn_plain(EAX owner, EDX unused, EBX angles, ECX pos, stack damage) */
static uint32_t b_proj_spawn_plain(const uint32_t* a, int n)
{
    proj_spawn_plain(proj_env(), (int32_t)rt_arg(a, n, 0), rt_arg(a, n, 2), rt_arg(a, n, 3), (int32_t)rt_arg(a, n, 4));
    return 0;
}

/* the fire functions (EAX owner, EDX target, EBX angles, ECX pos, stack damage; 0x004146f8 pops a
 * second, unused stack word) */
#define PROJ_FIRE(fn)                                                                                  \
    static uint32_t b_##fn(const uint32_t* a, int n)                                                   \
    {                                                                                                  \
        return (uint32_t)fn(proj_env(), (int32_t)rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2),    \
                            rt_arg(a, n, 3), (int32_t)rt_arg(a, n, 4));                                \
    }
PROJ_FIRE(proj_fire_homing)
PROJ_FIRE(proj_fire_colored)
PROJ_FIRE(proj_fire_dark_tooth)
PROJ_FIRE(proj_fire_ghost)

/* 0x00414114 proj_fire_freeze: the harness compared no result */
static uint32_t b_proj_fire_freeze(const uint32_t* a, int n)
{
    proj_fire_freeze(proj_env(), (int32_t)rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3),
                     (int32_t)rt_arg(a, n, 4));
    return 0;
}

/* 0x00414f98 projectile_tick(EAX slot) */
static uint32_t b_proj_tick(const uint32_t* a, int n)
{
    proj_tick(proj_env(), rt_ptr(rt_arg(a, n, 0)));
    return 0;
}

/* 0x00415b54 projectile_update_all() */
static uint32_t b_proj_update_all(const uint32_t* a, int n)
{
    (void)a; (void)n;
    proj_update_all(proj_env());
    return 0;
}

void rt_bind_weapons(void)
{
    rt_bind(0x00450380u, b_fx_sprite_spawn, "fx_sprite_spawn");
    rt_bind(0x00450864u, b_fx_explosion8, "fx_explosion8");
    rt_bind(0x00450984u, b_fx_explosion16, "fx_explosion16");
    rt_bind(0x00450aa0u, b_fx_explosion48, "fx_explosion48");
    rt_bind(0x00450c50u, b_fx_smoke_burst, "fx_smoke_burst");
    rt_bind(0x004508f4u, b_fx_explosion12, "fx_explosion12");
    rt_bind(0x00450a10u, b_fx_explosion24, "fx_explosion24");
    rt_bind(0x00450c08u, b_fx_freeze_burst, "fx_freeze_burst");
    rt_bind(0x004512f8u, b_fx_freeze_glow, "fx_freeze_glow");
    rt_bind(0x00451058u, b_fx_smoke_small, "fx_smoke_small");
    rt_bind(0x0045109cu, b_fx_smoke, "fx_smoke");
    rt_bind(0x004507a0u, b_fx_puff, "fx_puff");
    rt_bind(0x00450f04u, b_fx_ground_flame, "fx_ground_flame");
    rt_bind(0x00450718u, b_fx_bullet_spark, "fx_bullet_spark");
    rt_bind(0x00450fd4u, b_fx_bullet_smoke, "fx_bullet_smoke");
    rt_bind(0x004513c8u, b_fx_ricochet, "fx_ricochet");
    rt_bind(0x00450f44u, b_fx_flame_bit, "fx_flame_bit");
    rt_bind(0x004516bcu, b_fx_impact_debris, "fx_impact_debris");
    rt_bind(0x004517e0u, b_fx_impact_particles_update, "fx_impact_particles_update");
    rt_bind(0x0044a4ccu, b_ground_fire_spawn, "ground_fire_spawn");

    rt_bind(0x00457138u, b_fx_splash_at, "fx_splash_at");
    rt_bind(0x00457314u, b_fx_pooled_blast, "fx_pooled_blast");
    rt_bind(0x004534c8u, b_light_flicker_tick, "light_flicker_tick");
    rt_bind(0x00411e28u, b_eiffel_tower_start_fall, "eiffel_tower_start_fall");

    rt_bind(0x00412dc4u, b_bullet_spawn, "bullet_spawn");
    rt_bind(0x00412fb8u, b_bullets_move, "bullets_move");

    rt_bind(0x00415d1cu, b_weapon_apply_hit, "weapon_apply_hit");

    rt_bind(0x0044c7e4u, b_slam_records_reset, "slam_records_reset");
    rt_bind(0x0044c81cu, b_slam_state_init, "slam_state_init");
    rt_bind(0x00470640u, b_blast_anim_step, "blast_anim_step");
    rt_bind(0x0044bb18u, b_shields_update, "shields_update");
    rt_bind(0x00415bc4u, b_screen_flash_start, "screen_flash_start");
    rt_bind(0x004498d8u, b_pickup_respawn_tick, "pickup_respawn_tick");

    rt_bind(0x00412670u, b_proj_store_model, "proj_store_model");
    rt_bind(0x00413e90u, b_proj_object_init, "proj_object_init");
    rt_bind(0x0041354cu, b_proj_trail_start, "proj_trail_start");
    rt_bind(0x004136d4u, b_proj_trail_uvs, "proj_trail_uvs");
    rt_bind(0x00413a44u, b_proj_trail_step, "proj_trail_step");
    rt_bind(0x00414000u, b_proj_spawn_plain, "proj_spawn_plain");
    rt_bind(0x00414458u, b_proj_fire_homing, "proj_fire_homing");
    rt_bind(0x004146f8u, b_proj_fire_colored, "proj_fire_colored");
    rt_bind(0x00414114u, b_proj_fire_freeze, "proj_fire_freeze");
    rt_bind(0x004142acu, b_proj_fire_dark_tooth, "proj_fire_dark_tooth");
    rt_bind(0x00414328u, b_proj_fire_ghost, "proj_fire_ghost");
    rt_bind(0x00414f98u, b_proj_tick, "projectile_tick");
    rt_bind(0x00415b54u, b_proj_update_all, "projectile_update_all");
}

RT_BIND_AREA(weapons)
