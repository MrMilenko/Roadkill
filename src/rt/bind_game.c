/*
 * bind_game.c: the car driving model, chase camera, player car input / spawn / update, the front
 * end's bitmap fonts, scene engine, TIM backgrounds and the spawn picker (src/game/: car.c,
 * camera.c, car_input.c, car_spawn.c, car_update.c, font.c, scene.c, shell_bg.c, spawn.c), called
 * the way their harnesses (tools/emu/diff_car*.py, diff_camera.py, diff_font.py, diff_scene.py,
 * diff_shell_bg.py, diff_spawn.py) called them.
 *
 * These are the older ports with host pointer context structs: their globals are rt_ptr() of the
 * original's addresses, value fields are read from game memory on every call, and every callback
 * is the original call (rt_call, EAX, EDX, EBX, ECX, then the stack words).
 *
 * Not bound here:
 *   0x00437850 player_car_setup: only its camera part is ported (car_camera_init); the rest
 *              (spawn, weapons, model slot) is not.
 *   0x0042a020 car_terrain_step: only its first part (to 0x0042a4e7, car_ground_contact) is
 *              ported; the surface effects after it are not.
 */
#include <stdio.h>
#include <stdlib.h>

#include "rt_env.h"
#include "qol_cars.h"
#include "assets/exe_carstats.h"
#include "game/camera.h"
#include "game/car.h"
#include "game/car_input.h"
#include "game/car_spawn.h"
#include "game/car_update.h"
#include "game/font.h"
#include "game/scene.h"
#include "game/shell_bg.h"
#include "game/spawn.h"
#include "math/tm2math.h"

#define A(i) rt_arg(a, n, (i))

static uint32_t ga(const void* p) { return p ? rt_addr(p) : 0; }
static uint8_t* hp(uint32_t a) { return a ? rt_ptr(a) : NULL; }

static uint32_t call0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static uint32_t call2(uint32_t fn, uint32_t eax, uint32_t edx) { uint32_t a[2] = {eax, edx}; return rt_call(fn, a, 2); }
static uint32_t call3(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx) { uint32_t a[3] = {eax, edx, ebx}; return rt_call(fn, a, 3); }
static uint32_t call4(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx)
{
    uint32_t a[4] = {eax, edx, ebx, ecx};
    return rt_call(fn, a, 4);
}

/* The host side state these ports keep instead of the original's globals: tm2math's tables (sin
 * from SINTAB.DAT, atan from TM2.EXE), the car stats table (from TM2.EXE), the frame ticks / fps.
 * Loaded once from TM2_DATA (default disc/files, where tm2 finds TM2.EXE by default), refreshed
 * from game memory before every call. (rand: crt_rand 0x004899c2 is bound to tm2_rand by the
 * core, so the host rand state is the game's.) */
static void gsync(void)
{
    static int loaded;
    if (!loaded) {
        const char* dir = getenv("TM2_DATA");
        char s[1024], x[1024];
        if (!dir || !*dir) dir = "disc/files";
        snprintf(s, sizeof s, "%s/SINTAB.DAT", dir);
        snprintf(x, sizeof x, "%s/TM2.EXE", dir);
        if (tm2_math_load_tables(s, x) != 0) rt_stop("cannot load the math tables (TM2_DATA)", 0);
        if (tm2_car_stats_load(x) != 0) rt_stop("cannot load the car stats (TM2_DATA)", 0);
        loaded = 1;
    }
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
    tm2_set_frame_fps((int32_t)rt_r32(0x004e8304u));
}

/* ---- the driving model's effects (CarFx, car.h CAR_FX_*) ---- */

static uint32_t body20(uint8_t* car, int player) { return ga(car) + car_layout(player)->body + 0x20u; }

