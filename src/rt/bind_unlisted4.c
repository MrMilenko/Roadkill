/*
 * bind_unlisted4.c: unlisted 0x00498508 (player enumeration) and its EnumPlayers callback 0x004989d0
 * (stdcall, no register arguments: id, type, DPNAME*, flags, context at a[0..4], the runtime's compact
 * convention), ported in src/platform/pf8_dplay.c.
 */
#include "rt_env.h"
#include "platform/pf8_dplay.h"

static uint32_t b_enum_players(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_00498508(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}
static uint32_t b_enum_players_cb(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_004989d0(&e, 0, 0, 0, 0, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3),
                              rt_arg(a, n, 4));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted4(void)
{
    rt_bind(0x00498508u, b_enum_players, "unlisted_enum_players");
    rt_bind(0x004989d0u, b_enum_players_cb, "unlisted_enum_players_cb");
}

RT_BIND_AREA(unlisted4)
