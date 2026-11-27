/*
 * bind_match_mt2_ui_net.c: the network session dialog pieces 0x004670fc..0x00469a8c (host and
 * join screens, src/match/mt2_ui_net.c), called the way tools/emu/diff_mt2_ui_net.py and
 * diff_mt2_ui_netinfo.py called them.
 */
#include "rt_env.h"
#include "match/mt2_ui_net.h"
#include "qol_arenas.h"

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
        if (!rt_is_host_ptr(v)) a[i] = (uint32_t)v;
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
        if (rt_is_host_ptr(args[i]) && !is_game_ptr(args[i])) a[i] = f + (uint32_t)(args[i] - lo);
    uint32_t r = rt_call(fn, a, n);
    const uint8_t* g = rt_ptr(f);
    for (uint32_t k = 0; k < span; k++)
        if (g[k] != snap[k]) ((uint8_t*)lo)[k] = g[k];
    rt_frame_pop(span);
    rt_frame_pop(span);
    return r;
}

static uint8_t* u_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

/* the port's one callback: fn (an original address, a method table entry, or an import slot with
 * its stdcall words), args = EAX, EDX, EBX, ECX, then the stack words */
static uint32_t u_call(void* u, uint32_t fn, int n, const uintptr_t* args) { (void)u; return xcall(fn, args, n); }

static const Mt2UiNet* env(void)
{
    static Mt2UiNet e;
    static int done;
    if (!done) {
        e.g = rt_ptr(0x0050f9a8u);
        e.game_type = rt_ptr(0x00624ea8u);
        e.arena = rt_ptr(0x00624ec7u);
        e.resolve = u_resolve;
        e.call = u_call;
        done = 1;
    }
    return &e;
}

#define R(i) rt_arg(a, n, i)
#define NOARG (void)a; (void)n
#define V(f) static uint32_t b_##f(const uint32_t* a, int n) { NOARG; mt2_ui_net_##f(env()); return 0; }

/* 0x004670fc / 0x00467160 (text a, text b): EAX, EDX */
static uint32_t b_list_add(const uint32_t* a, int n) { mt2_ui_net_list_add(env(), R(0), R(1)); return 0; }
static uint32_t b_list_add_player(const uint32_t* a, int n) { mt2_ui_net_list_add_player(env(), R(0), R(1)); return 0; }
static uint32_t b_host_run(const uint32_t* a, int n) { NOARG; return mt2_ui_net_host_run(env()); }
static uint32_t b_join_run(const uint32_t* a, int n) { NOARG; return mt2_ui_net_join_run(env()); }
/* 0x00469a8c (item in EAX) */
static uint32_t b_join_select(const uint32_t* a, int n) { mt2_ui_net_join_select(env(), R(0)); return 0; }
/* + [gameplay] hidden_arenas (qol_arenas.h) */
static uint32_t b_host_build(const uint32_t* a, int n)
{
    NOARG;
    qola_host_build(1);
    mt2_ui_net_host_build(env());
    qola_host_build(0);
    return 0;
}
V(host_destroy)
V(host_start)
V(join_restore_cancel)
V(join_restore)
V(join_build)
V(join_destroy)
V(join_info)
V(join_go)

void rt_bind_match_mt2_ui_net(void)
{
    rt_bind(0x004670fcu, b_list_add, "netdlg_list_add");
    rt_bind(0x00467160u, b_list_add_player, "netdlg_list_add_player");
    rt_bind(0x00467740u, b_host_run, "netdlg_host_run");
    rt_bind(0x00467904u, b_host_build, "netdlg_host_build");
    rt_bind(0x004682c8u, b_host_destroy, "netdlg_host_destroy");
    rt_bind(0x004684ecu, b_host_start, "netdlg_host_start");
    rt_bind(0x00468850u, b_join_restore_cancel, "netdlg_join_restore_cancel");
    rt_bind(0x00468988u, b_join_restore, "netdlg_join_restore");
    rt_bind(0x00468aa4u, b_join_run, "netdlg_join_run");
    rt_bind(0x00468db8u, b_join_build, "netdlg_join_build");
    rt_bind(0x004692bcu, b_join_destroy, "netdlg_join_destroy");
    rt_bind(0x004693ccu, b_join_info, "netdlg_join_info");
    rt_bind(0x004699c4u, b_join_go, "netdlg_join_go");
    rt_bind(0x00469a8cu, b_join_select, "netdlg_join_select");
}

RT_BIND_AREA(match_mt2_ui_net)
