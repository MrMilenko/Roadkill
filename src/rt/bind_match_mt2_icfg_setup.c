/*
 * bind_match_mt2_icfg_setup.c: the input configuration screen's set up 0x0045f3b4 / 0x0045fc3c
 * (src/match/mt2_icfg_setup.c), called the way tools/emu/diff_mt2_icfg_setup.py called them.
 *
 * Emulator stand ins made real: SetRect (0x00c90574) is passed rect 0 by the port (the
 * original's local, never read): here a 16 byte frame. call_buf's 300 byte host buffer is copied
 * to a frame, passed in eax (reg 0) or edx (reg 1), and copied back.
 */
#include "rt_env.h"
#include "match/mt2_icfg_setup.h"

static uint8_t* s_resolve(void* u, uint32_t a) { (void)u; return rt_ptr(a); }

static uint32_t s_call(void* u, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                       const uint32_t* st, int k)
{
    (void)u;
    if (fn == 0x00c90574u || fn == 0x00c904f8u) {   /* Win32 imports: the stdcall words */
        uint32_t a[8] = {0};
        if (k > 8) k = 8;
        for (int i = 0; i < k; i++) a[i] = st[i];
        if (fn == 0x00c90574u && a[0] == 0) {
            a[0] = rt_frame_push(16);
            uint32_t r = rt_call(fn, a, k);
            rt_frame_pop(16);
            return r;
        }
        return rt_call(fn, a, k);
    }
    uint32_t a[4 + 8] = {eax, edx, ebx, ecx};
    if (k > 8) k = 8;
    for (int i = 0; i < k; i++) a[4 + i] = st[i];
    return rt_call(fn, a, 4 + k);
}

static uint32_t s_call_buf(void* u, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                           char* buf, int reg)
{
    (void)u;
    uint32_t b = rt_frame_push(300);
    memcpy(rt_ptr(b), buf, 300);
    uint32_t a[4] = {eax, edx, ebx, ecx};
    a[reg ? 1 : 0] = b;
    uint32_t r = rt_call(fn, a, 4);
    memcpy(buf, rt_ptr(b), 300);
    rt_frame_pop(300);
    return r;
}

static const Mt2IcfgSetup S = {s_resolve, s_call, s_call_buf, NULL};

static uint32_t b_setup(const uint32_t* a, int n) { mt2_icfg_setup(&S, (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1)); return 0; }
static uint32_t b_create_widgets(const uint32_t* a, int n) { (void)a; (void)n; mt2_icfg_create_widgets(&S); return 0; }

void rt_bind_match_mt2_icfg_setup(void)
{
    rt_bind(0x0045f3b4u, b_create_widgets, "inputcfg_create_widgets");
    rt_bind(0x0045fc3cu, b_setup, "inputcfg_setup");
}

RT_BIND_AREA(match_mt2_icfg_setup)
