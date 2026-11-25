/*
 * bind_carhit.c: the car hit response of src/game/car_hit.c (0x00419604, 0x0041f40c, 0x0041fc3c,
 * 0x00420738, 0x0042026c), the combo detector 0x0044efa0 (car_input.c) and the wheel dust
 * 0x0042f250 (car.c), called the way tools/emu/diff_car_hit.py / diff_car_hit_apply.py /
 * diff_car_hit_recover.py / diff_car_input.py called the originals: the block in eax (game
 * address), the kind flag in dl (player 1, AI 0; the originals test the low byte), then the
 * other registers. The env callbacks call the original addresses with the arguments the
 * harnesses recorded on the original side.
 *
 * Vectors the ports pass as host pointers are handed on as game addresses: a pointer into game
 * memory as it is, a port local copied into a frame first (the original passes its own local).
 *
 * The car contact loop of 0x0041f40c (CarHitApplyEnv.car_contacts) runs the separate port
 * src/game/gp5_contacts.c (the range 0x0041f80e..0x0041fa74).
 *
 * The car / object branch of 0x00419604 (CarHitEnv.hit_entity) runs the separate port
 * src/game/gp6_car_object.c (the range 0x00419d43..0x0041b859) in a frame laid out as the
 * original's (see h_hit_entity).
 *
 * Nothing of these steps is skipped any more, so TM2_CARHIT_STRICT (which made a skipped piece
 * stop the run) is gone.
 *
 * Host state: tm2math's tables and frame ticks / fps, copied from 0x004e8300 / 0x004e8304 before
 * each call (tables from TM2_DATA, default disc/files).
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "game/car.h"
#include "game/car_hit.h"
#include "game/car_input.h"
#include "game/gp5_contacts.h"
#include "game/gp6_car_object.h"
#include "math/tm2math.h"

#define A(i) rt_arg(a, n, (i))

static uint32_t ga(const void* p) { return p ? rt_addr(p) : 0; }
static uint32_t call0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static uint32_t call1(uint32_t fn, uint32_t eax) { uint32_t v[1] = {eax}; return rt_call(fn, v, 1); }
static uint32_t call2(uint32_t fn, uint32_t eax, uint32_t edx) { uint32_t v[2] = {eax, edx}; return rt_call(fn, v, 2); }
static uint32_t call3(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx) { uint32_t v[3] = {eax, edx, ebx}; return rt_call(fn, v, 3); }
static uint32_t call4(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx)
{
    uint32_t v[4] = {eax, edx, ebx, ecx};
    return rt_call(fn, v, 4);
}

static void host(void)
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

/* a vector argument as a game address: in game memory as it is, else copied into a frame
 * (vec_done pops it) */
typedef struct { uint32_t addr, pushed; } Vec;
static Vec vec_arg(const int32_t* p)
{
    Vec v = {0, 0};
    if (!p) return v;
    const uint8_t* b = (const uint8_t*)p;
    if (b >= rt_base && b + 12 <= rt_base + RT_SIZE) { v.addr = rt_addr(p); return v; }
    v.addr = rt_frame_push(12);
    v.pushed = 1;
    memcpy(rt_ptr(v.addr), p, 12);
    return v;
}
static void vec_done(Vec v) { if (v.pushed) rt_frame_pop(12); }

/* The originals pass the car's own position (body +0x20, a pointer into the car block) where
 * the ports pass a copy of it: the effect 0x00450de4 (whose object stays at the turbo block +0x44)
 * and the crash rumble 0x0046f974 get the block address when the copy matches it. cur_car is
 * the car of the running adapter. */
static uint32_t cur_car;
static int cur_player;
static Vec car_pos_arg(const int32_t* pos)
{
    uint32_t cand[2] = {cur_car + (cur_player ? 0x6c4u : 0x708u) + 0x20u, cur_car + 0x6e4u};
    for (int i = 0; i < 2 && cur_car; i++)
        if (memcmp(rt_ptr(cand[i]), pos, 12) == 0) { Vec v = {cand[i], 0}; return v; }
    return vec_arg(pos);
}


/* ---- CarHitEnv (0x00419604) ---- */

