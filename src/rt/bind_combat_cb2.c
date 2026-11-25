/*
 * bind_combat_cb2.c: runtime bindings of the combat ports cb2_arcs, cb2_arena_setup,
 * cb2_ground_fires, cb2_launch, cb2_special, cb2_world_tables, gp3_roller, gp3_tornado, gp3_wfs.
 *
 * Each adapter calls the port the way its harness (tools/emu/diff_cb2_*.py, diff_gp3_*.py) did.
 * Callbacks are rt_call of the original address with the registers the harness hooks read (eax,
 * edx, ebx, ecx, then stack words). Host pointers the ports hand to callees (their locals: a
 * vector, a primitive, a ground query result) go through a frame on the runtime's stack and are
 * copied back after the call; pointers already into game memory are passed as their address.
 */
#include <stdio.h>

#include "rt_env.h"
#include "assets/pe.h"
#include "math/tm2math.h"
#include "combat/cb2_arcs.h"
#include "combat/cb2_arena_setup.h"
#include "combat/cb2_ground_fires.h"
#include "combat/cb2_launch.h"
#include "combat/cb2_special.h"
#include "combat/cb2_world_tables.h"
#include "combat/gp3_roller.h"
#include "combat/gp3_tornado.h"
#include "combat/gp3_wfs.h"

/* ---- helpers ---- */

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
static uint32_t ga(const void* p) { return rt_addr(p); }   /* a pointer into game memory */

/* the ports that read tm2_frame_ticks60 / tm2_frame_fps see the game's current values */
static void sync_frame(void)
{
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
}

static uint32_t cb_rand(void* u) { (void)u; return c0(0x004899c2u); }
static int16_t cb_ticks(void* u) { (void)u; return (int16_t)c0(0x004931d4u); }
static int32_t cb_sound_mode(void* u) { (void)u; return (int32_t)c0(0x0046cc54u); }
/* mode 0: 0x0046f974(pos), 1: 0x0046f980(pos, -1) */
static void cb_sound_at(void* u, int32_t* pos, int mode)
{
    (void)u;
    Hb h;
    uint32_t p = hb_in(&h, pos, 12, 0);
    if (mode) c2(0x0046f980u, p, 0xffffffffu);
    else c1(0x0046f974u, p);
    hb_out(&h);
}
/* 0: pan 0x0046f990, 1: volume 0x0046f988 */
static uint32_t cb_sound_get(void* u, int which) { (void)u; return c0(which ? 0x0046f988u : 0x0046f990u); }
static void cb_hud(void* u, int32_t a, int32_t b) { (void)u; c2(0x00436040u, (uint32_t)a, (uint32_t)b); }
static uint32_t pick_addr(int which) { return which ? 0x00449048u : 0x00448de4u; }

/* ---- 0x00447748 weaponArcsDraw(ot) ---- */

static int32_t* ar_resolve(void* u, uint32_t h) { (void)u; return (int32_t*)(void*)rt_ptr(h); }
static void ar_smoke(void* u, uint32_t pos) { (void)u; c2(0x00451274u, pos, 0); }
static void ar_expl(void* u, uint32_t pos) { (void)u; c2(0x00450864u, pos, 0); }
static void ar_damage(void* u, int32_t t, int32_t a, int32_t b, int32_t c, int32_t owner)
{
    (void)u;
    uint32_t x[5] = {(uint32_t)t, (uint32_t)a, (uint32_t)b, (uint32_t)c, (uint32_t)owner};
    rt_call(0x004210e4u, x, 5);
}
static void ar_draw(void* u, uint32_t ot, uint8_t* prim)
{
    (void)u;
    Hb h;
    uint32_t p = hb_in(&h, prim, 64, 0);
    c2(0x00491698u, ot, p);
    hb_out(&h);
}

/* The original stores the address of its stack local (the circling end point) into a record's
 * +0x10, dead once it returns. Here that local is a frame on the runtime's stack for the call. */
