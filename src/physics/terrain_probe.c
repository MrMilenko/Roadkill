/*
 * terrain_probe.c: per probe terrain update, ported from TM2.EXE (see terrain_probe.h).
 */
#include "terrain_probe.h"

#include "terrain_tile.h"
#include "../math/tm2math.h"

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }

static const uint8_t* tile_at(const RmpWorld* w, int32_t k) { return w->tiles + (int64_t)k * RMP_TILE_SIZE; }

int32_t rmp_extra_tiles(uint8_t event_type, int32_t zone) {
    if (event_type != 8) return 0;
    if ((zone > 3 && zone < 12) || (zone > 13 && zone < 16) || (zone > 16 && zone < 19)) return 2;
    return 0;
}

/* tile local (u, v) of world offset (dx, dy): rotate by the tile's sin/cos, 12 bit fixed point */
static int32_t rect_height(const uint8_t* t, int32_t dx, int32_t dy) {
    int32_t s = i16(t, 8), c = i16(t, 10);
    int32_t u = tm2_wadd(tm2_wmul(c, dx), -tm2_wmul(s, dy)) / 4096;
    int32_t v = tm2_wadd(tm2_wmul(c, dy), tm2_wmul(s, dx)) / 4096;
    return rmp_height_rect(t, u, v);
}

static int32_t ring_height(const uint8_t* t, int32_t dx, int32_t dy) {
    int32_t a = tm2_atan2(dx, dy);
    int32_t r = (int32_t)rmp_isqrt(tm2_wadd(tm2_wmul(dx, dx), tm2_wmul(dy, dy)));
    return rmp_height_ring(t, r, a);
}

void rmp_probe_slope(const RmpWorld* w, const uint8_t* body, uint8_t* s, int32_t tile) {
    const uint8_t* t = tile_at(w, tile);
    int32_t dx = i32(s, 0x3c) - i32(t, 20), dy = i32(s, 0x40) - i32(t, 24);
    int32_t dx2 = dx - i32(body, 0xc), dy2 = dy - i32(body, 0x10);
    int32_t h1, h2;
    if (t[0] == 2) { h1 = ring_height(t, dx, dy); h2 = ring_height(t, dx2, dy2); }
    else { h1 = rect_height(t, dx, dy); h2 = rect_height(t, dx2, dy2); }
    w16(s, 0x14, (int32_t)((uint32_t)(h1 - h2) << 5) / tm2_frame_ticks60());
}

/* the original's "set the slope latch" block */
static void latch(uint8_t* s) {
    s[1] = 1;
    w16(s, 0x2a, i16(s, 0x44));
    s[0x12] = 0;
    s[0x13] = 0;
}

/* tile has one flat height: z0 == z1 == z2 == z3 */
static int flat(const uint8_t* t) {
    int16_t z = i16(t, 12);
    return z == i16(t, 14) && z == i16(t, 16) && z == i16(t, 18);
}

