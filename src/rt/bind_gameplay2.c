/*
 * bind_gameplay2.c: car shake / sway / spin / view, Mr. Slam's grab and swing, streaks, missile
 * trails and steering, car damage and wreck, projectile hit test, the AI car update, the Dark
 * Tooth weapon logic, world object loading, the font init, the HUD and score overlay
 * (src/gameplay/gp_shake.c .. gp4_score.c), called the way tools/emu/diff_gameplay_*.py and
 * diff_gp2_* / diff_gp3_* / diff_gp4_*.py called them: registers EAX, EDX, EBX, ECX then the stack
 * words; the context struct's globals as rt_ptr(<original address>), scalar copies read fresh on
 * every call; each callback an rt_call of the address the harness stubbed, with the registers that
 * stub read (and the stack words it popped). One WMem region over the whole address space.
 *
 * Real equivalents of emulator only interfaces:
 *   - a callback that gets a host pointer to a C local of the port (a point, an angle triple, an
 *     &out, a string) passes the original a game address: in place when the pointer is in game
 *     memory, else a copy on the frame stack (copied back, only the bytes the port's local has,
 *     for outputs);
 *   - the generic call tables (gp3_aicar's fn[], gp3_boss / gp4_hud's call with `local`
 *     arguments) are mapped per callee onto its registers and stack words from the harness's
 *     callee tables; the HUD's stack primitive handed to 0x004967f4 (the draw list keeps it) and
 *     the other `local` arguments of those two get a fixed scratch block on the game heap, so the
 *     address lives as long as the original's stack slot did;
 *   - the ports' tm2math ticks / fps are set from 0x004e8300 / 0x004e8304 before each call, the
 *     tables loaded once (TM2_DATA, default disc/files). rand: see bind_ai.c.
 * Port code, not a decompiled function.
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "gameplay/gp2_damage.h"
#include "gameplay/gp2_grab.h"
#include "gameplay/gp2_phit.h"
#include "gameplay/gp2_swing.h"
#include "gameplay/gp2_wreck.h"
#include "gameplay/gp3_aicar.h"
#include "gameplay/gp3_boss.h"
#include "gameplay/gp3_wlo.h"
#include "gameplay/gp4_font.h"
#include "gameplay/gp4_hud.h"
#include "gameplay/gp4_score.h"
#include "gameplay/gp_shake.h"
#include "gameplay/gp_slam.h"
#include "gameplay/gp_spin.h"
#include "gameplay/gp_spinout.h"
#include "gameplay/gp_steer.h"
#include "gameplay/gp_streaks.h"
#include "gameplay/gp_sway.h"
#include "gameplay/gp_trails.h"
#include "gameplay/gp_view.h"
#include "math/tm2math.h"

/* ---- helpers ---- */

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

static WMem s_mem;
static const WMem* wm(void)
{
    s_mem.n = 1;
    s_mem.r[0].addr = 0;
    s_mem.r[0].size = RT_SIZE;
    s_mem.r[0].host = rt_base;
    return &s_mem;
}

static uint8_t* hp(uint32_t a) { return a ? rt_ptr(a) : NULL; }
static int in_game(const void* p, uint32_t n)
{
    uintptr_t q = (uintptr_t)p, b = (uintptr_t)rt_base;
    return q >= b && q + n <= b + RT_SIZE;
}
static uint32_t ga(const void* p) { return p ? rt_addr(p) : 0; }

/* a host buffer as a game address: in place when in game memory, else a frame copy */
typedef struct { uint32_t addr, fr, n; void* host; } Tmp;
static Tmp tin(const void* p, uint32_t n)
{
    Tmp t = {0, 0, n, (void*)(uintptr_t)p};
    if (!p) return t;
    if (in_game(p, n)) { t.addr = rt_addr(p); return t; }
    t.fr = n;
    t.addr = rt_frame_push(n);
    memcpy(rt_ptr(t.addr), p, n);
    return t;
}
static void tdone(Tmp* t, int back)
{
    if (!t->fr) return;
    if (back) memcpy(t->host, rt_ptr(t->addr), t->n);
    rt_frame_pop(t->fr);
}

static uint32_t c0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static uint32_t c1(uint32_t fn, uint32_t a0) { uint32_t a[1] = {a0}; return rt_call(fn, a, 1); }
static uint32_t c2(uint32_t fn, uint32_t a0, uint32_t a1) { uint32_t a[2] = {a0, a1}; return rt_call(fn, a, 2); }
static uint32_t c3(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2) { uint32_t a[3] = {a0, a1, a2}; return rt_call(fn, a, 3); }
static uint32_t c4(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    uint32_t a[4] = {a0, a1, a2, a3};
    return rt_call(fn, a, 4);
}
static uint32_t c5(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t s0)
{
    uint32_t a[5] = {a0, a1, a2, a3, s0};
    return rt_call(fn, a, 5);
}
/* fn(EAX, local p of n bytes) with the local copied in and (back) out */
static uint32_t c_loc(uint32_t fn, uint32_t a0, void* p, uint32_t n, int back)
{
    Tmp t = tin(p, n);
    uint32_t r = c2(fn, a0, t.addr);
    tdone(&t, back);
    return r;
}

#define A(i) rt_arg(a, n, (i))
#define SP(i) ((int32_t)A(i))