static uint32_t b_arcs_draw(const uint32_t* a, int n)
{
    uint32_t local = rt_frame_push(16);
    Cb2ArcsEnv e = {rt_ptr(0x004ff7f8u), (int32_t*)(void*)rt_ptr(0x004fef68u), ar_resolve, local, cb_rand,
                    ar_smoke, ar_expl, ar_damage, cb_ticks, ar_draw, NULL};
    cb2_arcs_draw(&e, rt_arg(a, n, 0));
    rt_frame_pop(16);
    return 0;
}

/* ---- 0x004576a8 arenaSetup(al = event) ---- */

/* The port packs a callee's register arguments then its stack words, the runtime's order
 * (0x00494610: eax + six stack words, a[0 .. 6]) */
static uint32_t as_call(void* u, uint32_t addr, const uint32_t* args, int nargs)
{
    (void)u;
    return rt_call(addr, args, nargs);
}

static uint32_t b_arena_setup(const uint32_t* a, int n)
{
    Cb2ArenaEnv e = {
        (int32_t*)(void*)rt_ptr(0x0050e0b4u), (int32_t*)(void*)rt_ptr(0x0050e0d4u),
        (const int32_t*)(void*)rt_ptr(0x00503084u), (const int16_t*)(void*)rt_ptr(0x00503028u),
        (const uint32_t*)(void*)rt_ptr(0x00503048u), (uint32_t*)(void*)rt_ptr(0x0050c2b8u),
        (int32_t*)(void*)rt_ptr(0x00502fb4u), (int32_t*)(void*)rt_ptr(0x005075e4u), rt_ptr(0x0050c2c4u),
        (int32_t*)(void*)rt_ptr(0x00502f7cu), (int32_t*)(void*)rt_ptr(0x0050c2c0u), as_call, NULL,
    };
    cb2_arena_setup(&e, (uint8_t)rt_arg(a, n, 0));
    return 0;
}

/* ---- 0x0044a59c groundFiresUpdate ---- */

static uint8_t* gf_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static int32_t gf_hcount(void* u) { (void)u; return (int32_t)c0(0x0045afb4u); }
static int32_t gf_acount(void* u) { (void)u; return (int32_t)c0(0x0045b078u); }
static uint8_t* gf_hobj(void* u, int32_t i) { (void)u; return rt_ptr(c1(0x0045ae24u, (uint32_t)i)); }
static uint8_t* gf_hcar(void* u, int32_t i) { (void)u; return rt_ptr(c1(0x0045afbcu, (uint32_t)i)); }
static uint8_t* gf_aobj(void* u, int32_t i) { (void)u; return rt_ptr(c1(0x0045ae2cu, (uint32_t)i)); }
static uint8_t* gf_acar(void* u, int32_t i) { (void)u; return rt_ptr(c1(0x0045b080u, (uint32_t)i)); }
static uint8_t* gf_actors(void* u) { (void)u; return rt_ptr(c0(0x00471aecu)); }
static uint8_t* gf_marker(void* u, int32_t i) { (void)u; return rt_ptr(c1(0x00411e14u, (uint32_t)i)); }
static uint32_t gf_flame5(void* u, uint8_t* pos, int32_t a) { (void)u; return c2(0x00450de4u, ga(pos), (uint32_t)a); }
static void gf_actor_hit(void* u, int32_t i, int32_t* vec)
{
    (void)u;
    Hb h;
    uint32_t p = hb_in(&h, vec, 12, 0);
    c2(0x004723bcu, (uint32_t)i, p);
    hb_out(&h);
}
static void gf_pair(void* u, uint8_t* pos, int32_t a) { (void)u; c2(0x00450e64u, ga(pos), (uint32_t)a); }
static void gf_x16(void* u, uint8_t* pos, int32_t a) { (void)u; c2(0x00450984u, ga(pos), (uint32_t)a); }
static uint8_t gf_event(void* u) { (void)u; return (uint8_t)c0(0x00474508u); }
static void gf_clut(void* u, int32_t a, int32_t b, int32_t c) { (void)u; c3(0x00499984u, (uint32_t)a, (uint32_t)b, (uint32_t)c); }

