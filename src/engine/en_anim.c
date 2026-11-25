/*
 * en_anim.c: scene culling test and DPC animation switch steps (see en_anim.h).
 */
#include "en_anim.h"

#include "../math/tm2math.h"

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static void wr16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* 0x004a0c1c */
void en_anim_delay(const EnAnimEnv* e, uint8_t* n, int on) {
    int32_t lo = rd16(n + (on ? 0x14 : 0x18)), hi = rd16(n + (on ? 0x16 : 0x1a));
    if (hi - lo > 1) {
        int32_t r = e->rand(e->user);
        wr16(n + 6, (uint16_t)(lo + r % (hi + 1 - lo)));
    } else {
        wr16(n + 6, (uint16_t)lo);
    }
}

/* the common start: add the frame ticks, 1 when the delay has run out */
static int due(const EnAnimEnv* e, uint8_t* n) {
    uint16_t t = (uint16_t)(rd16(n + 0xe) + e->ticks);
    wr16(n + 0xe, t);
    return t > rd16(n + 6);
}

static void trig(const EnAnimEnv* e, const uint8_t* n) {
    if (rd32(n + 0x1c) != 0 && n[5] == n[0xc]) e->trigger(e->user, rd32(n + 0x1c), n[0x13]);
}

/* 0x004a09f8 */
void en_anim_loop(const EnAnimEnv* e, uint8_t* n) {
    if (!due(e, n)) return;
    if (n[0xc] == 0) en_anim_delay(e, n, 1);
    n[0xc] = (uint8_t)(n[0xc] + 1);
    if (n[0xc] > n[9]) n[0xc] = n[8];
    trig(e, n);
    if (n[0xc] == 0) en_anim_delay(e, n, 0);
    wr16(n + 0xe, 0);
}

/* 0x004a0a78 */
void en_anim_pingpong(const EnAnimEnv* e, uint8_t* n) {
    if (!due(e, n)) return;
    if (n[0xc] == 0) en_anim_delay(e, n, 1);
    n[0xc] = (uint8_t)(n[0xc] + n[0xd]);
    if (n[0xc] == 0) en_anim_delay(e, n, 0);
    if (n[0xc] == n[9]) n[0xd] = 0xff;
    else if (n[0xc] == n[8]) n[0xd] = 1;
    trig(e, n);
    wr16(n + 0xe, 0);
}

/* 0x004a0b08 */
void en_anim_random(const EnAnimEnv* e, uint8_t* n) {
    if (!due(e, n)) return;
    int32_t d = (int32_t)n[0xb] - 1;
    int32_t r = e->rand(e->user);
    n[0xc] = (uint8_t)(d == -1 ? 0 : r % d);   /* d == 0: divide error in the original */
    en_anim_delay(e, n, n[0xc] != 0);
    trig(e, n);
    wr16(n + 0xe, 0);
}

/* 0x004a0b7c */
void en_anim_weighted(const EnAnimEnv* e, uint8_t* n) {
    if (!due(e, n)) return;
    int32_t cnt = (int32_t)n[0xb] - 1, total = 0;
    for (int32_t i = 1; i <= cnt; i++) total += i;
    int32_t r = e->rand(e->user);
    int32_t pick = total ? r % total : 0;     /* total == 0: divide error in the original */
    int32_t frame = cnt, acc = 0, k = 0;
    for (int32_t w = cnt; w > 0; w--) {
        acc += w;
        k++;
        if (pick < acc) { frame = k; break; }
    }
    n[0xc] = (uint8_t)frame;
    trig(e, n);
}

void en_anim_step(const EnAnimEnv* e, uint8_t* n) {
    switch (n[0xa]) {
    case 0: en_anim_loop(e, n); break;
    case 1: en_anim_pingpong(e, n); break;
    case 2: en_anim_random(e, n); break;
    case 3: en_anim_weighted(e, n); break;
    default: break;
    }
}

/* 0x004a0c94 */
void en_anim_set_step(const EnAnimEnv* e, uint8_t* n, uint32_t frame) {
    if (!n || frame >= n[6] || n[0] != 9) return;
    n[7] = (uint8_t)frame;
    if (rd32(n + 0xc) != 0 && n[7] == n[9]) e->trigger(e->user, rd32(n + 0xc), n[8]);
}

/* 0x004a043c */
int en_sphere_visible(const EnCullEnv* e, const int32_t bounds[3], int32_t radius, const int32_t pos[3],
                      const int32_t mat_c[9], const int32_t mat_d[9], uint8_t shift) {
    if (e->force) return 1;
    int32_t c[3], a[3], b[3];
    for (int k = 0; k < 3; k++) c[k] = tm2_wadd(pos[k], bounds[k]);
    int32_t nr = (int32_t)(0u - (uint32_t)radius);
    tm2_mat_vec_mul_row(mat_c, c, a);
    if (nr > a[0] || nr > a[1] || nr > a[2]) return 0;
    tm2_mat_vec_mul_row(mat_d, c, b);
    if (nr > b[0] || nr > b[1]) return 0;
    int32_t lim = (int32_t)(0u - ((uint32_t)e->depth << (shift & 31)) - (uint32_t)radius);
    if (lim > b[2]) return 0;
    return 1;
}
