/*
 * bind_unlisted10.c: unlisted 0x00465f60 (run the network game dialog), ported in
 * src/platform/pf8_netrun.c; the four registers (compact: a Watcom method with register arguments).
 */
#include "rt_env.h"
#include "platform/pf8_netrun.h"

static uint32_t b_net_dialog_run(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_00465f60(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted10(void)
{
    rt_bind(0x00465f60u, b_net_dialog_run, "unlisted_net_dialog_run");
}

RT_BIND_AREA(unlisted10)