/* the shared car lookups (registers EAX, EDX, EBX as the harnesses' stubs read them) */
static int32_t cb_human_count(void* u) { (void)u; return (int32_t)c0(0x0045afb4u); }
static int32_t cb_ai_count(void* u) { (void)u; return (int32_t)c0(0x0045b078u); }
static uint8_t* cb_human_car_p(void* u, int32_t i) { (void)u; return hp(c1(0x0045afbcu, (uint32_t)i)); }
static uint8_t* cb_ai_car_p(void* u, int32_t i) { (void)u; return hp(c1(0x0045b080u, (uint32_t)i)); }
static uint32_t cb_human_car(void* u, int32_t i) { (void)u; return c1(0x0045afbcu, (uint32_t)i); }
static uint32_t cb_ai_car(void* u, int32_t i) { (void)u; return c1(0x0045b080u, (uint32_t)i); }
static uint32_t cb_human_obj(void* u, int32_t i) { (void)u; return c1(0x0045ae24u, (uint32_t)i); }
static uint32_t cb_ai_obj(void* u, int32_t i) { (void)u; return c1(0x0045ae2cu, (uint32_t)i); }
static uint32_t cb_entity(void* u, int32_t id) { (void)u; return c1(0x0045b0b8u, (uint32_t)id); }
/* 0x0045b0ec(ent, &human, &index) */
static uint8_t cb_entity_car(void* u, uint32_t ent, uint8_t* human, int32_t* index)
{
    (void)u;
    Tmp th = tin(human, 1), ti = tin(index, 4);
    uint8_t r = (uint8_t)c3(0x0045b0ecu, ent, th.addr, ti.addr);
    tdone(&ti, 1);
    tdone(&th, 1);
    return r;
}
static void cb_debris_p(void* u, int32_t kind, const int32_t p[3]) { (void)u; c_loc(0x004516bcu, (uint32_t)kind, (void*)(uintptr_t)p, 12, 0); }
static void cb_fx_p(void* u, uint32_t fn, const int32_t p[3], int32_t zero)
{
    (void)u;
    Tmp t = tin(p, 12);
    c2(fn, t.addr, (uint32_t)zero);
    tdone(&t, 0);
}
static int32_t cb_rand(void* u) { (void)u; return (int32_t)c0(0x004899c2u); }
static void cb_spin_hit(void* u, uint32_t car, uint8_t human, int32_t x, int32_t y) { (void)u; c4(0x00421920u, car, human, (uint32_t)x, (uint32_t)y); }
static int32_t cb_snd_mode(void* u) { (void)u; return (int32_t)c0(0x0046cc54u); }
static void cb_snd_loc0(void* u, uint32_t pos) { (void)u; c1(0x0046f974u, pos); }
static void cb_snd_loc1(void* u, uint32_t pos, int32_t v) { (void)u; c2(0x0046f980u, pos, (uint32_t)v); }
static int32_t cb_snd_vol(void* u) { (void)u; return (int32_t)c0(0x0046f990u); }
static int32_t cb_snd_pan(void* u) { (void)u; return (int32_t)c0(0x0046f988u); }
static void cb_snd_play(void* u, int32_t id, int32_t pan, int32_t vol, int32_t prio) { (void)u; c4(0x0046f8b0u, (uint32_t)id, (uint32_t)pan, (uint32_t)vol, (uint32_t)prio); }
/* 0x004320d0(key, stats, body, a, stack b) */
static void cb_carry(void* u, int32_t key, uint32_t stats, uint32_t body, int32_t x, int32_t y) { (void)u; c5(0x004320d0u, (uint32_t)key, stats, body, (uint32_t)x, (uint32_t)y); }
static int32_t cb_claw_get(void* u) { (void)u; return (int32_t)c0(0x00453ec8u); }
static void cb_claw_set(void* u, int32_t v) { (void)u; c1(0x00453f10u, (uint32_t)v); }
static int32_t cb_arm_get(void* u) { (void)u; return (int32_t)c0(0x00453ec0u); }
static void cb_arm_set(void* u, int32_t v) { (void)u; c1(0x00453ed0u, (uint32_t)v); }
static void cb_spinout(void* u, int32_t key, int32_t x, int32_t y) { (void)u; c3(0x00431488u, (uint32_t)key, (uint32_t)x, (uint32_t)y); }
static void cb_release(void* u, int32_t key, int32_t b34, int32_t front) { (void)u; c3(0x00432340u, (uint32_t)key, (uint32_t)b34, (uint32_t)front); }
static int32_t cb_object_id(void* u, int32_t i, uint8_t human) { (void)u; return (int32_t)c2(0x0045b154u, (uint32_t)i, human); }
/* 0x004210e4(id, amount, pos, ecx, stack owner) */
static void cb_damage(void* u, int32_t id, int32_t amount, uint32_t pos, int32_t ecx, int32_t owner)
{
    (void)u;
    c5(0x004210e4u, (uint32_t)id, (uint32_t)amount, pos, (uint32_t)ecx, (uint32_t)owner);
}
static uint32_t cb_pool_get(void* u) { (void)u; return c0(0x00470d08u); }
static uint32_t cb_actors_get(void* u) { (void)u; return c0(0x00471aecu); }

/* ---- gp_shake.c / gp_sway.c: no context ---- */

/* 0x00420be8(car, kind, axes, frames, stack second) */
static uint32_t b_shake(const uint32_t* a, int n)
{
    host_sync();
    gp_car_shake_start(rt_ptr(A(0)), (uint8_t)A(1), (uint8_t)A(2), (int16_t)A(3), (uint8_t)A(4));
    return 0;
}
/* 0x00420e48(car, second) */
static uint32_t b_sway(const uint32_t* a, int n) { host_sync(); gp_car_body_sway(rt_ptr(A(0)), (uint8_t)A(1)); return 0; }

/* ---- gp_slam.c ---- */

/* 0x00431ce8(car, human) -> al */
static uint32_t b_slam_find(const uint32_t* a, int n)
{
    static GpSlamEnv e;
    host_sync();
    e.mem = wm();
    e.user = NULL;
    e.human_count = cb_human_count;
    e.ai_count = cb_ai_count;
    e.human_car = cb_human_car_p;
    e.ai_car = cb_ai_car_p;
    return gp_slam_find_target(&e, rt_ptr(A(0)), (uint8_t)A(1));
}

/* ---- gp_spin.c ---- */

static int32_t sp_event(void* u) { (void)u; return (int32_t)c0(0x00474508u); }
/* 0x00421920(car, human, kind, keep_speed) */
static uint32_t b_spin_hit(const uint32_t* a, int n)
{
    static GpSpinHitEnv e;
    host_sync();
    e.ticks = (int16_t)rt_r16(0x004e8300u);
    e.parity = (int32_t)rt_r32(0x00a0364cu);
    e.user = NULL;
    e.event_type = sp_event;
    gp_car_spin_hit(&e, rt_ptr(A(0)), (uint8_t)A(1), (uint8_t)A(2), (uint8_t)A(3));
    return 0;
}

/* ---- gp_spinout.c ---- */

static void so_spin(void* u, uint8_t* car, int32_t human, int32_t spin, int32_t zero) { (void)u; c4(0x00421920u, ga(car), (uint32_t)human, (uint32_t)spin, (uint32_t)zero); }
/* 0x00431488(id, spin, fx) */
static uint32_t b_spinout(const uint32_t* a, int n)
{
    static GpSpinEnv e;
    host_sync();
    e.user = NULL;
    e.entity = cb_entity;
    e.entity_car = cb_entity_car;
    e.human_car = cb_human_car_p;
    e.ai_car = cb_ai_car_p;
    e.spin = so_spin;
    e.debris = cb_debris_p;
    e.fx = cb_fx_p;
    gp_car_spin_out_by_object(&e, (int16_t)A(0), SP(1), SP(2));
    return 0;
}

/* ---- gp_steer.c: the harness gave no callbacks, trail frames 0, split 0 ---- */

