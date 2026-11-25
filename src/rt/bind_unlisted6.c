/*
 * bind_unlisted6.c: mid-function entries used as vtable slots (tools/rt/midentries.tsv), ported in
 * src/platform/pf8_mid.c. 0x004ac9bc replaces the weak `return 2` in bind_core_rt.c.
 */
#include "rt_env.h"
#include "platform/pf8_mid.h"

static uint32_t b_00473140(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_00473140(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}
static uint32_t b_004ac9bc(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_004ac9bc(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted6(void)
{
    rt_bind(0x00473140u, b_00473140, "mid_00473140_ret");
    rt_bind(0x004ac9bcu, b_004ac9bc, "mid_004ac9bc_ret2");
}

RT_BIND_AREA(unlisted6)
