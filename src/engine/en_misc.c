/*
 * en_misc.c: small leaves of the engine range, ported from TM2.EXE (see en_misc.h).
 */
#pragma STDC FP_CONTRACT OFF
#include "en_misc.h"

#include <math.h>
#include <string.h>

static uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
static void wr16(uint8_t* p, uint32_t v) { uint16_t w = (uint16_t)v; memcpy(p, &w, 2); }
static uint32_t k1(const uint8_t* r) { return rd32(r + 0x34) << 4 >> 28; }
static uint32_t k2(const uint8_t* r) { return (rd32(r + 0x34) << 1) >> 29; }

void en_prim_set_flag3c(uint8_t* r, int32_t on) {
    if (on) r[0x3f] |= 2; else r[0x3f] &= 0xfd;
}

void en_prim_nop(uint8_t* r) { (void)r; }

void en_prim_set_38(uint8_t* r, int32_t a, int32_t b) {
    r[0x38] &= 0xbf;
    wr32(r + 0x38, rd32(r + 0x38) | ((uint32_t)a & 1) << 6);
    uint16_t w;
    memcpy(&w, r + 0x38, 2);
    wr16(r + 0x38, w & 0xe07fu);
    wr32(r + 0x38, rd32(r + 0x38) | ((uint32_t)b & 0x3f) << 7);
}

void en_prim_set_mode36(uint8_t* r, int32_t mode) {
    uint8_t m = (uint8_t)mode;
    r[0x36] &= 0xf8;
    if (m == 0) { r[0x38] &= 0xfe; return; }
    r[0x36] |= 1;
    if (m == 2) r[0x38] |= 1; else r[0x38] &= 0xfe;
}

void en_prim_set_mode36b(uint8_t* r, int32_t mode) {
    uint8_t m = (uint8_t)mode;
    if (m == 1) { r[0x36] = (uint8_t)((r[0x36] & 0xc7) | 8); return; }
    if (m == 2) {
        uint32_t bits = (rd32(r + 0x34) << 13 >> 29) & 7;
        r[0x36] &= 0xc7;
        wr32(r + 0x34, rd32(r + 0x34) | bits << 19);
        return;
    }
    r[0x36] &= 0xc7;
}

void en_prim_set_word(uint8_t* r, int32_t v) {
    wr16(r + 0x3c + 4 * ((r[0] & 0x7fu) + k2(r)), (uint32_t)v);
}

void en_prim_set_rgb(uint8_t* r, int32_t i, int32_t b, int32_t g, int32_t red) {
    uint8_t* p = r + 0x3c + 4 * i;
    wr32(p, rd32(p) & 0xff000000u);
    wr32(p, rd32(p) | ((uint32_t)(uint8_t)red << 16 | (uint32_t)(uint8_t)g << 8 | (uint8_t)b));
}

void en_prim_set_rgb24(uint8_t* r, int32_t i, int32_t c) {
    uint8_t* p = r + 0x3c + 4 * i;
    wr32(p, (rd32(p) & 0xff000000u) | ((uint32_t)c & 0xffffffu));
}

void en_prim_set_vec(uint8_t* r, int32_t i, int32_t x, int32_t y, int32_t z) {
    wr32(r + 4 + 12 * i, (uint32_t)x);
    wr32(r + 8 + 12 * i, (uint32_t)y);
    wr32(r + 0xc + 12 * i, (uint32_t)z);
}

void en_prim_set_vecp(uint8_t* r, int32_t i, const int32_t v[3]) { en_prim_set_vec(r, i, v[0], v[1], v[2]); }

void en_prim_set_s16x3(uint8_t* r, int32_t i, const uint8_t v[6]) {
    memcpy(r + 0x3c + 4 * k1(r) + 8 * i, v, 6);
}

void en_prim_set_pair(uint8_t* r, int32_t i, int32_t a, int32_t b) {
    uint8_t* p = r + 0x3c + 4 * k2(r) + 4 * i;
    wr16(p, (uint32_t)a);
    wr16(p + 2, (uint32_t)b);
}