/* 0x004147c8(slot) */
static uint32_t b_steer(const uint32_t* a, int n)
{
    static ProjEnv e;
    host_sync();
    memset(&e, 0, sizeof e);
    e.mem = wm();
    e.slots = rt_ptr(PROJ_SLOTS_ADDR);
    e.models = (uint32_t*)(void*)rt_ptr(PROJ_MODELS_ADDR);
    e.trail_frames = rt_r32(0x004f11d8u);
    e.split = (int32_t)rt_r32(0x00520e90u);
    e.snd_volume = (const int32_t*)(const void*)rt_ptr(0x00c8500cu);
    e.snd_pan = (const int32_t*)(const void*)rt_ptr(0x00c85058u);
    gp_proj_steer(&e, rt_ptr(A(0)));
    return 0;
}

/* ---- gp_streaks.c: the port's light block (a local of the original) lives in one frame for the
 * whole call; each callback copies it in and back ---- */

static uint32_t s_light;
static uint8_t* s_light_host;
#define LIGHT_SIZE 0x44u
static uint32_t lin(uint8_t* l)
{
    if (in_game(l, LIGHT_SIZE)) return rt_addr(l);
    s_light_host = l;
    memcpy(rt_ptr(s_light), l, LIGHT_SIZE);
    return s_light;
}
static void lout(uint8_t* l) { if (!in_game(l, LIGHT_SIZE)) memcpy(l, rt_ptr(s_light), LIGHT_SIZE); }
static void st_li(void* u, uint8_t* l, int32_t x) { (void)u; c2(0x0048e468u, lin(l), (uint32_t)x); lout(l); }
static void st_lm(void* u, uint8_t* l, int32_t x, int32_t y) { (void)u; c3(0x0048e6d4u, lin(l), (uint32_t)x, (uint32_t)y); lout(l); }
static void st_ff(void* u, uint32_t v) { (void)u; c1(0x00415c8cu, v); }
static void st_pa(void* u, uint32_t v, uint32_t x) { (void)u; c2(0x00448b1cu, v, x); }
static void st_pb(void* u) { (void)u; c0(0x00449424u); }
static void st_pc(void* u, uint32_t v, uint32_t x) { (void)u; c2(0x00447748u, v, x); }
/* 0x0048e810 / 0x0048e7b8(l, i, x, y, stack z) */
static void st_ld(void* u, uint8_t* l, int32_t i, int32_t x, int32_t y, int32_t z) { (void)u; c5(0x0048e810u, lin(l), (uint32_t)i, (uint32_t)x, (uint32_t)y, (uint32_t)z); lout(l); }
static void st_lc(void* u, uint8_t* l, int32_t i, int32_t x, int32_t y, int32_t z) { (void)u; c5(0x0048e7b8u, lin(l), (uint32_t)i, (uint32_t)x, (uint32_t)y, (uint32_t)z); lout(l); }
static void st_draw(void* u, uint32_t v, uint8_t* l) { (void)u; c2(0x00491698u, v, lin(l)); lout(l); }

/* 0x0041321c(view, arg) */
static uint32_t b_streaks(const uint32_t* a, int n)
{
    static GpStreakEnv e;
    e.mem = wm();
    e.streaks = rt_ptr(0x004f0910u);
    e.flash_frames = (const int32_t*)(const void*)rt_ptr(0x004f5df0u);
    e.user = NULL;
    e.light_init = st_li;
    e.light_mode = st_lm;
    e.flash_fade = st_ff;
    e.pass_a = st_pa;
    e.pass_b = st_pb;
    e.pass_c = st_pc;
    e.light_dir = st_ld;
    e.light_col = st_lc;
    e.draw = st_draw;
    uint32_t saved = s_light;
    s_light = rt_frame_push(LIGHT_SIZE);
    gp_streaks_draw(&e, A(0), A(1));
    rt_frame_pop(LIGHT_SIZE);
    s_light = saved;
    return 0;
}

/* ---- gp_trails.c ---- */

static uint32_t tr_new(void* u) { (void)u; return c0(0x00492d90u); }
static void tr_add(void* u, uint32_t obj) { (void)u; c1(0x00492c4cu, obj); }
static uint32_t b_trails(const uint32_t* a, int n)
{
    (void)a; (void)n;
    static GpTrailEnv e;
    e.mem = wm();
    e.slots = rt_ptr(0x004f11dcu);
    e.slots_addr = 0x004f11dcu;
    e.user = NULL;
    e.obj_new = tr_new;
    e.obj_add = tr_add;
    gp_missile_trails_init(&e);
    return 0;
}

/* ---- gp_view.c ---- */

/* 0x0045b1e8(a, pos, ang): pos / ang are locals of the port */
static void vw_place(void* u, int32_t x, const int32_t pos[3], const int16_t ang[3])
{
    (void)u;
    Tmp tp = tin(pos, 12), ta = tin(ang, 6);
    c3(0x0045b1e8u, (uint32_t)x, tp.addr, ta.addr);
    tdone(&ta, 0);
    tdone(&tp, 0);
}
static const uint8_t* vw_watched(void* u) { (void)u; return hp(c0(0x0045b0a0u)); }
static void vw_pos_real(void* u, const uint8_t* p, int32_t v)
{
    (void)u;
    Tmp t = tin(p, 12);
    c2(0x00494d34u, t.addr, (uint32_t)v);
    tdone(&t, 0);
}
static void vw_rot(void* u, const uint8_t* p, int32_t v)
{
    (void)u;
    Tmp t = tin(p, 8);
    c2(0x00494cdcu, t.addr, (uint32_t)v);
    tdone(&t, 0);
}
/* 0x004225a0(car, mode) */
static uint32_t b_view(const uint32_t* a, int n)
{
    static GpViewEnv e;
    host_sync();
    e.user = NULL;
    e.place = vw_place;
    e.watched = vw_watched;
    e.view_pos = vw_pos_real;
    e.view_rot = vw_rot;
    gp_car_set_view_mode(&e, rt_ptr(A(0)), (int8_t)A(1));
    return 0;
}

/* ---- gp2_damage.c ---- */