static void fx_cb(void* u, int id, uint8_t* car, int player, int32_t arg)
{
    (void)u;
    uint32_t c = ga(car), v = (uint32_t)arg;
    switch (id) {
    case CAR_FX_SKID: call2(0x004516bcu, 1, body20(car, player)); break;
    case CAR_FX_SPARKS: call2(0x004512b4u, body20(car, player), 0); break;
    case CAR_FX_SCREEN: call4(0x0046f8b0u, 6, 0, 0, v); break;
    case CAR_FX_SPIN_HIT: call4(0x00421920u, c, (uint32_t)player, 4, 1); break;
    /* the original: 0x0046f974(pos) when the split mode 0x0046cc54 is 0, else 0x0046f980(pos, -1):
     * both 0x004b8010(pos, arg) */
    case CAR_FX_LAND: call2(0x004b8010u, c + 0x6e4u, v); break;
    case CAR_FX_SHAKE_HARD:
    case CAR_FX_SHAKE: {
        /* sound 5 / 3 at volume 0x0046f990, pan 0x0046f988, priority arg */
        uint32_t vol = call0(0x0046f990u);
        uint32_t pan = call0(0x0046f988u);
        call4(0x0046f8b0u, id == CAR_FX_SHAKE_HARD ? 5u : 3u, pan, vol, v);
        break;
    }
    case CAR_FX_DUST: {
        /* car is the probe state here: its position (+0x3C) 16 up, in a local like the original's */
        uint32_t p = rt_frame_push(12);
        memcpy(rt_ptr(p), car + 0x3c, 12);
        rt_w32(p + 8, rt_r32(p + 8) + 0x10u);
        call2(0x00450718u, p, 0);
        rt_frame_pop(12);
        break;
    }
    case CAR_FX_SET_STATE: call2(0x004225a0u, c, v); break;
    case CAR_FX_TURBO_FLAME: call2(0x00450c08u, body20(car, player), 0); break;
    case CAR_FX_TURBO_SMOKE: call2(0x004512f8u, body20(car, player), 0); break;
    case CAR_FX_RUMBLE_STOP: call2(0x0046f860u, 0, v); break;
    case CAR_FX_RUMBLE: { uint32_t s[5] = {0, 0, 0, 99, v}; rt_call(0x0046f840u, s, 5); break; }
    case CAR_FX_TURBO_OBJ: rt_w16(v + 0xcu, 0); break;
    case CAR_FX_TRANSITION: call4(0x0046f8b0u, v, 0, 0, 99); break;
    default: rt_stop("car fx id", (uint32_t)id);
    }
}

static const CarFx* carfx(void)
{
    static CarFx f = {fx_cb, NULL, 0, {0, 0}, 0};
    f.land_flag = (int)rt_r32(0x00520e90u);
    f.rumble[0] = rt_r8(0x00621184u);
    f.rumble[1] = rt_r8(0x00621185u);
    f.ai_turbo_mode = (int32_t)rt_r32(0x00502f98u);   /* 0x0045b3dc */
    return &f;
}

#define SURFACES 0x004fdc76u   /* GRP 0x004fca18 + 0x125E */

/* ---- car.c ---- */

/* 0x00418cf0 (body, controls) */
static uint32_t b_car_snapshot(const uint32_t* a, int n) { gsync(); car_snapshot(rt_ptr(A(0)), rt_ptr(A(1))); return 0; }
/* 0x00418450 / 0x00419168 (car, player) */
static uint32_t b_car_orient(const uint32_t* a, int n) { gsync(); car_orient(rt_ptr(A(0)), (int)A(1)); return 0; }
static uint32_t b_car_heading_follow(const uint32_t* a, int n) { gsync(); car_heading_follow(rt_ptr(A(0)), (int)A(1)); return 0; }
/* 0x004168e4 (car, player) */
static uint32_t b_car_drive_input(const uint32_t* a, int n) { gsync(); car_drive_input(rt_ptr(A(0)), (int)A(1), carfx()); return 0; }
/* 0x0042d470 (car, player) */
static uint32_t b_car_drive(const uint32_t* a, int n)
{
    gsync();
    car_drive(rt_ptr(A(0)), (int)A(1), rt_ptr(SURFACES), carfx());
    return 0;
}

/* ---- camera.c ---- */

static void cam_view_set(void* u, const int32_t pos[3], const int16_t ang[3]) { (void)u; call3(0x0045b1e8u, 0, ga(pos), ga(ang)); }
static void cam_view_out(void* u, const uint8_t rot[8], const int32_t pos[3])
{
    (void)u;
    call2(0x00494cdcu, ga(rot), 0);
    /* pos is the port's local: copied into a frame, as the original passes its own local */
    uint32_t p = rt_frame_push(12);
    memcpy(rt_ptr(p), pos, 12);
    call2(0x00494d34u, p, 0);
    rt_frame_pop(12);
}

/* 0x0043cdc0 (car). The original's timer_get_elapsed 0x00493038 calls (profiling, result unused)
 * are not made. */