static uint32_t b_ground_fires(const uint32_t* a, int n)
{
    (void)a; (void)n;
    Cb2GroundFiresEnv e = {rt_ptr(0x004ff308u), (const int32_t*)(void*)rt_ptr(0x004e6a5cu), gf_resolve,
                           gf_hcount, gf_acount, gf_hobj, gf_hcar, gf_aobj, gf_acar, gf_actors, gf_marker,
                           cb_ticks, cb_rand, gf_flame5, gf_actor_hit, gf_pair, gf_x16, gf_event, gf_clut, NULL};
    cb2_ground_fires_update(&e);
    return 0;
}

/* ---- 0x00444004 weaponLaunchProjectile(car, kind, bl = human) ---- */

static const uint8_t* la_model(void* u, int human, int32_t id)
{
    (void)u;
    return rt_ptr(c1(human ? 0x0045ae24u : 0x0045ae2cu, (uint32_t)id));
}
static void la_pick(void* u, int which, int32_t id, int32_t* target, uint32_t human)
{
    (void)u;
    Hb h;
    uint32_t p = hb_in(&h, target, 4, 0);
    c3(pick_addr(which), (uint32_t)id, p, human);
    hb_out(&h);
}
static int32_t la_launch(void* u, uint32_t addr, uint32_t m, int32_t target, int32_t* ang, int32_t* pos, int32_t nn)
{
    (void)u;
    Hb ha, hp;
    uint32_t pa = hb_in(&ha, ang, 12, 0), pp = hb_in(&hp, pos, 12, 0);
    uint32_t r;
    if (addr == 0x00414178u) r = c4(addr, m, pa, pp, (uint32_t)nn);
    else {
        uint32_t x[5] = {m, (uint32_t)target, pa, pp, (uint32_t)nn};
        r = rt_call(addr, x, 5);
    }
    hb_out(&hp);
    hb_out(&ha);
    return (int32_t)r;
}
static void la_play(void* u, int has_index, int32_t snd, uint32_t b, uint32_t c, int32_t d, int32_t index)
{
    (void)u;
    if (has_index) {
        uint32_t x[5] = {(uint32_t)snd, b, c, (uint32_t)d, (uint32_t)index};
        rt_call(0x0046f8ccu, x, 5);
    } else {
        c4(0x0046f8b0u, (uint32_t)snd, b, c, (uint32_t)d);
    }
}
static uint8_t* la_proj(void* u, int32_t i) { (void)u; return rt_ptr(c1(0x00415b3cu, (uint32_t)i)); }

static uint32_t b_weapon_launch(const uint32_t* a, int n)
{
    Cb2LaunchEnv e = {la_model, la_pick, la_launch, cb_sound_mode, cb_sound_at, cb_sound_get, la_play, cb_hud,
                      la_proj, NULL};
    cb2_weapon_launch(&e, rt_ptr(rt_arg(a, n, 0)), rt_arg(a, n, 1), (uint8_t)rt_arg(a, n, 2));
    return 0;
}

/* ---- 0x0044342c special_weapon_trigger(car, dl = human) ---- */

static uint8_t* sp_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t sp_model(void* u, int human, int32_t id) { (void)u; return c1(human ? 0x0045ae24u : 0x0045ae2cu, (uint32_t)id); }
static void sp_net(void* u, int32_t a, uint32_t b) { (void)u; c2(0x00473568u, (uint32_t)a, b); }
static void sp_launch(void* u, uint8_t* car, int32_t kind, uint32_t human) { (void)u; c3(0x00444004u, ga(car), (uint32_t)kind, human); }
static void sp_noe(void* u, int32_t a) { (void)u; c1(0x0046fca0u, (uint32_t)a); }
static void sp_drop(void* u, uint8_t* car, int32_t kind, uint32_t human) { (void)u; c3(0x00448118u, ga(car), (uint32_t)kind, human); }
static void sp_play(void* u, int32_t snd, uint32_t b, uint32_t c, int32_t d) { (void)u; c4(0x0046f8b0u, (uint32_t)snd, b, c, (uint32_t)d); }
static void sp_spin(void* u, uint8_t* car, uint32_t human, int32_t a, int32_t b)
{
    (void)u;
    c4(0x00421920u, ga(car), human, (uint32_t)a, (uint32_t)b);
}
static int32_t sp_state(void* u) { (void)u; return (int32_t)c0(0x0045c340u); }
static void sp_pick(void* u, int32_t id, int32_t* target, uint32_t human) { la_pick(u, 0, id, target, human); }
static int32_t sp_pair(void* u, int32_t base, int32_t target, int32_t* ang, int32_t* pos, int32_t nn, uint32_t vec)
{
    (void)u;
    Hb ha, hp;
    uint32_t pa = hb_in(&ha, ang, 12, 0), pp = hb_in(&hp, pos, 12, 0);
    uint32_t x[6] = {(uint32_t)base, (uint32_t)target, pa, pp, (uint32_t)nn, vec};
    uint32_t r = rt_call(0x004146f8u, x, 6);
    hb_out(&hp);
    hb_out(&ha);
    return (int32_t)r;
}
static int32_t sp_one(void* u, uint32_t m, int32_t target, int32_t* ang, int32_t* pos, int32_t nn)
{
    return la_launch(u, 0x00414114u, m, target, ang, pos, nn);
}

