/*
 * bind_match_session.c: multiplayer session helpers and screen state flags 0x004618f8..0x00469a74
 * (src/match/mt_session.c), called the way tools/emu/diff_match_session.py called them.
 */
#include "rt_env.h"
#include "match/mt_session.h"

static uint32_t c0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static uint32_t c1(uint32_t fn, uint32_t eax) { uint32_t a[1] = {eax}; return rt_call(fn, a, 1); }
static uint32_t c2(uint32_t fn, uint32_t eax, uint32_t edx) { uint32_t a[2] = {eax, edx}; return rt_call(fn, a, 2); }

static uint8_t* s_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t cb_net_18(void* u, uint32_t net) { (void)u; return c1(0x004aaa58u, net); }
static uint32_t cb_net_0c(void* u, uint32_t net) { (void)u; return c1(0x004aaa5cu, net); }
static uint32_t cb_net_4aa5b8(void* u, uint32_t net) { (void)u; return c1(0x004aa5b8u, net); }
static void cb_obj_4abd38(void* u, uint32_t obj) { (void)u; c1(0x004abd38u, obj); }
static int32_t cb_session_count(void* u, uint32_t s) { (void)u; return (int32_t)c1(0x00498968u, s); }
static uint32_t cb_session_498634(void* u, uint32_t s, uint32_t a) { (void)u; return c2(0x00498634u, s, a); }
static uint32_t cb_session_player_id(void* u, uint32_t s, int32_t i) { (void)u; return c2(0x0049864cu, s, (uint32_t)i); }
static uint32_t cb_session_4986dc(void* u, uint32_t s, int32_t i) { (void)u; return c2(0x004986dcu, s, (uint32_t)i); }
static void cb_net_pump(void* u) { (void)u; c0(0x00463f78u); }
/* timeGetTime: the import slot 0x00c90640 the original calls through (no arguments) */
static uint32_t cb_time_now(void* u) { (void)u; return c0(0x00c90640u); }
static void cb_obj_4a6f80(void* u, uint32_t obj) { (void)u; c1(0x004a6f80u, obj); }
static void cb_call_c8(void* u, uint32_t fn, uint32_t obj, int32_t b) { (void)u; c2(fn, obj, (uint32_t)b); }
static uint8_t cb_is_multiplayer(void* u) { (void)u; return (uint8_t)c0(0x004632b0u); }

static const MtCars* cars(void)
{
    static MtCars c;
    static int done;
    if (!done) {
        c.human_count = (int32_t*)rt_ptr(0x005075e0u);
        c.human = rt_ptr(0x005030a8u);
        c.ai_count = (int32_t*)rt_ptr(0x0050c2b0u);
        c.ai = rt_ptr(0x0050760cu);
        c.ai_objects = (uint32_t*)rt_ptr(0x005075e8u);
        c.car_value = (uint32_t*)rt_ptr(0x00507590u);
        c.spawn_objects = (uint32_t*)rt_ptr(0x0050c2b8u);
        c.counter_b4 = (int32_t*)rt_ptr(0x0050e0b4u);
        c.counter_d4 = (int32_t*)rt_ptr(0x0050e0d4u);
        c.type_table = (const int16_t*)rt_ptr(0x004e7428u);
        c.pair_i16 = (int16_t*)rt_ptr(0x0050db64u);
        c.pair_i32 = (int32_t*)rt_ptr(0x0050dd28u);
        c.game_mode = (int32_t*)rt_ptr(0x00502fa4u);
        c.singleplayer = rt_ptr(0x00502f9cu);
        c.flag_c2c4 = rt_ptr(0x0050c2c4u);
        c.flag_2f7c = rt_ptr(0x00502f7cu);
        c.flag_2f88 = rt_ptr(0x00502f88u);
        c.flag_2f94 = rt_ptr(0x00502f94u);
        c.watched_player = (int32_t*)rt_ptr(0x00502f8cu);
        c.watched_ai = (int32_t*)rt_ptr(0x00502f90u);
        c.state_2f78 = (int32_t*)rt_ptr(0x00502f78u);
        c.resolve = s_resolve;
        c.is_multiplayer = cb_is_multiplayer;
        done = 1;
    }
    return &c;
}

