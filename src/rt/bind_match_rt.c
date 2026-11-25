/*
 * bind_match_rt.c: the frame loop leaves 0x0046be2c..0x0046cc54 (src/match/mt_rt.c), called the
 * way tools/emu/diff_match_rt.py called them (rtScheduleEvent: event AL, value EDX).
 */
#include "rt_env.h"
#include "match/mt_rt.h"

static const MtRt* env(void)
{
    static MtRt r;
    static int done;
    if (!done) {
        r.event_count = (int32_t*)rt_ptr(0x0058a93cu);
        r.events = rt_ptr(0x0058a914u);
        r.flag_940 = rt_ptr(0x0058a940u);
        r.v_520e94 = (uint32_t*)rt_ptr(0x00520e94u);
        r.v_520e90 = (uint32_t*)rt_ptr(0x00520e90u);
        r.cur_buffer = (uint32_t*)rt_ptr(0x004e7c68u);
        r.v_510dc4 = (uint32_t*)rt_ptr(0x00510dc4u);
        r.buf_510e70 = (uint32_t*)rt_ptr(0x00510e70u);
        r.buf_518e74 = (uint32_t*)rt_ptr(0x00518e74u);
        r.buf_520e84 = (uint32_t*)rt_ptr(0x00520e84u);
        done = 1;
    }
    return &r;
}

#define NOARG (void)a; (void)n
static uint32_t b_schedule_event(const uint32_t* a, int n)
{
    mt_rt_schedule_event(env(), (uint8_t)rt_arg(a, n, 0), rt_arg(a, n, 1));
    return 0;
}
static uint32_t b_events_reset(const uint32_t* a, int n) { NOARG; mt_rt_events_reset(env()); return 0; }
static uint32_t b_buffers_init(const uint32_t* a, int n) { NOARG; mt_rt_buffers_init(env()); return 0; }
static uint32_t b_get_520e94(const uint32_t* a, int n) { NOARG; return mt_rt_get_520e94(env()); }
static uint32_t b_get_520e90(const uint32_t* a, int n) { NOARG; return mt_rt_get_520e90(env()); }
static uint32_t b_cur_buffer(const uint32_t* a, int n) { NOARG; return mt_rt_cur_buffer(env()); }
static uint32_t b_get_510dc4(const uint32_t* a, int n) { NOARG; return mt_rt_get_510dc4(env()); }
static uint32_t b_screen_buffer(const uint32_t* a, int n) { NOARG; return mt_rt_screen_buffer(); }
static uint32_t b_buffer_520e9c(const uint32_t* a, int n) { NOARG; return mt_rt_buffer_520e9c(); }
static uint32_t b_buffer_58291c(const uint32_t* a, int n) { NOARG; return mt_rt_buffer_58291c(); }

void rt_bind_match_rt(void)
{
    rt_bind(0x0046be2cu, b_get_510dc4, "rt_get_510dc4");
    rt_bind(0x0046ca60u, b_get_520e94, "rt_get_520e94");
    rt_bind(0x0046ca68u, b_schedule_event, "rtScheduleEvent");
    rt_bind(0x0046cb44u, b_events_reset, "rt_events_reset");
    rt_bind(0x0046cb9cu, b_screen_buffer, "rt_screen_buffer");
    rt_bind(0x0046cba4u, b_cur_buffer, "rt_cur_buffer");
    rt_bind(0x0046cbecu, b_buffer_520e9c, "rt_buffer_520e9c");
    rt_bind(0x0046cbf4u, b_buffer_58291c, "rt_buffer_58291c");
    rt_bind(0x0046cbfcu, b_buffers_init, "rt_buffers_init");
    rt_bind(0x0046cc54u, b_get_520e90, "get_flag_520e90");
}

RT_BIND_AREA(match_rt)