static uint32_t b_car_chase_camera(const uint32_t* a, int n)
{
    gsync();
    CamEnv e;
    e.watched = rt_ptr(call0(0x0045b0a0u));
    e.split = (int)call0(0x0046cc54u);
    e.multiplayer = (int)call0(0x004632b0u);
    e.lock_frames = (int16_t*)(void*)rt_ptr(0x004fef24u);
    e.prev_pos = (int32_t*)(void*)rt_ptr(0x004fef18u);
    e.fixed_pos = (const int32_t*)(const void*)rt_ptr(0x004fef04u);
    e.fixed_ang = (const int16_t*)(const void*)rt_ptr(0x004fef10u);
    e.view_set = cam_view_set;
    e.view_out = cam_view_out;
    e.user = NULL;
    car_chase_camera(rt_ptr(A(0)), &e);
    return 0;
}

/* ---- car_input.c ---- */

static uint8_t in_input(void* u, int id, int slot) { (void)u; return (uint8_t)call2(0x004936acu, (uint32_t)id, (uint32_t)slot); }
static void in_event(void* u, int id, uint8_t* car, int32_t x, int32_t y, int32_t z)
{
    (void)u;
    switch (id) {
    case CAR_IN_SET_STATE: call2(0x004225a0u, car ? ga(car) : 0x005030a8u, (uint32_t)x); break;
    case CAR_IN_SOUND: call2(0x00436040u, (uint32_t)x, 0); break;
    case CAR_IN_SCREEN: call4(0x0046f8b0u, (uint32_t)x, 0, 0, (uint32_t)y); break;
    case CAR_IN_CAMERA: call3(0x00470320u, (uint32_t)x, (uint32_t)y, (uint32_t)z); break;
    default: rt_stop("car input event id", (uint32_t)id);
    }
}

#define IN_SPECTATE (0x004ffedcu + 4u * 0x20u)

/* 0x00417734 (car). The spectator globals are copied in and written back. */
static uint32_t b_car_read_input(const uint32_t* a, int n)
{
    gsync();
    CarInputEnv e;
    e.input = in_input;
    e.event = in_event;
    e.user = NULL;
    e.watch_player = (int32_t)rt_r32(0x00502f8cu);
    e.watch_ai_flag = (int32_t)rt_r32(0x00502f88u);
    e.watch_ai = (int32_t)rt_r32(0x00502f90u);
    e.mode = (int32_t)rt_r32(0x00502fa4u);
    e.ai_count = (int32_t)rt_r32(0x0050c2b0u);
    e.player_count = (int32_t)rt_r32(0x005075e0u);
    e.spectate = (int32_t)rt_r32(IN_SPECTATE);
    car_read_input(rt_ptr(A(0)), &e);
    rt_w32(0x00502f8cu, (uint32_t)e.watch_player);
    rt_w32(0x00502f88u, (uint32_t)e.watch_ai_flag);
    rt_w32(0x00502f90u, (uint32_t)e.watch_ai);
    rt_w32(0x00502fa4u, (uint32_t)e.mode);
    rt_w32(0x0050c2b0u, (uint32_t)e.ai_count);
    rt_w32(0x005075e0u, (uint32_t)e.player_count);
    rt_w32(IN_SPECTATE, (uint32_t)e.spectate);
    return 0;
}

/* ---- car_spawn.c ---- */

static void sp_rotate(void* u, int which, int32_t angle) { (void)u; call2(which ? 0x00453ed0u : 0x00453f10u, (uint32_t)angle, 0); }
static uint8_t* sp_car_at(void* u, int32_t i) { (void)u; return rt_ptr(0x005030a8u + (uint32_t)i * CAR_BLOCK_SIZE); }
static uint8_t* sp_entity(void* u, uint8_t* car)
{
    (void)u;
    int16_t slot = (int16_t)(car[0] | car[1] << 8);
    return hp(rt_r32(0x00503088u + 4u * (uint32_t)(int32_t)slot));
}