static const MtSession* env(void)
{
    static MtSession e;
    static int done;
    if (!done) {
        e.resolve = s_resolve;
        e.net = 0x0050f9a8u;
        e.session = (uint32_t*)rt_ptr(0x0050f9dcu);
        e.obj_9d8 = (uint32_t*)rt_ptr(0x0050f9d8u);
        e.slot_state = (int32_t*)rt_ptr(0x0050f580u);
        e.slot_value = (int32_t*)rt_ptr(0x0050f5bcu);
        e.player_5fc = (int32_t*)rt_ptr(0x0050f5fcu);
        e.player_64c = (int32_t*)rt_ptr(0x0050f64cu);
        e.cur_player = (int32_t*)rt_ptr(0x0050f8ccu);
        e.prev_player = (int32_t*)rt_ptr(0x0050f8d0u);
        e.change_time = (uint32_t*)rt_ptr(0x0050f8d4u);
        e.flags_8e0 = rt_ptr(0x0050f8e0u);
        e.names = rt_ptr(0x0050f8f4u);
        e.records = rt_ptr(0x00624db8u);
        e.player_bytes = rt_ptr(0x00624eadu);
        e.f_e5bc = (int32_t*)rt_ptr(0x0050e5bcu);
        e.f_f9e4 = (int32_t*)rt_ptr(0x0050f9e4u);
        e.f_fa0c = (int32_t*)rt_ptr(0x0050fa0cu);
        e.f_fa68 = (int32_t*)rt_ptr(0x0050fa68u);
        e.f_faa0 = (int32_t*)rt_ptr(0x0050faa0u);
        e.f_e5b8 = (int32_t*)rt_ptr(0x0050e5b8u);
        e.obj_e440 = (uint32_t*)rt_ptr(0x0050e440u);
        e.f_e100 = (int32_t*)rt_ptr(0x0050e100u);
        e.cars = cars();
        e.net_18 = cb_net_18;
        e.net_0c = cb_net_0c;
        e.net_4aa5b8 = cb_net_4aa5b8;
        e.obj_4abd38 = cb_obj_4abd38;
        e.session_count = cb_session_count;
        e.session_498634 = cb_session_498634;
        e.session_player_id = cb_session_player_id;
        e.session_4986dc = cb_session_4986dc;
        e.net_pump = cb_net_pump;
        e.time_now = cb_time_now;
        e.obj_4a6f80 = cb_obj_4a6f80;
        e.call_c8 = cb_call_c8;
        done = 1;
    }
    return &e;
}

#define R(i) rt_arg(a, n, i)
#define NOARG (void)a; (void)n

