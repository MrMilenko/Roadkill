/*
 * bind_match_registry.c: objRegistryAdd 0x0046ff40 (src/match/mt_registry.c), called the way
 * tools/emu/diff_match_registry.py called it: (obj, id) in EAX, EDX.
 */
#include "rt_env.h"
#include "match/mt_registry.h"

static uint8_t* g_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

static uint32_t b_obj_registry_add(const uint32_t* a, int n)
{
    static MtRegistry r;
    if (!r.block) { r.block = rt_ptr(0x0062129cu); r.resolve = g_resolve; }
    mt_obj_registry_add(&r, rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
    return 0;
}

void rt_bind_match_registry(void)
{
    rt_bind(0x0046ff40u, b_obj_registry_add, "objRegistryAdd");
}

RT_BIND_AREA(match_registry)
