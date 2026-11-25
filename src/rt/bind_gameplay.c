/*
 * bind_gameplay.c: arena landmarks, effects resets, HUD, radar, zone tiles, missile variants,
 * AI fire decision, spawn picks, crush contact (src/gameplay/gp_landmark.c .. gp_crush.c), called
 * the way tools/emu/diff_gameplay_*.py called them: registers EAX, EDX, EBX, ECX then the stack
 * words; the context struct's globals as rt_ptr(<original address>), scalar copies of globals read
 * fresh on every call; each callback an rt_call of the address the harness stubbed, with the
 * registers that stub read. One WMem region over the whole address space.
 *
 * Real equivalents of emulator only interfaces:
 *   - a callback that gets a host pointer to a C local of the port (a point, an &out) passes the
 *     original a game address: in place when the pointer is in game memory, else a copy on the
 *     frame stack (copied back for outputs);
 *   - the ports' tm2math frame ticks / fps are set from 0x004e8300 / 0x004e8304 before each call,
 *     its tables loaded once (TM2_DATA, default disc/files). rand: see bind_ai.c.
 * Port code, not a decompiled function.
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "gameplay/gp_aifire.h"
#include "gameplay/gp_cars.h"
#include "gameplay/gp_crush.h"
#include "gameplay/gp_hud.h"
#include "gameplay/gp_landmark.h"
#include "gameplay/gp_misc.h"
#include "gameplay/gp_nodes.h"
#include "gameplay/gp_nullsubs.h"
#include "gameplay/gp_proj.h"
#include "gameplay/gp_radar.h"
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
static uint32_t ga(const void* p) { return p ? rt_addr(p) : 0; }

/* a host buffer as a game address: in place when in game memory, else a frame copy */
typedef struct { uint32_t addr, fr, n; void* host; } Tmp;
static Tmp tin(const void* p, uint32_t n)
{
    Tmp t = {0, 0, n, (void*)(uintptr_t)p};
    uintptr_t q = (uintptr_t)p, b = (uintptr_t)rt_base;
    if (!p) return t;
    if (q >= b && q + n <= b + RT_SIZE) { t.addr = (uint32_t)(q - b); return t; }
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

#define A(i) rt_arg(a, n, (i))
#define SP(i) ((int32_t)A(i))

/* ---- gp_landmark.c ---- */

static void lm_section(void* u, int32_t z) { (void)u; c1(0x0042f2dcu, (uint32_t)z); }
static uint32_t lm_jitter(void* u, uint32_t pos, int32_t x, int32_t y, int32_t z)
{
    (void)u;
    return c4(0x0044a40cu, pos, (uint32_t)x, (uint32_t)y, (uint32_t)z);
}
static void lm_fx(void* u, uint32_t fn, uint32_t pos, int32_t arg) { (void)u; c2(fn, pos, (uint32_t)arg); }
static void lm_sound(void* u, int32_t id, int32_t x, int32_t y, int32_t z) { (void)u; c4(0x00470320u, (uint32_t)id, (uint32_t)x, (uint32_t)y, (uint32_t)z); }
static void lm_flash(void* u, int32_t f, int32_t r, int32_t g, int32_t b) { (void)u; c4(0x00415bc4u, (uint32_t)f, (uint32_t)r, (uint32_t)g, (uint32_t)b); }
/* 0x0048a738(obj, pos, ang, spin, then on the stack vel, s1..s4), ret 0x14 */
static void lm_place(void* u, uint32_t obj, uint32_t pos, uint32_t ang, uint32_t spin, uint32_t vel, int32_t s1,
                     int32_t s2, int32_t s3, int32_t s4)
{
    (void)u;
    uint32_t a[9] = {obj, pos, ang, spin, vel, (uint32_t)s1, (uint32_t)s2, (uint32_t)s3, (uint32_t)s4};
    rt_call(0x0048a738u, a, 9);
}
static int32_t lm_split(void* u) { (void)u; return (int32_t)c0(0x0046cc54u); }
static void lm_locate(void* u, uint32_t pos) { (void)u; c1(0x0046f974u, pos); }
static void lm_locate_split(void* u, uint32_t pos, int32_t v) { (void)u; c2(0x0046f980u, pos, (uint32_t)v); }
static int32_t lm_volume(void* u) { (void)u; return (int32_t)c0(0x0046f990u); }
static uint32_t lm_pan(void* u) { (void)u; return c0(0x0046f988u); }
static void lm_play(void* u, int32_t id, uint32_t pan, int32_t vol, int32_t prio) { (void)u; c4(0x0046f8b0u, (uint32_t)id, pan, (uint32_t)vol, (uint32_t)prio); }
static void lm_topple_a(void* u) { (void)u; c0(0x004325a8u); }
static void lm_topple_b(void* u) { (void)u; c0(0x004325c8u); }

static GpLmEnv* lm(void)
{
    static GpLmEnv e;
    host_sync();
    e.d = rt_ptr(GP_LM_BASE);
    e.mem = wm();
    e.trail_frames = (uint32_t*)(void*)rt_ptr(0x004f11d8u);
    e.grp = rt_ptr(0x004fca18u);
    e.user = NULL;
    e.section_disable = lm_section;
    e.jitter = lm_jitter;
    e.fx = lm_fx;
    e.sound = lm_sound;
    e.flash = lm_flash;
    e.place = lm_place;
    e.split = lm_split;
    e.locate = lm_locate;
    e.locate_split = lm_locate_split;
    e.volume = lm_volume;
    e.pan = lm_pan;
    e.play = lm_play;
    e.topple_a = lm_topple_a;
    e.topple_b = lm_topple_b;
    return &e;
}
#define POS(i) ((const int32_t*)(const void*)rt_ptr(A(i)))

static uint32_t b_lm_reset(const uint32_t* a, int n) { (void)a; (void)n; gp_landmarks_reset(lm()); return 0; }
static uint32_t b_lm_a_add(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_landmark_a_add(e, A(0), POS(1), SP(2)); return 0; }
static uint32_t b_lm_a_addr(const uint32_t* a, int n) { return gp_landmark_a_addr(SP(0)); }
static uint32_t b_lm_a_blast(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_landmark_a_blast(e, POS(0)); return 0; }
static uint32_t b_lm_a_break(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_landmark_a_break(e, SP(0)); return 0; }
static uint32_t b_lm_a_update(const uint32_t* a, int n) { (void)a; (void)n; gp_landmark_a_update(lm()); return 0; }
static uint32_t b_lm_b_mark(const uint32_t* a, int n) { (void)a; (void)n; gp_landmark_b_mark(lm()); return 0; }
static uint32_t b_lm_b_add(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_landmark_b_add(e, A(0), SP(1), SP(2)); return 0; }
static uint32_t b_lm_b_blast(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_landmark_b_blast(e, POS(0)); return 0; }
static uint32_t b_lm_b_break(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_landmark_b_break(e, SP(0)); return 0; }
static uint32_t b_lm_b_update(const uint32_t* a, int n) { (void)a; (void)n; gp_landmark_b_update(lm()); return 0; }
static uint32_t b_burn_set(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_burn_set(e, A(0), POS(1)); return 0; }
static uint32_t b_burn_countdown(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_burn_countdown(e, (int8_t)A(0)); return 0; }
static uint32_t b_burn_update(const uint32_t* a, int n) { (void)a; (void)n; gp_burn_update(lm()); return 0; }
static uint32_t b_burn_point(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_burn_point_set(e, POS(0), SP(1)); return 0; }
static uint32_t b_debris_point(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_debris_point_set(e, POS(0), SP(1)); return 0; }
static uint32_t b_mount_update(const uint32_t* a, int n) { (void)a; (void)n; gp_mount_update(lm()); return 0; }
static uint32_t b_mount_set(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_mount_set(e, A(0)); return 0; }
static uint32_t b_mount_part(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_mount_part_set(e, A(0), SP(1)); return 0; }
static uint32_t b_trail_frames(const uint32_t* a, int n) { GpLmEnv* e = lm(); gp_trail_frames_set(e, A(0)); return 0; }

/* ---- gp_misc.c ---- */

static int32_t mi_owner(void* u) { (void)u; return (int32_t)c0(0x0045bab8u); }
static int32_t mi_multi(void* u) { (void)u; return (int32_t)c0(0x004632b0u); }

static GpMiscEnv* mi(void)
{
    static GpMiscEnv e;
    e.mem = wm();
    e.effects = rt_ptr(0x004f0938u);
    e.slots = rt_ptr(0x004f11dcu);
    e.flash_frames = (int32_t*)(void*)rt_ptr(0x004f5df0u);
    e.flash = rt_ptr(0x004f5dc4u);
    e.msg = (int32_t*)(void*)rt_ptr(0x004fb15cu);
    e.toggle_a = rt_ptr(0x004e685eu);
    e.toggle_b = rt_ptr(0x004e6860u);
    e.cycle = rt_ptr(0x004f6a38u);
    e.user = NULL;
    e.msg_owner = mi_owner;
    e.multiplayer = mi_multi;
    return &e;
}

static uint32_t b_fx_reset_all(const uint32_t* a, int n) { (void)a; (void)n; gp_effects_reset_all(mi()); return 0; }
static uint32_t b_fx_reset(const uint32_t* a, int n) { (void)a; (void)n; gp_effects_reset(mi()); return 0; }
static uint32_t b_flash_fade(const uint32_t* a, int n) { (void)a; (void)n; gp_flash_fade(mi()); return 0; }
static uint32_t b_msg_reset(const uint32_t* a, int n) { (void)a; (void)n; gp_hud_msg_reset(mi()); return 0; }
static uint32_t b_msg_post(const uint32_t* a, int n) { gp_hud_msg_post(mi(), SP(0), SP(1)); return 0; }
static uint32_t b_hud_color(const uint32_t* a, int n) { gp_hud_color(SP(0), rt_ptr(A(1))); return 0; }
static uint32_t b_toggle_a(const uint32_t* a, int n) { gp_toggle_a(mi(), SP(0)); return 0; }
static uint32_t b_toggle_b(const uint32_t* a, int n) { gp_toggle_b(mi(), SP(0)); return 0; }
static uint32_t b_cycle(const uint32_t* a, int n) { gp_cycle(mi(), SP(0)); return 0; }

/* ---- gp_nodes.c ---- */

static GpNodesEnv* nd(void)
{
    static GpNodesEnv e;
    e.tiles = rt_ptr(0x004fb17cu);
    e.grp = rt_ptr(0x004fca18u);
    e.way = rt_ptr(0x004fe1b0u);
    return &e;
}
static uint32_t b_area_nodes_remove(const uint32_t* a, int n) { gp_area_nodes_remove(nd(), SP(0)); return 0; }
static uint32_t b_zone_tiles_raise(const uint32_t* a, int n) { gp_zone_tiles_raise(nd(), SP(0), SP(1)); return 0; }
static uint32_t b_zone_tiles_set(const uint32_t* a, int n) { gp_zone_tiles_set(nd(), SP(0), SP(1)); return 0; }

/* ---- gp_nullsubs.c: the originals return with every register as it came (EAX = EAX) ---- */

static uint32_t b_null_human(const uint32_t* a, int n) { gp_car_bind_human_slot_stub(); return A(0); }
static uint32_t b_null_ai(const uint32_t* a, int n) { gp_car_bind_ai_slot_stub(); return A(0); }
static uint32_t b_null_hud(const uint32_t* a, int n) { gp_hud_nullsub(); return A(0); }

/* ---- gp_proj.c: the harness gave no callbacks (these variants make none) ---- */

static ProjEnv* pj(void)
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
    return &e;
}
/* owner EAX, target EDX, angles EBX, position ECX, damage (and 0x00413f7c's target object) on the stack */
#define GP_FIRE(nm, fn)                                                                                   \
    static uint32_t nm(const uint32_t* a, int n)                                                        \
    {                                                                                                   \
        return (uint32_t)fn(pj(), SP(0), A(1), A(2), A(3), SP(4));                                      \
    }