int rmp_probe_update(const RmpWorld* w, const uint8_t* body, uint8_t* s) {
    int hit = 0;
    int32_t prev = i16(s, 0xa);
    uint8_t was_on = s[0];
    s[5] = 0;

    int32_t zone = i16(s, 0x10);
    for (int32_t k = w->zone_first[zone]; k <= w->zone_last[zone]; k++)
        if (rmp_tile_test(body, s, tile_at(w, k), k)) hit = 1;

    int32_t extra = rmp_extra_tiles(w->event_type, zone);
    s[8] = 0;
    if (extra > 0) {
        for (int32_t k = w->tile_count - extra; k < w->tile_count; k++) {
            if (rmp_tile_test(body, s, tile_at(w, k), k)) {
                s[8] = 1;
                w16(s, 0xe, w->tile_count - k - 1);
                hit = 1;
            }
        }
    }

    if (!hit) {
        if (s[3]) hit = 1;
        w16(s, 0x24, i16(s, 0x26));
    }

    if (hit && !was_on) {
        /* landed */
        w16(s, 0xa, i16(s, 0xc));
        s[0] = 1;
        if (i16(s, 0x24) < i32(s, 0x44)) {
            if (!s[1]) latch(s);
            w16(s, 0x16, i16(s, 0x44));
            w16(s, 0x14, -32 / tm2_frame_ticks60());
        }
    } else if (was_on && !hit) {
        /* left the ground */
        if (!s[1] && i16(s, 0x24) < i16(s, 0x1c) + 10) {
            latch(s);
            rmp_probe_slope(w, body, s, prev);
            s[0x28] = 0;
            s[0x29] = 0;
            return 0;
        }
    } else {
        if (!hit) return 0;
        /* still on the ground: watch for a tile change */
        if (i16(s, 0xc) == prev) return 1;
        w16(s, 0xa, i16(s, 0xc));
        if (!s[1]) {
            int32_t cur = i16(s, 0xa);
            const uint8_t *tp = tile_at(w, prev), *tc = tile_at(w, cur);
            int change = 1;
            if (tp[0] == 2 && tc[0] == 2 && tp[1] != 0 && tc[1] != 0) change = 0;
            if (change && flat(tp) && flat(tc) && i16(tp, 12) == i16(tc, 12)) change = 0;
            if (change) {
                latch(s);
                rmp_probe_slope(w, body, s, prev);
            }
        }
    }
    s[0x28] = 0;
    s[0x29] = 0;
    return hit;
}

void rmp_probe_integrate(uint8_t* s, int on_terrain) {
    int32_t t = tm2_frame_ticks60();
    if (!s[1]) {
        w16(s, 0x14, (int32_t)((uint32_t)(i16(s, 0x24) - i16(s, 0x16)) << 5) / t);
        s[1] = 0;
        w16(s, 0x12, 0);
        w16(s, 0x28, i16(s, 0x28) + t);
        w16(s, 0x16, i16(s, 0x24));
        if (!on_terrain) { w16(s, 0x16, 0); s[0] = 0; }
        return;
    }
    int32_t v = (i16(s, 0x14) * t) / 32;
    int32_t a = i16(s, 0x12);
    if (a < 1) {
        w16(s, 0x2c, 0);
    } else {
        v -= 1;
        w16(s, 0x2c, tm2_wmul(tm2_wmul(tm2_wmul(a, t), -2), a) / 16);
    }
    v += i16(s, 0x2c) / 32;
    if (-32 * t < v) w16(s, 0x12, i16(s, 0x12) + t);
    int16_t h = (int16_t)(i16(s, 0x16) + (int16_t)v);
    w16(s, 0x16, h);
    if (h < i16(s, 0x24) && (8 < i16(s, 0x12) || i16(s, 0x16) + 10 < i16(s, 0x24))) {
        w16(s, 0x12, 0);
        s[1] = 0;
        if (!on_terrain) { w16(s, 0x16, 0); s[0] = 0; }
        else w16(s, 0x16, i16(s, 0x24));
    }
    w16(s, 0x28, 0);
}

void rmp_probe_finish(int32_t speed, int32_t span, uint8_t* s, int on_terrain) {
    int32_t a = (int32_t)(speed < 0 ? 0u - (uint32_t)speed : (uint32_t)speed);
    int32_t t = tm2_frame_ticks60();
    if (!s[3]) {
        int32_t r = (a / 32) / 2;
        if (r < 0) r = -r;
        r = (int32_t)((uint32_t)r << 5) / t;
        int16_t r16 = (int16_t)r;
        if (r < i16(s, 0x14)) w16(s, 0x14, r16);
        else if (i16(s, 0x14) < -r) w16(s, 0x14, -r16);
    }
    if (!s[0] && !s[1]) {
        w16(s, 0x14, -32 / t);
        w16(s, 0x12, 0);
        if (i16(s, 0x28) < 600) w16(s, 0x28, i16(s, 0x28) + t);
    } else {
        rmp_probe_integrate(s, on_terrain);
        if (!s[1]) {
            if (a < span / 2) w16(s, 0x14, (i16(s, 0x14) * a) / (span / 2));
        } else if (i16(s, 0x2a) < i32(s, 0x44)) {
            w16(s, 0x2a, i16(s, 0x44));
        }
    }
}
