/*
 * bind_world.c: the world / arena ports (src/world: collapse.c, terrain_load.c, track.c, warps.c,
 * world_misc.c), called the way tools/emu/diff_world_*.py called them. Each context struct gets
 * its globals as rt_ptr(<original address>) (pointers read from their globals at every call, as
 * the original reads them), and each callback calls the original address with the original's
 * registers (EAX, EDX, EBX, ...).
 *
 * Runtime equivalents of harness artefacts:
 *  - a callback that gets a host pointer the port keeps on the host stack (a vector, a path
 *    buffer, a string literal) passes the callee a copy in a runtime frame and copies it back
 *    (gaddr_begin / gaddr_end); pointers into game memory are passed as their game address.
 *  - the harness set the port's 60 Hz ticks with tm2_set_frame_ticks60; here every adapter
 *    loads it from the original's global 0x004e8300 first (sync_ticks).
 *  - the error printf of 0x00451604 gets the original's own format string, found in the image.
 *
 * NOT synced: the ports' rand (tm2_rand, src/core/tm2rand.c) keeps its state on the host, while
 * the original's rand 0x004899c2 keeps it in the C runtime's thread data (0x004899b8 + 0). The
 * sequences are independent until the core gives tm2_rand the game's state (see the report).
 */
#include "rt_env.h"
#include "math/tm2math.h"
#include "world/collapse.h"
#include "world/terrain_load.h"
#include "world/track.h"
#include "world/warps.h"
#include "world/world_misc.h"

/* ---- helpers ------------------------------------------------------------------------------ */

static void sync_ticks(void) { tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u)); }

static uint8_t* res(uint32_t h) { return h ? rt_ptr(h) : NULL; }
static uint8_t* w_resolve(void* u, uint32_t h) { (void)u; return res(h); }

static int in_rt(const void* p)
{
    uintptr_t q = (uintptr_t)p, b = (uintptr_t)rt_base;
    return q >= b && q < b + RT_SIZE;
}

/* the game address of host memory p (n bytes): itself when it is in game memory, else a copy on
 * the frame stack (*pushed = the frame size to give gaddr_end) */
static uint32_t gaddr_begin(const void* p, uint32_t n, uint32_t* pushed)
{
    *pushed = 0;
    if (!p) return 0;
    if (in_rt(p)) return rt_addr(p);
    *pushed = (n + 15u) & ~15u;
    uint32_t a = rt_frame_push(*pushed);
    memcpy(rt_ptr(a), p, n);
    return a;
}
static void gaddr_end(void* p, uint32_t a, uint32_t n, uint32_t pushed)
{
    if (!pushed) return;
    if (p) memcpy(p, rt_ptr(a), n);
    rt_frame_pop(pushed);
}

static uint32_t call1(uint32_t fn, uint32_t eax) { uint32_t a[1] = {eax}; return rt_call(fn, a, 1); }
static uint32_t call2(uint32_t fn, uint32_t eax, uint32_t edx) { uint32_t a[2] = {eax, edx}; return rt_call(fn, a, 2); }
static uint32_t call3(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    uint32_t a[3] = {eax, edx, ebx};
    return rt_call(fn, a, 3);
}

/* fn(EAX = vector p (12 bytes), EDX = x) with p copied in and out when it is a host local */
static uint32_t call_vec(uint32_t fn, int32_t* p, uint32_t edx, int nargs)
{
    uint32_t pushed, g = gaddr_begin(p, 12, &pushed);
    uint32_t a[2] = {g, edx};
    uint32_t r = rt_call(fn, a, nargs);
    gaddr_end(p, g, 12, pushed);
    return r;
}

/* shared callbacks: car count 0x0045afb4, player car block 0x0045afbc, objRegistrySetStep 0x00470320 */
static int32_t cb_car_count(void* u) { (void)u; return (int32_t)rt_call(0x0045afb4u, NULL, 0); }
static const uint8_t* cb_car_c(void* u, int32_t i) { (void)u; return res(call1(0x0045afbcu, (uint32_t)i)); }
static uint8_t* cb_car(void* u, int32_t i) { (void)u; return res(call1(0x0045afbcu, (uint32_t)i)); }
static void cb_set_step(void* u, int32_t id, int32_t a, int32_t b)
{
    (void)u;
    call3(0x00470320u, (uint32_t)id, (uint32_t)a, (uint32_t)b);
}