GP_FIRE(b_fire_0a, gp_proj_fire_type0a)
GP_FIRE(b_fire_3d, gp_proj_fire_type3d)
GP_FIRE(b_fire_0f, gp_proj_fire_type0f)
GP_FIRE(b_fire_0f_b, gp_proj_fire_type0f_b)
static uint32_t b_fire_33(const uint32_t* a, int n) { return (uint32_t)gp_proj_fire_type33(pj(), SP(0), A(1), A(2), A(3), SP(4), A(5)); }
/* 0x00414178: owner EAX, angles EDX, position EBX, damage ECX */
static uint32_t b_spawn_0c(const uint32_t* a, int n) { return (uint32_t)gp_proj_spawn_type0c(pj(), SP(0), A(1), A(2), SP(3)); }

/* ---- gp_radar.c ---- */

static int32_t ra_split(void* u) { (void)u; return (int32_t)c0(0x0046cc54u); }
static uint8_t ra_wide(void* u) { (void)u; return (uint8_t)c0(0x004958acu); }
static void ra_begin(void* u) { (void)u; c0(0x0046cba4u); }
static void ra_prim(void* u, uint32_t prim) { (void)u; c1(0x004967f4u, prim); }

static GpRadarEnv* ra(void)
{
    static GpRadarEnv e;
    e.views = rt_ptr(0x004faa3cu);
    e.scr_x = (const int32_t*)(const void*)rt_ptr(0x004e670cu);
    e.scr_y = (const int32_t*)(const void*)rt_ptr(0x004e6710u);
    e.user = NULL;
    e.split = ra_split;
    e.wide = ra_wide;
    e.arrows = rt_ptr(0x004f696cu);
    e.images = rt_ptr(0x004fb11cu);
    e.views_addr = 0x004faa3cu;
    e.arrows_addr = 0x004f696cu;
    e.begin = ra_begin;
    e.add_prim = ra_prim;
    return &e;
}
/* 0x004361d4(unused EAX, view EDX) */
static uint32_t b_radar_draw(const uint32_t* a, int n) { gp_radar_draw(ra(), SP(1)); return 0; }
static uint32_t b_radar_init(const uint32_t* a, int n) { (void)a; (void)n; gp_radar_init(ra()); return 0; }
/* 0x00436a40(type, pos, angle, view) */
static uint32_t b_radar_blip(const uint32_t* a, int n)
{
    gp_radar_blip(ra(), A(0), (const int32_t*)(const void*)rt_ptr(A(1)), SP(2), SP(3));
    return 0;
}