static uint32_t b_special_trigger(const uint32_t* a, int n)
{
    Cb2SpecialEnv e = {(const int32_t*)(void*)rt_ptr(0x004ffed8u), (const uint32_t*)(void*)rt_ptr(0x004ffab8u),
                       rt_ptr(0x004ffac8u), sp_resolve, sp_model, sp_net, sp_launch, cb_hud, sp_noe, sp_drop,
                       cb_sound_mode, cb_sound_at, cb_sound_get, sp_play, sp_spin, sp_state, sp_pick, sp_pair,
                       sp_one, NULL};
    cb2_special_trigger(&e, rt_ptr(rt_arg(a, n, 0)), (uint8_t)rt_arg(a, n, 1));
    return 0;
}

/* ---- 0x00453fac worldWarpsSetup(event), 0x00454ae0 worldCollapseReset ----
 * The port's tables come from the code of TM2.EXE; the runtime already holds the image at its
 * addresses, so the extraction reads it there (one section covering the loaded image) instead of
 * opening the file again. Extracted once, on first use. */

static Cb2WarpsTable wt_warps;
static Cb2CollapseTable wt_collapse;
static int wt_state;   /* 0 not yet, 1 ready */

static void wt_load(void)
{
    if (wt_state) return;
    PeImage pe;
    memset(&pe, 0, sizeof pe);
    pe.data = rt_ptr(0x00400000u);
    pe.size = 0x00e0d000u - 0x00400000u;
    pe.image_base = 0x00400000u;
    pe.nsec = 1;
    pe.sec[0].va = 0x00400000u;
    pe.sec[0].raw_size = (uint32_t)pe.size;
    pe.sec[0].raw_off = 0;
    char err[256];
    if (cb2_warps_table_extract(&pe, &wt_warps, err, sizeof err) ||
        cb2_collapse_table_extract(&pe, &wt_collapse, err, sizeof err)) {
        fprintf(stderr, "tm2: world tables: %s\n", err);
        rt_stop("world tables not extracted", 0x00453facu);
    }
    wt_state = 1;
}

static uint32_t b_warps_setup(const uint32_t* a, int n)
{
    wt_load();
    cb2_world_warps_setup(rt_ptr(CB2_WARPS_VA), (int32_t)rt_arg(a, n, 0), &wt_warps);
    return 0;
}

static uint32_t b_collapse_reset(const uint32_t* a, int n)
{
    (void)a; (void)n;
    wt_load();
    cb2_world_collapse_reset(rt_ptr(CB2_COLLAPSE_VA), &wt_collapse);
    return 0;
}

/* ---- 0x0044ad14 rolling_weapon_update ---- */

