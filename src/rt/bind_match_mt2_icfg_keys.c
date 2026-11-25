/*
 * bind_match_mt2_icfg_keys.c: the "press a key" readers 0x004605b0 / 0x00460b04
 * (src/match/mt2_icfg_keys.c), called the way tools/emu/diff_mt2_icfg_keys.py called them.
 *
 * call_buf: the port's 256 byte host buffer stands for the original's stack buffer; here it is
 * copied to a frame, passed as ebx (0x004a559c, read into) or eax (0x004a91b0, compared), and
 * copied back.
 */
#include "rt_env.h"
#include "match/mt2_icfg_keys.h"

static uint8_t* k_resolve(void* u, uint32_t a) { (void)u; return rt_ptr(a); }

static uint32_t k_call(void* u, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                       const uint32_t* st, int k)
{
    (void)u;
    uint32_t a[4 + 8] = {eax, edx, ebx, ecx};
    if (k > 8) k = 8;
    for (int i = 0; i < k; i++) a[4 + i] = st[i];
    return rt_call(fn, a, 4 + k);
}

static uint32_t k_call_buf(void* u, uint32_t fn, uint32_t eax, uint32_t edx, char* buf)
{
    (void)u;
    uint32_t b = rt_frame_push(0x100);
    memcpy(rt_ptr(b), buf, 0x100);
    uint32_t a[4] = {eax, edx, 0, 0};
    if (fn == 0x004a559cu) a[2] = b; else a[0] = b;
    uint32_t r = rt_call(fn, a, 4);
    memcpy(buf, rt_ptr(b), 0x100);
    rt_frame_pop(0x100);
    return r;
}

static const Mt2IcfgKeys K = {k_resolve, k_call, k_call_buf, NULL};

static uint32_t b_read_keyboard(const uint32_t* a, int n) { mt2_icfg_read_keyboard(&K, (int32_t)rt_arg(a, n, 0)); return 0; }
static uint32_t b_read_dev4(const uint32_t* a, int n) { mt2_icfg_read_dev4(&K, (int32_t)rt_arg(a, n, 0)); return 0; }

void rt_bind_match_mt2_icfg_keys(void)
{
    rt_bind(0x004605b0u, b_read_keyboard, "inputcfg_read_keyboard");
    rt_bind(0x00460b04u, b_read_dev4, "inputcfg_read_dev4");
}

RT_BIND_AREA(match_mt2_icfg_keys)
