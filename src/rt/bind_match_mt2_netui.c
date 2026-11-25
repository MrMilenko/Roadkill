/*
 * bind_match_mt2_netui.c: the network setup screen 0x00465110, 0x004653d4, 0x004658f0,
 * 0x00465a3c, 0x00465b7c, 0x00465cd4 (src/match/mt2_netui.c), called the way
 * tools/emu/diff_mt2_netui.py called them.
 */
#include "rt_env.h"
#include "match/mt2_netui.h"

/* A call whose arguments may be host pointers (uintptr_t interfaces). A host pointer into the
 * game address space becomes its game address. A host pointer to the port's own locals (its
 * buffers on the host stack: text, a registry key object, a size word), which the emulator harness
 * compared by content only, gets a game address copy: one frame holding the host bytes from the
 * lowest such pointer to XWIN past the highest (their layout kept), translated, and after the
 * call the bytes the callee changed are written back. */
#define XWIN 0x200u
#define XMAX 16
static int is_game_ptr(uintptr_t v) { return v >= (uintptr_t)rt_base && v < (uintptr_t)rt_base + RT_SIZE; }

static uint32_t xcall(uint32_t fn, const uintptr_t* args, int n)
{
    uint32_t a[XMAX];
    uintptr_t lo = UINTPTR_MAX, hi = 0;
    if (n > XMAX) rt_stop("too many arguments", fn);
    for (int i = 0; i < n; i++) {
        uintptr_t v = args[i];
        if (v <= 0xffffffffu) a[i] = (uint32_t)v;
        else if (is_game_ptr(v)) a[i] = rt_addr((const void*)v);
        else { if (v < lo) lo = v; if (v > hi) hi = v; }
    }
    if (!hi) return rt_call(fn, a, n);
    uint32_t span = (uint32_t)(hi - lo) + XWIN;
    if (hi - lo > 0x10000u) rt_stop("host locals too far apart", fn);
    uint32_t f = rt_frame_push(span);
    uint8_t* snap = rt_ptr(rt_frame_push(span));
    memcpy(rt_ptr(f), (const void*)lo, span);
    memcpy(snap, (const void*)lo, span);
    for (int i = 0; i < n; i++)
        if (args[i] > 0xffffffffu && !is_game_ptr(args[i])) a[i] = f + (uint32_t)(args[i] - lo);
    uint32_t r = rt_call(fn, a, n);
    const uint8_t* g = rt_ptr(f);
    for (uint32_t k = 0; k < span; k++)
        if (g[k] != snap[k]) ((uint8_t*)lo)[k] = g[k];
    rt_frame_pop(span);
    rt_frame_pop(span);
    return r;
}

static uint8_t* u_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

/* the port's dispatcher: args = EAX, EDX, EBX, ECX, then the stack words. For a Win32 import
 * (lstrcmpA 0x00c90318, MessageBoxA 0x00c90524) the port passes four zero register words first;
 * the harness dropped them: the stdcall words are args[4..]. */
static uint32_t u_call(void* u, uint32_t addr, const uintptr_t* args, int n)
{
    (void)u;
    if (rt_import_name(addr)) return n > 4 ? xcall(addr, args + 4, n - 4) : xcall(addr, args, 0);
    return xcall(addr, args, n);
}

static const Mt2NetUi* env(void)
{
    static Mt2NetUi e;
    static int done;
    if (!done) {
        e.session = (const uint32_t*)rt_ptr(0x0050f9dcu);
        e.enumr = (const uint32_t*)rt_ptr(0x0050f9d4u);
        e.result = (uint32_t*)rt_ptr(0x0050f9e4u);
        e.result2 = (uint32_t*)rt_ptr(0x0050f9e8u);
        e.provider = (uint32_t*)rt_ptr(0x0050f9ecu);
        e.w = (uint32_t*)rt_ptr(0x0050f9f0u);
        e.defaults = (const uint32_t*)rt_ptr(0x004e7ae8u);
        e.first = (int32_t*)rt_ptr(0x004e7ae4u);
        e.bdfd20 = (uint32_t*)rt_ptr(0x00bdfd20u);
        e.resolve = u_resolve;
        e.call = u_call;
        done = 1;
    }
    return &e;
}

#define NOARG (void)a; (void)n

static uint32_t b_run(const uint32_t* a, int n) { NOARG; return mt2_netui_run(env()); }
static uint32_t b_build(const uint32_t* a, int n) { NOARG; mt2_netui_build(env()); return 0; }
static uint32_t b_free(const uint32_t* a, int n) { NOARG; mt2_netui_free(env()); return 0; }
/* the click handlers: their arguments are not read */
static uint32_t b_connect_a(const uint32_t* a, int n) { NOARG; mt2_netui_connect(env(), 2, 0x004e14e1u); return 0; }
static uint32_t b_connect_b(const uint32_t* a, int n) { NOARG; mt2_netui_connect(env(), 3, 0x004e14e9u); return 0; }
/* 0x00465cd4 (list widget in EAX) */
static uint32_t b_select(const uint32_t* a, int n) { mt2_netui_select(env(), rt_arg(a, n, 0)); return 0; }

void rt_bind_match_mt2_netui(void)
{
    rt_bind(0x00465110u, b_run, "netui_run");
    rt_bind(0x004653d4u, b_build, "netui_build");
    rt_bind(0x004658f0u, b_free, "netui_free");
    rt_bind(0x00465a3cu, b_connect_a, "netui_connect_a");
    rt_bind(0x00465b7cu, b_connect_b, "netui_connect_b");
    rt_bind(0x00465cd4u, b_select, "netui_select");
}

RT_BIND_AREA(match_mt2_netui)
