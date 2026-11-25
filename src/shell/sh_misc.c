/*
 * sh_misc.c: small shell helpers (see sh_misc.h).
 */
#include "sh_misc.h"

void sh_players_reset_ex(const ShEnv* e, int full) {
    if (full) {
        for (uint32_t b = 0; b < 8; b++) {
            uint32_t p = 0x00622d24u + 0x40cu * b;
            sh_w16(e, p, 0);
            sh_w32(e, p + 8, 0);
            sh_w32(e, p + 0xc, 0);
            sh_w32(e, p + 0x10, 0);
            sh_w16(e, p + 2, 0);
            sh_w16(e, p + 4, 0);
            uint32_t keep = sh_r16(e, p + 0x14);
            sh_w16(e, p + 6, 0);
            sh_w16(e, p + 0x14, keep & 0xc000u);
            sh_w8(e, p + 0x16, 0x7f);
            for (uint32_t k = 0x17; k <= 0x3f3; k++) sh_w8(e, p + k, 0);
        }
        for (uint32_t k = 0; k < 8; k++) sh_w8(e, 0x00624d8cu + k, 0);
    }
    sh_w32(e, 0x00624d84u, 0);
    sh_w8(e, 0x00622d3au, 0x7f);
    for (uint32_t k = 0; k < 8; k++) sh_w32(e, 0x00624d94u + 4 * k, e->frame_parity ? e->frame_parity(e->user) : 0);
    sh_w8(e, 0x00624d88u, 0);
}

void sh_players_reset(const ShEnv* e) { sh_players_reset_ex(e, 1); }

void sh_cmd_set_value(const ShEnv* e, uint32_t cmd) {
    sh_w32(e, sh_r32(e, cmd + 0xc) + 0x48, sh_r32(e, cmd + 0x10));
}

int32_t sh_cmd_step_back(const ShEnv* e, uint32_t cmd) {
    uint32_t t = sh_r32(e, cmd + 0xc);
    int32_t n = (int32_t)sh_r32(e, t + 0xb8);
    int32_t v = (int32_t)(sh_r32(e, t + 0x48) + (uint32_t)n - 1u);
    if (n == 0 || (n == -1 && v == INT32_MIN)) return 0;   /* the original faults (divide error) */
    sh_w32(e, t + 0x48, (uint32_t)(v % n));
    return v / n;
}

void sh_audio_defaults(const ShEnv* e, uint32_t c) {
    sh_w32(e, c + 0x7c, 0x9999);
    sh_w32(e, c + 0x78, 0xe666);
    sh_w32(e, c + 0x80, 0);
    sh_w32(e, c + 0x84, 0);
    static const uint8_t src[4] = {0x7c, 0x78, 0x80, 0x84}, d1[4] = {0x68, 0x64, 0x6c, 0x70}, d2[4] = {0x54, 0x50, 0x58, 0x5c};
    for (int k = 0; k < 4; k++) {
        uint32_t v = sh_r32(e, c + src[k]);
        sh_w32(e, c + d1[k], v);
        sh_w32(e, c + d2[k], v);
    }
}

#define NET_BUF 0x00622d3au
#define NET_COUNT 0x00624d84u

void sh_net_flush(const ShEnv* e, uint8_t reset) {
    int32_t c = (int32_t)sh_r32(e, NET_COUNT);
    sh_w8(e, NET_BUF + (uint32_t)c, 0x7f);
    if (e->net_send) e->net_send(e->user, 0x22, 0x00622d24u, c + 0x17);
    if (reset) sh_players_reset_ex(e, 0);
}

/* the fill point, or 0 (after net_overflow) when fewer than room bytes are free */
static int net_room(const ShEnv* e, int32_t limit, uint32_t* at) {
    int32_t c = (int32_t)sh_r32(e, NET_COUNT);
    if (c >= limit) {
        if (e->net_overflow) e->net_overflow(e->user);
        return 0;
    }
    *at = NET_BUF + (uint32_t)c;
    return 1;
}

static void net_advance(const ShEnv* e, int32_t n) { sh_w32(e, NET_COUNT, sh_r32(e, NET_COUNT) + (uint32_t)n); }

void sh_net_msg_ab(const ShEnv* e, uint8_t a, uint8_t b) {
    uint32_t p;
    if (!net_room(e, 0x3db, &p)) return;
    sh_w8(e, p, 0x2a); sh_w8(e, p + 1, a); sh_w8(e, p + 2, b); sh_w8(e, p + 3, 0);
    net_advance(e, 4);
}

void sh_net_msg_a32(const ShEnv* e, uint8_t a, uint32_t v) {
    uint32_t p;
    if (!net_room(e, 0x3d6, &p)) return;
    sh_w8(e, p, 0x2a); sh_w8(e, p + 1, a);
    net_advance(e, 8);
    sh_w32(e, p + 4, v);
}

void sh_net_msg_abc(const ShEnv* e, uint8_t a, uint8_t b, uint8_t c) {
    uint32_t p;
    if (!net_room(e, 0x3da, &p)) return;
    sh_w8(e, p, 0x2a); sh_w8(e, p + 1, a); sh_w8(e, p + 2, b);
    net_advance(e, 4);
    sh_w8(e, p + 3, c);
}

void sh_net_msg_a16_32(const ShEnv* e, uint8_t a, uint16_t h, uint32_t v) {
    uint32_t p;
    if (!net_room(e, 0x3d6, &p)) return;
    sh_w8(e, p, 0x2a); sh_w8(e, p + 1, a); sh_w16(e, p + 2, h);
    net_advance(e, 8);
    sh_w32(e, p + 4, v);
}