/* ---- gp_hud.c ---- */

static int32_t hu_split(void* u) { (void)u; return (int32_t)c0(0x0046cc54u); }
static void hu_image_load(void* u, uint32_t data, uint32_t slot, int32_t one) { (void)u; c3(0x00496da0u, data, slot, (uint32_t)one); }
/* 0x00496da0(src, out, n): out_addr 0 is a local of the original: a frame here */
static uint32_t hu_images(void* u, uint32_t src, uint8_t* out, uint32_t out_addr, int32_t cnt)
{
    (void)u;
    if (out_addr) return c3(0x00496da0u, src, out_addr, (uint32_t)cnt);
    uint32_t sz = cnt > 0 ? 8u * (uint32_t)cnt : 8u;
    Tmp t = tin(out, sz);
    uint32_t r = c3(0x00496da0u, src, t.addr, (uint32_t)cnt);
    tdone(&t, 1);
    return r;
}

static GpHudEnv* hu(void)
{
    static GpHudEnv e;
    e.d = rt_ptr(GP_HUD_BASE);
    e.scr_x = (const uint16_t*)(const void*)rt_ptr(0x004e673cu);
    e.scr_y = (const uint16_t*)(const void*)rt_ptr(0x004e6740u);
    e.user = NULL;
    e.split = hu_split;
    e.image_load = hu_image_load;
    e.images = hu_images;
    return &e;
}
static uint32_t b_hud_panels(const uint32_t* a, int n) { (void)a; (void)n; gp_hud_panels_init(hu()); return 0; }
static uint32_t b_hud_mirror(const uint32_t* a, int n) { (void)a; (void)n; gp_hud_mirror_init(hu()); return 0; }
static uint32_t b_hud_arrows(const uint32_t* a, int n) { gp_hud_radar_arrows(hu(), SP(0), A(1)); return 0; }
static uint32_t b_hud_dash(const uint32_t* a, int n) { gp_hud_dash_icons(hu(), hp(A(0))); return 0; }

