/*
 * gpu_dispatch.c: command_dispatch 0x004ad874, ported from TM2.EXE (see gpu.h, docs/RENDERER.md).
 *
 * Ported command codes (byte +7 with bit 1, PS1 semi transparency, cleared):
 *   0x20 flat triangle   +4 u8 r, g, b; +8 i16 x, y per vertex at +8, +0xc, +0x10
 *   0x28 flat quad       vertices at +8, +0xc, +0x10, +0x14, edges 0-1, 1-3, 3-2, 2-0
 *   0xb0 flat n-gon      +4 u16 screen color; +6 u8 n; +8 n x (i32 x, i32 y) already in
 *                        1/32 pixel with the guard band (x * 32 + 0x200)
 *   0x40 line            +4 r, g, b; +8 i16 x0, y0; +0xc i16 x1, y1; clipped, open pixels only
 *   0x50 line            +8 x0, y0; +0x10 x1, y1; ORIGINAL QUIRK: always color 0x00fe
 *   0xc0 line            as 0x40 with no clipping and no masks
 *   anything else        nothing
 *   0x24 0x2c 0x74 0x7c 0x90 textured: see gpu_tex.c
 * 0xa0 (3D, float) is src/render/gpu3d.c (docs/RENDERER_3D.md); it needs g->g3d set, else -1.
 */
#include "gpu3d.h"
#include "gpu.h"

#include <string.h>

static int16_t i16at(const uint8_t* p, int o) { int16_t v; memcpy(&v, p + o, 2); return v; }
static int32_t i32at(const uint8_t* p, int o) { int32_t v; memcpy(&v, p + o, 4); return v; }

/* screen coordinate to edge units: (v << 5) + 0x200, 32 bit wrap */
static int32_t sub(int16_t v) { return (int32_t)(((uint32_t)(int32_t)v << 5) + 0x200u); }

static void set_flat_color(TM2Gpu* g, uint32_t c) {
    g->r.color16 = (uint16_t)c;
    uint32_t c2 = (c & 0xffff) << 16 | (c & 0xffff);
    g->r.color32 = c2;
    g->r.color64[0] = c2;
    g->r.color64[1] = c2;
}

static uint32_t rgb24(const uint8_t* p) {
    return (uint32_t)p[4] << 16 | (uint32_t)p[5] << 8 | p[6];
}

static void walk_polygon(TM2Gpu* g, int32_t (*v)[2], int n) {
    for (int i = 0; i < n; i++) gpu_edge_setup(GPU_EDGE(g, i), v[i], v[(i + 1) % n]);
    for (int i = 0; i < n; i++) gpu_edge_walk(g, GPU_EDGE(g, i));
}

int gpu_dispatch(TM2Gpu* g, const uint8_t* p) {
    g->r.ntiles = 0;
    g->r.f7fc = 0;
    g->r.stamp++;
    g->r.hit = 0;
    uint8_t code = p[7] & 0xfd;
    switch (code) {
    case 0x20: case 0x28: {
        int n = code == 0x20 ? 3 : 4;
        int32_t v[4][2];
        if (n == 3) {
            for (int i = 0; i < 3; i++) { v[i][0] = sub(i16at(p, 8 + 4 * i)); v[i][1] = sub(i16at(p, 10 + 4 * i)); }
        } else {
            static const int ord[4] = {0, 1, 3, 2};
            for (int i = 0; i < 4; i++) { v[i][0] = sub(i16at(p, 8 + 4 * ord[i])); v[i][1] = sub(i16at(p, 10 + 4 * ord[i])); }
        }
        g->r.row_min = 1000;
        g->r.row_max = 0;
        walk_polygon(g, v, n);
        g->r.color24 = rgb24(p);
        set_flat_color(g, gpu_rgb24_to_16(g->r.format, g->r.color24));
        if (g->r.hit) gpu_flat_collect(g);
        return 0;
    }
    case 0xb0: {
        g->r.row_max = 0;
        g->r.row_min = 1000;
        uint16_t c;
        memcpy(&c, p + 4, 2);
        set_flat_color(g, c);
        int n = p[6];
        /* edges i -> i + 1 for i < n - 1 in slots 0.., the closing edge n - 1 -> 0 in slot
         * n - 1 (slot -1 when n = 0: an edge from +0 to +8). n > 8 runs into the tile grid in
         * the original (the edge slots end at 0x00be6150); the port caps it. */
        if (n > GPU_EDGES - 1) n = GPU_EDGES - 1;
        int32_t v[GPU_EDGES][2];
        for (int i = 0; i < n; i++) { v[i][0] = i32at(p, 8 + 8 * i); v[i][1] = i32at(p, 12 + 8 * i); }
        for (int i = 0; i < n - 1; i++) gpu_edge_setup(GPU_EDGE(g, i), v[i], v[i + 1]);
        if (n > 0) {
            gpu_edge_setup(GPU_EDGE(g, n - 1), v[n - 1], v[0]);
        } else {
            int32_t a[2] = {i32at(p, 0), i32at(p, 4)}, b[2] = {i32at(p, 8), i32at(p, 12)};
            gpu_edge_setup(GPU_EDGE(g, -1), a, b);
        }
        for (int i = 0; i < n; i++) gpu_edge_walk(g, GPU_EDGE(g, i));
        if (g->r.hit) gpu_flat_collect(g);
        return 0;
    }
    case 0x40: case 0xc0:
        g->r.color24 = rgb24(p);
        g->r.color16 = (uint16_t)gpu_rgb24_to_16(g->r.format, g->r.color24);
        if (code == 0x40) gpu_line(g, i16at(p, 8), i16at(p, 10), i16at(p, 12), i16at(p, 14));
        else gpu_line_raw(g, i16at(p, 8), i16at(p, 10), i16at(p, 12), i16at(p, 14));
        return 0;
    case 0x50:
        g->r.color16 = 0xfe;
        gpu_line(g, i16at(p, 8), i16at(p, 10), i16at(p, 16), i16at(p, 18));
        return 0;
    case 0x24: case 0x2c: case 0x74: case 0x7c: case 0x90:
        gpu_tex_prim(g, p, code);
        return 0;
    case 0xa0:
        /* the 3D float n-gon (src/render/gpu3d.c); draws nothing until its state is set up */
        if (!g->g3d) return -1;
        gpu3d_prim(g, g->g3d, (const Gpu3dLights*)g->lights, p);
        return 0;
    default:
        return 0;
    }
}
