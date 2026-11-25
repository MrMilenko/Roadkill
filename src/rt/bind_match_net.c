/*
 * bind_match_net.c: network sends, the start of match handshake, the receive pump and
 * humanCarsRestart (src/match/mt_net.c), called the way tools/emu/diff_match_net.py called them.
 */
#include "rt_env.h"
#include "match/mt_net.h"

static uint32_t c0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static uint32_t c1(uint32_t fn, uint32_t eax) { uint32_t a[1] = {eax}; return rt_call(fn, a, 1); }

static uint8_t* n_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t cb_net_18(void* u, uint32_t net) { (void)u; return c1(0x004aaa58u, net); }
static uint32_t cb_net_0c(void* u, uint32_t net) { (void)u; return c1(0x004aaa5cu, net); }
static uint32_t cb_net_iface(void* u, uint32_t net) { (void)u; return c1(0x004aa68cu, net); }
/* the DirectPlay interface's Send (vtable +0x68, read from game memory), stdcall (this, from, to,
 * flags, data, size) */
static uint32_t cb_send(void* u, uint32_t fn, uint32_t iface, uint32_t from, uint32_t to, uint32_t flags,
                        uint32_t data, uint32_t size)
{
    (void)u;
    uint32_t a[6] = {iface, from, to, flags, data, size};
    return rt_call(fn, a, 6);
}
static int32_t cb_session_count(void* u, uint32_t s) { (void)u; return (int32_t)c1(0x00498968u, s); }
static void cb_input_poll(void* u, uint32_t v) { (void)u; c1(0x00493748u, v); }
static void cb_net_pump(void* u) { (void)u; c0(0x00463f78u); }
static uint32_t cb_time_now(void* u) { (void)u; return c0(0x00c90640u); }

static const MtNet* env(void)
{
    static MtNet e;
    static int done;
    if (!done) {
        e.packet = rt_ptr(0x0050f150u);
        e.seq11 = (uint32_t*)rt_ptr(0x004e7ad8u);
        e.seq11_hi = (uint16_t*)rt_ptr(0x0050f560u);
        e.seq22 = (uint32_t*)rt_ptr(0x004e7ad0u);
        e.seq22_hi = (uint16_t*)rt_ptr(0x0050f550u);
        e.session = (const uint32_t*)rt_ptr(0x0050f9dcu);
        e.ready = (int32_t*)rt_ptr(0x0050f5fcu);
        e.acks = (int32_t*)rt_ptr(0x0050f64cu);
        e.last_time = (uint32_t*)rt_ptr(0x0050f8d4u);
        e.now = (uint32_t*)rt_ptr(0x0050f8d8u);
        e.resolve = n_resolve;
        e.net = 0x0050f9a8u;
        e.net_18 = cb_net_18;
        e.net_0c = cb_net_0c;
        e.net_iface = cb_net_iface;
        e.send = cb_send;
        e.session_count = cb_session_count;
        e.input_poll = cb_input_poll;
        e.net_pump = cb_net_pump;
        e.time_now = cb_time_now;
        done = 1;
    }
    return &e;
}

/* ---- the receive pump. The port keeps its receive buffer object (0x84 bytes) and the from / to
 * / size words on the host stack and passes host pointers; under the emulator those were only
 * compared as "LOCAL + offset". Here the object gets a game address frame for the whole pump
 * (pushed at buf_init, popped at buf_done: the object's own code may keep pointers into it), and
 * Receive gets a 12 byte frame for from / to / size, copied back to the port's words. */
#define PUMP_DEPTH 8
static struct { uintptr_t host; uint32_t game; } pobj[PUMP_DEPTH];
static int npobj;

