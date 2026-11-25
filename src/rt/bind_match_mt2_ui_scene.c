/*
 * bind_match_mt2_ui_scene.c: the scene event dispatch 0x00469b60 and model item 0x00469d14
 * (src/match/mt2_ui_scene.c), called the way tools/emu/diff_mt2_ui_scene.py called them.
 */
#include "rt_env.h"
#include "match/mt2_ui_scene.h"

static uint8_t* u_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static void u_call_fn(void* u, uint32_t fn, uint32_t eax, uint32_t edx)
{
    (void)u;
    uint32_t a[4] = {eax, edx, 0, 0};
    rt_call(fn, a, 4);
}
static uint32_t u_call(void* u, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    (void)u;
    uint32_t a[4] = {eax, edx, ebx, 0};
    return rt_call(fn, a, 4);
}

static const Mt2UiScene* env(void)
{
    static Mt2UiScene e;
    if (!e.resolve)
        e = (Mt2UiScene){(const int32_t*)rt_ptr(0x0050fb28u), (const uint32_t*)rt_ptr(0x0050fb2cu),
                         (const int32_t*)rt_ptr(0x0050fb4cu), (const uint32_t*)rt_ptr(0x0050fb50u),
                         (const int32_t*)rt_ptr(0x0050fb90u), (const uint32_t*)rt_ptr(0x0050fb94u),
                         (const uint32_t*)rt_ptr(0x00510a70u), u_resolve, u_call_fn, u_call, NULL};
    return &e;
}

/* 0x00469b60 (eax msg) */
static uint32_t b_dispatch(const uint32_t* a, int n) { mt2_ui_scene_dispatch(env(), rt_arg(a, n, 0)); return 0; }
/* 0x00469d14 (eax item, edx draw target) */
static uint32_t b_model_item(const uint32_t* a, int n) { mt2_ui_scene_model_item(env(), rt_arg(a, n, 0), rt_arg(a, n, 1)); return 0; }

void rt_bind_match_mt2_ui_scene(void)
{
    rt_bind(0x00469b60u, b_dispatch, "scene_event_dispatch");
    rt_bind(0x00469d14u, b_model_item, "scene_model_item");
}

RT_BIND_AREA(match_mt2_ui_scene)
