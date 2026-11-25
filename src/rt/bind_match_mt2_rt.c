/*
 * bind_match_mt2_rt.c: rtMainLoop 0x0046c6fc (src/match/mt2_rt.c), called the way
 * tools/emu/diff_mt2_rt.py called it: no arguments, the event in AL.
 */
#include "rt_env.h"
#include "match/mt2_rt.h"

static uint8_t* r_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t r_call(void* u, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    (void)u;
    uint32_t a[4] = {eax, edx, ebx, 0};
    return rt_call(fn, a, 4);
}

static uint32_t b_main_loop(const uint32_t* a, int n)
{
    (void)a; (void)n;
    static Mt2Rt r;
    if (!r.active) {
        r.active = (uint32_t*)rt_ptr(0x00520e94u);
        r.cur_buffer = (uint32_t*)rt_ptr(0x004e7c68u);
        r.target_fps = (int32_t*)rt_ptr(0x00520e98u);
        r.event_count = (int32_t*)rt_ptr(0x0058a93cu);
        r.events = rt_ptr(0x0058a91cu);
        r.event_cur = rt_ptr(0x0058a940u);
        r.ticks_total = (int32_t*)rt_ptr(0x0058a944u);
        r.frame_timer = (uint32_t*)rt_ptr(0x00518e74u);
        r.display_mode = (const int32_t*)rt_ptr(0x00c84da8u);
        r.resolve = r_resolve;
        r.call = r_call;
    }
    return mt2_rt_main_loop(&r);
}

void rt_bind_match_mt2_rt(void)
{
    rt_bind(0x0046c6fcu, b_main_loop, "rtMainLoop");
}

RT_BIND_AREA(match_mt2_rt)