static uint32_t b_net_is_multiplayer(const uint32_t* a, int n) { NOARG; return mt_net_is_multiplayer(env()); }
static uint32_t b_session_count(const uint32_t* a, int n) { NOARG; return (uint32_t)mt_session_count(env()); }
static uint32_t b_net_flag(const uint32_t* a, int n) { NOARG; return mt_net_flag(env()); }
static uint32_t b_session_call(const uint32_t* a, int n) { return mt_session_call(env(), R(0)); }
static uint32_t b_net_reset(const uint32_t* a, int n) { NOARG; return mt_net_reset(env()); }
static uint32_t b_session_bind_ids(const uint32_t* a, int n) { NOARG; mt_session_bind_ids(env()); return 0; }
static uint32_t b_session_player_bytes(const uint32_t* a, int n) { NOARG; mt_session_player_bytes(env()); return 0; }
static uint32_t b_session_set_current(const uint32_t* a, int n) { mt_session_set_current(env(), (int32_t)R(0)); return 0; }
static uint32_t b_session_slot_value(const uint32_t* a, int n) { return (uint32_t)mt_session_slot_value(env(), (int32_t)R(0)); }
static uint32_t b_session_slots_reset(const uint32_t* a, int n) { NOARG; mt_session_slots_reset(env()); return 0; }
static uint32_t b_player_slot(const uint32_t* a, int n) { return (uint32_t)mt_player_slot(env(), (int32_t)R(0)); }
static uint32_t b_player_name_slot(const uint32_t* a, int n) { return (uint32_t)mt_player_name_slot(env(), (int32_t)R(0)); }
static uint32_t b_session_renumber(const uint32_t* a, int n) { NOARG; return mt_session_renumber(env()); }
static uint32_t b_player_id_by_slot(const uint32_t* a, int n) { return (uint32_t)mt_player_id_by_slot(env(), (int32_t)R(0)); }
static uint32_t b_session_copy_names(const uint32_t* a, int n) { NOARG; mt_session_copy_names(env()); return 0; }
static uint32_t b_flags_8e0_reset(const uint32_t* a, int n) { NOARG; mt_flags_8e0_reset(env()); return 0; }
static uint32_t b_flags_8e8_clear(const uint32_t* a, int n) { NOARG; mt_flags_8e8_clear(env()); return 0; }
#define SCR(f) static uint32_t b_##f(const uint32_t* a, int n) { NOARG; mt_##f(env()); return 0; }
SCR(screen_4618f8)
SCR(screen_461f6c)
SCR(screen_461f84)
SCR(screen_465cbc)
SCR(screen_4671c8)
SCR(screen_4684c0)
SCR(screen_4684d8)
SCR(screen_469a60)
SCR(screen_469a74)
static uint32_t b_screen_467224(const uint32_t* a, int n) { mt_screen_467224(env(), (int32_t)R(0)); return 0; }
static uint32_t b_const_2(const uint32_t* a, int n) { NOARG; return (uint32_t)mt_const_2(); }
static uint32_t b_obj_call_c8(const uint32_t* a, int n) { return (uint32_t)mt_obj_call_c8(env(), R(0)); }

void rt_bind_match_session(void)
{
    rt_bind(0x004632b0u, b_net_is_multiplayer, "net_is_multiplayer");
    rt_bind(0x004632c8u, b_session_count, "session_player_count");
    rt_bind(0x004632d4u, b_net_flag, "net_flag_0c");
    rt_bind(0x004632e0u, b_session_call, "session_call_498634");
    rt_bind(0x0046329cu, b_net_reset, "net_reset");
    rt_bind(0x00463268u, b_session_bind_ids, "session_bind_ids");
    rt_bind(0x00463bdcu, b_session_player_bytes, "session_player_bytes");
    rt_bind(0x00463514u, b_session_set_current, "session_set_current");
    rt_bind(0x00463c9cu, b_session_slot_value, "session_slot_value");
    rt_bind(0x00463cc8u, b_session_slots_reset, "session_slots_reset");
    rt_bind(0x00464cdcu, b_player_slot, "player_slot");
    rt_bind(0x00464cf0u, b_player_name_slot, "player_name_slot");
    rt_bind(0x00464d68u, b_session_renumber, "session_renumber");
    rt_bind(0x00464e84u, b_player_id_by_slot, "player_id_by_slot");
    rt_bind(0x00464ef4u, b_session_copy_names, "session_copy_names");
    rt_bind(0x00464dccu, b_flags_8e0_reset, "flags_50f8e0_reset");
    rt_bind(0x00464ff8u, b_flags_8e8_clear, "flags_50f8e8_clear");
    rt_bind(0x004618f8u, b_screen_4618f8, "screen_4618f8");
    rt_bind(0x00461f6cu, b_screen_461f6c, "screen_461f6c");
    rt_bind(0x00461f84u, b_screen_461f84, "screen_461f84");
    rt_bind(0x00465cbcu, b_screen_465cbc, "screen_465cbc");
    rt_bind(0x004671c8u, b_screen_4671c8, "screen_4671c8");
    rt_bind(0x00467224u, b_screen_467224, "screen_467224");
    rt_bind(0x004684c0u, b_screen_4684c0, "screen_4684c0");
    rt_bind(0x004684d8u, b_screen_4684d8, "screen_4684d8");
    rt_bind(0x00469a60u, b_screen_469a60, "screen_469a60");
    rt_bind(0x00469a74u, b_screen_469a74, "screen_469a74");
    rt_bind(0x00462fe0u, b_const_2, "const_2");
    rt_bind(0x00462d80u, b_obj_call_c8, "obj_call_c8");
}

RT_BIND_AREA(match_session)