/* ---- gp_aifire.c ---- */

static void af_act(void* u, uint8_t* car, int32_t arg) { (void)u; c2(0x0043e6e0u, ga(car), (uint32_t)arg); }
static uint32_t b_ai_fire(const uint32_t* a, int n)
{
    static GpAiFireEnv e;
    host_sync();
    e.mem = wm();
    e.user = NULL;
    e.act = af_act;
    gp_ai_weapon_fire_decide(&e, rt_ptr(A(0)));
    return 0;
}

/* ---- gp_cars.c ---- */

static void ca_spawn_reset(void* u) { (void)u; c0(0x0043bd60u); }
static int32_t ca_count(void* u) { (void)u; return (int32_t)c0(0x0045afb4u); }
static int32_t ca_pcount(void* u) { (void)u; return (int32_t)c0(0x004632c8u); }
static uint8_t* ca_car(void* u, int32_t i) { (void)u; return hp(c1(0x0045afbcu, (uint32_t)i)); }
static int32_t ca_pick(void* u, int32_t c) { (void)u; return (int32_t)c1(0x0043289cu, (uint32_t)c); }
static void ca_reset(void* u, uint32_t v) { (void)u; c1(0x00464e84u, v); }
static void ca_event(void* u, int32_t k, int32_t x, uint32_t y) { (void)u; c3(0x00473650u, (uint32_t)k, (uint32_t)x, y); }
static void ca_flush(void* u, int32_t x, int32_t y) { (void)u; c2(0x00473568u, (uint32_t)x, (uint32_t)y); }

