/*
 * bind_core_rt.c: runtime level bindings the ports share.
 *
 * rand / srand: the ported game code (tm2rand.c) and many ports that call tm2_rand directly share
 * one state; binding the original's rand 0x004899c2 / srand 0x004899e6 to the same functions puts
 * the game's calls on it too. Port: the state is a host static, not the runtime's per thread data
 * (+0xc of the block 0x004899b8 returns); nothing else reads it there.
 */
#include "rt_env.h"
#include "core/tm2rand.h"
#include "platform/pf2_env.h"

uint32_t pf4_crt_sprintf(Pf2Env* e, uint32_t buf, uint32_t fmt, uint32_t va_ptr);

static uint32_t b_rand(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)tm2_rand(); }
static uint32_t b_srand(const uint32_t* a, int n) { tm2_srand(rt_arg(a, n, 0)); return 0; }

/* 0x004b9380: slot 0 of the music object's vtable 0x004e62ac, a lone `ret` (Ghidra never split it
 * into a function; docs/PLATFORM.md "Music object"). Weak: an unlisted-code port may replace it. */
static uint32_t b_music_nop(const uint32_t* a, int n) { (void)n; return rt_arg(a, n, 0); }

/* 0x004ac9bc: `mov eax, 2; ret`, the tail of FUN_004ac9b0 used on its own as a window object's
 * vtable slot ("not handled" for window_dispatch). Weak: a mid-function entry port may replace it. */
static uint32_t b_ret2(const uint32_t* a, int n) { (void)a; (void)n; return 2; }

/* 0x004b27e6 crt_sprintf(buffer, format, ...), cdecl, no register arguments: the words from a[0].
 * The port takes the game address of the variable arguments (on the original's stack): they are
 * copied to a frame. Hand written (the generator cannot express varargs); not in bind_t.sorted. */
static uint32_t b_sprintf(const uint32_t* a, int n)
{
    uint32_t nv = n > 2 ? (uint32_t)(n - 2) : 0;
    uint32_t va = rt_frame_push(4 * nv + 4);
    for (uint32_t i = 0; i < nv; i++) rt_w32(va + 4 * i, a[2 + i]);
    Pf2Env e = rt_pf2env();
    uint32_t r = pf4_crt_sprintf(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), va);
    rt_en2env_done();
    rt_frame_pop(4 * nv + 4);
    return r;
}

/* the Watcom exit path's internals called by the ported crt_exit 0x004bc957: 0x004cabbd (unhook
 * the exception chain, flush the streams) and 0x004cb299 (finalizers): host side, nothing to do
 * (the host closes its files at exit); ExitProcess then ends the run. */
static uint32_t b_exit_nop(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

void rt_bind_core_rt(void)
{
    rt_bind_weak(0x004cabbdu, b_exit_nop, "crt_exit_unhook_flush");
    rt_bind_weak(0x004cb299u, b_exit_nop, "crt_exit_finalizers");
    rt_bind(0x004b27e6u, b_sprintf, "crt_sprintf");
    rt_bind_weak(0x004ac9bcu, b_ret2, "vslot_ret2_004ac9bc");
    /* 0x00473140: the lone `ret` that ends sound_stop_if_on, called on its own as a no-op slot */
    rt_bind_weak(0x00473140u, b_music_nop, "ret_00473140");
    rt_bind_weak(0x004b9380u, b_music_nop, "music_vslot0_ret");
    rt_bind_weak(0x004899c2u, b_rand, "crt_rand");
    rt_bind_weak(0x004899e6u, b_srand, "crt_srand");
}

RT_BIND_AREA(core_rt)