static uint32_t dm_attacker(void* u, int32_t slot, uint8_t* kind) { (void)u; return c_loc(0x0045b028u, (uint32_t)slot, kind, 1, 1); }
static uint8_t dm_find(void* u, int32_t id, uint8_t* human, int32_t* idx)
{
    (void)u;
    Tmp th = tin(human, 1), ti = tin(idx, 4);
    uint8_t r = (uint8_t)c3(0x0045b178u, (uint32_t)id, th.addr, ti.addr);
    tdone(&ti, 1);
    tdone(&th, 1);
    return r;
}
static void dm_knock(void* u, uint32_t car, uint8_t human, uint32_t pos, int32_t push) { (void)u; c4(0x00421800u, car, human, pos, (uint32_t)push); }
static void dm_fburst(void* u, uint32_t pos, int32_t z) { (void)u; c2(0x00450c08u, pos, (uint32_t)z); }
static void dm_fglow(void* u, uint32_t pos, int32_t z) { (void)u; c2(0x004512f8u, pos, (uint32_t)z); }
static uint8_t dm_f94(void* u) { (void)u; return (uint8_t)c0(0x0045bb50u); }
/* 0x00453b10(obj, rate, rgb, amount) */
static void dm_flicker(void* u, uint32_t obj, int32_t rate, const uint8_t rgb[3], int32_t amount)
{
    (void)u;
    Tmp t = tin(rgb, 3);
    c4(0x00453b10u, obj, (uint32_t)rate, t.addr, (uint32_t)amount);
    tdone(&t, 0);
}
static void dm_debris(void* u, int32_t k, uint32_t pos) { (void)u; c2(0x004516bcu, (uint32_t)k, pos); }
static uint8_t dm_single(void* u) { (void)u; return (uint8_t)c0(0x0045c348u); }
static uint8_t dm_multi(void* u) { (void)u; return (uint8_t)c0(0x004632b0u); }
static void dm_hide(void* u, int32_t t, int32_t z) { (void)u; c2(0x00453c38u, (uint32_t)t, (uint32_t)z); }
static int32_t dm_alive(void* u) { (void)u; return (int32_t)c0(0x0045bab8u); }
static void dm_hud(void* u, int32_t x, int32_t y) { (void)u; c2(0x00436040u, (uint32_t)x, (uint32_t)y); }
static uint8_t dm_event(void* u) { (void)u; return (uint8_t)c0(0x00474508u); }
static int32_t dm_mode(void* u) { (void)u; return (int32_t)c0(0x0045c338u); }
static int32_t dm_near(void* u, uint32_t pos, int32_t skip) { (void)u; return (int32_t)c2(0x0045acb8u, pos, (uint32_t)skip); }
static uint8_t dm_boss(void* u, uint32_t car) { (void)u; return (uint8_t)c1(0x0045a8bcu, car); }
static uint32_t dm_tfield(void* u) { (void)u; return c0(0x0044df04u); }
static uint32_t dm_tobj(void* u) { (void)u; return c0(0x0044df10u); }
/* 0x0048a738(node, pos, dir, spin, stack drift, hold, scale, end, rnd), ret 0x14 */
static void dm_burst(void* u, uint32_t node, uint32_t pos, const int16_t dir[3], const int16_t spin[3],
                     const int32_t drift[3], int32_t hold, int32_t scale, int32_t end, int32_t rnd)
{
    (void)u;
    Tmp td = tin(dir, 6), ts = tin(spin, 6), tr = tin(drift, 12);
    uint32_t x[9] = {node, pos, td.addr, ts.addr, tr.addr, (uint32_t)hold, (uint32_t)scale, (uint32_t)end, (uint32_t)rnd};
    rt_call(0x0048a738u, x, 9);
    tdone(&tr, 0);
    tdone(&ts, 0);
    tdone(&td, 0);
}
static void dm_expl(void* u, uint32_t pos, int32_t z) { (void)u; c2(0x00450aa0u, pos, (uint32_t)z); }
static void dm_smoke(void* u, uint32_t pos, int32_t z) { (void)u; c2(0x0045109cu, pos, (uint32_t)z); }
static uint32_t dm_cobj(void* u, int32_t t) { (void)u; return c1(0x00453f70u, (uint32_t)t); }
/* 0x0048a488(obj, node, target, dir, stack scale, life), ret 8 */
static void dm_break(void* u, uint32_t obj, uint32_t node, const int32_t target[3], const int16_t dir[3],
                     int32_t scale, int32_t life)
{
    (void)u;
    Tmp tt = tin(target, 12), td = tin(dir, 6);
    uint32_t x[6] = {obj, node, tt.addr, td.addr, (uint32_t)scale, (uint32_t)life};
    rt_call(0x0048a488u, x, 6);
    tdone(&td, 0);
    tdone(&tt, 0);
}
static void dm_choice(void* u, int32_t t, int32_t s, int32_t b) { (void)u; c3(0x0047068cu, (uint32_t)t, (uint32_t)s, (uint32_t)b); }
static void dm_smoke2(void* u, uint32_t pos, int32_t z) { (void)u; c2(0x00451058u, pos, (uint32_t)z); }

/* 0x004210e4(obj, amount, pos, kind, stack attacker) */
static uint32_t b_damage(const uint32_t* a, int n)
{
    static Gp2DamageEnv e;
    host_sync();
    e.mem = wm();
    e.user = NULL;
    e.attacker_car = dm_attacker;
    e.find_by_id = dm_find;
    e.human_car = cb_human_car;
    e.ai_car = cb_ai_car;
    e.human_obj = cb_human_obj;
    e.ai_obj = cb_ai_obj;
    e.knock_back = dm_knock;
    e.freeze_burst = dm_fburst;
    e.freeze_glow = dm_fglow;
    e.flag_502f94 = dm_f94;
    e.flicker = dm_flicker;
    e.rand = cb_rand;
    e.spin_hit = cb_spin_hit;
    e.debris = dm_debris;
    e.singleplayer = dm_single;
    e.multiplayer = dm_multi;
    e.body_hide = dm_hide;
    e.alive_count = dm_alive;
    e.hud_msg = dm_hud;
    e.event_type = dm_event;
    e.game_mode = dm_mode;
    e.nearest_player = dm_near;
    e.boss_defeated = dm_boss;
    e.target_field = dm_tfield;
    e.target_obj = dm_tobj;
    e.burst = dm_burst;
    e.explosion = dm_expl;
    e.smoke = dm_smoke;
    e.car_object = dm_cobj;
    e.break_model = dm_break;
    e.choice_state = dm_choice;
    e.smoke_small = dm_smoke2;
    gp2_car_damage(&e, SP(0), SP(1), A(2), SP(3), (int16_t)A(4));
    return 0;
}

/* ---- gp2_grab.c / gp2_swing.c ---- */

static void gr_find(void* u, uint32_t car, uint8_t human) { (void)u; c2(0x00431ce8u, car, human); }
static uint8_t sw_find(void* u, uint32_t car, uint8_t human) { (void)u; return (uint8_t)c2(0x00431ce8u, car, human); }

