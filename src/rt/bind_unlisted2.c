/*
 * bind_unlisted2.c: further reached "unlisted" functions (tools/rt/unlisted.tsv) ported in
 * src/platform/pf8_*.c, bound at their original addresses; each adapter passes the four registers
 * (eax, edx, ebx, ecx) as the emulator diffs did (tools/emu/diff_pf8_*.py).
 */
#include "rt_env.h"
#include "platform/pf8_netdlg.h"
#include "platform/pf8_cheats.h"

/* 0x004662b8: the network game dialog's controls (eax, edx, ebx, ecx as the caller left them) */
static uint32_t b_net_dialog_build(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_net_dialog_build(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

/* 0x0044e1cc: the cheat / debug code handler (eax = the code) */
static uint32_t b_cheat_codes(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_0044e1cc(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted2(void)
{
    rt_bind(0x0044e1ccu, b_cheat_codes, "unlisted_cheat_codes");
    rt_bind(0x004662b8u, b_net_dialog_build, "unlisted_net_dialog_build");
}

RT_BIND_AREA(unlisted2)
