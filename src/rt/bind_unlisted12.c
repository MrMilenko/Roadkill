/*
 * bind_unlisted12.c: unlisted 0x004aa900, 0x004aa974 (session methods: refresh the list, copy the players),
 * ported in src/platform/pf8_session.c; the four registers (compact: Watcom register arguments).
 */
#include "rt_env.h"
#include "platform/pf8_session.h"

#define B(name, fn) \
    static uint32_t name(const uint32_t* a, int n) \
    { \
        Pf2Env e = rt_pf2env(); \
        uint32_t r = fn(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3)); \
        rt_en2env_done(); \
        return r; \
    }
B(b_session_refresh, pf8_004aa900)
B(b_session_copy_players, pf8_004aa974)

void rt_bind_unlisted12(void)
{
    rt_bind(0x004aa900u, b_session_refresh, "unlisted_session_refresh");
    rt_bind(0x004aa974u, b_session_copy_players, "unlisted_session_copy_players");
}

RT_BIND_AREA(unlisted12)
