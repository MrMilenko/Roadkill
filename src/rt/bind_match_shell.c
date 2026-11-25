/*
 * bind_match_shell.c: the front end helpers 0x0045f080..0x00468774 (src/match/mt_shell.c),
 * called the way tools/emu/diff_match_shell.py called them.
 */
#include "rt_env.h"
#include "match/mt_shell.h"

static uint32_t call2(uint32_t fn, uint32_t eax, uint32_t edx)
{
    uint32_t a[2] = {eax, edx};
    return rt_call(fn, a, 2);
}

static uint8_t* s_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t s_save_data(void* u, uint32_t a) { (void)u; return call2(0x004a81e8u, a, 0); }
static uint32_t s_unlocked(void* u, int32_t w) { (void)u; return call2(0x0044eee4u, (uint32_t)w, 0); }
static void s_net_set_type(void* u, uint32_t net, int32_t t) { (void)u; call2(0x004aaa74u, net, (uint32_t)t); }
/* [[obj +0x34] +0xd0](obj, on): fn is the address the port read from the button's table */
static void s_button_select(void* u, uint32_t fn, uint32_t obj, int32_t on) { (void)u; call2(fn, obj, (uint32_t)on); }

static const MtShell* env(void)
{
    static MtShell e;
    static int done;
    if (!done) {
        e.state_table = rt_ptr(0x0050e5ccu);
        e.cars = rt_ptr(0x0050e6d4u);
        e.arena = rt_ptr(0x00624ec7u);
        e.arena_cur = (const int32_t*)rt_ptr(0x0050fa70u);
        e.game_type = rt_ptr(0x00624ea8u);
        e.buttons = (const uint32_t*)rt_ptr(0x0050fa7cu);
        e.resolve = s_resolve;
        e.save_data = s_save_data;
        e.unlocked = s_unlocked;
        e.net_set_type = s_net_set_type;
        e.button_select = s_button_select;
        done = 1;
    }
    return &e;
}

static uint32_t b_init_state_table(const uint32_t* a, int n) { (void)a; (void)n; mt_inputcfg_init_state_table(env()); return 0; }
static uint32_t b_load_data(const uint32_t* a, int n) { (void)a; (void)n; mt_inputcfg_load_data(env()); return 0; }
static uint32_t b_arena_next(const uint32_t* a, int n) { (void)a; (void)n; mt_arena_next(env()); return 0; }
static uint32_t b_arena_prev(const uint32_t* a, int n) { (void)a; (void)n; mt_arena_prev(env()); return 0; }
static uint32_t b_game_type_7(const uint32_t* a, int n) { mt_game_type_7(env(), rt_arg(a, n, 0)); return 0; }
static uint32_t b_game_type_6(const uint32_t* a, int n) { mt_game_type_6(env(), rt_arg(a, n, 0)); return 0; }
static uint32_t b_game_type_5(const uint32_t* a, int n) { mt_game_type_5(env(), rt_arg(a, n, 0)); return 0; }

void rt_bind_match_shell(void)
{
    rt_bind(0x0045f080u, b_init_state_table, "inputcfg_init_state_table");
    rt_bind(0x0045f1e8u, b_load_data, "inputcfg_load_data");
    rt_bind(0x004685d4u, b_arena_next, "arena_next");
    rt_bind(0x00468650u, b_arena_prev, "arena_prev");
    rt_bind(0x004686d4u, b_game_type_7, "game_type_7");
    rt_bind(0x00468724u, b_game_type_6, "game_type_6");
    rt_bind(0x00468774u, b_game_type_5, "game_type_5");
}

RT_BIND_AREA(match_shell)
