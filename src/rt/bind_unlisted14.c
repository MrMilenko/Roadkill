/*
 * bind_unlisted14.c: unlisted 0x00463d1c, 0x00463d58, 0x00467018, 0x004675c0, 0x00467638, 0x004676d0 (network
 * dialog message helpers), ported in src/platform/pf8_netmsg.c. Compact: four register arguments, then the
 * stack words (0x00463d1c one, 0x00463d58 two) at a[4..].
 */
#include "rt_env.h"
#include "platform/pf8_netmsg.h"

#define B(name, fn) \
    static uint32_t name(const uint32_t* a, int n) \
    { \
        Pf2Env e = rt_pf2env(); \
        uint32_t r = fn(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3)); \
        rt_en2env_done(); \
        return r; \
    }
B(b_net_chat_to, pf8_00467018)
B(b_net_chat_private, pf8_004675c0)
B(b_net_ready_toggle, pf8_00467638)
B(b_net_kick, pf8_004676d0)

static uint32_t b_net_msg_all(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_00463d1c(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3),
                              rt_arg(a, n, 4));
    rt_en2env_done();
    return r;
}

static uint32_t b_net_msg_to(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_00463d58(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3),
                              rt_arg(a, n, 4), rt_arg(a, n, 5));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted14(void)
{
    rt_bind(0x00463d1cu, b_net_msg_all, "unlisted_net_msg_all");
    rt_bind(0x00463d58u, b_net_msg_to, "unlisted_net_msg_to");
    rt_bind(0x00467018u, b_net_chat_to, "unlisted_net_chat_to");
    rt_bind(0x004675c0u, b_net_chat_private, "unlisted_net_chat_private");
    rt_bind(0x00467638u, b_net_ready_toggle, "unlisted_net_ready_toggle");
    rt_bind(0x004676d0u, b_net_kick, "unlisted_net_kick");
}

RT_BIND_AREA(unlisted14)