/* 0x004316b4(car, human) */
static uint32_t b_grab(const uint32_t* a, int n)
{
    static Gp2GrabEnv e;
    host_sync();
    e.mem = wm();
    e.user = NULL;
    e.sound_mode = cb_snd_mode;
    e.sound_locate0 = cb_snd_loc0;
    e.sound_locate1 = cb_snd_loc1;
    e.sound_vol = cb_snd_vol;
    e.sound_pan = cb_snd_pan;
    e.sound_play = cb_snd_play;
    e.carry = cb_carry;
    e.find_target = gr_find;
    e.claw_get = cb_claw_get;
    e.claw_set = cb_claw_set;
    e.arm_get = cb_arm_get;
    e.arm_set = cb_arm_set;
    e.spin = cb_spinout;
    e.release = cb_release;
    e.object_from_owner = cb_entity;
    e.find_by_object = cb_entity_car;
    e.human_car = cb_human_car;
    e.ai_car = cb_ai_car;
    e.object_id = cb_object_id;
    e.damage = cb_damage;
    e.rand = cb_rand;
    gp2_slam_grab_update(&e, A(0), (uint8_t)A(1));
    return 0;
}

static void sw_pool_hit(void* u, int32_t id, int32_t cnt, int32_t v) { (void)u; c3(0x00470d3cu, (uint32_t)id, (uint32_t)cnt, (uint32_t)v); }
static void sw_actor_hit(void* u, int32_t i, const int32_t dir[3]) { (void)u; c_loc(0x004723bcu, (uint32_t)i, (void*)(uintptr_t)dir, 12, 0); }

/* 0x00430bb4(car, human) */
static uint32_t b_swing(const uint32_t* a, int n)
{
    static Gp2SwingEnv e;
    host_sync();
    e.mem = wm();
    e.user = NULL;
    e.sound_mode = cb_snd_mode;
    e.sound_locate0 = cb_snd_loc0;
    e.sound_locate1 = cb_snd_loc1;
    e.sound_vol = cb_snd_vol;
    e.sound_pan = cb_snd_pan;
    e.sound_play = cb_snd_play;
    e.carry = cb_carry;
    e.find_target = sw_find;
    e.claw_get = cb_claw_get;
    e.claw_set = cb_claw_set;
    e.arm_get = cb_arm_get;
    e.arm_set = cb_arm_set;
    e.spin = cb_spinout;
    e.release = cb_release;
    e.object_from_owner = cb_entity;
    e.find_by_object = cb_entity_car;
    e.human_car = cb_human_car;
    e.ai_car = cb_ai_car;
    e.object_id = cb_object_id;
    e.damage = cb_damage;
    e.pool_get = cb_pool_get;
    e.pool_hit = sw_pool_hit;
    e.actors_get = cb_actors_get;
    e.actor_hit = sw_actor_hit;
    gp2_slam_swing_update(&e, A(0), (uint8_t)A(1));
    return 0;
}

/* ---- gp2_phit.c ---- */

static uint8_t ph_multi(void* u) { (void)u; return (uint8_t)c0(0x004632b0u); }
static void ph_freeze(void* u, int32_t t, int32_t v) { (void)u; c2(0x00470940u, (uint32_t)t, (uint32_t)v); }
static uint32_t ph_flame(void* u, uint32_t pos, int32_t v) { (void)u; return c2(0x00450de4u, pos, (uint32_t)v); }

/* 0x004126b4(pos, self, radius, kind) -> hit */
static uint32_t b_phit(const uint32_t* a, int n)
{
    static Gp2PhitEnv e;
    host_sync();
    e.mem = wm();
    e.user = NULL;
    e.human_count = cb_human_count;
    e.ai_count = cb_ai_count;
    e.human_obj = cb_human_obj;
    e.human_car = cb_human_car;
    e.ai_obj = cb_ai_obj;
    e.ai_car = cb_ai_car;
    e.multiplayer = ph_multi;
    e.freeze_state = ph_freeze;
    e.flame = ph_flame;
    e.spin_hit = cb_spin_hit;
    e.pool_get = cb_pool_get;
    e.actors_get = cb_actors_get;
    return (uint32_t)gp2_projectile_hit_test(&e, A(0), SP(1), SP(2), SP(3));
}

/* ---- gp2_wreck.c ---- */

static uint32_t wr_target(void* u, int32_t key, uint8_t* human) { (void)u; return c_loc(0x0045afc8u, (uint32_t)key, human, 1, 1); }
static uint32_t wr_flame(void* u, uint32_t pos, int32_t v) { (void)u; return c2(0x00450e24u, pos, (uint32_t)v); }
static uint32_t wr_jitter(void* u, uint32_t pos, int32_t x, int32_t y, int32_t z) { (void)u; return c4(0x0044a40cu, pos, (uint32_t)x, (uint32_t)y, (uint32_t)z); }
static void wr_smoke_small(void* u, uint32_t pos, int32_t z) { (void)u; c2(0x00451058u, pos, (uint32_t)z); }
static void wr_smoke_a2(void* u, uint32_t pos, int32_t z) { (void)u; c2(0x00451014u, pos, (uint32_t)z); }
static uint32_t wr_frame(void* u) { (void)u; return c0(0x004931f4u); }
static void wr_pt(uint32_t fn, const int32_t p[3], int32_t z)
{
    Tmp t = tin(p, 12);
    c2(fn, t.addr, (uint32_t)z);
    tdone(&t, 0);
}
static void wr_expl12(void* u, const int32_t p[3], int32_t z) { (void)u; wr_pt(0x004508f4u, p, z); }
static void wr_expl16(void* u, const int32_t p[3], int32_t z) { (void)u; wr_pt(0x00450984u, p, z); }
static void wr_smoke(void* u, const int32_t p[3], int32_t z) { (void)u; wr_pt(0x0045109cu, p, z); }
static int32_t wr_mode(void* u) { (void)u; return (int32_t)c0(0x0045c338u); }
static void wr_setup(void* u, uint32_t car, int32_t t, int32_t v) { (void)u; c3(0x00437850u, car, (uint32_t)t, (uint32_t)v); }
static int32_t wr_net_msg(void* u, int32_t x, uint32_t y) { (void)u; return (int32_t)c2(0x004735b4u, (uint32_t)x, y); }
static void wr_net_send(void* u, int32_t m) { (void)u; c1(0x00493748u, (uint32_t)m); }
static void wr_net_flush(void* u) { (void)u; c0(0x0049364cu); }
static int32_t wr_approach(void* u, int32_t x, int32_t t, int32_t r) { (void)u; return (int32_t)c3(0x0048ca54u, (uint32_t)x, (uint32_t)t, (uint32_t)r); }

/* 0x00422704(car, human) */
static uint32_t b_wreck(const uint32_t* a, int n)
{
    static Gp2WreckEnv e;
    host_sync();
    e.mem = wm();
    e.user = NULL;
    e.target_car = wr_target;
    e.flame = wr_flame;
    e.rand = cb_rand;
    e.jitter = wr_jitter;
    e.smoke_small = wr_smoke_small;
    e.smoke_a2 = wr_smoke_a2;
    e.frame = wr_frame;
    e.expl12 = wr_expl12;
    e.expl16 = wr_expl16;
    e.smoke = wr_smoke;
    e.object_from_owner = cb_entity;
    e.damage = cb_damage;
    e.game_mode = wr_mode;
    e.car_setup = wr_setup;
    e.net_msg = wr_net_msg;
    e.net_send = wr_net_send;
    e.net_flush = wr_net_flush;
    e.approach = wr_approach;
    gp2_car_wreck_update(&e, A(0), (uint8_t)A(1));
    return 0;
}