/* 0x00437ac4 (car) */
static uint32_t b_car_spawn_player(const uint32_t* a, int n)
{
    gsync();
    CarSpawnEnv e;
    e.nodes = rt_ptr(0x004fe1e4u);
    e.node_count = (int32_t)rt_r32(0x004fe1b0u);
    e.spawns = (SpawnList*)(void*)rt_ptr(0x004fe1b4u);
    e.spawn_count = rt_r8(0x004fe1e1u);
    e.mode = (int32_t)rt_r32(0x00502fa4u);
    e.grp = rt_ptr(0x004fca18u);
    e.surfaces = rt_ptr(SURFACES);
    e.fx = carfx();
    e.multiplayer = (int)call0(0x004632b0u);
    e.rotate = sp_rotate;
    e.car_count = (int32_t)rt_r32(0x005075e0u);
    e.car_at = sp_car_at;
    e.entity = sp_entity;
    e.user = NULL;
    car_spawn_player(rt_ptr(A(0)), &e);
    return 0;
}

/* ---- car_update.c ---- */

#define UP_STEP(nm, addr) static void nm(void* u, uint8_t* car) { (void)u; call2((addr), ga(car), 1); }
UP_STEP(up_drive_input, 0x004168e4u)
UP_STEP(up_wheels_place, 0x0042c4d4u)
UP_STEP(up_terrain, 0x0042a020u)
UP_STEP(up_4305fc, 0x004305fcu)
UP_STEP(up_420e48, 0x00420e48u)
UP_STEP(up_430bb4, 0x00430bb4u)
UP_STEP(up_4316b4, 0x004316b4u)
UP_STEP(up_drive, 0x0042d470u)
UP_STEP(up_422704, 0x00422704u)
UP_STEP(up_453c78, 0x00453c78u)
UP_STEP(up_43e6e0, 0x0043e6e0u)
UP_STEP(up_417640, 0x00417640u)
static void up_pickup(void* u, int32_t x, int32_t y) { (void)u; call3(0x00432340u, (uint32_t)x, (uint32_t)y, 1); }
static void up_453538(void* u, int32_t m, int32_t s) { (void)u; call2(0x00453538u, (uint32_t)m, (uint32_t)s); }
static void up_init_model(void* u, uint8_t* car, int32_t m) { (void)u; call3(0x00437850u, ga(car), (uint32_t)m, 1); }
static void up_sound(void* u, int32_t id) { (void)u; call2(0x00436040u, (uint32_t)id, 0); }
static void up_respawn(void* u, int32_t m, int32_t x) { (void)u; call2(0x00470940u, (uint32_t)m, (uint32_t)x); }
static uint8_t* up_object(void* u, uint32_t addr) { (void)u; return hp(addr); }

/* 0x00415f00 (car). The original's timer_get_elapsed calls around the terrain steps (profiling,
 * result unused) are not made. */
static uint32_t b_car_player_update(const uint32_t* a, int n)
{
    gsync();
    static CarUpdateOps o = {up_drive_input, up_wheels_place, up_terrain, up_4305fc, up_420e48, up_430bb4,
                             up_4316b4, up_drive, up_422704, up_453c78, up_43e6e0, up_417640, up_pickup,
                             up_453538, up_init_model, up_sound, up_respawn, up_object, NULL, 0, 0, 0, 0};
    o.event_type = rt_r8(0x00624ec7u);
    o.multiplayer = (int)call0(0x004632b0u);
    o.land_flag = (int32_t)rt_r32(0x00520e90u);
    o.frame_parity = (int32_t)rt_r32(0x00a0364cu);
    qolc_player_frame(A(0));   /* [gameplay] play_dark_tooth (qol_cars.h) */
    car_player_update(rt_ptr(A(0)), &o);
    return 0;
}

/* ---- font.c ---- */

static TM2Font font_of(uint32_t font)
{
    TM2Font f = {hp(rt_r32(0x005027f8u + 4u * font)), (int32_t)rt_r32(0x005025d8u + 4u * font)};
    return f;
}

static void font_draw(void* u, uint8_t* sprite) { (void)u; uint32_t x[1] = {ga(sprite)}; rt_call(0x004967f4u, x, 1); }

static TM2FontPrint font_printer(uint32_t font)
{
    TM2FontPrint p;
    p.x = (int32_t*)(void*)rt_ptr(0x005024d0u);
    p.y = (int32_t*)(void*)rt_ptr(0x005024d4u);
    p.base_u = rt_r8(0x005024d8u + 8u * font);
    p.base_v = rt_r8(0x005024dcu + 8u * font);
    p.sprite = rt_ptr(0x00502518u + 0x18u * font);
    p.draw = font_draw;
    p.user = NULL;
    return p;
}