/* 0x00450de4(pos, 1) */
static uint32_t h_effect(void* u, const int32_t pos[3])
{
    (void)u;
    Vec p = car_pos_arg(pos);
    uint32_t r = call2(0x00450de4u, p.addr, 1);
    vec_done(p);
    return r;
}
static void h_special(void* u, uint8_t* car, int player, int32_t x, int32_t y) { (void)u; call4(0x004499acu, ga(car), (uint32_t)player, (uint32_t)x, (uint32_t)y); }
static void h_pickup(void* u, int32_t x, int32_t y, int32_t k) { (void)u; call3(0x00470d3cu, (uint32_t)x, (uint32_t)y, (uint32_t)k); }
/* 0x0045a8bc(id a): the original tests al */
static int h_check(void* u, int32_t x) { (void)u; return (uint8_t)call1(0x0045a8bcu, (uint32_t)x); }
static void h_set_state(void* u, uint8_t* car, int player, int kind, int flag) { (void)u; call4(0x00421920u, ga(car), (uint32_t)player, (uint32_t)kind, (uint32_t)flag); }
static void h_event(void* u, int32_t kind, uint8_t* car, int player) { (void)u; call3(0x004543f8u, (uint32_t)kind, ga(car), (uint32_t)player); }
/* 0x00415d1c(type, id a, id b, n, then on the stack body + 0x38, 0); body is in the car block */
static void h_wall_damage(void* u, int32_t type, int32_t x, int32_t y, int32_t k, uint8_t* body)
{
    (void)u;
    uint32_t v[6] = {(uint32_t)type, (uint32_t)x, (uint32_t)y, (uint32_t)k, ga(body), 0};
    rt_call(0x00415d1cu, v, 6);
}
static int32_t h_entity_type(void* u, uint32_t ent) { (void)u; return (int32_t)rt_r32(ent + 0x158u); }
/* the car / object branch (gp6_car_object.c) in a frame of 0x00419604's layout, filled with
 * what that function holds when it enters the branch: +0x6C record index, +0x70 car, +0x1B4 kind,
 * +0x1B8 drop = 0, +0x78 AI car +0x28 (0 for a player car), +0x7C / +0x84 the summary's impact and
 * bounce angles (the wrapped values car_hit.c stored at summary +0x0A / +0x0C), +0x88 the car's
 * index (i16 car +0 / +4), +0x8C controls, +0x90 stats, +0x94 turbo, +0x98 record + 4, +0x9C
 * summary, +0x170 car id (car +0x90 / +0x30); ebp = body. Returns the drop flag +0x1B8. */
static int h_hit_entity(void* u, uint8_t* car, int player, int index)
{
    (void)u;
    uint32_t c = ga(car), fb = rt_frame_push(GP6_FRAME_SIZE);
    uint32_t base = c + (player ? 0x21cu : 0xccu), i = (uint32_t)index;
    uint32_t sum = base + 0x19cu + 0x24u * i;
    rt_w32(fb + 0x6c, i);
    rt_w32(fb + 0x70, c);
    rt_w8(fb + 0x1b4, (uint8_t)(player != 0));
    rt_w8(fb + 0x1b8, 0);
    rt_w32(fb + 0x78, player ? 0 : rt_r32(c + 0x28));
    rt_w32(fb + 0x7c, (uint32_t)(int32_t)(int16_t)rt_r16(sum + 0xa));
    rt_w32(fb + 0x84, (uint32_t)(int32_t)(int16_t)rt_r16(sum + 0xc));
    rt_w32(fb + 0x88, (uint32_t)(int32_t)(int16_t)rt_r16(c + (player ? 0u : 4u)));
    rt_w32(fb + 0x8c, c + (player ? 0x9au : 0x68u));
    rt_w32(fb + 0x90, c + (player ? 0x138u : 0x4ecu));
    rt_w32(fb + 0x94, c + (player ? 0x848u : 0x834u));
    rt_w32(fb + 0x98, base + 0x1cu + 0x20u * i);
    rt_w32(fb + 0x9c, sum);
    rt_w16(fb + 0x170, rt_r16(c + (player ? 0x90u : 0x30u)));
    En2Env e = rt_en2env();
    gp6_car_object_hit(&e, fb, c + (player ? 0x6c4u : 0x708u));
    rt_en2env_done();
    int drop = rt_r8(fb + 0x1b8) != 0;
    rt_frame_pop(GP6_FRAME_SIZE);
    return drop;
}

/* 0x00419604(car, player in dl, quiet in bl) */
static uint32_t b_car_hits_resolve(const uint32_t* a, int n)
{
    static const CarHitEnv env = {h_effect, h_special, h_pickup, h_check, h_set_state, h_event,
                                  h_wall_damage, h_entity_type, h_hit_entity, NULL};
    host();
    uint32_t sc = cur_car; int sp = cur_player;
    cur_car = A(0); cur_player = (uint8_t)A(1) != 0;
    car_hits_resolve(rt_ptr(A(0)), cur_player, (uint8_t)A(2) != 0, &env);
    cur_car = sc; cur_player = sp;
    return 0;
}