/* ---- gp3_aicar.c: fn[k] is callee k of the harness's table (address, register argument count);
 * a host pointer argument into game memory becomes its game address ---- */

static const struct { uint32_t addr; uint8_t nargs; } aic[GP3A_N] = {
    {0x0042c3e0u, 2}, {0x00432940u, 1}, {0x0043a944u, 1}, {0x00470940u, 2}, {0x0045afc8u, 2}, {0x00425674u, 1},
    {0x0045b3dcu, 0}, {0x00474508u, 0}, {0x0045b3fcu, 1}, {0x00427838u, 1}, {0x004899c2u, 0}, {0x00410010u, 1},
    {0x00429fd0u, 1}, {0x00427710u, 1}, {0x004291dcu, 2}, {0x0042ce2cu, 1}, {0x00418cf0u, 2}, {0x0042577cu, 1},
    {0x004334c8u, 2}, {0x004259ecu, 1}, {0x00433000u, 2}, {0x00427dd8u, 1}, {0x0045c348u, 0}, {0x00430414u, 2},
    {0x00432f2cu, 1}, {0x0043e6e0u, 2}, {0x00456c70u, 0}, {0x00421920u, 4}, {0x00430460u, 3}, {0x004295fcu, 2},
    {0x00432ba0u, 1}, {0x00439588u, 1}, {0x00470320u, 3}, {0x004168e4u, 2}, {0x0042c4d4u, 2}, {0x0042a020u, 2},
    {0x004220b8u, 2}, {0x004305fcu, 2}, {0x00420e48u, 2}, {0x0042d470u, 2}, {0x00422704u, 2}, {0x0043953cu, 2},
    {0x00453538u, 2}, {0x00432340u, 3}, {0x004316b4u, 2}, {0x00430bb4u, 2}, {0x00418450u, 2}, {0x0041858cu, 2},
    {0x0045c248u, 2}, {0x0045c2a0u, 0}, {0x00453c78u, 2}, {0x0043af58u, 3}, {0x00418d38u, 2}, {0x00417640u, 2},
};

static uint32_t aic_arg(uintptr_t v) { return in_game((const void*)v, 1) ? rt_addr((const void*)v) : (uint32_t)v; }

#define AIC(k)                                                                                       \
    static uintptr_t aic_##k(void* u, uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3)         \
    {                                                                                                \
        (void)u;                                                                                     \
        return aic_call(k, a0, a1, a2, a3);                                                          \
    }

static uintptr_t aic_call(int k, uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3)
{
    if (k == GP3A_TARGET) {
        /* 0x0045afc8(id, &human): the chased car's block (host pointer for the port) */
        uint8_t* hum = (uint8_t*)a1;
        return (uintptr_t)hp(c_loc(aic[k].addr, (uint32_t)a0, hum, 1, 1));
    }
    if (k == GP3A_430460) {
        /* 0x00430460(car, out, 1): the original writes three words; the port reads the first */
        int32_t* out = (int32_t*)a1;
        uint32_t f = rt_frame_push(12);
        memcpy(rt_ptr(f), out, 4);
        c3(aic[k].addr, aic_arg(a0), f, (uint32_t)a2);
        memcpy(out, rt_ptr(f), 4);
        rt_frame_pop(12);
        return 0;
    }
    uint32_t x[4] = {aic_arg(a0), aic_arg(a1), aic_arg(a2), aic_arg(a3)};
    return rt_call(aic[k].addr, x, aic[k].nargs);
}

AIC(0) AIC(1) AIC(2) AIC(3) AIC(4) AIC(5) AIC(6) AIC(7) AIC(8) AIC(9)
AIC(10) AIC(11) AIC(12) AIC(13) AIC(14) AIC(15) AIC(16) AIC(17) AIC(18) AIC(19)
AIC(20) AIC(21) AIC(22) AIC(23) AIC(24) AIC(25) AIC(26) AIC(27) AIC(28) AIC(29)
AIC(30) AIC(31) AIC(32) AIC(33) AIC(34) AIC(35) AIC(36) AIC(37) AIC(38) AIC(39)
AIC(40) AIC(41) AIC(42) AIC(43) AIC(44) AIC(45) AIC(46) AIC(47) AIC(48) AIC(49)
AIC(50) AIC(51) AIC(52) AIC(53)

static const Gp3AFn aic_fns[] = {
    aic_0, aic_1, aic_2, aic_3, aic_4, aic_5, aic_6, aic_7, aic_8, aic_9,
    aic_10, aic_11, aic_12, aic_13, aic_14, aic_15, aic_16, aic_17, aic_18, aic_19,
    aic_20, aic_21, aic_22, aic_23, aic_24, aic_25, aic_26, aic_27, aic_28, aic_29,
    aic_30, aic_31, aic_32, aic_33, aic_34, aic_35, aic_36, aic_37, aic_38, aic_39,
    aic_40, aic_41, aic_42, aic_43, aic_44, aic_45, aic_46, aic_47, aic_48, aic_49,
    aic_50, aic_51, aic_52, aic_53,
};
typedef char aic_count_check[(sizeof aic_fns / sizeof aic_fns[0] == GP3A_N) ? 1 : -1];

