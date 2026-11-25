/*
 * bind_match_misc.c: the shell / car helpers 0x0045edd0, 0x0045ee20, 0x0045ee90
 * (src/match/mt_misc.c), called the way tools/emu/diff_match_misc.py called them (i in EAX).
 */
#include "rt_env.h"
#include "match/mt_misc.h"

static uint32_t mcall(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx)
{
    uint32_t a[4] = {eax, edx, ebx, ecx};
    return rt_call(fn, a, 4);
}
#define MCB(nm, addr) \
    static uint32_t nm(void* u, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx) \
    { (void)u; return mcall((addr), eax, edx, ebx, ecx); }

MCB(cb_car_type_release, 0x00453c38u)
MCB(cb_key_set_pad, 0x004a1ff8u)
MCB(cb_key_set_2278, 0x004a2278u)
MCB(cb_key_set_27bc, 0x004a27bcu)
MCB(cb_list_reset, 0x004a2e43u)
MCB(cb_list_create, 0x004a2e60u)
MCB(cb_list_462168, 0x00462168u)
MCB(cb_list_4a3620, 0x004a3620u)

static uint8_t* m_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

static const MtMisc* env(void)
{
    static MtMisc m;
    static int done;
    if (!done) {
        m.human = rt_ptr(0x005030a8u);
        m.human_count = (const int32_t*)rt_ptr(0x005075e0u);
        m.human_objects = (const uint32_t*)rt_ptr(0x00503088u);
        m.flags_568 = rt_ptr(0x00507568u);
        m.key_defaults = rt_ptr(0x004e7448u);
        m.list_mode = (uint32_t*)rt_ptr(0x004e7550u);
        m.resolve = m_resolve;
        m.car_type_release = cb_car_type_release;
        m.key_set_pad = cb_key_set_pad;
        m.key_set_2278 = cb_key_set_2278;
        m.key_set_27bc = cb_key_set_27bc;
        m.list_reset = cb_list_reset;
        m.list_create = cb_list_create;
        m.list_462168 = cb_list_462168;
        m.list_4a3620 = cb_list_4a3620;
        done = 1;
    }
    return &m;
}

static uint32_t b_human_car_leave(const uint32_t* a, int n) { return (uint32_t)mt_human_car_leave(env(), (int32_t)rt_arg(a, n, 0)); }
static uint32_t b_key_defaults(const uint32_t* a, int n) { (void)a; (void)n; mt_key_defaults(env()); return 0; }
static uint32_t b_shell_lists_init(const uint32_t* a, int n) { (void)a; (void)n; return mt_shell_lists_init(env()); }

void rt_bind_match_misc(void)
{
    rt_bind(0x0045edd0u, b_human_car_leave, "human_car_leave");
    rt_bind(0x0045ee20u, b_key_defaults, "key_defaults");
    rt_bind(0x0045ee90u, b_shell_lists_init, "shell_lists_init");
}

RT_BIND_AREA(match_misc)
