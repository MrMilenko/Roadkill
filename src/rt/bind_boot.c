/*
 * bind_boot.c: the start of the game: WinMain (en8_main.c).
 */
#include "rt_env.h"
#include "engine/en8_main.h"

/* 0x004a8b04 WinMain(hInstance, hPrev, cmdline, nShow), stdcall: no register arguments, so the
 * four stack words are a[0 .. 3] (docs/RUNTIME.md).
 * edi_in (the caller's edi, used after a longjmp back to the setjmp) is 0 here: the runtime's
 * start up passes no window. */
static uint32_t b_game_main(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    uint32_t r = en8_game_main(&e, 0, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_boot(void)
{
    rt_bind(0x004a8b04u, b_game_main, "game_main");
}

RT_BIND_AREA(boot)
