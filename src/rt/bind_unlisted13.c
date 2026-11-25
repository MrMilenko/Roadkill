/*
 * bind_unlisted13.c: unlisted 0x004672fc (network dialog start / ready handler), ported in
 * src/platform/pf8_netstart.c; the four registers (compact: a Watcom method with register arguments).
 */
#include "rt_env.h"
#include "platform/pf8_netstart.h"

static uint32_t b_net_start(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_004672fc(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted13(void)
{
    rt_bind(0x004672fcu, b_net_start, "unlisted_net_start");
}

RT_BIND_AREA(unlisted13)