/* 0x00451a9c (font, s) */
static uint32_t b_font_string_width(const uint32_t* a, int n)
{
    TM2Font f = font_of(A(0));
    return (uint32_t)font_string_width(&f, rt_ptr(A(1)));
}
/* 0x00451f54 (font, s) */
static uint32_t b_font_print(const uint32_t* a, int n)
{
    TM2Font f = font_of(A(0));
    TM2FontPrint p = font_printer(A(0));
    return (uint32_t)font_print(&f, &p, rt_ptr(A(1)));
}
/* 0x00452404 (font, unused, out, s) */
static uint32_t b_font_sprite_print(const uint32_t* a, int n)
{
    TM2Font f = font_of(A(0));
    TM2FontPrint p = font_printer(A(0));
    return (uint32_t)font_sprite_print(&f, &p, rt_ptr(A(2)), rt_ptr(A(3)));
}
/* 0x00452204 / 0x0045222c (font, x, y, unused, then on the stack out, s) */
static uint32_t b_font_sprite_print_xy(const uint32_t* a, int n)
{
    TM2Font f = font_of(A(0));
    TM2FontPrint p = font_printer(A(0));
    font_sprite_print_xy(&f, &p, (int32_t)A(1), (int32_t)A(2), rt_ptr(A(4)), rt_ptr(A(5)));
    return 0;
}
static uint32_t b_font_sprite_print_centered(const uint32_t* a, int n)
{
    TM2Font f = font_of(A(0));
    TM2FontPrint p = font_printer(A(0));
    font_sprite_print_centered(&f, &p, (int32_t)A(1), (int32_t)A(2), rt_ptr(A(4)), rt_ptr(A(5)));
    return 0;
}

/* ---- scene.c ---- */

/* 0x00469c40 (rec, name, value): returns rec */
static uint32_t b_scene_image_init(const uint32_t* a, int n)
{
    return ga(scene_image_init(rt_ptr(A(0)), (const char*)rt_ptr(A(1)), A(2)));
}
/* 0x00469cdc (obj): the node at obj +4 */
static uint32_t b_scene_choice_get(const uint32_t* a, int n)
{
    return (uint8_t)scene_choice_get(hp(rt_r32(A(0) + 4u)));
}
/* 0x0046a0d8 (scene, rect or 0) */
static uint32_t b_scene_request_redraw(const uint32_t* a, int n)
{
    scene_request_redraw(rt_ptr(A(0)), (const int32_t*)(const void*)hp(A(1)));
    return 0;
}
static uint32_t b_scene_add_aux(const uint32_t* a, int n) { return (uint32_t)scene_add_aux(rt_ptr(A(0)), A(1)); }
static uint32_t b_scene_add_decor(const uint32_t* a, int n) { return (uint32_t)scene_add_decor(rt_ptr(A(0)), A(1)); }
static uint32_t b_scene_add_item(const uint32_t* a, int n) { return (uint32_t)scene_add_item(rt_ptr(A(0)), A(1)); }
/* 0x0046a18c (scene, event, source, type, then on the stack command) */
static uint32_t b_scene_add_action(const uint32_t* a, int n)
{
    return (uint32_t)scene_add_action(rt_ptr(A(0)), (uint8_t)A(1), A(2), (uint8_t)A(3), A(4));
}
/* 0x0046a2e4 (scene, event, source) */
static uint32_t b_scene_post_event(const uint32_t* a, int n)
{
    return (uint32_t)scene_post_event(rt_ptr(A(0)), (uint8_t)A(1), A(2));
}
/* a command's run: its vtable (+8) entry 0, with the command in EAX */
static void scene_run_once(void* u, uint32_t cmd)
{
    (void)u;
    uint32_t x[1] = {cmd};
    rt_call(rt_r32(rt_r32(cmd + 8u)), x, 1);
}
/* 0x0046a36c (scene) */
static uint32_t b_scene_dispatch_events(const uint32_t* a, int n)
{
    scene_dispatch_events(rt_ptr(A(0)), scene_run_once, NULL);
    return 0;
}

/* ---- shell_bg.c ---- */

#define BG_BUF 0x0058a998u
#define BG_SIZE 0x96014u    /* the buffer's size (shell_bg.h) */

/* 0x0046f784 (name). The harness stubbed the file read; here it is the original's call:
 * file_load_resource 0x00498d18(buffer, name, ebx = a 12 byte local of the original's frame),
 * then the port converts the buffer to the display format 0x00496284 (al: 1 = 565). */
