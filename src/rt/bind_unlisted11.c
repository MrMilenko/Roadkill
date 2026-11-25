/*
 * bind_unlisted11.c: unlisted 0x00466e94, 0x00466f44, 0x00467244 (network dialog helpers: hide, chat send, start
 * enable), ported in src/platform/pf8_netmisc.c; the four registers (compact: Watcom register arguments).
 */
#include "rt_env.h"
#include "platform/pf8_netmisc.h"

#define B(name, fn) \
    static uint32_t name(const uint32_t* a, int n) \
    { \
        Pf2Env e = rt_pf2env(); \
        uint32_t r = fn(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3)); \
        rt_en2env_done(); \
        return r; \
    }
B(b_net_dialog_hide, pf8_00466e94)
B(b_net_chat_send, pf8_00466f44)
B(b_net_start_enable, pf8_00467244)

void rt_bind_unlisted11(void)
{
    rt_bind(0x00466e94u, b_net_dialog_hide, "unlisted_net_dialog_hide");
    rt_bind(0x00466f44u, b_net_chat_send, "unlisted_net_chat_send");
    rt_bind(0x00467244u, b_net_start_enable, "unlisted_net_start_enable");
}

RT_BIND_AREA(unlisted11)