/* ---- collapse.c --------------------------------------------------------------------------- */

static int32_t cb_game_mode(void* u) { (void)u; return (int32_t)rt_call(0x0045c338u, NULL, 0); }
static int cb_spawn_blast(void* u, int32_t* p) { (void)u; return (int)(call_vec(0x00457314u, p, 0, 1) & 0xFFu); }
static void cb_fx(void* u, int which, int32_t* p, int32_t arg)
{
    static const uint32_t fx[4] = {0x00450be0u, 0x00450b94u, 0x004512f8u, 0x004512b4u};
    (void)u;
    if (which >= 0 && which < 4) call_vec(fx[which], p, (uint32_t)arg, 2);
}
static void cb_section_disable(void* u, int32_t s) { (void)u; call1(0x0042f2dcu, (uint32_t)s); }
static void cb_section_start(void* u, int32_t s) { (void)u; call1(0x0042f520u, (uint32_t)s); }
static void cb_ai_reroute(void* u, int32_t s, int32_t v) { (void)u; call2(0x0042f6d4u, (uint32_t)s, (uint32_t)v); }
static void cb_section_move(void* u, int32_t s, int32_t v) { (void)u; call2(0x0042f7d0u, (uint32_t)s, (uint32_t)v); }
static void cb_section_sink(void* u, int32_t s, int32_t v) { (void)u; call2(0x0042f850u, (uint32_t)s, (uint32_t)v); }

static WorldCollapseEnv collapse_env(void)
{
    WorldCollapseEnv e = {
        rt_ptr(0x00502988u), rt_ptr(0x00502e74u), rt_ptr(0x004fca18u), rt_r32(0x00502f5cu),
        w_resolve, cb_car_count, cb_car_c, cb_game_mode, cb_spawn_blast, cb_fx,
        cb_section_disable, cb_section_start, cb_ai_reroute, cb_section_move, cb_section_sink,
        cb_set_step, NULL};
    sync_ticks();
    return e;
}

/* 0x00454fb8 (EAX = object) */
static uint32_t b_collapse_register(const uint32_t* a, int n)
{
    WorldCollapseEnv e = collapse_env();
    uint32_t h = rt_arg(a, n, 0);
    world_collapse_register(&e, h, rt_ptr(h));
    return 0;
}
/* 0x00455954 (EAX = section) */
static uint32_t b_collapse_unlink(const uint32_t* a, int n)
{
    WorldCollapseEnv e = collapse_env();
    world_collapse_unlink(&e, (int32_t)rt_arg(a, n, 0));
    return 0;
}
/* 0x00455014 () */
static uint32_t b_collapse_clear_done(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WorldCollapseEnv e = collapse_env();
    world_collapse_clear_done(&e);
    return 0;
}
/* 0x00455678 (EAX = section) */
static uint32_t b_collapse_move(const uint32_t* a, int n)
{
    WorldCollapseEnv e = collapse_env();
    world_collapse_move_section(&e, (int32_t)rt_arg(a, n, 0));
    return 0;
}
/* 0x00455d34 (EAX = vector) */
static uint32_t b_collapse_debris(const uint32_t* a, int n)
{
    WorldCollapseEnv e = collapse_env();
    world_collapse_debris(&e, (int32_t*)(void*)rt_ptr(rt_arg(a, n, 0)));
    return 0;
}
/* 0x004559e0 (EAX = section) */
static uint32_t b_collapse_explosions(const uint32_t* a, int n)
{
    WorldCollapseEnv e = collapse_env();
    world_collapse_explosions(&e, (int32_t)rt_arg(a, n, 0));
    return 0;
}
/* 0x00455050 () */
static uint32_t b_collapse_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WorldCollapseEnv e = collapse_env();
    world_collapse_update(&e);
    return 0;
}

/* ---- terrain_load.c ----------------------------------------------------------------------- */

/* file_build_path 0x00499408(EAX = out, EDX = name, EBX = kind): out is the port's 300 byte host
 * buffer and name a host string, so both go through frame copies */
