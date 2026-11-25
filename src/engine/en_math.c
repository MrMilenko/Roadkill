/*
 * en_math.c: small engine leaves, ported from TM2.EXE 0x0048cca8, 0x0048cd3c, 0x0048ce88,
 * 0x0048bbcc, 0x004aab90, 0x0048b460 (see en_math.h).
 */
#include "en_math.h"

#include <string.h>

static int32_t mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t fx(uint32_t s) {
    int32_t v = (int32_t)s;
    return (int32_t)(s + (v < 0 ? 0xfffu : 0u)) >> 12;
}

void en_mat_vec_mul(const int32_t m[9], const int32_t v[3], int32_t out[3]) {
    int32_t r[3];
    for (int i = 0; i < 3; i++)
        r[i] = fx((uint32_t)mul(m[3 * i], v[0]) + (uint32_t)mul(m[3 * i + 1], v[1]) + (uint32_t)mul(m[3 * i + 2], v[2]));
    memcpy(out, r, sizeof r);
}

void en_mat_t_vec_mul_s16(const int32_t m[9], const int16_t v[3], int32_t out[3]) {
    int32_t r[3];
    for (int i = 0; i < 3; i++)
        r[i] = fx((uint32_t)mul(m[i], v[0]) + (uint32_t)mul(m[i + 3], v[1]) + (uint32_t)mul(m[i + 6], v[2]));
    memcpy(out, r, sizeof r);
}

void en_copy_words(int32_t* dst, const int32_t* src, int32_t n) {
    for (int32_t i = 0; i < n; i++) dst[i] = src[i];
}

void en_view_set_size(EnViewSize* v, int32_t w, int32_t h, int hw) {
    v->w = w;
    v->h = h;
    if (hw) {
        v->scale_x = (float)w;
        v->scale_y = (float)h;
    } else {
        v->scale_x = (float)((double)(int32_t)((uint32_t)w << 5) + -0.5);
        v->scale_y = (float)(-0.5 + (double)(int32_t)((uint32_t)h << 5));
    }
}

void en_name_init(uint8_t dst[5]) {
    memset(dst, 0, 5);
}

static uint32_t rd32(uint8_t* m, uint32_t base, size_t size, uint32_t a) {
    uint32_t o = a - base, v = 0;
    if ((size_t)o <= size && size - o >= 4) memcpy(&v, m + o, 4);
    return v;
}

void en_mesh_set_quadrants(uint8_t* mem, uint32_t base, size_t size, uint32_t node) {
    uint32_t verts = rd32(mem, base, size, node + 4);
    uint32_t p = rd32(mem, base, size, node + 0xc);
    for (uint32_t i = 0; i < rd32(mem, base, size, node + 0x10); i++) {
        int16_t idx = (int16_t)(rd32(mem, base, size, p + 2) >> 16);
        uint32_t vw = rd32(mem, base, size, verts + 8u * (uint32_t)(int32_t)idx);
        int16_t x = (int16_t)vw, y = (int16_t)(vw >> 16);
        int alt = i == 2 || i == 4 || i == 6;
        int q;
        if (x < 0) q = y >= 0 ? (alt ? 1 : 2) : (alt ? 0 : 3);
        else q = y < 0 ? (alt ? 2 : 1) : (alt ? 3 : 0);
        uint32_t o = p - base;
        if ((size_t)o <= size && size - o >= 0x18) {
            mem[o + 0x15] &= 0x1f;
            uint32_t f;
            memcpy(&f, mem + o + 0x14, 4);
            f |= (uint32_t)(q & 7) << 13;
            memcpy(mem + o + 0x14, &f, 4);
        }
        uint32_t sz = (size_t)(p + 2 - base) < size ? mem[p + 2 - base] : 0;
        p += sz * 4;
    }
}