static uint8_t* res_cb(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

/* 0x00422e08(car) */
static uint32_t b_ai_car_update(const uint32_t* a, int n)
{
    static Gp3AiCarEnv e;
    host_sync();
    e.grp = rt_ptr(0x004fca18u);
    e.nodes = rt_ptr(0x004fe1e4u);
    e.resolve = res_cb;
    for (int k = 0; k < GP3A_N; k++) e.fn[k] = aic_fns[k];
    e.user = NULL;
    gp3_ai_car_update(&e, rt_ptr(A(0)));
    return 0;
}

/* ---- the call(addr, n, args, local, local2) ports: gp3_boss, gp4_hud ----
 * Per callee: the registers it reads (a = EAX, d = EDX, b = EBX, c = ECX, in argument order), its
 * stack word count, and which arguments are the caller's locals (with the bytes the port's local
 * has). Locals get fixed scratch on the game heap (see the file comment). */

typedef struct {
    uint32_t addr;
    const char* regs;
    uint8_t ns;
    int8_t li[2];
    uint8_t lsz[2];
} Callee;

static const Callee boss_callees[] = {
    {0x004469bcu, "adb", 0, {-1, -1}, {0, 0}}, {0x004899c2u, "", 0, {-1, -1}, {0, 0}},
    {0x00474508u, "", 0, {-1, -1}, {0, 0}}, {0x00449048u, "adb", 0, {1, -1}, {4, 0}},
    {0x00448de4u, "adbc", 0, {1, -1}, {4, 0}}, {0x0045ae2cu, "a", 0, {-1, -1}, {0, 0}},
    {0x0045ae24u, "a", 0, {-1, -1}, {0, 0}}, {0x0045afbcu, "a", 0, {-1, -1}, {0, 0}},
    {0x004142acu, "adbc", 1, {2, 3}, {12, 12}}, {0x0045a8bcu, "a", 0, {-1, -1}, {0, 0}},
    {0x0046f974u, "a", 0, {-1, -1}, {0, 0}}, {0x0046f990u, "", 0, {-1, -1}, {0, 0}},
    {0x0046f988u, "", 0, {-1, -1}, {0, 0}}, {0x0046f8b0u, "adbc", 0, {-1, -1}, {0, 0}},
    {0x0045acb8u, "ad", 0, {-1, -1}, {0, 0}}, {0x00450984u, "ad", 0, {-1, -1}, {0, 0}},
    {0x004210e4u, "adbc", 1, {-1, -1}, {0, 0}}, {0x00450de4u, "ad", 0, {-1, -1}, {0, 0}},
    {0x00451014u, "adc", 0, {-1, -1}, {0, 0}}, {0x004508f4u, "ad", 0, {-1, -1}, {0, 0}},
    {0x0048df8cu, "adbc", 0, {3, -1}, {4, 0}}, {0x004401fcu, "ad", 0, {-1, -1}, {0, 0}},
    {0x0044342cu, "ad", 0, {-1, -1}, {0, 0}}, {0x00443c9cu, "ad", 0, {-1, -1}, {0, 0}},
    {0x0046f860u, "ad", 0, {-1, -1}, {0, 0}}, {0x0046f898u, "ad", 0, {-1, -1}, {0, 0}},
    {0x0046feb0u, "", 0, {-1, -1}, {0, 0}},
};

static const Callee hud_callees[] = {
    {0x0045c348u, "", 0, {-1, -1}, {0, 0}}, {0x0046be2cu, "", 0, {-1, -1}, {0, 0}},
    {0x004958acu, "", 0, {-1, -1}, {0, 0}}, {0x004938a4u, "ad", 0, {0, 1}, {4, 4}},
    {0x0045afbcu, "a", 0, {-1, -1}, {0, 0}}, {0x004361d4u, "ad", 0, {-1, -1}, {0, 0}},
    {0x0045bab8u, "", 0, {-1, -1}, {0, 0}}, {0x00452d28u, "adbc", 0, {-1, -1}, {0, 0}},
    {0x00452204u, "adbc", 3, {5, -1}, {4, 0}}, {0x0045222cu, "adbc", 3, {-1, -1}, {0, 0}},
    {0x00452d48u, "a", 0, {-1, -1}, {0, 0}}, {0x00429f7cu, "ad", 0, {1, -1}, {1, 0}},
    {0x0045bb20u, "a", 0, {-1, -1}, {0, 0}}, {0x00436120u, "ad", 0, {-1, -1}, {0, 0}},
    {0x004967f4u, "a", 0, {0, -1}, {0x18, 0}}, {0x004632b0u, "", 0, {-1, -1}, {0, 0}},
    {0x00474500u, "", 0, {-1, -1}, {0, 0}}, {0x00474510u, "a", 0, {-1, -1}, {0, 0}},
    {0x00451a5cu, "adbc", 0, {-1, -1}, {0, 0}}, {0x00496dccu, "adb", 0, {-1, -1}, {0, 0}},
    {0x0045aee8u, "a", 0, {-1, -1}, {0, 0}}, {0x00437134u, "", 0, {-1, -1}, {0, 0}},
    {0x0044eee4u, "a", 0, {-1, -1}, {0, 0}}, {0x004931dcu, "", 0, {-1, -1}, {0, 0}},
    {0x004958e4u, "", 0, {-1, -1}, {0, 0}}, {0x004958d8u, "", 0, {-1, -1}, {0, 0}},
};

/* scratch for the locals: 2 blocks of 0x20 per table, allocated once on the game heap */
static uint32_t scratch(int table, int i)
{
    static uint32_t s[2];
    if (!s[table]) {
        s[table] = rt_heap_grow(0x40);
        if (!s[table]) rt_stop("heap full", 0x40);
    }
    return s[table] + 0x20u * (uint32_t)i;
}

static uint32_t table_call(int table, const Callee* tab, int ntab, uint32_t addr, int n, const uint32_t* args,
                           void* l1, void* l2)
{
    const Callee* c = NULL;
    for (int i = 0; i < ntab; i++)
        if (tab[i].addr == addr) { c = &tab[i]; break; }
    uint32_t v[16] = {0};
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++) v[i] = args[i];
    void* loc[2] = {l1, l2};
    uint32_t la[2] = {0, 0};
    if (c) {
        for (int k = 0; k < 2; k++) {
            if (c->li[k] < 0 || !loc[k] || c->li[k] >= n) continue;
            la[k] = scratch(table, k);
            memcpy(rt_ptr(la[k]), loc[k], c->lsz[k]);
            v[c->li[k]] = la[k];
        }
    }
    uint32_t x[4 + 12] = {0};
    int nx;
    if (c) {
        int nr = (int)strlen(c->regs);
        for (int i = 0; i < nr && i < n; i++) {
            int r = c->regs[i] == 'a' ? 0 : c->regs[i] == 'd' ? 1 : c->regs[i] == 'b' ? 2 : 3;
            x[r] = v[i];
        }
        for (int i = 0; i < c->ns && nr + i < n; i++) x[4 + i] = v[nr + i];
        nx = 0;
        for (int i = 0; i < nr; i++) {
            int r = c->regs[i] == 'a' ? 1 : c->regs[i] == 'd' ? 2 : c->regs[i] == 'b' ? 3 : 4;
            if (r > nx) nx = r;
        }
        if (c->ns) nx = 4 + c->ns;
    } else {
        /* not in the harness's table (an import such as timeGetTime): the words in order */
        for (int i = 0; i < n; i++) x[i] = v[i];
        nx = n;
    }
    uint32_t r = rt_call(addr, x, nx);
    if (c)
        for (int k = 0; k < 2; k++)
            if (la[k]) memcpy(loc[k], rt_ptr(la[k]), c->lsz[k]);
    return r;
}