static void cb_build_path(void* u, char* out, const char* name, int32_t kind)
{
    (void)u;
    uint32_t po, pn;
    uint32_t go = gaddr_begin(out, 300, &po);
    uint32_t nl = (uint32_t)strlen(name) + 1;
    uint32_t gn = gaddr_begin(name, nl, &pn);
    call3(0x00499408u, go, gn, (uint32_t)kind);
    gaddr_end(NULL, gn, nl, pn);
    gaddr_end(out, go, 300, po);
}
/* file_load_resource 0x00498d18(EAX = buffer, EDX = path) */
static void cb_load_resource(void* u, uint8_t* buffer, const char* path)
{
    (void)u;
    uint32_t pp, pl = (uint32_t)strlen(path) + 1;
    uint32_t gp = gaddr_begin(path, pl, &pp);
    call2(0x00498d18u, rt_addr(buffer), gp);   /* the load buffer is game memory */
    gaddr_end(NULL, gp, pl, pp);
}

static WorldTerrainEnv terrain_env(void)
{
    WorldTerrainEnv e = {
        (int32_t*)(void*)rt_ptr(0x004fb174u), (int32_t*)(void*)rt_ptr(0x004fb178u), rt_ptr(0x004fb17cu),
        (int32_t*)(void*)rt_ptr(0x004fe1acu), rt_ptr(0x004fca18u), res(rt_r32(0x00a3a5c8u)),
        cb_build_path, cb_load_resource, NULL};
    return e;
}

#define TERRAIN(kind, arena) \
    static uint32_t b_terrain_##kind##_##arena(const uint32_t* a, int n) \
    { (void)a; (void)n; WorldTerrainEnv e = terrain_env(); \
      world_terrain_load_##kind(WORLD_ARENA_##arena, &e); return 0; }
TERRAIN(rmp, FWY) TERRAIN(rmp, DISH) TERRAIN(rmp, ICE) TERRAIN(rmp, LAVA) TERRAIN(rmp, PARIS)
TERRAIN(rmp, HKONG) TERRAIN(rmp, ROOF) TERRAIN(rmp, DEN) TERRAIN(rmp, SWAMP) TERRAIN(rmp, CYB)
TERRAIN(rmp, SROOF)
TERRAIN(grp, FWY) TERRAIN(grp, DISH) TERRAIN(grp, ICE) TERRAIN(grp, LAVA) TERRAIN(grp, PARIS)
TERRAIN(grp, HKONG) TERRAIN(grp, ROOF) TERRAIN(grp, DEN) TERRAIN(grp, SWAMP) TERRAIN(grp, CYB)
TERRAIN(grp, SROOF)

/* ---- track.c ------------------------------------------------------------------------------ */

static int32_t cb_ground(void* u, const int32_t p[3])
{
    (void)u;
    return (int32_t)call_vec(0x0042c2d4u, (int32_t*)(uintptr_t)p, 0, 1);
}
static void cb_place(void* u, int32_t i) { (void)u; call1(0x00455ef0u, (uint32_t)i); }
static void cb_track_sound(void* u, int32_t id, uint8_t* pos)
{
    (void)u;
    uint32_t pushed, g = gaddr_begin(pos, 12, &pushed);
    call2(0x00472e50u, (uint32_t)id, g);
    gaddr_end(pos, g, 12, pushed);
}

static WorldTrackEnv track_env(void)
{
    WorldTrackEnv e = {
        res(rt_r32(0x00502f18u)), rt_ptr(0x004fb17cu), (int32_t)rt_r32(0x004fb174u), w_resolve,
        cb_ground, cb_car_count, cb_car_c, cb_place, cb_track_sound, NULL};
    sync_ticks();
    return e;
}

/* 0x0043b440 (EAX = record, EDX = point, EBX = height, ECX = angle) */
static uint32_t b_track_surface_set(const uint32_t* a, int n)
{
    WorldTrackEnv e = track_env();
    world_track_surface_set(&e, (int32_t)rt_arg(a, n, 0), (const int32_t*)(const void*)rt_ptr(rt_arg(a, n, 1)),
                            (int32_t)rt_arg(a, n, 2), (int32_t)rt_arg(a, n, 3));
    return 0;
}
/* 0x00456cc4 (EAX = record) */
static uint32_t b_track_ground(const uint32_t* a, int n)
{
    WorldTrackEnv e = track_env();
    world_track_vehicle_ground(&e, (int32_t)rt_arg(a, n, 0));
    return 0;
}
/* 0x0045606c () */
static uint32_t b_track_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WorldTrackEnv e = track_env();
    world_track_vehicles_update(&e);
    return 0;
}

