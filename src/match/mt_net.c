/*
 * mt_net.c: network packet sends and the start of match handshake (see mt_net.h).
 */
#include "mt_net.h"

#include <string.h>

static uint32_t r32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }

#define PACKET 0x0050f150u
#define BUSY 0x8877010eu

static void send_loop(const MtNet* n, uint32_t iface, uint32_t from, uint32_t to, uint32_t flags, uint32_t size) {
    uint32_t fn = r32(n->resolve(n->user, r32(n->resolve(n->user, iface))) + 0x68);
    while (n->send(n->user, fn, iface, from, to, flags, PACKET, size) == BUSY)
        fn = r32(n->resolve(n->user, r32(n->resolve(n->user, iface))) + 0x68);
}

static void stamp11(const MtNet* n) {
    uint32_t seq = *n->seq11;
    memcpy(n->packet + 8, &seq, 4);
    uint16_t hi = *n->seq11_hi;
    *n->seq11 = seq + 1;
    memcpy(n->packet + 2, &hi, 2);
    if (seq + 1 == 0) *n->seq11_hi = (uint16_t)(hi + 1);
}

void mt_net_send(const MtNet* n, uint32_t type, const uint8_t* data, int32_t size) {
    if (size > 0x400 || size < 0) return;
    uint32_t iface = n->net_iface(n->user, n->net);
    uint32_t from = r32(n->resolve(n->user, *n->session) + 0x30);
    if (type == 0x11) {
        memcpy(n->packet, data, (size_t)size);
        n->packet[0] = 0x11;
        stamp11(n);
        send_loop(n, iface, from, 0, 1, (uint32_t)size);
    } else if (type == 0x22) {
        memcpy(n->packet + 0xc, data, (size_t)size);
        n->packet[0] = 0x22;
        uint32_t seq = *n->seq22;
        memcpy(n->packet + 8, &seq, 4);
        uint16_t hi = *n->seq22_hi;
        memcpy(n->packet + 2, &hi, 2);
        *n->seq22 = seq + 1;
        if (seq + 1 == 0) *n->seq22_hi = (uint16_t)(*n->seq22_hi + 1);
        send_loop(n, iface, from, 0, 0, (uint32_t)size + 0xc);
    }
}

void mt_net_send_to(const MtNet* n, uint32_t to, uint8_t b, const uint8_t* data, uint32_t size) {
    uint32_t iface = n->net_iface(n->user, n->net);
    uint32_t from = r32(n->resolve(n->user, *n->session) + 0x30);
    memcpy(n->packet, data, size);
    n->packet[1] = b;
    n->packet[0] = 0x11;
    stamp11(n);
    send_loop(n, iface, from, to, 1, size);
}

static void hello(const MtNet* n, int32_t i) {
    uint8_t pkt[0x20];
    memset(pkt, 0, sizeof pkt);
    pkt[1] = n->net_0c(n->user, n->net) != 0;
    memcpy(pkt + 0x10, &i, 4);
    mt_net_send(n, 0x11, pkt, 0x20);
}

int32_t mt_net_wait_ready(const MtNet* n, int32_t i) {
    uint32_t mp = n->net_18(n->user, n->net);
    if (!(uint8_t)mp) return 1;
    n->input_poll(n->user, mp);
    n->net_pump(n->user);
    if (n->net_0c(n->user, n->net)) {
        int32_t* a = (int32_t*)((uint8_t*)n->acks + (uint32_t)i * 32u);
        a[0] = 1;
        int32_t cnt = n->session_count(n->user, *n->session), got = 0;
        for (int k = 0; k < 8; k++) got += a[k] == 1;
        if (got == cnt) n->ready[i] = 1;
        if (n->ready[i] == 1) {
            hello(n, i);
            hello(n, i);
            hello(n, i);
            return 1;
        }
        uint32_t t = n->time_now(n->user);
        *n->now = t;
        if ((int32_t)(t - *n->last_time) > 0x12c) *n->last_time = t;
        return 0;
    }
    if (n->ready[i] == 1) return 1;
    uint32_t t = n->time_now(n->user);
    *n->now = t;
    if ((int32_t)(t - *n->last_time) > 0x12c) {
        hello(n, i);
        *n->last_time = *n->now;
    }
    return 0;
}

void mt_net_pump(const MtNetPump* p) {
    const MtNet* n = p->net;
    void* u = n->user;
    struct { uint8_t obj[0x84]; uint32_t size, from, to; } st;
    memset(&st, 0, sizeof st);
    st.size = 0x400;
    p->buf_init(u, (uintptr_t)st.obj, 0, 0, 0);
    uint32_t iface = n->net_iface(u, n->net);
    int done = 0;
    do {
        st.size = 0x400;
        uint32_t fn = r32(n->resolve(u, r32(n->resolve(u, iface))) + 0x64);
        uint32_t r = p->receive(u, fn, iface, &st.from, &st.to, 1, PACKET, &st.size);
        if (r != 0) {
            done = 1;
            if (r != 0x887700beu) p->buf_error(u, (uintptr_t)st.obj, 0, 0, 0);
        } else if (st.from == 0) {
            p->system_msg(u, PACKET, 0, 0, 0);
        } else {
            uint8_t t = n->packet[0];
            if (t == 0x11) {
                p->game_msg(u, st.from, st.to, 0, 0);
            } else if (t == 0x22) {
                int32_t res = (int32_t)p->player_msg(u, *n->session, st.from, 0, 0);
                if (res > 0) p->player_data(u, PACKET + 0xc, st.size, (uint32_t)res, 0);
            }
        }
    } while (!done);
    p->buf_done(u, (uintptr_t)st.obj, 0, 0, 0);
}

static void wait_second(const MtNetRestart* r) {
    void* u = r->net->user;
    uint32_t end = r->timer(u) + 1000000u;
    if (end == 0) return;
    do {
        r->pump(u);
        r->input_poll(u);
    } while (r->timer(u) < end);
}

void mt_net_cars_restart(const MtNetRestart* r) {
    const MtNet* n = r->net;
    uint8_t hello[0x20], pkt[0x20];
    memset(hello, 0, sizeof hello);
    hello[1] = 0x18;
    int32_t i = 0;
    if (*r->server == -1) {
        do {
            wait_second(r);
            mt_net_send(n, 0x11, hello, 0x20);
        } while (*r->server == -1);
    }
    for (;;) {
        const uint8_t* s = n->resolve(n->user, *n->session);
        int32_t cnt;
        memcpy(&cnt, s + 0x24, 4);
        if (i >= cnt) break;
        wait_second(r);
        if (r->got[i]) {
            r->car_reset(n->user, i);
            i++;
        }
        s = n->resolve(n->user, *n->session);
        memcpy(&cnt, s + 0x24, 4);
        if (i >= cnt) continue;
        memset(pkt, 0, sizeof pkt);
        pkt[1] = 0x11;
        uint32_t id = r32(n->resolve(n->user, r32(s + 4 + 4 * i)));
        memcpy(pkt + 0x10, &id, 4);
        mt_net_send_to(n, (uint32_t)*r->server, 0x11, pkt, 0x20);
    }
}
