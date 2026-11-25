/*
 * bind_match_scmd.c: the scene commands 0x0046a728..0x0046ab34 (src/match/mt_scmd.c), called the
 * way tools/emu/diff_match_scmd.py called them: eax = the command record (a game address).
 */
#include "rt_env.h"
#include "match/mt_scmd.h"

static uint8_t* c_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
/* 0x004899c2 tm2_rand() */
static int32_t c_rand(void* u) { (void)u; uint32_t a[4] = {0}; return (int32_t)rt_call(0x004899c2u, a, 4); }
/* the choice validator: value in eax */
static uint32_t c_validate(void* u, uint32_t fn, uint32_t v) { (void)u; uint32_t a[4] = {v, 0, 0, 0}; return rt_call(fn, a, 4); }

static const MtSceneCmd* env(void)
{
    static MtSceneCmd e;
    if (!e.scene) e = (MtSceneCmd){rt_ptr(0x0050fad8u), c_resolve, c_rand, c_validate, NULL};
    return &e;
}

#define CMD(nm, f) static uint32_t nm(const uint32_t* a, int n) { f(rt_arg(a, n, 0), env()); return 0; }
CMD(b_place, mt_scmd_place)
CMD(b_orient, mt_scmd_orient)
CMD(b_move, mt_scmd_move)
CMD(b_turn, mt_scmd_turn)
CMD(b_move_turn, mt_scmd_move_turn)
CMD(b_choice, mt_scmd_choice)

void rt_bind_match_scmd(void)
{
    rt_bind(0x0046a728u, b_place, "scene_cmd_place");
    rt_bind(0x0046a744u, b_orient, "scene_cmd_orient");
    rt_bind(0x0046a760u, b_move, "scene_cmd_move");
    rt_bind(0x0046a878u, b_turn, "scene_cmd_turn");
    rt_bind(0x0046a9c0u, b_move_turn, "scene_cmd_move_turn");
    rt_bind(0x0046ab34u, b_choice, "scene_cmd_choice");
}

RT_BIND_AREA(match_scmd)
