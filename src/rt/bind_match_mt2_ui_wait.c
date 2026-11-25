/*
 * bind_match_mt2_ui_wait.c: the scene wait commands 0x0046ace8 (key / click) and 0x0046b15c
 * (timeout) (src/match/mt2_ui_wait.c), called the way tools/emu/diff_mt2_ui_wait.py called them:
 * eax = the command record.
 *
 * Emulator-only interface replaced: the original keeps a 0x3c byte mouse state buffer and the
 * x, y it reads on its stack (esp, esp + 0x3c, esp + 0x40) and passes the buffer in eax to
 * 0x004b2820, 0x004b2b00 and 0x004b2ee0. The port does not pass it (eax 0, mouse_read gets host
 * x, y); here the adapter gives the call a real frame and the callbacks hand that buffer on.
 */
#include "rt_env.h"
#include "match/mt2_ui_wait.h"

#define MOUSE_FRAME 0x48u
static uint32_t mouse_buf;   /* the current call's frame: buffer +0, x +0x3c, y +0x40 */

static uint8_t* w_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t w_call(void* u, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    (void)u;
    if (fn == 0x004b2b00u || fn == 0x004b2ee0u) eax = mouse_buf;
    uint32_t a[4] = {eax, edx, ebx, 0};
    return rt_call(fn, a, 4);
}
static void w_mouse_read(void* u, int32_t* x, int32_t* y)
{
    (void)u;
    uint32_t a[4] = {mouse_buf, mouse_buf + 0x3cu, mouse_buf + 0x40u, 0};
    rt_call(0x004b2820u, a, 4);
    *x = (int32_t)rt_r32(mouse_buf + 0x3cu);
    *y = (int32_t)rt_r32(mouse_buf + 0x40u);
}

static const Mt2UiWait* env(void)
{
    static Mt2UiWait e;
    if (!e.resolve)
        e = (Mt2UiWait){(int32_t*)rt_ptr(0x004e7b98u), (int32_t*)rt_ptr(0x0050fad4u), w_resolve, w_call, w_mouse_read, NULL};
    return &e;
}

static uint32_t b_wait_input(const uint32_t* a, int n)
{
    uint32_t saved = mouse_buf;
    mouse_buf = rt_frame_push(MOUSE_FRAME);
    mt2_ui_wait_input(env(), rt_arg(a, n, 0));
    rt_frame_pop(MOUSE_FRAME);
    mouse_buf = saved;
    return 0;
}
static uint32_t b_wait_timeout(const uint32_t* a, int n) { mt2_ui_wait_timeout(env(), rt_arg(a, n, 0)); return 0; }

void rt_bind_match_mt2_ui_wait(void)
{
    rt_bind(0x0046ace8u, b_wait_input, "scene_cmd_wait_input");
    rt_bind(0x0046b15cu, b_wait_timeout, "scene_cmd_wait_timeout");
}

RT_BIND_AREA(match_mt2_ui_wait)
