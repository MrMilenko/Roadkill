/*
 * bind_unlisted5.c: unlisted 0x00497e34 (list view sync with an entry list), ported in
 * src/platform/pf8_sesslist.c; the four registers as tools/emu/diff_pf8_sesslist.py passes them.
 */
#include "rt_env.h"
#include "platform/pf8_sesslist.h"

static uint32_t b_list_sync(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_00497e34(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted5(void)
{
    rt_bind(0x00497e34u, b_list_sync, "unlisted_list_sync");
}

RT_BIND_AREA(unlisted5)
