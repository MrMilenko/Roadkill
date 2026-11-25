/*
 * mt2_net.c: network message handlers and the join request (see mt2_net.h).
 */
#include "mt2_net.h"

#include <string.h>

#define NET 0x0050f9a8u

static uint32_t r32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void w32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t pk(const Mt2Net* n, int off) { return r32(n->packet + off); }
static uint8_t* sess(const Mt2Net* n) { return n->resolve(n->user, *n->session); }
static uint8_t* rec(const Mt2Net* n, int32_t i) {
    return n->resolve(n->user, r32(sess(n) + 4 + (uint32_t)i * 4u));
}

/* index of the first session record whose +off equals key (count re-read each step), else -1 */
static int32_t find_rec(const Mt2Net* n, int off, uint32_t key) {
    for (int32_t i = 0; i < (int32_t)r32(sess(n) + 0x24); i++)
        if (r32(rec(n, i) + off) == key) return i;
    return -1;
}

/* a 0x20 byte packet: +1 kind, +0x10..+0x1c four words (other bytes: zeros, see the header) */
static void msg(uint8_t* b, uint8_t kind, uint32_t a, uint32_t c, uint32_t d, uint32_t e) {
    memset(b, 0, 0x20);
    b[1] = kind;
    w32(b + 0x10, a); w32(b + 0x14, c); w32(b + 0x18, d); w32(b + 0x1c, e);
}

/* the handshake reset of all 20 players (inlined three times in the original) */
static void reset20(const Mt2Net* n) {
    for (int32_t i = 0; i < 20; i++) {
        n->ready[i] = 0;
        for (int j = 0; j < 8; j++) n->acks[i * 8 + j] = 0;
        if (n->net_0c(n->user, NET, 0, 0, 0) != 0) n->acks[i * 8] = 1;
        *n->last_time = n->time_now(n->user, 0, 0, 0, 0);
        *n->prev = *n->cur;
        *n->cur = i;
    }
}

/* take slot c for player u with value v (record +0x130 = 2) */
static void take(const Mt2Net* n, int32_t c, uint32_t u, int32_t v) {
    n->joined[(int32_t)u] = 1;
    n->slot[(int32_t)u] = v;
    n->set_11c(n->user, *n->session, (uint32_t)c, u, 0);
    w32(rec(n, c) + 0x130, 2);
}

static void dplay_74(const Mt2Net* n) {
    uint32_t s = *n->session;
    uint32_t iface = *n->dplay;
    uint32_t fn = r32(n->resolve(n->user, r32(n->resolve(n->user, iface))) + 0x74);
    n->dplay_call(n->user, fn, iface, r32(n->resolve(n->user, s) + 0x30), s + 0x30, 0x138, 2);
}