static uint8_t* mem_cb(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t boss_call(void* u, uint32_t addr, int n, const uint32_t* args, void* l1, void* l2)
{
    (void)u;
    return table_call(0, boss_callees, (int)(sizeof boss_callees / sizeof boss_callees[0]), addr, n, args, l1, l2);
}
static uint32_t hud_call(void* u, uint32_t addr, int n, const uint32_t* args, void* l1, void* l2)
{
    (void)u;
    return table_call(1, hud_callees, (int)(sizeof hud_callees / sizeof hud_callees[0]), addr, n, args, l1, l2);
}

/* 0x0043e6e0(car, human) */
static uint32_t b_boss(const uint32_t* a, int n)
{
    static Gp3BossEnv e;
    host_sync();
    e.g_count = rt_ptr(0x004e69d4u);
    e.g_bomb = rt_ptr(0x004fef28u);
    e.g_bomb_addr = 0x004fef28u;
    e.g_obj = rt_ptr(0x004ffed4u);
    e.mem = mem_cb;
    e.call = boss_call;
    e.user = NULL;
    gp3_boss_update(&e, A(0), (uint8_t)A(1));
    return 0;
}

/* 0x00434710(view) */
static uint32_t b_hud_draw(const uint32_t* a, int n)
{
    static Gp4HudEnv e;
    host_sync();
    e.mem = mem_cb;
    e.call = hud_call;
    e.user = NULL;
    gp4_hud_draw(&e, SP(0));
    return 0;
}

/* ---- gp3_wlo.c / gp4_font.c: call(addr, EAX, EDX, EBX) ---- */

static uint32_t call3_cb(void* u, uint32_t addr, uint32_t x, uint32_t y, uint32_t z) { (void)u; return c3(addr, x, y, z); }

/* 0x0044f20c(type, rec, ctx) */
static uint32_t b_world_load(const uint32_t* a, int n)
{
    static Gp3WloEnv e;
    host_sync();
    e.scene_root_done = (uint32_t*)(void*)rt_ptr(0x00500014u);
    e.last_record = (uint32_t*)(void*)rt_ptr(0x0050001cu);
    e.object_counter = (int32_t*)(void*)rt_ptr(0x00500020u);
    e.collision_counter = (int32_t*)(void*)rt_ptr(0x00500024u);
    e.resolve = res_cb;
    e.call = call3_cb;
    e.user = NULL;
    gp3_world_load_object(&e, SP(0), A(1), A(2));
    return 0;
}

/* 0x0045289c() */
static uint32_t b_font_init(const uint32_t* a, int n)
{
    (void)a; (void)n;
    static Gp4FontEnv e;
    e.g = rt_ptr(0x005024d0u);
    e.pages = rt_ptr(0x00a30c48u);
    e.page_of = (const int32_t*)(const void*)rt_ptr(0x004e73d4u);
    e.desc = (const uint32_t*)(const void*)rt_ptr(0x004e73f4u);
    e.mem = mem_cb;
    e.call = call3_cb;
    e.user = NULL;
    return gp4_font_init(&e);
}

/* ---- gp4_score.c ---- */

static uint32_t sc_call(void* u, uint32_t addr, uint32_t x, uint32_t y) { (void)u; return c2(addr, x, y); }
/* the port's strings are its locals: a frame copy */
static uint32_t str_in(const char* s, uint32_t* fr)
{
    uint32_t len = (uint32_t)strlen(s) + 1;
    if (in_game(s, len)) { *fr = 0; return rt_addr(s); }
    *fr = len;
    uint32_t g = rt_frame_push(len);
    memcpy(rt_ptr(g), s, len);
    return g;
}
static void sc_draw(void* u, int32_t font, int32_t x, int32_t y, const char* s)
{
    (void)u;
    uint32_t fr, g = str_in(s, &fr);
    c4(0x00451a5cu, (uint32_t)font, (uint32_t)x, (uint32_t)y, g);
    if (fr) rt_frame_pop(fr);
}
static int32_t sc_width(void* u, int32_t font, const char* s)
{
    (void)u;
    uint32_t fr, g = str_in(s, &fr);
    int32_t r = (int32_t)c2(0x00451a9cu, (uint32_t)font, g);
    if (fr) rt_frame_pop(fr);
    return r;
}
static int32_t sc_cw(void* u, int32_t font, uint8_t ch) { (void)u; return (int32_t)c2(0x00451bf0u, (uint32_t)font, ch); }

/* 0x00437134() */
static uint32_t b_score(const uint32_t* a, int n)
{
    (void)a; (void)n;
    static Gp4ScoreEnv e;
    e.mode = rt_ptr(0x004f6a38u);
    e.scores = rt_ptr(0x00624db8u);
    e.list_arg = (const int32_t*)(const void*)rt_ptr(0x0050f9dcu);
    e.names = (const uint32_t*)(const void*)rt_ptr(0x004e6870u);
    e.k = (const float*)(const void*)rt_ptr(0x004e002au);
    e.mem = mem_cb;
    e.call = sc_call;
    e.draw = sc_draw;
    e.width = sc_width;
    e.char_width = sc_cw;
    e.user = NULL;
    gp4_score_draw(&e);
    return 0;
}

void rt_bind_gameplay2(void)
{
    rt_bind(0x00420be8u, b_shake, "car_shake_start");
    rt_bind(0x00420e48u, b_sway, "car_body_sway");
    rt_bind(0x00431ce8u, b_slam_find, "slam_find_target");
    rt_bind(0x00421920u, b_spin_hit, "car_spin_hit");
    rt_bind(0x00431488u, b_spinout, "car_spin_out_by_object");
    rt_bind(0x004147c8u, b_steer, "proj_steer");
    rt_bind(0x0041321cu, b_streaks, "streaks_draw");
    rt_bind(0x004133d0u, b_trails, "missile_trails_init");
    rt_bind(0x004225a0u, b_view, "car_set_view_mode");
    rt_bind(0x004210e4u, b_damage, "car_damage");
    rt_bind(0x004316b4u, b_grab, "weaponSlamGrabUpdate");
    rt_bind(0x004126b4u, b_phit, "projectileHitTest");
    rt_bind(0x00430bb4u, b_swing, "weaponSlamSwingUpdate");
    rt_bind(0x00422704u, b_wreck, "car_wreck_update");
    rt_bind(0x00422e08u, b_ai_car_update, "ai_car_update");
    rt_bind(0x0043e6e0u, b_boss, "aiDarkToothBossUpdate");
    rt_bind(0x0044f20cu, b_world_load, "worldLoadObject");
    rt_bind(0x0045289cu, b_font_init, "fontInit");
    rt_bind(0x00434710u, b_hud_draw, "hudDrawSingleView");
    rt_bind(0x00437134u, b_score, "score_overlay_draw");
}

RT_BIND_AREA(gameplay2)