/* ---- CarHitApplyEnv (0x0041f40c, 0x0041fc3c, 0x00420738) ---- */

/* 0x0041fa98(active, summary) */
static void p_pass(void* u, uint8_t* car, int player, uint8_t* act, uint8_t* sum) { (void)u; (void)car; (void)player; call2(0x0041fa98u, ga(act), ga(sum)); }
/* the contact loop (gp5_contacts.c): 0x0045b028(id, &kind) with the kind byte in a frame,
 * 0x00421920, 0x0041fa98(contact, active); the point count is the model slot's +0xF4 */
static uint32_t cur_slot;
static uint8_t* c_car_by_id(void* u, int32_t id, uint8_t* is_player)
{
    (void)u;
    uint32_t f = rt_frame_push(4);
    rt_w8(f, *is_player);
    uint32_t c = call2(0x0045b028u, (uint32_t)id, f);
    *is_player = rt_r8(f);
    rt_frame_pop(4);
    return c ? rt_ptr(c) : NULL;
}
static void c_pass(void* u, uint8_t* contact, uint8_t* act) { (void)u; call2(0x0041fa98u, ga(contact), ga(act)); }
static void p_contacts(void* u, uint8_t* car, int player)
{
    (void)u;
    static const Gp5ContactEnv ce = {c_car_by_id, h_set_state, c_pass, NULL};
    gp5_car_contacts(car, player, (int32_t)rt_r32(cur_slot + 0xf4u), &ce);
}
/* the crash rumble: 0x0046f974(pos) when the split mode 0x0046cc54 is 0, else 0x0046f980(pos, -1);
 * then, when shown, sounds 5 and 3 at pan 0x0046f988 / volume 0x0046f990 (0x0046f8b0) */
static void p_crash(void* u, const int32_t pos[3], int show, int32_t s5, int32_t s3)
{
    (void)u;
    Vec p = car_pos_arg(pos);
    if (call0(0x0046cc54u) == 0) call1(0x0046f974u, p.addr);
    else call2(0x0046f980u, p.addr, 0xffffffffu);
    vec_done(p);
    if (show) {
        uint32_t vol = call0(0x0046f990u), pan = call0(0x0046f988u);
        call4(0x0046f8b0u, 5, pan, vol, (uint32_t)s5);
        vol = call0(0x0046f990u);
        pan = call0(0x0046f988u);
        call4(0x0046f8b0u, 3, pan, vol, (uint32_t)s3);
    }
}
/* 0x0045b154(index, player) then 0x004210e4(that, amount, normal, 0, then on the stack other) */
static void p_damage(void* u, int32_t index, int player, int32_t amount, const int32_t* normal, int32_t other)
{
    (void)u;
    uint32_t who = call2(0x0045b154u, (uint32_t)index, (uint32_t)player);
    Vec nv = vec_arg(normal);
    uint32_t v[5] = {who, (uint32_t)amount, nv.addr, 0, (uint32_t)other};
    rt_call(0x004210e4u, v, 5);
    vec_done(nv);
}
/* 0x00420be8(car, kind, 2, 2, then on the stack player) */
static void p_crash_state(void* u, uint8_t* car, int kind, int player)
{
    (void)u;
    uint32_t v[5] = {ga(car), (uint32_t)kind, 2, 2, (uint32_t)player};
    rt_call(0x00420be8u, v, 5);
}
/* 0x00450718(pos, 0) */
static void p_sparks(void* u, const int32_t pos[3])
{
    (void)u;
    Vec p = vec_arg(pos);
    call2(0x00450718u, p.addr, 0);
    vec_done(p);
}

static const CarHitApplyEnv APPLY = {p_pass, p_contacts, p_crash, p_damage, p_crash_state, p_sparks, NULL};

/* 0x0041f40c(car, player in dl): the point count is the car model's (+0xF4 of 0x0045ae24(i16 car
 * +0) for a player car, 0x0045ae2c(i16 car +4) for an AI car), looked up as the original does */