void en_prim_set_uv_rect(uint8_t* r, int32_t u, int32_t v, int32_t w, int32_t h) {
    uint8_t* p = r + 0x3c + 4 * k2(r);
    uint32_t u1 = (uint32_t)w + (uint32_t)u, v1 = (uint32_t)v + (uint32_t)h;
    wr16(p + 0, (uint32_t)u);  wr16(p + 2, (uint32_t)v);
    wr16(p + 4, u1);           wr16(p + 6, (uint32_t)v);
    wr16(p + 8, u1);           wr16(p + 10, v1);
    wr16(p + 12, (uint32_t)u); wr16(p + 14, v1);
}

void en_cfg_store(EnRenderCfg* c, uint32_t off, uint32_t v) { memcpy((uint8_t*)c + off, &v, 4); }

void en_cfg_set_tex_shift(EnRenderCfg* c, int32_t n) {
    c->tex_shift = n;
    c->tex_inv = (float)(1.0 / (double)(int32_t)(1u << (n & 31)));
}

void en_cfg_set_depth_scale(EnRenderCfg* c, int32_t v) { c->depth_scale = (float)v; }

int32_t en_cfg_get_depth_scale(const EnRenderCfg* c) {
    double t = trunc((double)c->depth_scale);
    if (!(t >= -2147483648.0 && t < 2147483648.0)) return INT32_MIN;
    return (int32_t)t;
}

int32_t en_render_mode_get(const int32_t* mode) { return *mode; }
void en_render_mode_set(int32_t* mode, int32_t v) { if (v >= 0 && v <= 3) *mode = v; }

void en_named_set_str(uint8_t* r, uint32_t off, const char* s) {
    uint8_t* d = r + off;
    for (;;) {
        if ((*d++ = (uint8_t)*s++) == 0) break;
        if ((*d++ = (uint8_t)*s++) == 0) break;
    }
}
uint32_t en_named_addr(uint32_t r, uint32_t off) { return r + off; }
void en_named_set32(uint8_t* r, uint32_t off, uint32_t v) { wr32(r + off, v); }
uint32_t en_named_get32(const uint8_t* r, uint32_t off) { return rd32(r + off); }
uint32_t en_magic(void) { return 0x424d5144u; }
int32_t en_check_f170(int32_t a, int32_t b) { (void)a; return b == 0xf170 ? 0 : 2; }

/* 0x0048e468. Per type: +2, +3, +1, +0x3f, the +0x36 word, +0, how many of +0x43 / +0x47 / +0x4b
 * get +0x3f's value. (The +0x36 word and +0 were zeroed above, so the original's masks of their
 * old values give 0.) */
void en_prim_init(uint8_t* r, int32_t type) {
    static const struct { uint8_t b2, b3, b1, b3f; uint16_t w36; uint8_t b0, nv; } t[10] = {
        { 0x10, 4, 1, 0x40, 0x0042, 0, 0 }, { 0x11, 5, 2, 0x50, 0x0042, 0, 1 }, { 0x16, 5, 1, 0x20, 0x0143, 0, 0 },
        { 0x18, 6, 1, 0x28, 0x0144, 0, 0 }, { 0x1a, 9, 1, 0x24, 0x1683, 3, 0 }, { 0x1d, 0xb, 1, 0x2c, 0x1784, 4, 0 },
        { 0x18, 7, 3, 0x30, 0x0383, 0, 2 }, { 0x1b, 9, 4, 0x38, 0x0484, 0, 3 }, { 0x1c, 0xb, 3, 0x34, 0x38c3, 3, 2 },
        { 0x20, 0xe, 4, 0x3c, 0x49c4, 4, 3 },
    };
    wr32(r + 0x34, 0);
    wr32(r + 0x38, rd32(r + 0x38) & 0xffc0ffbfu);
    r[0] = 0;
    r[0x3a] |= 0x3f;
    uint8_t k = (uint8_t)type;
    if (k > 9) return;
    r[2] = t[k].b2; r[3] = t[k].b3; r[1] = t[k].b1; r[0x3f] = t[k].b3f;
    wr16(r + 0x36, t[k].w36);
    r[0] = t[k].b0;
    for (int i = 0; i < t[k].nv; i++) r[0x43 + 4 * i] = t[k].b3f;
}
