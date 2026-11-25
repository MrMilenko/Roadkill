/*
 * bind_unlisted.c: the reached functions Ghidra never split ("unlisted", tools/rt/unlisted.tsv) ported in
 * src/platform/pf7_*.c, bound at their original addresses. Each adapter passes the four registers
 * (eax, edx, ebx, ecx) as the emulator diffs did (tools/emu/diff_pf7_*.py).
 */
#include "rt_env.h"
#include "platform/pf7_small.h"
#include "platform/pf7_dtor.h"

#define ADAPT(x) \
    static uint32_t b_##x(const uint32_t* a, int n) \
    { \
        Pf2Env e = rt_pf2env(); \
        uint32_t r = pf7_##x(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3)); \
        rt_en2env_done(); \
        return r; \
    }

ADAPT(00462f50)
ADAPT(00462f60)
ADAPT(00462f80)
ADAPT(00462f90)
ADAPT(00462fa0)
ADAPT(00462fd0)
ADAPT(00462ff0)
ADAPT(00463000)
ADAPT(00463010)
ADAPT(00463020)
ADAPT(00463030)
ADAPT(00463040)
ADAPT(00463050)
ADAPT(00463060)
ADAPT(00463070)
ADAPT(00463080)
ADAPT(00463090)
ADAPT(004630a0)
ADAPT(004630b0)
ADAPT(004630c0)
ADAPT(004630d0)
ADAPT(004630e0)
ADAPT(004630f0)
ADAPT(00463100)
ADAPT(00463110)
ADAPT(00463120)
ADAPT(00463130)
ADAPT(00463140)
ADAPT(00463150)
ADAPT(00463160)
ADAPT(00463170)
ADAPT(00463180)
ADAPT(00463190)
ADAPT(004631a0)
ADAPT(004631b0)
ADAPT(004631c0)
ADAPT(004631d0)
ADAPT(004631e0)
ADAPT(004631f0)
ADAPT(00463200)
ADAPT(00463240)
ADAPT(00467594)
ADAPT(004675a8)
ADAPT(0046761c)
ADAPT(0046b320)
ADAPT(0046b330)
ADAPT(00473da0)
ADAPT(00477620)
ADAPT(00477630)
ADAPT(00477640)
ADAPT(00485440)
ADAPT(00485530)
ADAPT(00485610)
ADAPT(00485900)
ADAPT(00485a50)
ADAPT(00485a80)
ADAPT(00485dc0)
ADAPT(00485e90)
ADAPT(00489980)
ADAPT(00489990)
ADAPT(004899a0)
ADAPT(004899b0)
ADAPT(00497990)
ADAPT(00498534)
ADAPT(004a21b0)
ADAPT(004a2620)
ADAPT(004a2e33)
ADAPT(004a5f0c)
ADAPT(004a6150)
ADAPT(004a6244)
ADAPT(004a6404)
ADAPT(004a6540)
ADAPT(004a65d0)
ADAPT(004a69f0)
ADAPT(004a6a39)
ADAPT(004a8ae0)
ADAPT(004aa3a8)
ADAPT(004ac680)
ADAPT(004acd40)
ADAPT(004b27d3)
ADAPT(004b3010)
ADAPT(004b7f90)
ADAPT(004b8550)
ADAPT(004b8574)
ADAPT(004b9380)
ADAPT(004b9390)
ADAPT(004b93b4)
ADAPT(004ba49c)
ADAPT(004ba4ac)
ADAPT(004baa68)
ADAPT(004bae1c)
ADAPT(004bae34)
ADAPT(004bba30)
ADAPT(004bfed8)
ADAPT(004bfefc)
ADAPT(00462e60)
ADAPT(00462ed0)
ADAPT(00462f10)
ADAPT(00489900)
ADAPT(00489940)
ADAPT(004a6960)
ADAPT(004a69a0)
ADAPT(004acb30)