static GpCarsEnv* ca(void)
{
    static GpCarsEnv e;
    host_sync();
    e.spawn_count = (const int8_t*)(const void*)rt_ptr(0x004fe1e1u);
    e.user = NULL;
    e.spawn_reset = ca_spawn_reset;
    e.car_count = ca_count;
    e.player_count = ca_pcount;
    e.car = ca_car;
    e.spawn_pick = ca_pick;
    e.car_reset = ca_reset;
    e.event = ca_event;
    e.event_flush = ca_flush;
    return &e;
}
static uint32_t b_pick_spawns(const uint32_t* a, int n) { (void)a; (void)n; gp_cars_pick_spawns(ca()); return 0; }
/* 0x00432838(flag): ORIGINAL BUG, the event's third word is the caller's ESI, which the runtime
 * does not have: 0 */
static uint32_t b_spawn_events(const uint32_t* a, int n) { gp_cars_spawn_events(ca(), (uint8_t)A(0), 0); return 0; }

/* ---- gp_crush.c ---- */

static uint32_t cr_entity(void* u, int32_t type) { (void)u; return c1(0x0045b0b8u, (uint32_t)type); }
/* 0x0045b0ec(ent, &human, &index) */
static uint8_t cr_entity_car(void* u, uint32_t ent, uint8_t* human, int32_t* index)
{
    (void)u;
    Tmp th = tin(human, 1), ti = tin(index, 4);
    uint8_t r = (uint8_t)c3(0x0045b0ecu, ent, th.addr, ti.addr);
    tdone(&ti, 1);
    tdone(&th, 1);
    return r;
}
static uint8_t* cr_human(void* u, int32_t i) { (void)u; return hp(c1(0x0045afbcu, (uint32_t)i)); }
static uint8_t* cr_ai(void* u, int32_t i) { (void)u; return hp(c1(0x0045b080u, (uint32_t)i)); }
static void cr_debris(void* u, int32_t kind, const int32_t p[3])
{
    (void)u;
    Tmp t = tin(p, 12);
    c2(0x004516bcu, (uint32_t)kind, t.addr);
    tdone(&t, 0);
}
static void cr_fx(void* u, uint32_t fn, const int32_t p[3], int32_t zero)
{
    (void)u;
    Tmp t = tin(p, 12);
    c2(fn, t.addr, (uint32_t)zero);
    tdone(&t, 0);
}

/* 0x004305fc(car, human) */
static uint32_t b_crush(const uint32_t* a, int n)
{
    static GpCrushEnv e;
    host_sync();
    e.mem = wm();
    e.parity = (int32_t)rt_r32(0x00a0364cu);
    e.user = NULL;
    e.entity = cr_entity;
    e.entity_car = cr_entity_car;
    e.human_car = cr_human;
    e.ai_car = cr_ai;
    e.debris = cr_debris;
    e.fx = cr_fx;
    gp_car_crush_contact(&e, rt_ptr(A(0)), (uint8_t)A(1));
    return 0;
}