void mt2_net_game_msg(const Mt2Net* n, uint32_t from) {
    uint8_t b[0x20];
    uint32_t u = pk(n, 0x10);
    int32_t k, c;
    switch (n->packet[1]) {
    case 0:
        k = (int32_t)n->find_114(n->user, *n->session, from, 0, 0);
        w32((uint8_t*)n->acks + (int32_t)((u << 5) + (uint32_t)k * 4u), 1);
        return;
    case 1:
        n->ready[(int32_t)u] = 1;
        return;
    case 2:
    case 3: {
        uint32_t node = n->list_find(n->user, *n->players, from, 0, 0);
        if (node) (n->packet[1] == 2 ? n->text_0fc : n->text_160)(n->user, node + 0x14, (uintptr_t)(n->packet + 0x20), 0, 0);
        return;
    }
    case 4: {
        int32_t state;
        k = (int32_t)n->find_114(n->user, *n->session, from, 0, 0);
        if (n->joined[(int32_t)u] == 0) {
            c = find_rec(n, 0x114, (uint32_t)k);
            if (c > -1) {
                state = 2;
                take(n, c, u, k);
            } else {
                state = n->stale;   /* ORIGINAL BUG: uninitialized */
            }
        } else {
            state = 1;
        }
        if (state == 2) {
            msg(b, 6, u, 0, 0, 0);
            n->send_to(n->user, from, 6, (uintptr_t)b, 0x20);
            msg(b, 8, u, from, 0, 0);
            n->send(n->user, 0x11, (uintptr_t)b, 0x20, 0);
            return;
        }
        uint32_t key = n->entry_key(n->user, *n->session, (uint32_t)n->slot[(int32_t)u], 0, 0);
        msg(b, 7, u, key, 0, 0);
        n->send_to(n->user, from, 7, (uintptr_t)b, 0x20);
        return;
    }
    case 5:
        k = (int32_t)n->find_114(n->user, *n->session, pk(n, 0x14), 0, 0);
        c = find_rec(n, 0x114, (uint32_t)k);
        if (c > -1) {
            n->joined[(int32_t)u] = 0;
            n->slot[(int32_t)u] = -1;
            n->set_11c(n->user, *n->session, (uint32_t)c, 0xffffffffu, 0);
            w32(rec(n, c) + 0x130, 0);
        }
        return;
    case 6:
        if (n->joined[(int32_t)u] != 0) return;
        c = find_rec(n, 0x114, 0);
        if (c > -1) take(n, c, u, 0);
        return;
    case 7:
    case 8:
        k = (int32_t)n->find_114(n->user, *n->session, pk(n, 0x14), 0, 0);
        c = find_rec(n, 0x114, (uint32_t)k);
        if (c > -1) {
            if (n->packet[1] == 7) w32(rec(n, 0) + 0x130, 1);
            take(n, c, u, k);
        }
        return;
    case 9:
        *n->flag_8dc = 1;
        return;
    case 10:
        *n->flag_8dc = 0;
        return;
    case 0xd:
        n->scr_1c8(n->user, 0, 0, 0, 0);
        return;
    case 0xe:
        n->set_flag(n->user, u, 0, 0, 0);
        return;
    case 0xf:
        msg(b, 0x10, (uint32_t)n->stats[0], (uint32_t)n->stats[1], (uint32_t)n->stats[2], 0);
        n->send_to(n->user, from, 0x10, (uintptr_t)b, 0x20);
        return;
    case 0x10:
        c = find_rec(n, 0, from);
        if (c > -1) {
            k = (int32_t)n->find_114(n->user, *n->session, from, 0, 0);
            int32_t* s = n->stats + k * 3;
            s[0] = (int32_t)pk(n, 0x10);
            s[1] = (int32_t)pk(n, 0x14);
            s[2] = (int32_t)pk(n, 0x18);
            n->got_8e0[c] = 1;
        }
        return;
    case 0x11:
        if (*n->flag_8f0 != 0) {
            k = (int32_t)n->find_114(n->user, *n->session, pk(n, 0x10), 0, 0);
            uint32_t car = n->car_by_id(n->user, (uint32_t)k, 0, 0, 0);
            uint32_t v = r32(n->resolve(n->user, car) + 0x208);
            msg(b, 0x12, pk(n, 0x10), v, 0, 0);
            n->send_to(n->user, from, 0x12, (uintptr_t)b, 0x20);
        }
        return;
    case 0x12:
        c = find_rec(n, 0, pk(n, 0x10));
        if (c > -1) {
            k = (int32_t)n->find_114(n->user, *n->session, pk(n, 0x10), 0, 0);
            uint32_t car = n->car_by_id(n->user, (uint32_t)k, 0, 0, 0);
            w32(n->resolve(n->user, car) + 0x208, pk(n, 0x14));
            n->find_114(n->user, *n->session, from, 0, 0);   /* result unused */
            n->got_8e8[c] = 1;
        }
        return;
    case 0x13:
        n->pl_194(n->user, *n->players, from, 1, 0);
        n->pl_1e0(n->user, *n->players, from, 2, 0);
        return;
    case 0x14:
        n->pl_194(n->user, *n->players, from, 0, 0);
        n->pl_1e0(n->user, *n->players, from, 1, 0);
        return;
    case 0x15:
        if (n->net_0c(n->user, NET, 0, 0, 0) == 0) n->scr_224(n->user, 1, 0, 0, 0);
        return;
    case 0x16: {
        n->pl_1e0(n->user, *n->players, pk(n, 0x10), pk(n, 0x14), 0);
        uint32_t node = n->list_find(n->user, *n->players, pk(n, 0x10), 0, 0);
        if (node) w32(n->resolve(n->user, node) + 0x10, pk(n, 0x18));
        return;
    }
    case 0x17:
        n->pl_250(n->user, *n->players, from, 1, 0);
        n->pl_1e0(n->user, *n->players, from, 4, 0);
        *n->server = (int32_t)from;
        reset20(n);
        return;
    case 0x18:
        if (n->net_0c(n->user, NET, 0, 0, 0) != 0) {
            msg(b, 0x17, 0, 0, 0, 0);
            n->send_to(n->user, from, 0x17, (uintptr_t)b, 0x20);
        }
        return;
    default:
        return;
    }
}

