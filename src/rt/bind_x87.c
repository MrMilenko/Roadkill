/*
 * bind_x87.c: the view projection, view creation / border and the DirectSound close (en10_*.c),
 * whose x87 transcendental helpers (crt_ftan / fatan / fcos / fsin, the log 0x004c9f14) are
 * computed with host libm (the user's decision: trig at the x87 helper boundary). The en10 ports
 * should be built with -ffp-contract=off -fno-builtin (like the other x87 ports).
 */
#include "rt_env.h"
#include "engine/en10_viewport.h"
#include "engine/en10_view.h"
#include "engine/en10_dsound.h"

/* 0x00493ed0 level_setup_viewport(eax view; stack x, y, w, h fractions, fov, unused): one register
 * argument, so the stack words are a[1 .. 6] */
static uint32_t b_level_setup_viewport(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    en10_level_setup_viewport(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3),
                              rt_arg(a, n, 4), rt_arg(a, n, 5), rt_arg(a, n, 6));
    rt_en2env_done();
    return 0;
}
/* 0x00494610 viewCreate(eax view; the same six stack words, a[1 .. 6]) */
static uint32_t b_view_create(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    en10_view_create(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3), rt_arg(a, n, 4),
                     rt_arg(a, n, 5), rt_arg(a, n, 6));
    rt_en2env_done();
    return 0;
}
/* 0x0049437c subsys_menu_options(eax view, edx border) */
static uint32_t b_view_set_border(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    uint32_t r = en10_view_set_border(&e, rt_arg(a, n, 0), rt_arg(a, n, 1));
    rt_en2env_done();
    return r;
}
/* 0x004b93cc dsound_device_dtor(eax device) */
static uint32_t b_dsound_device_dtor(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    uint32_t r = en10_dsound_device_dtor(&e, rt_arg(a, n, 0));
    rt_en2env_done();
    return r;
}
/* 0x004b9674 dsound_close(eax device) */
static uint32_t b_dsound_close(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    uint32_t r = en10_dsound_close(&e, rt_arg(a, n, 0));
    rt_en2env_done();
    return r;
}

void rt_bind_x87(void)
{
    rt_bind(0x00493ed0u, b_level_setup_viewport, "level_setup_viewport");
    rt_bind(0x00494610u, b_view_create, "viewCreate");
    rt_bind(0x0049437cu, b_view_set_border, "subsys_menu_options");
    rt_bind(0x004b93ccu, b_dsound_device_dtor, "dsound_device_dtor");
    rt_bind(0x004b9674u, b_dsound_close, "dsound_close");
}

RT_BIND_AREA(x87)