void rt_bind_gameplay(void)
{
    rt_bind(0x00410700u, b_lm_reset, "landmarks_reset");
    rt_bind(0x004107f4u, b_lm_a_add, "landmark_a_add");
    rt_bind(0x00410878u, b_lm_a_addr, "landmark_a_addr");
    rt_bind(0x00410890u, b_lm_a_blast, "landmark_a_blast");
    rt_bind(0x00410b34u, b_lm_a_break, "landmark_a_break");
    rt_bind(0x00410df4u, b_lm_a_update, "landmark_a_update");
    rt_bind(0x00411270u, b_lm_b_mark, "landmark_b_mark");
    rt_bind(0x004112a4u, b_lm_b_add, "landmark_b_add");
    rt_bind(0x00411334u, b_lm_b_blast, "landmark_b_blast");
    rt_bind(0x004114c0u, b_lm_b_break, "landmark_b_break");
    rt_bind(0x004116b0u, b_lm_b_update, "landmark_b_update");
    rt_bind(0x004119e4u, b_burn_set, "burn_set");
    rt_bind(0x00411b30u, b_burn_countdown, "burn_countdown");
    rt_bind(0x00411b48u, b_burn_update, "burn_update");
    rt_bind(0x00411dc8u, b_burn_point, "burn_point_set");
    rt_bind(0x00411de8u, b_debris_point, "debris_point_set");
    rt_bind(0x00411fccu, b_mount_update, "mount_update");
    rt_bind(0x00412564u, b_mount_set, "mount_set");
    rt_bind(0x004125a8u, b_mount_part, "mount_part_set");
    rt_bind(0x00412638u, b_trail_frames, "trail_frames_set");
    rt_bind(0x00412d58u, b_fx_reset_all, "effects_reset_all");
    rt_bind(0x00412dacu, b_fx_reset, "effects_reset");
    rt_bind(0x00415c8cu, b_flash_fade, "flash_fade");
    rt_bind(0x00436010u, b_msg_reset, "hud_msg_reset");
    rt_bind(0x00436040u, b_msg_post, "hud_msg_post");
    rt_bind(0x00436120u, b_hud_color, "hud_color");
    rt_bind(0x00436f60u, b_toggle_a, "toggle_a");
    rt_bind(0x00436f80u, b_toggle_b, "toggle_b");
    rt_bind(0x00436f98u, b_cycle, "cycle");
    rt_bind(0x0042f520u, b_area_nodes_remove, "area_nodes_remove");
    rt_bind(0x0042f7d0u, b_zone_tiles_raise, "zone_tiles_raise");
    rt_bind(0x0042f850u, b_zone_tiles_set, "zone_tiles_set");
    rt_bind(0x00422e04u, b_null_human, "car_bind_human_slot_stub");
    rt_bind(0x0042a018u, b_null_ai, "car_bind_ai_slot_stub");
    rt_bind(0x00435c50u, b_null_hud, "hud_nullsub");
    rt_bind(0x00413f4cu, b_fire_0a, "proj_fire_type0a");
    rt_bind(0x00413f7cu, b_fire_33, "proj_fire_type33");
    rt_bind(0x00414178u, b_spawn_0c, "proj_spawn_type0c");
    rt_bind(0x00414244u, b_fire_3d, "proj_fire_type3d");
    rt_bind(0x00414530u, b_fire_0f, "proj_fire_type0f");
    rt_bind(0x00414618u, b_fire_0f_b, "proj_fire_type0f_b");
    rt_bind(0x004361d4u, b_radar_draw, "radar_draw");
    rt_bind(0x004367f4u, b_radar_init, "radar_init");
    rt_bind(0x00436a40u, b_radar_blip, "radar_blip");
    rt_bind(0x00435934u, b_hud_dash, "hud_dash_icons");
    rt_bind(0x00435c54u, b_hud_panels, "hud_panels_init");
    rt_bind(0x00435e78u, b_hud_mirror, "hud_mirror_init");
    rt_bind(0x00436ff4u, b_hud_arrows, "hud_radar_arrows");
    rt_bind(0x00410010u, b_ai_fire, "ai_weapon_fire_decide");
    rt_bind(0x004327fcu, b_pick_spawns, "cars_pick_spawns");
    rt_bind(0x00432838u, b_spawn_events, "cars_spawn_events");
    rt_bind(0x004305fcu, b_crush, "car_crush_contact");
}

RT_BIND_AREA(gameplay)