/* ---- warps.c ------------------------------------------------------------------------------ */

static void cb_probes_clear(void* u, uint8_t* q) { (void)u; call1(0x004300e8u, rt_addr(q)); }
static int32_t cb_find_zone(void* u, int32_t x, int32_t y, int32_t z)
{
    (void)u;
    return (int32_t)call3(0x0042cba8u, (uint32_t)x, (uint32_t)y, (uint32_t)z);
}
static void cb_probes_zone(void* u, uint8_t* q, int32_t z) { (void)u; call2(0x0043033cu, rt_addr(q), (uint32_t)z); }
static void cb_probes_floor(void* u, uint8_t* q, int32_t z) { (void)u; call2(0x00430368u, rt_addr(q), (uint32_t)z); }
static void cb_drive(void* u, uint8_t* car, int32_t human) { (void)u; call2(0x0042d470u, rt_addr(car), (uint32_t)human); }
static uint8_t cb_event_type(void* u) { (void)u; return (uint8_t)rt_call(0x00474508u, NULL, 0); }
static void cb_ai_place(void* u, uint8_t* car) { (void)u; call1(0x00432940u, rt_addr(car)); }

/* 0x0045450c () */
static uint32_t b_warps_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WorldWarpsEnv e = {rt_ptr(0x00502ea0u), w_resolve, cb_probes_clear, cb_find_zone, cb_probes_zone,
                       cb_probes_floor, cb_drive, cb_event_type, cb_ai_place, NULL};
    world_warps_update(&e);
    return 0;
}

/* ---- world_misc.c ------------------------------------------------------------------------- */

#define MISSILES 0x004f11dcu

/* 0x00415b3c (EAX = index): the record's address or 0 */
static uint32_t b_missile_record(const uint32_t* a, int n)
{
    uint8_t* p = world_missile_record(rt_ptr(MISSILES), (int32_t)rt_arg(a, n, 0));
    return p ? rt_addr(p) : 0;
}

static uint32_t cb_entity_alloc(void* u) { (void)u; return rt_call(0x00492d90u, NULL, 0); }
static void cb_entity_insert(void* u, uint32_t ent) { (void)u; call1(0x00492c4cu, ent); }

/* 0x00413960 () (the harness passed two random registers, unread) */
static uint32_t b_missiles_create(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WorldEntityEnv e = {cb_entity_alloc, cb_entity_insert, w_resolve, NULL};
    world_missiles_create(rt_ptr(MISSILES), &e);
    return 0;
}

