/*
 * bind_match_cars.c: the small car system accessors 0x00458034..0x0045c348 (src/match/mt_cars.c),
 * called the way tools/emu/diff_match_cars.py called them: the arguments in EAX, EDX, EBX, ECX.
 */
#include "rt_env.h"
#include "match/mt_cars.h"

static uint8_t* c_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint8_t c_is_multiplayer(void* u)
{
    (void)u;
    uint32_t a[4] = {0, 0, 0, 0};
    return (uint8_t)rt_call(0x004632b0u, a, 4);
}

static const MtCars* env(void)
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
        c.resolve = c_resolve;
        c.is_multiplayer = c_is_multiplayer;
        done = 1;
    }
    return &c;
}

#define R(i) ((int32_t)rt_arg(a, n, i))
#define NOARG (void)a; (void)n

static uint32_t b_set_human_count(const uint32_t* a, int n) { mt_set_human_count(env(), R(0)); return 0; }
static uint32_t b_ai_object(const uint32_t* a, int n) { return mt_ai_object(env(), R(0)); }
/* EDX / EBX: game addresses of the i32[3] position and the i16[3] angles */
static uint32_t b_spawn_point_set(const uint32_t* a, int n)
{
    mt_spawn_point_set(env(), R(0), (const int32_t*)rt_ptr((uint32_t)R(1)), (const int16_t*)rt_ptr((uint32_t)R(2)));
    return 0;
}
static uint32_t b_flag_c2c4(const uint32_t* a, int n) { NOARG; return mt_flag_c2c4(env()); }
static uint32_t b_flag_2f7c(const uint32_t* a, int n) { NOARG; return mt_flag_2f7c(env()); }
static uint32_t b_flag_2f88(const uint32_t* a, int n) { NOARG; return mt_flag_2f88(env()); }
static uint32_t b_flag_2f94(const uint32_t* a, int n) { NOARG; return mt_flag_2f94(env()); }
static uint32_t b_car_value(const uint32_t* a, int n) { return mt_car_value(env(), R(0)); }
static uint32_t b_nop_45b3f8(const uint32_t* a, int n) { NOARG; mt_nop_45b3f8(); return 0; }
static uint32_t b_watched_player(const uint32_t* a, int n) { NOARG; return (uint32_t)mt_watched_player(env()); }
static uint32_t b_watched_ai(const uint32_t* a, int n) { NOARG; return (uint32_t)mt_watched_ai(env()); }
static uint32_t b_state_2f78(const uint32_t* a, int n) { NOARG; return (uint32_t)mt_state_2f78(env()); }
static uint32_t b_human_set_id(const uint32_t* a, int n) { mt_human_set_id(env(), R(0), R(1)); return 0; }
/* returns the host pointer of a human car: its game address */
static uint32_t b_human_by_id(const uint32_t* a, int n) { return rt_addr(mt_human_by_id(env(), R(0))); }
static uint32_t b_counter_d4(const uint32_t* a, int n) { return (uint32_t)mt_counter_d4(env(), R(0)); }
static uint32_t b_counter_b4(const uint32_t* a, int n) { return (uint32_t)mt_counter_b4(env(), R(0)); }
static uint32_t b_counter_b4_inc(const uint32_t* a, int n) { mt_counter_b4_inc(env(), R(0)); return 0; }
static uint32_t b_counter_b4_set_by_id(const uint32_t* a, int n) { mt_counter_b4_set_by_id(env(), R(0), R(1)); return 0; }
static uint32_t b_counter_d4_set_by_id(const uint32_t* a, int n) { mt_counter_d4_set_by_id(env(), R(0), R(1)); return 0; }
static uint32_t b_cars_alive(const uint32_t* a, int n) { NOARG; return (uint32_t)mt_cars_alive(env()); }
static uint32_t b_car_type_index(const uint32_t* a, int n) { return (uint32_t)mt_car_type_index(env(), R(0)); }
/* only ax is meaningful (the original's high half of eax is left over from the index); zero here */
static uint32_t b_pair_i16(const uint32_t* a, int n) { return (uint16_t)mt_pair_i16(env(), R(0), R(1)); }
static uint32_t b_pair_i32(const uint32_t* a, int n) { return (uint32_t)mt_pair_i32(env(), R(0), R(1)); }
static uint32_t b_set_game_mode(const uint32_t* a, int n) { mt_set_game_mode(env(), R(0)); return 0; }
static uint32_t b_singleplayer(const uint32_t* a, int n) { NOARG; return mt_singleplayer(env()); }

void rt_bind_match_cars(void)
{
    rt_bind(0x00458034u, b_set_human_count, "human_car_set_count");
    rt_bind(0x0045ae2cu, b_ai_object, "ai_car_object");
    rt_bind(0x0045b1e8u, b_spawn_point_set, "spawn_point_set");
    rt_bind(0x0045b2f4u, b_flag_c2c4, "flag_50c2c4_get");
    rt_bind(0x0045b2fcu, b_flag_2f7c, "flag_502f7c_get");
    rt_bind(0x0045b3f0u, b_car_value, "car_value_get");
    rt_bind(0x0045b3f8u, b_nop_45b3f8, "nop_45b3f8");
    rt_bind(0x0045b54cu, b_watched_player, "watched_player_get");
    rt_bind(0x0045b554u, b_watched_ai, "watched_ai_get");
    rt_bind(0x0045b55cu, b_flag_2f88, "flag_502f88_get");
    rt_bind(0x0045b978u, b_human_set_id, "human_car_set_id");
    rt_bind(0x0045b988u, b_human_by_id, "human_car_by_id");
    rt_bind(0x0045b9c4u, b_counter_d4, "counter_50e0d4_get");
    rt_bind(0x0045b9ccu, b_counter_b4, "counter_50e0b4_get");
    rt_bind(0x0045b9d4u, b_counter_b4_inc, "counter_50e0b4_inc");
    rt_bind(0x0045b9dcu, b_counter_b4_set_by_id, "counter_50e0b4_set_by_id");
    rt_bind(0x0045ba20u, b_counter_d4_set_by_id, "counter_50e0d4_set_by_id");
    rt_bind(0x0045bab8u, b_cars_alive, "cars_alive_count");
    rt_bind(0x0045bb20u, b_car_type_index, "car_type_index");
    rt_bind(0x0045bb50u, b_flag_2f94, "flag_502f94_get");
    rt_bind(0x0045c1f4u, b_pair_i16, "car_pair_i16");
    rt_bind(0x0045c20cu, b_pair_i32, "car_pair_i32");
    rt_bind(0x0045c328u, b_set_game_mode, "game_set_mode");
    rt_bind(0x0045c340u, b_state_2f78, "state_502f78_get");
    rt_bind(0x0045c348u, b_singleplayer, "is_singleplayer");
}

RT_BIND_AREA(match_cars)