static uint32_t b_bg_tim_load(const uint32_t* a, int n)
{
    uint32_t loc = rt_frame_push(0x18);
    call3(0x00498d18u, BG_BUF, qolc_info_tim(A(0)), loc + 0xcu);   /* + play_dark_tooth (qol_cars.h) */
    rt_frame_pop(0x18);
    int to_565 = (uint8_t)call0(0x00496284u) != 0;
    return (uint32_t)bg_tim_load(rt_ptr(BG_BUF), BG_SIZE, to_565);
}

/* The TM2Display bg_tim_draw locks: the original's screen lock 0x004961b8 (sets the screen
 * pointer 0x00a04674 and pitch 0x00a04670) and unlock 0x004961d4. */
static int bg_lock(TM2Display* d)
{
    call0(0x004961b8u);
    d->pixels = hp(rt_r32(0x00a04674u));
    d->pitch = (int32_t)rt_r32(0x00a04670u);
    return 0;
}
static void bg_unlock(TM2Display* d) { (void)d; call0(0x004961d4u); }

/* 0x0046f7c4 (). The original returns what the unlock wrapper leaves in eax (nothing reads it): 0. */
static uint32_t b_bg_tim_draw(const uint32_t* a, int n)
{
    (void)a; (void)n;
    TM2Display d;
    memset(&d, 0, sizeof d);
    d.width = (int32_t)call0(0x004958d8u);
    d.height = (int32_t)call0(0x004958e4u);
    d.format_565 = (uint8_t)call0(0x00496284u) != 0;
    d.lock = bg_lock;
    d.unlock = bg_unlock;
    d.present = NULL;
    bg_tim_draw(rt_ptr(BG_BUF), BG_SIZE, &d);
    return 0;
}

/* ---- spawn.c ---- */

/* 0x0043289c (count): the spawn list 0x004fe1b4 */
static uint32_t b_spawn_pick(const uint32_t* a, int n)
{
    return (uint32_t)spawn_pick((SpawnList*)(void*)rt_ptr(0x004fe1b4u), (int32_t)A(0));
}

void rt_bind_game(void)
{
    rt_bind(0x00418cf0u, b_car_snapshot, "car_snapshot");
    rt_bind(0x00418450u, b_car_orient, "car_orient");
    rt_bind(0x00419168u, b_car_heading_follow, "car_heading_follow");
    rt_bind(0x004168e4u, b_car_drive_input, "car_drive_input");
    rt_bind(0x0042d470u, b_car_drive, "car_drive");
    rt_bind(0x0043cdc0u, b_car_chase_camera, "car_chase_camera");
    rt_bind(0x00417734u, b_car_read_input, "car_read_input");
    rt_bind(0x00437ac4u, b_car_spawn_player, "car_spawn_player");
    rt_bind(0x00415f00u, b_car_player_update, "car_player_update");
    rt_bind(0x00451a9cu, b_font_string_width, "font_string_width");
    rt_bind(0x00451f54u, b_font_print, "font_print");
    rt_bind(0x00452404u, b_font_sprite_print, "font_sprite_print");
    rt_bind(0x00452204u, b_font_sprite_print_xy, "font_sprite_print_xy");
    rt_bind(0x0045222cu, b_font_sprite_print_centered, "font_sprite_print_centered");
    rt_bind(0x00469c40u, b_scene_image_init, "scene_image_init");
    rt_bind(0x00469cdcu, b_scene_choice_get, "scene_choice_get");
    rt_bind(0x0046a0d8u, b_scene_request_redraw, "scene_request_redraw");
    rt_bind(0x0046a114u, b_scene_add_aux, "scene_add_aux");
    rt_bind(0x0046a138u, b_scene_add_decor, "scene_add_decor");
    rt_bind(0x0046a15cu, b_scene_add_item, "scene_add_item");
    rt_bind(0x0046a18cu, b_scene_add_action, "scene_add_action");
    rt_bind(0x0046a2e4u, b_scene_post_event, "scene_post_event");
    rt_bind(0x0046a36cu, b_scene_dispatch_events, "scene_dispatch_events");
    rt_bind(0x0046f784u, b_bg_tim_load, "bg_tim_load");
    rt_bind(0x0046f7c4u, b_bg_tim_draw, "bg_tim_draw");
    rt_bind(0x0043289cu, b_spawn_pick, "spawn_pick");
}

RT_BIND_AREA(game)