/* 0x00430414 (EAX = zone a, EDX = zone b) */
static uint32_t b_route_speed_limit(const uint32_t* a, int n)
{
    sync_ticks();
    return (uint32_t)world_ai_route_speed_limit(rt_ptr(0x004fdc99u), (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
}

/* 0x004325c8 () */
static uint32_t b_route_patch_eiffel(const uint32_t* a, int n)
{
    (void)a; (void)n;
    world_ai_route_patch_eiffel(rt_ptr(0x004fea6au));
    return 0;
}

/* 0x0045b488 () */
static uint32_t b_difficulty_scale(const uint32_t* a, int n)
{
    (void)a; (void)n;
    return (uint32_t)world_ai_difficulty_speed_scale(rt_r32(0x00502f98u));
}

/* 0x004125b0 () */
static uint32_t b_eiffel_reset(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WorldEiffelEnv e = {rt_ptr(0x004f08e0u), rt_ptr(rt_r32(0x004f08ecu)), cb_set_step, NULL};
    world_eiffel_reset(&e);
    return 0;
}

/* 0x0043bd74 (EAX = section) */
static uint32_t b_section_poly_reset(const uint32_t* a, int n)
{
    world_section_list_remove(rt_ptr(0x004fe1b4u), (int32_t)rt_arg(a, n, 0));
    return 0;
}

/* 0x0045c300 (EAX = section): al */
static uint32_t b_section_moving(const uint32_t* a, int n)
{
    return (uint32_t)world_section_has_moving_surfaces((int32_t)rt_arg(a, n, 0));
}

/* 0x00451458 () */
static uint32_t b_fx_sprites_reset(const uint32_t* a, int n)
{
    (void)a; (void)n;
    world_fx_sprites_reset(rt_ptr(0x00500304u), rt_ptr(0x00500050u));
    return 0;
}

/* 0x00496d04(EAX = src, EDX = dst, EBX = n) */
static void cb_debris_parse(void* u, const uint8_t* src, uint8_t* dst, int32_t k)
{
    (void)u;
    call3(0x00496d04u, rt_addr(src), rt_addr(dst), (uint32_t)k);
}
/* the original's format string "\nError Parsing Fragment Sprite %d\n", found once in the image */
static uint32_t debris_error_fmt(void)
{
    static uint32_t at;
    static const char s[] = "\nError Parsing Fragment Sprite %d\n";
    if (!at) {
        for (uint32_t p = 0x00400000u; p + sizeof s <= 0x00e0d000u; p++)
            if (rt_r8(p) == '\n' && memcmp(rt_ptr(p), s, sizeof s) == 0) { at = p; break; }
    }
    return at;
}
/* 0x004a1f1e, cdecl printf like: stack words (format, id) */
static void cb_debris_error(void* u, int32_t id)
{
    (void)u;
    uint32_t a[2] = {debris_error_fmt(), (uint32_t)id};
    rt_call(0x004a1f1eu, a, 2);
}

/* 0x00451604 (EAX = id, EDX = record) */
static uint32_t b_debris_anim_store(const uint32_t* a, int n)
{
    WorldDebrisAnimEnv e = {rt_ptr(0x00501f26u), cb_debris_parse, cb_debris_error, NULL};
    world_debris_animation_store((int32_t)rt_arg(a, n, 0), rt_ptr(rt_arg(a, n, 1)), &e);
    return 0;
}

static void cb_burst_debris(void* u, int32_t kind, int32_t* p)
{
    (void)u;
    uint32_t pushed, g = gaddr_begin(p, 12, &pushed);
    call2(0x004516bcu, (uint32_t)kind, g);
    gaddr_end(p, g, 12, pushed);
}
static void cb_burst_a(void* u, int32_t* p, int32_t arg) { (void)u; call_vec(0x00451014u, p, (uint32_t)arg, 2); }
static void cb_burst_b(void* u, int32_t* p, int32_t arg) { (void)u; call_vec(0x004512b4u, p, (uint32_t)arg, 2); }
static void cb_burst_c(void* u, int32_t* p, int32_t arg) { (void)u; call_vec(0x00451274u, p, (uint32_t)arg, 2); }

/* 0x00430b5c (EAX = vector) */
static uint32_t b_random_debris_burst(const uint32_t* a, int n)
{
    WorldDebrisBurstEnv e = {cb_burst_debris, cb_burst_a, cb_burst_b, cb_burst_c, NULL};
    world_fx_random_debris_burst((int32_t*)(void*)rt_ptr(rt_arg(a, n, 0)), &e);
    return 0;
}

/* 0x0048c50c(EAX = angle, EDX = out matrix) */
static void cb_rot_x(void* u, int32_t angle, uint8_t* out) { (void)u; call2(0x0048c50cu, (uint32_t)angle, rt_addr(out)); }

/* 0x00453e90 (EAX = object) */
static uint32_t b_slam_claw_register(const uint32_t* a, int n)
{
    uint32_t h = rt_arg(a, n, 0);
    world_slam_claw_register(h, res(h), (uint32_t*)(void*)rt_ptr(0x00502e94u), (int32_t*)(void*)rt_ptr(0x00502e9cu),
                             cb_rot_x, NULL);
    return 0;
}

/* 0x00457398 (EAX = vector) */
static uint32_t b_target_hit_at(const uint32_t* a, int n)
{
    world_fx_target_hit_at(res(rt_r32(0x00502f60u)), (const int32_t*)(const void*)rt_ptr(rt_arg(a, n, 0)),
                           cb_set_step, NULL);
    return 0;
}

/* 0x00455e94 () */
static uint32_t b_track_reset(const uint32_t* a, int n)
{
    (void)a; (void)n;
    world_track_vehicles_reset(rt_ptr(0x00502988u), 0x00502988u, (uint32_t*)(void*)rt_ptr(0x00502f18u));
    return 0;
}

static void cb_track_start(void* u, int32_t i) { (void)u; call1(0x00455ef0u, (uint32_t)i); }

/* 0x00455ec0 (EAX = object, EDX = record): the table through its pointer 0x00502f18 */
static uint32_t b_track_attach(const uint32_t* a, int n)
{
    uint32_t h = rt_arg(a, n, 0);
    world_track_vehicle_attach(res(rt_r32(0x00502f18u)), h, res(h), (int32_t)rt_arg(a, n, 1), cb_track_start, NULL);
    return 0;
}

/* 0x00456c70 (): al */
static uint32_t b_track_should_stop(const uint32_t* a, int n)
{
    (void)a; (void)n;
    return (uint32_t)world_track_vehicles_should_stop(cb_car_count(NULL), cb_car, NULL);
}

/* 0x0047070c (EAX = row, EDX = direction) */
static uint32_t b_scene_choice_step(const uint32_t* a, int n)
{
    world_scene_choice_step((const uint32_t*)(const void*)rt_ptr(0x006211dcu), (int32_t)rt_arg(a, n, 0),
                            (int32_t)rt_arg(a, n, 1), w_resolve, NULL);
    return 0;
}

/* player_car_setup 0x00437850(EAX = car, EDX = id, EBX = handicap) and 0x0047068c(EAX = id, EDX = 0) */
static void cb_car_setup(void* u, uint8_t* car, int32_t id, int32_t b)
{
    (void)u;
    call3(0x00437850u, rt_addr(car), (uint32_t)id, (uint32_t)b);
}
static void cb_car_select(void* u, int32_t id, int32_t z) { (void)u; call2(0x0047068cu, (uint32_t)id, (uint32_t)z); }

/* 0x0045ba64 (EAX = player index) */
static uint32_t b_player_car_reset(const uint32_t* a, int n)
{
    world_player_car_reset(rt_ptr(0x005030a8u + 0x898u * rt_arg(a, n, 0)), rt_ptr(0x00502f80u),
                           cb_car_setup, cb_car_select, NULL);
    return 0;
}

static void cb_set_state(void* u, uint8_t* car, int32_t st) { (void)u; call2(0x004225a0u, rt_addr(car), (uint32_t)st); }

/* 0x0045c350 () */
static uint32_t b_demo_watch_next(const uint32_t* a, int n)
{
    (void)a; (void)n;
    WorldDemoWatchEnv e = {rt_ptr(0x005030a8u), rt_ptr(0x0050760cu), (int32_t*)(void*)rt_ptr(0x00502f90u),
                           (int32_t)rt_r32(0x0050c2b0u), (int32_t)rt_r32(0x00502fd8u), (int32_t)rt_r32(0x00502fb0u),
                           (int32_t*)(void*)rt_ptr(0x00502facu), cb_set_state, NULL};
    world_demo_watch_next(&e);
    return 0;
}

void rt_bind_world(void)
{
    rt_bind(0x00454fb8u, b_collapse_register, "worldCollapseRegister");
    rt_bind(0x00455014u, b_collapse_clear_done, "worldCollapseClearDone");
    rt_bind(0x00455050u, b_collapse_update, "worldCollapseUpdate");
    rt_bind(0x00455678u, b_collapse_move, "worldCollapseMoveSection");
    rt_bind(0x00455954u, b_collapse_unlink, "worldCollapseUnlink");
    rt_bind(0x004559e0u, b_collapse_explosions, "worldCollapseExplosions");
    rt_bind(0x00455d34u, b_collapse_debris, "worldCollapseDebris");

    rt_bind(0x0043b4c0u, b_terrain_rmp_FWY, "terrainLoadRmpFwy");
    rt_bind(0x0043b528u, b_terrain_rmp_DISH, "terrainLoadRmpDish");
    rt_bind(0x0043b590u, b_terrain_rmp_ICE, "terrainLoadRmpIce");
    rt_bind(0x0043b5f8u, b_terrain_rmp_LAVA, "terrainLoadRmpLava");
    rt_bind(0x0043b660u, b_terrain_rmp_PARIS, "terrainLoadRmpParis");
    rt_bind(0x0043b6c8u, b_terrain_rmp_HKONG, "terrainLoadRmpHkong");
    rt_bind(0x0043b730u, b_terrain_rmp_ROOF, "terrainLoadRmpRoof");
    rt_bind(0x0043b798u, b_terrain_rmp_DEN, "terrainLoadRmpDen");
    rt_bind(0x0043b800u, b_terrain_rmp_SWAMP, "terrainLoadRmpSwamp");
    rt_bind(0x0043b868u, b_terrain_rmp_CYB, "terrainLoadRmpCyb");
    rt_bind(0x0043b8d0u, b_terrain_rmp_SROOF, "terrainLoadRmpSroof");
    rt_bind(0x0043b940u, b_terrain_grp_FWY, "terrainLoadGrpFwy");
    rt_bind(0x0043b9a0u, b_terrain_grp_DISH, "terrainLoadGrpDish");
    rt_bind(0x0043ba00u, b_terrain_grp_ICE, "terrainLoadGrpIce");
    rt_bind(0x0043ba60u, b_terrain_grp_LAVA, "terrainLoadGrpLava");
    rt_bind(0x0043bac0u, b_terrain_grp_PARIS, "terrainLoadGrpParis");
    rt_bind(0x0043bb20u, b_terrain_grp_HKONG, "terrainLoadGrpHkong");
    rt_bind(0x0043bb80u, b_terrain_grp_ROOF, "terrainLoadGrpRoof");
    rt_bind(0x0043bbe0u, b_terrain_grp_DEN, "terrainLoadGrpDen");
    rt_bind(0x0043bc40u, b_terrain_grp_SWAMP, "terrainLoadGrpSwamp");
    rt_bind(0x0043bc9cu, b_terrain_grp_CYB, "terrainLoadGrpCyb");
    rt_bind(0x0043bcf8u, b_terrain_grp_SROOF, "terrainLoadGrpSroof");

    rt_bind(0x0043b440u, b_track_surface_set, "trackVehicleSurfaceSet");
    rt_bind(0x0045606cu, b_track_update, "worldTrackVehiclesUpdate");
    rt_bind(0x00456cc4u, b_track_ground, "trackVehicleGround");

    rt_bind(0x0045450cu, b_warps_update, "worldWarpsUpdate");

    rt_bind(0x004125b0u, b_eiffel_reset, "eiffelTowerReset");
    rt_bind(0x00413960u, b_missiles_create, "missilesCreateObjects");
    rt_bind(0x00415b3cu, b_missile_record, "fxRecordGet");
    rt_bind(0x00430414u, b_route_speed_limit, "aiRouteSpeedLimit");
    rt_bind(0x00430b5cu, b_random_debris_burst, "fxRandomDebrisBurst");
    rt_bind(0x004325c8u, b_route_patch_eiffel, "aiRoutePatchEiffelTower");
    rt_bind(0x0043bd74u, b_section_poly_reset, "worldSectionPolyReset");
    rt_bind(0x00451458u, b_fx_sprites_reset, "fxSpritesReset");
    rt_bind(0x00451604u, b_debris_anim_store, "explodeStoreDebrisAnimation");
    rt_bind(0x00453e90u, b_slam_claw_register, "slamClawRegister");
    rt_bind(0x00455e94u, b_track_reset, "worldTrackVehiclesReset");
    rt_bind(0x00455ec0u, b_track_attach, "worldTrackVehicleAttach");
    rt_bind(0x00456c70u, b_track_should_stop, "trackVehiclesShouldStop");
    rt_bind(0x00457398u, b_target_hit_at, "fxTargetHitAt");
    rt_bind(0x0045b488u, b_difficulty_scale, "aiDifficultySpeedScale");
    rt_bind(0x0045ba64u, b_player_car_reset, "playerCarReset");
    rt_bind(0x0045c300u, b_section_moving, "worldSectionHasMovingSurfaces");
    rt_bind(0x0045c350u, b_demo_watch_next, "demoWatchNextAiCar");
    rt_bind(0x0047070cu, b_scene_choice_step, "sceneChoiceStep");
}

RT_BIND_AREA(world)