void rt_bind_unlisted(void)
{
    rt_bind(0x00462e60u, b_00462e60, "unlisted_dtor_00462e60");
    rt_bind(0x00462ed0u, b_00462ed0, "unlisted_dtor_00462ed0");
    rt_bind(0x00462f10u, b_00462f10, "unlisted_dtor_00462f10");
    rt_bind(0x00489900u, b_00489900, "unlisted_dtor_00489900");
    rt_bind(0x00489940u, b_00489940, "unlisted_dtor_00489940");
    rt_bind(0x004a6960u, b_004a6960, "unlisted_dtor_004a6960");
    rt_bind(0x004a69a0u, b_004a69a0, "unlisted_dtor_004a69a0");
    rt_bind(0x004acb30u, b_004acb30, "unlisted_dtor_004acb30");
    rt_bind(0x00462f50u, b_00462f50, "unlisted_00462f50");
    rt_bind(0x00462f60u, b_00462f60, "unlisted_00462f60");
    rt_bind(0x00462f80u, b_00462f80, "unlisted_00462f80");
    rt_bind(0x00462f90u, b_00462f90, "unlisted_00462f90");
    rt_bind(0x00462fa0u, b_00462fa0, "unlisted_00462fa0");
    rt_bind(0x00462fd0u, b_00462fd0, "unlisted_00462fd0");
    rt_bind(0x00462ff0u, b_00462ff0, "unlisted_00462ff0");
    rt_bind(0x00463000u, b_00463000, "unlisted_00463000");
    rt_bind(0x00463010u, b_00463010, "unlisted_00463010");
    rt_bind(0x00463020u, b_00463020, "unlisted_00463020");
    rt_bind(0x00463030u, b_00463030, "unlisted_00463030");
    rt_bind(0x00463040u, b_00463040, "unlisted_00463040");
    rt_bind(0x00463050u, b_00463050, "unlisted_00463050");
    rt_bind(0x00463060u, b_00463060, "unlisted_00463060");
    rt_bind(0x00463070u, b_00463070, "unlisted_00463070");
    rt_bind(0x00463080u, b_00463080, "unlisted_00463080");
    rt_bind(0x00463090u, b_00463090, "unlisted_00463090");
    rt_bind(0x004630a0u, b_004630a0, "unlisted_004630a0");
    rt_bind(0x004630b0u, b_004630b0, "unlisted_004630b0");
    rt_bind(0x004630c0u, b_004630c0, "unlisted_004630c0");
    rt_bind(0x004630d0u, b_004630d0, "unlisted_004630d0");
    rt_bind(0x004630e0u, b_004630e0, "unlisted_004630e0");
    rt_bind(0x004630f0u, b_004630f0, "unlisted_004630f0");
    rt_bind(0x00463100u, b_00463100, "unlisted_00463100");
    rt_bind(0x00463110u, b_00463110, "unlisted_00463110");
    rt_bind(0x00463120u, b_00463120, "unlisted_00463120");
    rt_bind(0x00463130u, b_00463130, "unlisted_00463130");
    rt_bind(0x00463140u, b_00463140, "unlisted_00463140");
    rt_bind(0x00463150u, b_00463150, "unlisted_00463150");
    rt_bind(0x00463160u, b_00463160, "unlisted_00463160");
    rt_bind(0x00463170u, b_00463170, "unlisted_00463170");
    rt_bind(0x00463180u, b_00463180, "unlisted_00463180");
    rt_bind(0x00463190u, b_00463190, "unlisted_00463190");
    rt_bind(0x004631a0u, b_004631a0, "unlisted_004631a0");
    rt_bind(0x004631b0u, b_004631b0, "unlisted_004631b0");
    rt_bind(0x004631c0u, b_004631c0, "unlisted_004631c0");
    rt_bind(0x004631d0u, b_004631d0, "unlisted_004631d0");
    rt_bind(0x004631e0u, b_004631e0, "unlisted_004631e0");
    rt_bind(0x004631f0u, b_004631f0, "unlisted_004631f0");
    rt_bind(0x00463200u, b_00463200, "unlisted_00463200");
    rt_bind(0x00463240u, b_00463240, "unlisted_00463240");
    rt_bind(0x00467594u, b_00467594, "unlisted_00467594");
    rt_bind(0x004675a8u, b_004675a8, "unlisted_004675a8");
    rt_bind(0x0046761cu, b_0046761c, "unlisted_0046761c");
    rt_bind(0x0046b320u, b_0046b320, "unlisted_0046b320");
    rt_bind(0x0046b330u, b_0046b330, "unlisted_0046b330");
    rt_bind(0x00473da0u, b_00473da0, "unlisted_00473da0");
    rt_bind(0x00477620u, b_00477620, "unlisted_00477620");
    rt_bind(0x00477630u, b_00477630, "unlisted_00477630");
    rt_bind(0x00477640u, b_00477640, "unlisted_00477640");
    rt_bind(0x00485440u, b_00485440, "unlisted_00485440");
    rt_bind(0x00485530u, b_00485530, "unlisted_00485530");
    rt_bind(0x00485610u, b_00485610, "unlisted_00485610");
    rt_bind(0x00485900u, b_00485900, "unlisted_00485900");
    rt_bind(0x00485a50u, b_00485a50, "unlisted_00485a50");
    rt_bind(0x00485a80u, b_00485a80, "unlisted_00485a80");
    rt_bind(0x00485dc0u, b_00485dc0, "unlisted_00485dc0");
    rt_bind(0x00485e90u, b_00485e90, "unlisted_00485e90");
    rt_bind(0x00489980u, b_00489980, "unlisted_00489980");
    rt_bind(0x00489990u, b_00489990, "unlisted_00489990");
    rt_bind(0x004899a0u, b_004899a0, "unlisted_004899a0");
    rt_bind(0x004899b0u, b_004899b0, "unlisted_004899b0");
    rt_bind(0x00497990u, b_00497990, "unlisted_00497990");
    rt_bind(0x00498534u, b_00498534, "unlisted_00498534");
    rt_bind(0x004a21b0u, b_004a21b0, "unlisted_004a21b0");
    rt_bind(0x004a2620u, b_004a2620, "unlisted_004a2620");
    rt_bind(0x004a2e33u, b_004a2e33, "unlisted_004a2e33");
    rt_bind(0x004a5f0cu, b_004a5f0c, "unlisted_004a5f0c");
    rt_bind(0x004a6150u, b_004a6150, "unlisted_004a6150");
    rt_bind(0x004a6244u, b_004a6244, "unlisted_004a6244");
    rt_bind(0x004a6404u, b_004a6404, "unlisted_004a6404");
    rt_bind(0x004a6540u, b_004a6540, "unlisted_004a6540");
    rt_bind(0x004a65d0u, b_004a65d0, "unlisted_004a65d0");
    rt_bind(0x004a69f0u, b_004a69f0, "unlisted_004a69f0");
    rt_bind(0x004a6a39u, b_004a6a39, "unlisted_004a6a39");
    rt_bind(0x004a8ae0u, b_004a8ae0, "unlisted_004a8ae0");
    rt_bind(0x004aa3a8u, b_004aa3a8, "unlisted_004aa3a8");
    rt_bind(0x004ac680u, b_004ac680, "unlisted_004ac680");
    rt_bind(0x004acd40u, b_004acd40, "unlisted_004acd40");
    rt_bind(0x004b27d3u, b_004b27d3, "unlisted_004b27d3");
    rt_bind(0x004b3010u, b_004b3010, "unlisted_004b3010");
    rt_bind(0x004b7f90u, b_004b7f90, "unlisted_004b7f90");
    rt_bind(0x004b8550u, b_004b8550, "unlisted_004b8550");
    rt_bind(0x004b8574u, b_004b8574, "unlisted_004b8574");
    rt_bind(0x004b9380u, b_004b9380, "unlisted_004b9380");
    rt_bind(0x004b9390u, b_004b9390, "unlisted_004b9390");
    rt_bind(0x004b93b4u, b_004b93b4, "unlisted_004b93b4");
    rt_bind(0x004ba49cu, b_004ba49c, "unlisted_004ba49c");
    rt_bind(0x004ba4acu, b_004ba4ac, "unlisted_004ba4ac");
    rt_bind(0x004baa68u, b_004baa68, "unlisted_004baa68");
    rt_bind(0x004bae1cu, b_004bae1c, "unlisted_004bae1c");
    rt_bind(0x004bae34u, b_004bae34, "unlisted_004bae34");
    rt_bind(0x004bba30u, b_004bba30, "unlisted_004bba30");
    rt_bind(0x004bfed8u, b_004bfed8, "unlisted_004bfed8");
    rt_bind(0x004bfefcu, b_004bfefc, "unlisted_004bfefc");
}

RT_BIND_AREA(unlisted)
