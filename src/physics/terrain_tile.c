/*
 * terrain_tile.c: RMP terrain tile tests, ported from TM2.EXE (see terrain_tile.h).
 */
#include "terrain_tile.h"

#include "../math/tm2math.h"

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }

int32_t rmp_height_rect(const uint8_t* t, int32_t u, int32_t v) {
    int32_t ev = i16(t, 6), eu = i16(t, 4), z0 = i16(t, 12);
    if (ev == 0 || eu == 0) return z0;
    if (t[1] == 0) {
        int32_t d = i16(t, 16) - z0;
        if (d == 0) return tm2_wmul(i16(t, 14) - z0, u) / eu + z0;
        return tm2_wmul(d, v) / ev + z0;
    }
    int32_t a = z0 + tm2_wmul(i16(t, 14) - z0, u) / eu;
    int32_t z2 = i16(t, 16);
    int32_t b = tm2_wmul(i16(t, 18) - z2, u) / eu + z2;
    return tm2_wmul(v, b - a) / ev + a;
}

int32_t rmp_height_ring(const uint8_t* t, int32_t r, int32_t ang) {
    int32_t a = ang - i16(t, 8);
    while (a < 0) a += 0x1000;
    while (a > 0xb4) a -= 0x1000;
    int32_t span = i16(t, 10) - i16(t, 8);
    int32_t r0 = i16(t, 4), z0 = i16(t, 12);
    int32_t dr = i16(t, 6) - r0;
    int32_t base, slope;
    if (t[1] == 0) {
        base = z0;
        slope = i16(t, 16) - z0;
        if (slope == 0) return span == 0 ? z0 : tm2_wmul(i16(t, 14) - z0, a) / span + z0;
    } else {
        if (span == 0) return z0;     /* original: divide fault */
        base = z0 + tm2_wmul(i16(t, 14) - z0, a) / span;
        int32_t z2 = i16(t, 16);
        slope = (tm2_wmul(i16(t, 18) - z2, a) / span + z2) - base;
    }
    if (dr == 0) return z0;           /* original: divide fault */
    return tm2_wmul(slope, r - r0) / dr + base;
}

uint32_t rmp_isqrt(int32_t v) {
    uint32_t res = 0, bit = 0x40000000;
    do {
        while ((int32_t)bit <= v && (int32_t)res <= (int32_t)(v - bit)) {
            int32_t nv = (int32_t)((uint32_t)v - bit - res);
            res = res >> 1 | bit;
            bit >>= 2;
            v = nv;
            if (bit == 0) return res;
        }
        res >>= 1;
        bit >>= 2;
    } while (bit != 0);
    return res;
}

static void hit(uint8_t* s) { s[0] = 1; s[5] = 1; }

int rmp_tile_test(const uint8_t* body, uint8_t* s, const uint8_t* t, int32_t k) {
    int32_t sx = i32(s, 0x3c), sy = i32(s, 0x40), sz = i32(s, 0x44);
    int32_t dx = sx - i32(t, 20), dy = sy - i32(t, 24);
    int32_t step = i32(body, 8);
    int16_t k16 = (int16_t)k;
    if (t[0] != 2) {
        int32_t sn = i16(t, 8), cs = i16(t, 10), eu = i16(t, 4), ev = i16(t, 6);
        int32_t u = tm2_fx12(tm2_wmul(cs, dx) - tm2_wmul(sn, dy));
        int32_t v = tm2_fx12(tm2_wmul(dy, cs) + tm2_wmul(sn, dx));
        if (!(u >= 0 && v >= 0 && u <= eu && v <= ev && (t[0] != 1 || v <= tm2_wmul(ev, u) / eu))) return 0;
        int32_t h = rmp_height_rect(t, u, v);
        if (!(s[5] == 0 || i16(s, 0x24) <= h)) return 0;
        if (h - sz < step) {
            hit(s); w16(s, 0xc, k16);
        } else {
            int32_t dx2 = dx - i32(body, 0xc), dy2 = dy - i32(body, 0x10);
            int32_t u2 = tm2_fx12(tm2_wmul(dx2, cs) - tm2_wmul(sn, dy2));
            int32_t v2 = tm2_fx12(tm2_wmul(sn, dx2) + tm2_wmul(cs, dy2));
            int32_t h2 = rmp_height_rect(t, u2, v2);
            if (h2 - (sz - i32(body, 0x14)) < step) { hit(s); w16(s, 0xc, k16); w16(s, 0x24, h); return 1; }
            if (i16(body, 0) != k) return 0;
            hit(s); w16(s, 0xc, k16);
        }
        w16(s, 0x24, h);
        return 1;
    }
    /* ring sector */
    int32_t ang = tm2_atan2(dx, dy);
    if (i16(t, 10) > 0x800 && ang < 0) ang += 0x1000;
    if (!(i16(t, 8) <= ang && ang <= i16(t, 10))) return 0;
    int32_t r = (int32_t)rmp_isqrt(tm2_wadd(tm2_wmul(dy, dy), tm2_wmul(dx, dx)));
    if (!(i16(t, 4) <= r && r <= i16(t, 6))) return 0;
    int32_t h = rmp_height_ring(t, r, ang);
    if (!(s[5] == 0 || i16(s, 0x24) <= h)) return 0;
    if (h - sz < step) {
        hit(s); w16(s, 0x24, h);
    } else {
        int32_t dx2 = dx - i32(body, 0xc), dy2 = dy - i32(body, 0x10);
        int32_t ang2 = tm2_atan2(dx2, dy2);
        int32_t r2 = (int32_t)rmp_isqrt(tm2_wadd(tm2_wmul(dy2, dy2), tm2_wmul(dx2, dx2)));
        int32_t h2 = rmp_height_ring(t, r2, ang2);
        if (h2 - (sz - i32(body, 0x14)) < step) { hit(s); w16(s, 0x24, h); w16(s, 0xc, k16); return 1; }
        if (i16(body, 0) != k) return 0;
        hit(s); w16(s, 0x24, h);
    }
    w16(s, 0xc, k16);
    return 1;
}
