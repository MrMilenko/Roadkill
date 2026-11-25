/*
 * bind_unlisted_boot.c: the unlisted functions on the boot path (static constructors of the XI table
 * 0x004ea5fc and what they call; tools/rt/unlisted.tsv) ported in src/platform/pf8_boot.c, bound at their
 * original addresses. Adapters pass the four registers (eax, edx, ebx, ecx) and, for the stdcall
 * DirectPlayEnumerate callback 0x004acf14, its five stack words, as tools/emu/diff_pf8_boot.py does.
 */
#include "rt_env.h"
#include "platform/pf8_boot.h"

#define ADAPT(x) \
    static uint32_t b_##x(const uint32_t* a, int n) \
    { \
        Pf2Env e = rt_pf2env(); \
        uint32_t r = pf8_##x(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3)); \
        rt_en2env_done(); \
        return r; \
    }

ADAPT(004aa3c0)
ADAPT(004ab73c)
ADAPT(004acdb8)
ADAPT(00498498)
ADAPT(004acb70)
ADAPT(004acc54)
ADAPT(00496b60)
ADAPT(004a7b4c)
ADAPT(004ac6a4)
ADAPT(004a5a50)
ADAPT(004b2f80)
ADAPT(004b3ccc)
ADAPT(004b3cc0)
ADAPT(004ba410)
ADAPT(004a4838)
static uint32_t b_004acf14(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    /* DirectPlayEnumerate callback: stdcall, 5 stack words, no register arguments: a[0..4] */
    uint32_t r = pf8_004acf14(&e, 0, 0, 0, 0, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3),
                              rt_arg(a, n, 4));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted_boot(void)
{
    rt_bind(0x004aa3c0u, b_004aa3c0, "unlisted_004aa3c0");
    rt_bind(0x004ab73cu, b_004ab73c, "unlisted_004ab73c");
    rt_bind(0x004acdb8u, b_004acdb8, "unlisted_004acdb8");
    rt_bind(0x00498498u, b_00498498, "unlisted_00498498");
    rt_bind(0x004acb70u, b_004acb70, "unlisted_004acb70");
    rt_bind(0x004acc54u, b_004acc54, "unlisted_004acc54");
    rt_bind(0x004acf14u, b_004acf14, "unlisted_004acf14");
    rt_bind(0x00496b60u, b_00496b60, "unlisted_00496b60");
    rt_bind(0x004a7b4cu, b_004a7b4c, "unlisted_004a7b4c");
    rt_bind(0x004ac6a4u, b_004ac6a4, "unlisted_004ac6a4");
    rt_bind(0x004a5a50u, b_004a5a50, "unlisted_004a5a50");
    rt_bind(0x004b2f80u, b_004b2f80, "unlisted_004b2f80");
    rt_bind(0x004b3cccu, b_004b3ccc, "unlisted_004b3ccc");
    rt_bind(0x004b3cc0u, b_004b3cc0, "unlisted_004b3cc0");
    rt_bind(0x004ba410u, b_004ba410, "unlisted_004ba410");
    rt_bind(0x004a4838u, b_004a4838, "unlisted_004a4838");
}

RT_BIND_AREA(unlisted_boot)
