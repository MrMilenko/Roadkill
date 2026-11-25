/*
 * bind_match_bind.c: carBindHumanSlot 0x0045803c (src/match/mt_bind.c), called the way
 * tools/emu/diff_match_bind.py called it: (i, key) in EAX, EDX. Car pointers the port hands its
 * callbacks are host pointers into the address space: passed on as their game addresses.
 */
#include "rt_env.h"
#include "match/mt_bind.h"

static uint32_t c4(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    uint32_t a[4] = {eax, edx, ebx, 0};
    return rt_call(fn, a, 4);
}

static uint8_t* b_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static void cb_car_init_entity(void* u, uint8_t* car) { (void)u; c4(0x00422e04u, rt_addr(car), 0, 0); }
static uint8_t cb_is_multiplayer(void* u) { (void)u; return (uint8_t)c4(0x004632b0u, 0, 0, 0); }
static void cb_car_setup(void* u, uint8_t* car, int32_t key, uint32_t handicap)
{
    (void)u;
    c4(0x00437850u, rt_addr(car), (uint32_t)key, handicap);
}
static void cb_car_set_state(void* u, uint8_t* car, int32_t s) { (void)u; c4(0x004225a0u, rt_addr(car), (uint32_t)s, 0); }
static void cb_object_activate(void* u, uint32_t o, int32_t on) { (void)u; c4(0x00492e50u, o, (uint32_t)on, 0); }

static const MtBind* env(void)
{
    static MtBind b;
    static int done;
    if (!done) {
        b.human = rt_ptr(0x005030a8u);
        b.slot_count = (int32_t*)rt_ptr(0x00503084u);
        b.slot_owner = (int16_t*)rt_ptr(0x00503028u);
        b.slot_objects = (uint32_t*)rt_ptr(0x00503048u);
        b.slot_index = (int32_t*)rt_ptr(0x00502fecu);
        b.slot_kind = rt_ptr(0x00502fdcu);
        b.human_objects = (uint32_t*)rt_ptr(0x00503088u);
        b.human_slot = (int32_t*)rt_ptr(0x00507570u);
        b.human_key = (int16_t*)rt_ptr(0x005075b0u);
        b.handicap = rt_ptr(0x00502f80u);
        b.keep_208 = rt_ptr(0x0050e0fcu);
        b.singleplayer = rt_ptr(0x00502f9cu);
        b.resolve = b_resolve;
        b.car_init_entity = cb_car_init_entity;
        b.is_multiplayer = cb_is_multiplayer;
        b.car_setup = cb_car_setup;
        b.car_set_state = cb_car_set_state;
        b.object_activate = cb_object_activate;
        done = 1;
    }
    return &b;
}

static uint32_t b_bind_human_slot(const uint32_t* a, int n)
{
    mt_bind_human_slot(env(), (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
    return 0;
}

void rt_bind_match_bind(void)
{
    rt_bind(0x0045803cu, b_bind_human_slot, "carBindHumanSlot");
}

RT_BIND_AREA(match_bind)