void mt2_net_system_msg(const Mt2Net* n, const uint8_t* m) {
    uint8_t b[0x20];
    uint32_t type = r32(m);
    if (type == 3) {
        if (r32(m + 4) == 1) {
            int32_t x = (int32_t)n->reg_window(n->user, 0x004e1340u, 0, 0, 0);
            w32(sess(n) + 0x40, x != -1);
            dplay_74(n);
        }
        reset20(n);
    } else if (type == 5) {
        uint32_t p = pk(n, 8);
        int32_t k = (int32_t)n->find_114(n->user, *n->session, p, 0, 0);
        for (int32_t i = 0; i < 20; i++)
            w32((uint8_t*)n->acks + (int32_t)((uint32_t)i * 0x20u + (uint32_t)k * 4u), 0);
        if (k > 0) {
            n->remove(n->user, *n->session, p, 0, 0);
            if (n->rt_get(n->user, 0, 0, 0, 0) != 0) {
                uint32_t car = n->car_get(n->user, (uint32_t)k, 3, 0, 0);
                w32(n->resolve(n->user, car) + 0x1d8, 0xffffffffu);
                n->hud_post(n->user, 0x3b, 3, 0, 0);
                n->car_leave(n->user, (uint32_t)k, 0, 0, 0);
            }
        }
    } else if (type == 0x101) {
        n->net_60(n->user, NET, 0, 0, 0);
        msg(b, 0x17, 0, 0, 0, 0);
        n->send(n->user, 0x11, (uintptr_t)b, 0x20, 0);
        dplay_74(n);
        reset20(n);
    } else if (type == 0x102) {
        uint32_t node = n->list_find(n->user, *n->players, r32(m + 8), 0, 0);
        if (!node) {
            n->pl_1e0(n->user, *n->players, r32(m + 8), 1, 0);
            node = n->list_find(n->user, *n->players, r32(m + 8), 0, 0);
        }
        uint8_t* d = n->resolve(n->user, node);
        uint32_t keep134 = r32(d + 0x134), keep114 = r32(d + 0x114);
        memmove(d, n->resolve(n->user, r32(m + 0xc)), 0x138);
        if (r32(d + 0xc) != 0) *n->server = (int32_t)r32(m + 8);
        w32(d + 0x114, keep114);
        w32(d + 0x134, keep134);
    }
}

int32_t mt2_net_join(const Mt2Net* n, int32_t p) {
    uint8_t b[0x20];
    n->word_30(n->user, *n->session, 0, 0, 0);
    if (n->net_0c(n->user, NET, 0, 0, 0) != 0) {
        int32_t state;
        if (n->joined[p] == 0) {
            int32_t c = find_rec(n, 0x114, 0);
            if (c > -1) {
                state = 2;
                take(n, c, (uint32_t)p, 0);
            } else {
                state = n->stale;   /* ORIGINAL BUG: uninitialized */
            }
        } else {
            state = 1;
        }
        if (state != 2) return state;
        uint32_t v = n->word_30(n->user, *n->session, 0, 0, 0);
        msg(b, 8, (uint32_t)p, v, 0, 0);
        n->send(n->user, 0x11, (uintptr_t)b, 0x20, 0);
        return 2;
    }
    n->pump(n->user, 0, 0, 0, 0);
    int32_t s = n->joined[p] == 1 ? n->slot[p] : -1;
    if (s != -1) return 0;
    w32(sess(n) + 0x160, 0);
    msg(b, 4, (uint32_t)p, 0, 0, 0);
    n->send(n->user, 0x11, (uintptr_t)b, 0x20, 0);
    uint32_t t0 = n->time_now(n->user, 0, 0, 0, 0);
    int32_t got = 0;
    for (;;) {
        if (r32(sess(n) + 0x160) != 0) return got;
        n->pump(n->user, 0, 0, 0, 0);
        uint32_t t = n->time_now(n->user, 0, 0, 0, 0);
        got = (int32_t)r32(sess(n) + 0x160);
        uint32_t eax = t - t0;
        if ((int32_t)(t - t0) > 300 && got == 0) {
            msg(b, 4, (uint32_t)p, 0, 0, 0);
            eax = n->send(n->user, 0x11, (uintptr_t)b, 0x20, 0);
            t0 = t;
        }
        n->input_poll(n->user, eax, 0, 0, 0);
        if ((n->input_check(n->user, 0x14, 0, 0, 0) & 0xff) != 0) return -1;
    }
}

void mt2_net_ask_stats(const Mt2Net* n) {
    uint8_t b[0x20];
    int32_t i = 1;
    while (i < (int32_t)r32(sess(n) + 0x24)) {
        uint32_t end = n->timer(n->user, 0, 0, 0, 0) + 1000000u;
        if (end != 0) {
            do {
                n->pump(n->user, 0, 0, 0, 0);
                n->input_poll0(n->user, 0, 0, 0, 0);
            } while (n->timer(n->user, 0, 0, 0, 0) < end);
        }
        if (n->got_8e0[i] != 0) i++;
        if (i < (int32_t)r32(sess(n) + 0x24)) {
            msg(b, 0xf, 0, 0, 0, 0);
            n->send_to(n->user, r32(rec(n, i)), 0xf, (uintptr_t)b, 0x20);
        }
    }
}