static uint32_t b_car_hits_gather(const uint32_t* a, int n)
{
    host();
    uint32_t car = A(0);
    int player = (uint8_t)A(1) != 0;
    uint32_t slot = call1(player ? 0x0045ae24u : 0x0045ae2cu, (uint32_t)(int32_t)(int16_t)rt_r16(car + (player ? 0u : 4u)));
    uint32_t sc = cur_car, ss = cur_slot; int sp = cur_player;
    cur_car = car; cur_player = player; cur_slot = slot;
    car_hits_gather(rt_ptr(car), player, (int32_t)rt_r32(slot + 0xf4u), &APPLY);
    cur_car = sc; cur_player = sp; cur_slot = ss;
    return 0;
}
/* 0x0041fc3c(car, player in dl) */
static uint32_t b_car_hit_push(const uint32_t* a, int n)
{
    host();
    car_hit_push(rt_ptr(A(0)), (uint8_t)A(1) != 0, &APPLY);
    return 0;
}
/* 0x00420738(car, player in dl, car hit in bl, angle, then on the stack index) */
static uint32_t b_car_hit_turn(const uint32_t* a, int n)
{
    host();
    car_hit_turn(rt_ptr(A(0)), (uint8_t)A(1) != 0, (uint8_t)A(2), (int32_t)A(3), (int32_t)A(4), &APPLY);
    return 0;
}

/* ---- CarRecoverEnv (0x0042026c) ---- */

/* grp_ground_height 0x0042c2d4(pos) */
static int32_t r_ground(void* u, const int32_t pos[3])
{
    (void)u;
    Vec p = vec_arg(pos);
    int32_t r = (int32_t)call1(0x0042c2d4u, p.addr);
    vec_done(p);
    return r;
}

/* 0x0042026c(car, player in dl): the car pair push table at 0x0050d0d8 */
static uint32_t b_car_hit_recover(const uint32_t* a, int n)
{
    host();
    CarRecoverEnv e = {(const int32_t*)(const void*)rt_ptr(0x0050d0d8u), r_ground, NULL};
    car_hit_recover(rt_ptr(A(0)), (uint8_t)A(1) != 0, &e);
    return 0;
}

/* ---- car_input.c: 0x0044efa0(combo block = car +0x7F0) ---- */

/* input_check_ready 0x004936ac(id, slot) */
static uint8_t in_input(void* u, int id, int slot) { (void)u; return (uint8_t)call2(0x004936acu, (uint32_t)id, (uint32_t)slot); }

/* the combo detector only reads the inputs (no events, no spectator state) */
static uint32_t b_car_combo_update(const uint32_t* a, int n)
{
    host();
    CarInputEnv e;
    memset(&e, 0, sizeof e);
    e.input = in_input;
    car_combo_update(rt_ptr(A(0)), &e);
    return 0;
}

/* ---- car.c: 0x0042f250(probe state, threshold) ---- */

/* the dust effect 0x00450718(probe position + 16 up, 0) (car.h CAR_FX_DUST: the "car" is the
 * probe state, position at +0x3C), in a local like the original's */
static void fx_cb(void* u, int id, uint8_t* car, int player, int32_t arg)
{
    (void)u; (void)player; (void)arg;
    if (id != CAR_FX_DUST) rt_stop("bind_carhit: unexpected car effect", (uint32_t)id);
    uint32_t p = rt_frame_push(12);
    memcpy(rt_ptr(p), car + 0x3c, 12);
    rt_w32(p + 8, rt_r32(p + 8) + 0x10u);
    call2(0x00450718u, p, 0);
    rt_frame_pop(12);
}

/* the original takes only the probe state and the threshold; the port's car / player arguments
 * are unused by the dust step */
static uint32_t b_car_wheel_dust(const uint32_t* a, int n)
{
    static CarFx f = {fx_cb, NULL, 0, {0, 0}, 0};
    host();
    car_wheel_dust(rt_ptr(A(0)), (int32_t)A(1), &f, NULL, 0);
    return 0;
}

void rt_bind_carhit(void)
{
    rt_bind(0x00419604u, b_car_hits_resolve, "car_hits_resolve");
    rt_bind(0x0041f40cu, b_car_hits_gather, "car_hits_gather");
    rt_bind(0x0041fc3cu, b_car_hit_push, "car_hit_push");
    rt_bind(0x00420738u, b_car_hit_turn, "car_hit_turn");
    rt_bind(0x0042026cu, b_car_hit_recover, "car_hit_recover");
    rt_bind(0x0044efa0u, b_car_combo_update, "car_combo_update");
    rt_bind(0x0042f250u, b_car_wheel_dust, "car_wheel_dust");
}

RT_BIND_AREA(carhit)
