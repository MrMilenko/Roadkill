/*
 * grp.c: GRP terrain zones, ported from TM2.EXE (see grp.h).
 */
#include "grp.h"

#include "terrain_tile.h"
#include "../math/tm2math.h"

#include <string.h>

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }

/* shape test in zone local (u, v), 0 <= u <= eu and 0 <= v <= ev already checked */
static int shape_inside(int shape, int32_t u, int32_t v, int32_t eu, int32_t ev) {
    if (shape == 0) return 1;
    if (shape == 2) return eu != 0 && v <= tm2_wmul(ev, u) / eu;
    if (shape != 1) return 0;
    int32_t q = eu / 4, hv = ev / 2;
    if (hv == 0) return 0;
    if (u < q) {
        if (hv < v) return tm2_wmul(v - hv, q) / hv < u;
        return tm2_wmul(hv - v, q) / hv < u;
    }
    if (u < eu - q) return 1;
    u -= tm2_wmul(eu, 3) / 4;
    if (v < hv) return u < tm2_wmul(v, q) / hv;
    return u < tm2_wmul(hv - (v - hv), q) / hv;
}

int32_t grp_find_zone(const uint8_t* g, int32_t x, int32_t y, int32_t z) {
    int32_t count = g[0x1790];
    int32_t best = count - 1;
    for (int32_t i = 0; i < count; i++) {
        int32_t dx = x - i32(g, 0xc + 12 * i), dy = y - i32(g, 0x10 + 12 * i);
        int32_t u = dx, v = dy;
        if (g[0x11d2 + i]) {
            int32_t s = i16(g, 0x2c8 + 2 * i), c = i16(g, 0x30e + 2 * i);
            u = tm2_wadd(tm2_wmul(dx, c), -tm2_wmul(dy, s)) / 4096;
            v = tm2_wadd(tm2_wmul(dx, s), tm2_wmul(dy, c)) / 4096;
        }
        int32_t eu = i32(g, 0x1b0 + 4 * i), ev = i32(g, 0x23c + 4 * i);
        if (u < 0 || v < 0 || u > eu || v > ev) continue;
        if (!shape_inside(g[0x123b + i], u, v, eu, ev)) continue;
        if (i32(g, 0x14 + 12 * i) < z + 0x50) return i;
        best = i;
    }
    return best;
}

int32_t grp_ground_height(const RmpWorld* w, const uint8_t* g, const int32_t p[3]) {
    /* the original builds a probe block on the stack (0x004300e8 clears it); only these fields
     * are read by the tile test */
    uint8_t body[0x18], st[0x48];
    int32_t z = p[2];
    int hit = 0;
    int32_t height;
    do {
        int32_t zone = grp_find_zone(g, p[0], p[1], z);
        memset(body, 0, sizeof body);
        memset(st, 0, sizeof st);
        body[0] = 0xff; body[1] = 0xff;                         /* current tile -1 */
        body[8] = 0x78;                                         /* step height 120 */
        int16_t floor_z = i16(g, 0x14 + 12 * zone);
        st[0x24] = (uint8_t)floor_z; st[0x25] = (uint8_t)(floor_z >> 8);
        for (int k = 0; k < 3; k++) {
            uint32_t v = (uint32_t)p[k];
            st[0x3c + 4 * k] = (uint8_t)v; st[0x3d + 4 * k] = (uint8_t)(v >> 8);
            st[0x3e + 4 * k] = (uint8_t)(v >> 16); st[0x3f + 4 * k] = (uint8_t)(v >> 24);
        }
        for (int32_t k = w->zone_first[zone]; k <= w->zone_last[zone]; k++)
            if (rmp_tile_test(body, st, w->tiles + (int64_t)k * RMP_TILE_SIZE, k)) hit = 1;
        height = i16(st, 0x24);
        if (g[0x11f5 + zone]) hit = 1;
        else {
            z -= 0x28;
            if (z < 0) hit = 1;
        }
    } while (!hit);
    return height;
}
