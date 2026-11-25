/*
 * bind_match_destroyed.c: carDestroyed 0x0045a39c (src/match/mt_destroyed.c), called the way
 * tools/emu/diff_match_destroyed.py called it: car in EAX (a game address), human in AL.
 *
 * The port's callbacks take uintptr_t registers: host pointers into the address space are passed
 * on as their game addresses. fx_451058 gets a pointer to the port's own host stack buffer
 * (int32 v[3]); the original's buffer is on its stack, so here it is copied into a frame.
 */
#include "rt_env.h"
#include "match/mt_destroyed.h"

static uint32_t g(uintptr_t v)
{
    uintptr_t lo = (uintptr_t)rt_base;
    if (v >= lo && v < lo + RT_SIZE) return (uint32_t)(v - lo);
    return (uint32_t)v;
}

static uint32_t dcall(uint32_t fn, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx, const uint32_t* st, int k)
{
    uint32_t a[4 + 4] = {g(eax), g(edx), g(ebx), g(ecx)};
    if (k > 4) k = 4;
    for (int i = 0; i < k; i++) a[4 + i] = st[i];
    return rt_call(fn, a, 4 + k);
}
#define DCB(nm, addr) \
    static uint32_t nm(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx, const uint32_t* st, int k) \
    { (void)u; return dcall((addr), eax, edx, ebx, ecx, st, k); }

DCB(cb_player_id, 0x00464e84u)
DCB(cb_net_send3, 0x00473650u)
DCB(cb_net_send2, 0x004735b4u)
DCB(cb_pickup_drop, 0x00432340u)
DCB(cb_sound_mode, 0x0046cc54u)
DCB(cb_music_call0, 0x0046f974u)
DCB(cb_music_call, 0x0046f980u)
DCB(cb_get_990, 0x0046f990u)
DCB(cb_get_988, 0x0046f988u)
DCB(cb_sound_play, 0x0046f8b0u)
DCB(cb_loop_stop, 0x0046f898u)
DCB(cb_sound_stop, 0x0046f918u)
DCB(cb_event_type, 0x00474508u)
DCB(cb_object_activate, 0x00492e50u)
DCB(cb_fx_457138, 0x00457138u)
DCB(cb_car_explode, 0x00421920u)
DCB(cb_fx_450984, 0x00450984u)
DCB(cb_damage, 0x004210e4u)
DCB(cb_car_state, 0x00420be8u)

/* 0x00451058 (int32 v[3]): the port's buffer is a host local, copied into a frame */
static uint32_t cb_fx_451058(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx, const uint32_t* st, int k)
{
    (void)u;
    uintptr_t lo = (uintptr_t)rt_base;
    if (eax >= lo && eax < lo + RT_SIZE) return dcall(0x00451058u, eax, edx, ebx, ecx, st, k);
    uint32_t f = rt_frame_push(16);
    memcpy(rt_ptr(f), (const void*)eax, 12);
    uint32_t r = dcall(0x00451058u, f, edx, ebx, ecx, st, k);
    rt_frame_pop(16);
    return r;
}

static uint8_t* d_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

static const MtDestroyed* env(void)
{
    static MtDestroyed e;
    static int done;
    if (!done) {
        e.flag_2f94 = (const int32_t*)rt_ptr(0x00502f94u);
        e.game_mode = (const int32_t*)rt_ptr(0x00502fa4u);
        e.kills = (int32_t*)rt_ptr(0x0050e0d4u);
        e.human_slot = (const int32_t*)rt_ptr(0x00507570u);
        e.ai_slot = (const int32_t*)rt_ptr(0x0050c28cu);
        e.slot_objects = (const uint32_t*)rt_ptr(0x00503048u);
        e.resolve = d_resolve;
        e.player_id = cb_player_id;
        e.net_send3 = cb_net_send3;
        e.net_send2 = cb_net_send2;
        e.pickup_drop = cb_pickup_drop;
        e.sound_mode = cb_sound_mode;
        e.music_call0 = cb_music_call0;
        e.music_call = cb_music_call;
        e.get_990 = cb_get_990;
        e.get_988 = cb_get_988;
        e.sound_play = cb_sound_play;
        e.loop_stop = cb_loop_stop;
        e.sound_stop = cb_sound_stop;
        e.event_type = cb_event_type;
        e.object_activate = cb_object_activate;
        e.fx_457138 = cb_fx_457138;
        e.fx_451058 = cb_fx_451058;
        e.car_explode = cb_car_explode;
        e.fx_450984 = cb_fx_450984;
        e.damage = cb_damage;
        e.car_state = cb_car_state;
        done = 1;
    }
    return &e;
}

static uint32_t b_car_destroyed(const uint32_t* a, int n)
{
    mt_car_destroyed(rt_ptr(rt_arg(a, n, 0)), (uint8_t)rt_arg(a, n, 1), env());
    return 0;
}

void rt_bind_match_destroyed(void)
{
    rt_bind(0x0045a39cu, b_car_destroyed, "carDestroyed");
}

RT_BIND_AREA(match_destroyed)