static uint32_t obj_game(uintptr_t host)
{
    for (int i = npobj - 1; i >= 0; i--) if (pobj[i].host == host) return pobj[i].game;
    rt_stop("net pump: receive buffer object not initialized", (uint32_t)host);
    return 0;
}
static uint32_t cb_buf_init(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx)
{
    (void)u; (void)edx; (void)ebx; (void)ecx;
    if (npobj == PUMP_DEPTH) rt_stop("net pump nested too deep", 0x00463f78u);
    uint32_t g = rt_frame_push(0x84u);
    pobj[npobj].host = eax;
    pobj[npobj].game = g;
    npobj++;
    return c1(0x004aab90u, g);
}
static uint32_t cb_buf_error(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx)
{
    (void)u; (void)edx; (void)ebx; (void)ecx;
    return c1(0x004aac00u, obj_game(eax));
}
static uint32_t cb_buf_done(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx)
{
    (void)u; (void)ebx; (void)ecx;
    uint32_t a[2] = {obj_game(eax), (uint32_t)edx};
    uint32_t r = rt_call(0x004aabc4u, a, 2);
    if (npobj > 0 && pobj[npobj - 1].host == eax) {
        npobj--;
        rt_frame_pop(0x84u);
    }
    return r;
}
/* Receive (vtable +0x64), stdcall (this, &from, &to, flags, data, &size) */
static uint32_t cb_receive(void* u, uint32_t fn, uint32_t iface, uint32_t* from, uint32_t* to, uint32_t flags,
                           uint32_t data, uint32_t* size)
{
    (void)u;
    uint32_t f = rt_frame_push(12u);
    rt_w32(f, *from);
    rt_w32(f + 4, *to);
    rt_w32(f + 8, *size);
    uint32_t a[6] = {iface, f, f + 4, flags, data, f + 8};
    uint32_t r = rt_call(fn, a, 6);
    *from = rt_r32(f);
    *to = rt_r32(f + 4);
    *size = rt_r32(f + 8);
    rt_frame_pop(12u);
    return r;
}
static uint32_t ucall(uint32_t fn, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx)
{
    uint32_t a[4] = {(uint32_t)eax, (uint32_t)edx, (uint32_t)ebx, (uint32_t)ecx};
    return rt_call(fn, a, 4);
}
static uint32_t cb_system_msg(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx)
{ (void)u; return ucall(0x00464088u, eax, edx, ebx, ecx); }
static uint32_t cb_game_msg(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx)
{ (void)u; return ucall(0x0046441cu, eax, edx, ebx, ecx); }
static uint32_t cb_player_msg(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx)
{ (void)u; return ucall(0x00498660u, eax, edx, ebx, ecx); }
static uint32_t cb_player_data(void* u, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx)
{ (void)u; return ucall(0x00473714u, eax, edx, ebx, ecx); }

static const MtNetPump* pump_env(void)
{
    static MtNetPump p;
    p.net = env();
    p.buf_init = cb_buf_init;
    p.buf_error = cb_buf_error;
    p.buf_done = cb_buf_done;
    p.receive = cb_receive;
    p.system_msg = cb_system_msg;
    p.game_msg = cb_game_msg;
    p.player_msg = cb_player_msg;
    p.player_data = cb_player_data;
    return &p;
}

/* ---- humanCarsRestart */
static uint32_t cb_timer(void* u) { (void)u; return c0(0x00493038u); }
static void cb_pump(void* u) { (void)u; c0(0x00463f78u); }
static void cb_input_poll0(void* u) { (void)u; c0(0x00493748u); }
static void cb_car_reset(void* u, int32_t i) { (void)u; c1(0x0045ba64u, (uint32_t)i); }

static const MtNetRestart* restart_env(void)
{
    static MtNetRestart r;
    r.net = env();
    r.server = (const int32_t*)rt_ptr(0x004e7ae0u);
    r.got = rt_ptr(0x0050f8e8u);
    r.timer = cb_timer;
    r.pump = cb_pump;
    r.input_poll = cb_input_poll0;
    r.car_reset = cb_car_reset;
    return &r;
}

#define R(i) rt_arg(a, n, i)
#define NOARG (void)a; (void)n

/* 0x00463e38 (type, data, size): data is a game address */
static uint32_t b_net_send(const uint32_t* a, int n) { mt_net_send(env(), R(0), rt_ptr(R(1)), (int32_t)R(2)); return 0; }
/* 0x00463d9c (to, b, data, size) */
static uint32_t b_net_send_to(const uint32_t* a, int n) { mt_net_send_to(env(), R(0), (uint8_t)R(1), rt_ptr(R(2)), R(3)); return 0; }
static uint32_t b_net_wait_ready(const uint32_t* a, int n) { return (uint32_t)mt_net_wait_ready(env(), (int32_t)R(0)); }
static uint32_t b_net_pump(const uint32_t* a, int n) { NOARG; mt_net_pump(pump_env()); return 0; }
static uint32_t b_cars_restart(const uint32_t* a, int n) { NOARG; mt_net_cars_restart(restart_env()); return 0; }

void rt_bind_match_net(void)
{
    rt_bind(0x00463e38u, b_net_send, "net_send");
    rt_bind(0x00463d9cu, b_net_send_to, "net_send_to");
    rt_bind(0x004632f4u, b_net_wait_ready, "net_wait_ready");
    rt_bind(0x00463f78u, b_net_pump, "net_receive_pump");
    rt_bind(0x0046500cu, b_cars_restart, "humanCarsRestart");
}

RT_BIND_AREA(match_net)