static uint8_t* gp_mem(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

/* `local`: the argument that points at the caller's stack (index, bytes), as the harness table */
static uint32_t ro_call(void* u, uint32_t addr, int n, const uint32_t* args, void* local)
{
    (void)u;
    uint32_t x[8];
    for (int i = 0; i < n && i < 8; i++) x[i] = args[i];
    if (!local) return rt_call(addr, x, n);
    int li;
    uint32_t sz;
    switch (addr) {
    case 0x0049cec0u: li = 1; sz = 0x24; break;
    case 0x0048b5a0u: li = 1; sz = 12; break;
    case 0x00421800u: li = 2; sz = 12; break;
    case 0x004723bcu: li = 1; sz = 12; break;
    default: rt_stop("roller: local argument for", addr); return 0;
    }
    Hb h;
    x[li] = hb_in(&h, local, sz, 0);
    uint32_t r = rt_call(addr, x, n);
    hb_out(&h);
    return r;
}

static uint32_t b_roller_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    sync_frame();
    Gp3RollerEnv e = {rt_ptr(0x004ff8d0u), 0x004ff8d0u, (const int32_t*)(void*)rt_ptr(0x004e6a68u), 0x004e6a34u,
                      gp_mem, ro_call, NULL};
    gp3_roller_update(&e);
    return 0;
}

/* ---- 0x0044bd3c weaponTornadoUpdate ---- */

static uint32_t to_call(void* u, uint32_t addr, int n, const uint32_t* args, const int32_t* vec)
{
    (void)u;
    uint32_t x[8];
    for (int i = 0; i < n && i < 8; i++) x[i] = args[i];
    if (!vec) return rt_call(addr, x, n);
    /* 0x004723bc(i, &copy of the vector): the copy lives on the caller's stack */
    Hb h;
    x[1] = hb_in(&h, vec, 12, 1);
    uint32_t r = rt_call(addr, x, n);
    hb_out(&h);
    return r;
}

static uint32_t b_tornado_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    sync_frame();
    Gp3TornadoEnv e = {rt_ptr(0x004ffe1cu), rt_ptr(0x004fef7cu), 0x004fef7cu,
                       (const int16_t*)(void*)rt_ptr(0x004e6a28u), (const int32_t*)(void*)rt_ptr(0x004e6a74u),
                       gp_mem, to_call, NULL};
    gp3_tornado_update(&e);
    return 0;
}

/* ---- 0x004401fc weapon_fire_special(car, dl = human) ---- */

static uint32_t wf_call(void* u, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                        uint32_t s0, uint32_t s1)
{
    (void)u;
    uint32_t x[6] = {eax, edx, ebx, ecx, s0, s1};
    return rt_call(fn, x, 6);
}

/* scratch: the port's 0x40 bytes of locals at a game address, a frame here. stale_e6 (the
 * uninitialized stack word the original copies into a mine object, ORIGINAL BUG) is 0. */
static uint32_t b_weapon_fire_special(const uint32_t* a, int n)
{
    Gp3WfsEnv e;
    memset(&e, 0, sizeof e);
    e.reg[0].base = 0;
    e.reg[0].size = RT_SIZE;
    e.reg[0].host = rt_base;
    e.nreg = 1;
    e.scratch = rt_frame_push(0x40);
    e.stale_e6 = 0;
    e.call = wf_call;
    gp3_weapon_fire_special(&e, rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1));
    rt_frame_pop(0x40);
    return 0;
}

void rt_bind_combat_cb2(void)
{
    rt_bind(0x00447748u, b_arcs_draw, "weaponArcsDraw");
    rt_bind(0x004576a8u, b_arena_setup, "arenaSetup");
    rt_bind(0x0044a59cu, b_ground_fires, "groundFiresUpdate");
    rt_bind(0x00444004u, b_weapon_launch, "weaponLaunchProjectile");
    rt_bind(0x0044342cu, b_special_trigger, "special_weapon_trigger");
    rt_bind(0x00453facu, b_warps_setup, "worldWarpsSetup");
    rt_bind(0x00454ae0u, b_collapse_reset, "worldCollapseReset");
    rt_bind(0x0044ad14u, b_roller_update, "rolling_weapon_update");
    rt_bind(0x0044bd3cu, b_tornado_update, "weaponTornadoUpdate");
    rt_bind(0x004401fcu, b_weapon_fire_special, "weapon_fire_special");
}

RT_BIND_AREA(combat_cb2)
