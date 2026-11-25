/*
 * bind_unlisted3.c: unlisted 0x004a6a60, the window / dialog procedure trampoline, ported in
 * src/platform/pf8_wndproc.c. stdcall (hwnd, msg, wparam, lparam) with no register arguments, so the
 * four stack words are a[0..3] (docs/RUNTIME.md, compact arguments). The port also takes the
 * caller's EAX / EDX / EBX / ECX (its harness feeds them; the trampoline overwrites all four before
 * any use): 0 here.
 */
#include "rt_env.h"
#include "platform/pf8_wndproc.h"

static uint32_t b_window_proc(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_window_proc(&e, 0, 0, 0, 0, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted3(void)
{
    rt_bind(0x004a6a60u, b_window_proc, "unlisted_window_proc");
}

RT_BIND_AREA(unlisted3)
