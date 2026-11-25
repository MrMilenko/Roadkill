/*
 * bind_match_mt2_net.c: the network message handlers and the join request 0x0046441c,
 * 0x00464088, 0x004637c4, 0x00464dec (src/match/mt2_net.c), called the way
 * tools/emu/diff_mt2_net.py called them.
 */
#include "rt_env.h"
#include "match/mt2_net.h"

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

static uint8_t* n_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

/* an Mt2NetFn callback onto the original address: EAX, EDX, EBX, ECX (data buffers of the port are
 * host pointers: xcall gives them game addresses) */
#define NCB(nm, fnexpr) \
    static uint32_t nm(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx) \
    { (void)u; uintptr_t v[4] = {eax, edx, ebx, ecx}; return xcall((fnexpr), v, 4); }
/* the same with no argument (callees that read none; imports take no stdcall words) */
#define NCB0(nm, fnexpr) \
    static uint32_t nm(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx) \
    { (void)u; (void)eax; (void)edx; (void)ebx; (void)ecx; return rt_call((fnexpr), NULL, 0); }

NCB(cb_net_0c, 0x004aaa5cu)
NCB(cb_net_60, 0x004aaa60u)
NCB0(cb_time_now, 0x00c90640u)     /* timeGetTime: its import slot */
NCB(cb_find_114, 0x00498660u)
NCB(cb_set_11c, 0x004986f8u)
NCB(cb_entry_key, 0x0049864cu)
NCB(cb_remove, 0x00498820u)
NCB(cb_word_30, 0x004988e4u)
NCB(cb_list_find, 0x00497c50u)
NCB(cb_pl_194, 0x00498194u)
NCB(cb_pl_1e0, 0x004981e0u)
NCB(cb_pl_250, 0x00498250u)
NCB(cb_text_0fc, 0x004670fcu)
NCB(cb_text_160, 0x00467160u)
NCB(cb_scr_1c8, 0x004671c8u)
NCB(cb_set_flag, 0x004671e8u)
NCB(cb_scr_224, 0x00467224u)
NCB(cb_car_by_id, 0x0045b988u)
NCB(cb_car_get, 0x0045afbcu)
NCB(cb_rt_get, 0x0046ca60u)
NCB(cb_hud_post, 0x00436040u)
NCB(cb_car_leave, 0x0045edd0u)
NCB(cb_reg_window, 0x00499884u)
NCB(cb_send, 0x00463e38u)
NCB(cb_send_to, 0x00463d9cu)
NCB(cb_pump, 0x00463f78u)
NCB(cb_input_poll, 0x00493748u)
NCB(cb_input_check, 0x004936acu)
NCB(cb_timer, 0x00493038u)
NCB0(cb_input_poll0, 0x00493748u)

/* DirectPlay vtable +0x74 (read from game memory), stdcall (this, id, &id, 0x138, 2) */
static uint32_t cb_dplay_call(void* u, uint32_t fn, uint32_t iface, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)u;
    uint32_t v[5] = {iface, a, b, c, d};
    return rt_call(fn, v, 5);
}

static const Mt2Net* env(void)
{
    static Mt2Net e;
    static int done;
    if (!done) {
        e.packet = rt_ptr(0x0050f150u);
        e.joined = (int32_t*)rt_ptr(0x0050f580u);
        e.slot = (int32_t*)rt_ptr(0x0050f5bcu);
        e.ready = (int32_t*)rt_ptr(0x0050f5fcu);
        e.acks = (int32_t*)rt_ptr(0x0050f64cu);
        e.cur = (int32_t*)rt_ptr(0x0050f8ccu);
        e.prev = (int32_t*)rt_ptr(0x0050f8d0u);
        e.last_time = (uint32_t*)rt_ptr(0x0050f8d4u);
        e.flag_8dc = (int32_t*)rt_ptr(0x0050f8dcu);
        e.got_8e0 = rt_ptr(0x0050f8e0u);
        e.got_8e8 = rt_ptr(0x0050f8e8u);
        e.flag_8f0 = rt_ptr(0x0050f8f0u);
        e.dplay = (const uint32_t*)rt_ptr(0x0050f9ccu);
        e.session = (const uint32_t*)rt_ptr(0x0050f9dcu);
        e.players = (const uint32_t*)rt_ptr(0x0050f9e0u);
        e.stats = (int32_t*)rt_ptr(0x00624db8u);
        e.server = (int32_t*)rt_ptr(0x004e7ae0u);
        /* ORIGINAL BUG: the uninitialized stack local (no free slot); 0 as the header says */
        e.stale = 0;
        e.resolve = n_resolve;
        e.net_0c = cb_net_0c;
        e.net_60 = cb_net_60;
        e.time_now = cb_time_now;
        e.find_114 = cb_find_114;
        e.set_11c = cb_set_11c;
        e.entry_key = cb_entry_key;
        e.remove = cb_remove;
        e.word_30 = cb_word_30;
        e.list_find = cb_list_find;
        e.pl_194 = cb_pl_194;
        e.pl_1e0 = cb_pl_1e0;
        e.pl_250 = cb_pl_250;
        e.text_0fc = cb_text_0fc;
        e.text_160 = cb_text_160;
        e.scr_1c8 = cb_scr_1c8;
        e.set_flag = cb_set_flag;
        e.scr_224 = cb_scr_224;
        e.car_by_id = cb_car_by_id;
        e.car_get = cb_car_get;
        e.rt_get = cb_rt_get;
        e.hud_post = cb_hud_post;
        e.car_leave = cb_car_leave;
        e.reg_window = cb_reg_window;
        e.dplay_call = cb_dplay_call;
        e.send = cb_send;
        e.send_to = cb_send_to;
        e.pump = cb_pump;
        e.input_poll = cb_input_poll;
        e.input_check = cb_input_check;
        e.timer = cb_timer;
        e.input_poll0 = cb_input_poll0;
        done = 1;
    }
    return &e;
}

#define R(i) rt_arg(a, n, i)

/* 0x0046441c (from) */
static uint32_t b_game_msg(const uint32_t* a, int n) { mt2_net_game_msg(env(), R(0)); return 0; }
/* 0x00464088 (msg): a game address (the pump passes the packet 0x0050f150) */
static uint32_t b_system_msg(const uint32_t* a, int n) { mt2_net_system_msg(env(), rt_ptr(R(0))); return 0; }
/* 0x004637c4 (player) */
static uint32_t b_join(const uint32_t* a, int n) { return (uint32_t)mt2_net_join(env(), (int32_t)R(0)); }
/* 0x00464dec () */
static uint32_t b_ask_stats(const uint32_t* a, int n) { (void)a; (void)n; mt2_net_ask_stats(env()); return 0; }

void rt_bind_match_mt2_net(void)
{
    rt_bind(0x0046441cu, b_game_msg, "net_game_msg");
    rt_bind(0x00464088u, b_system_msg, "net_system_msg");
    rt_bind(0x004637c4u, b_join, "net_join");
    rt_bind(0x00464decu, b_ask_stats, "net_ask_stats");
}

RT_BIND_AREA(match_mt2_net)
